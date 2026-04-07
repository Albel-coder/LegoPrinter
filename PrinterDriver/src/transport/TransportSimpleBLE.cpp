#include "TransportSimpleBLE.h"

#include <algorithm>
#include <chrono>
#include <cctype>

using namespace std::chrono_literals;

TransportSimpleBLE::TransportSimpleBLE() = default;

TransportSimpleBLE::~TransportSimpleBLE() {
    stopScan();
    disconnect();
}

bool TransportSimpleBLE::startScan(int timeoutSeconds) {
    stopScan();
    
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        scanResults.clear();
        discoveredPeripherals.clear();
    }

    stopScanRequested = false;
    scanning = true;
    scanThread = std::thread(&TransportSimpleBLE::scanWorker, this, timeoutSeconds);
    return true;
}

void TransportSimpleBLE::stopScan() {
    stopScanRequested = true;
    if (scanThread.joinable()) {
        scanThread.join();
    }
    scanning = false;
}

std::vector<DeviceInfo> TransportSimpleBLE::getScanResults() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return scanResults;
}

bool TransportSimpleBLE::connect(const std::string& address) {
    if (isConnected()) {
        return true;
    }

    SimpleBLE::Peripheral printerPeripheral;

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it = discoveredPeripherals.find(address);
        if (it != discoveredPeripherals.end()) {
            printerPeripheral = it->second;
        }
    }

    if (!printerPeripheral.initialized()) {
        if (!findPeripheralByAddress(address, printerPeripheral)) {
            return false;
        }
    }

    try {
        printerPeripheral.connect();
        if (!printerPeripheral.is_connected()) {
            return false;
        }

        peripheral = printerPeripheral;
        connectedAddress = address;

        maxWriteSize = 32;
        LOG_BLUETOOTH("maxWriteSize = %zu", maxWriteSize);

        cacheServicesAndCharacteristics();

        if (connectionCallback) {
            connectionCallback(true, connectedAddress);
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

bool TransportSimpleBLE::disconnect() {
    LOG_BLUETOOTH("TransportSimpleBLE::disconnect: start");

    bool wasConnected = false;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        wasConnected = peripheral.initialized() && peripheral.is_connected();
        if (wasConnected) {
            for (const auto& [key, cb] : subscriptions) {
                try {
                    peripheral.unsubscribe(key.serviceUuid, key.characteristicUuid);
                }
                catch (const std::exception& ex) {
                    LOG_ERROR("Exception during unsubscribe: %s", ex.what());
                }
                catch (...) {}
            }
        }
    }

    if (wasConnected) {
        try {
            LOG_BLUETOOTH("Calling peripheral.disconnect()");
            peripheral.disconnect();
            LOG_BLUETOOTH("peripheral.disconnect() returned");
        }
        catch (const std::exception& ex) {
            LOG_ERROR("Exception during peripheral disconnect: %s", ex.what());
        }
        catch (...) {
            LOG_ERROR("Unknown exception during peripheral disconnect");
        }
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        clearCache();
        connectedAddress.clear();
        peripheral = SimpleBLE::Peripheral();
    }

    if (connectionCallback) {
        LOG_BLUETOOTH("Calling connectionCallback(false)");
        connectionCallback(false, "");
    }

    LOG_BLUETOOTH("TransportSimpleBLE::disconnect: exit");
    return true;
}

bool TransportSimpleBLE::isConnected() {
    std::lock_guard<std::mutex> lock(stateMutex);
    try {
        return peripheral.initialized() && peripheral.is_connected();
    }
    catch (...) {
        return false;
    }
}

std::string TransportSimpleBLE::getConnectedAddress() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return connectedAddress;
}

std::vector<std::string> TransportSimpleBLE::getServices() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return cachedServices;
}

std::vector<Characteristic> TransportSimpleBLE::getCharacteristics(const std::string& serviceUUid) const {
    std::lock_guard<std::mutex> lock(stateMutex);
    auto it = cachedCharacteristics.find(serviceUUid);
    if (it != cachedCharacteristics.end()) {
        return it->second;
    }

    return {};
}

bool TransportSimpleBLE::read(const Characteristic& characteristic, std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!peripheral.is_connected()) {
        return false;
    }

    try {
        out = peripheral.read(characteristic.serviceUUid, characteristic.characteristicUuid);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool TransportSimpleBLE::write(const Characteristic& characteristic, const uint8_t* data, size_t length, bool withResponse) {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!peripheral.is_connected()) {
        return false;
    }

    try {
        std::vector<uint8_t> payload(data, data + length);
        if (withResponse) {
            peripheral.write_request(characteristic.serviceUUid, characteristic.characteristicUuid, payload);;
        }
        else {
            peripheral.write_command(characteristic.serviceUUid, characteristic.characteristicUuid, payload);
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

bool TransportSimpleBLE::subscribe(const Characteristic& characteristic, DataCallback callback) {
    if (!peripheral.is_connected()) {
        return false;
    }

    try {
        CharKey key{ characteristic.serviceUUid, characteristic.characteristicUuid };
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            subscriptions[key] = std::move(callback);
        }

        peripheral.notify(characteristic.serviceUUid, characteristic.characteristicUuid, 
            [this, characteristic](const std::vector<uint8_t>& data) {
            dispatchNotification(characteristic, data);
        });

        return true;
    }
    catch (...) {
        return false;
    }
}

bool TransportSimpleBLE::unsubscribe(const Characteristic& characteristic) {
    if (!peripheral.is_connected()) {
        return false;
    }

    try {
        CharKey key{ characteristic.serviceUUid, characteristic.characteristicUuid };
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            subscriptions.erase(key);
        }
        peripheral.unsubscribe(characteristic.serviceUUid, characteristic.characteristicUuid);
        return true;
    }
    catch (...) {
        return false;
    }
}

size_t TransportSimpleBLE::getMaxWriteSize() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return maxWriteSize;
}

void TransportSimpleBLE::setConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(stateMutex);
    connectionCallback = std::move(callback);
}

void TransportSimpleBLE::scanWorker(int timeoutSeconds) {
    try {
        auto adapters = SimpleBLE::Adapter::get_adapters();
        if (adapters.empty()) {
            scanning = false;
            return;
        }

        LOG_BLUETOOTH("scanWorker: adapters = %zu", adapters.size());
        for (const auto& device : scanResults) {
            LOG_BLUETOOTH("scan results: %s [%s] rssi = %d",
                device.name.c_str(),
                device.rssi);
        }

        adapter = adapters[0];

        adapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral scannedPeripheral) {
            DeviceInfo device;
            device.address = scannedPeripheral.address();
            device.name = scannedPeripheral.identifier();
            device.rssi = static_cast<int16_t>(scannedPeripheral.rssi());

            LOG_BLUETOOTH("scan found: name = '%s' address = %s, rssi = %d",
                scannedPeripheral.identifier().c_str(),
                scannedPeripheral.address().c_str(),
                static_cast<int>(scannedPeripheral.rssi()));

            for (const auto& md : scannedPeripheral.manufacturer_data()) {
                device.manufacturerData[md.first] = md.second;
            }

            {
                std::lock_guard<std::mutex> lock(stateMutex);
                auto it = std::find_if(scanResults.begin(), scanResults.end(), [&](const DeviceInfo& otherDevice) {
                    return otherDevice.address == device.address;
                });

                if (it == scanResults.end()) {
                    scanResults.push_back(std::move(device));
                }

                discoveredPeripherals[device.address] = scannedPeripheral;
            }
        });

        adapter.scan_start();

        const auto start = std::chrono::steady_clock::now();
        while (!stopScanRequested) {
            if (std::chrono::steady_clock::now() - start >= std::chrono::seconds(timeoutSeconds)) {
                break;
            }
            std::this_thread::sleep_for(50ms);
        }

        adapter.scan_stop();
    }
    catch (...) {
        try {
            adapter.scan_stop();
        } 
        catch (...) {}
    }

    scanning = false;
}

bool TransportSimpleBLE::findPeripheralByAddress(const std::string& address, SimpleBLE::Peripheral& out) {
    std::lock_guard<std::mutex> lock(stateMutex);
    try {
        auto adapters = SimpleBLE::Adapter::get_adapters();
        if (adapters.empty()) return false;

        adapter = adapters[0];
        adapter.scan_start();
        std::this_thread::sleep_for(2s);
        adapter.scan_stop();

        auto peripherals = adapter.scan_get_results();
        for (auto& p : peripherals) {
            if (p.address() == address) {
                out = p;
                return true;
            }
        }
    }
    catch (...) {
        return false;
    }

    return false;
}

void TransportSimpleBLE::cacheServicesAndCharacteristics() {
    std::lock_guard<std::mutex> lock(stateMutex);
    cachedCharacteristics.clear();
    cachedCharacteristics.clear();

    try {
        for (auto& service : peripheral.services()) {
            cachedServices.push_back(service.uuid());

            std::vector<Characteristic> chars;
            for (auto& characteristic : service.characteristics()) {
                chars.push_back(Characteristic{ service.uuid(), characteristic.uuid() });
            }
            cachedCharacteristics[service.uuid()] = std::move(chars);
        }
    }
    catch (...) {}
}

void TransportSimpleBLE::clearCache() {
    std::lock_guard<std::mutex> lock(stateMutex);
    cachedCharacteristics.clear();
    cachedServices.clear();
}

void TransportSimpleBLE::dispatchNotification(const Characteristic& characteristic, const std::vector<uint8_t>& data) {
    DataCallback callback;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        auto it = subscriptions.find(CharKey{ characteristic.serviceUUid, characteristic.characteristicUuid });
        if (it != subscriptions.end()) {
            callback = it->second;
        }
    }

    if (callback) {
        callback(characteristic, data.data(), data.size());
    }
}
