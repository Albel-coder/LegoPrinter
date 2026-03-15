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

bool TransportSimpleBLE::isConnected() {
    try {
        return peripheral.is_connected();
    }
    catch (...) {
        return false;
    }
}

bool TransportSimpleBLE::startScan(int timeoutSeconds) {
    stopScan();
    
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        scanResults.clear();
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
    disconnect();

    SimpleBLE::Peripheral printerPeripheral;
    if (!findPeripheralByAddress(address, printerPeripheral)) {
        return false;
    }

    try {
        printerPeripheral.connect();
        if (!printerPeripheral.is_connected()) {
            return false;
        }

        peripheral = printerPeripheral;
        connectedAddress = address;
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
    std::lock_guard<std::mutex> lock(stateMutex);

    if (peripheral.initialized()) {
        try {
            if (peripheral.is_connected()) {
                peripheral.disconnect();
            }
        }
        catch (...) {}
    }

    subscriptions.clear();
    clearCache();
    connectedAddress.clear();

    if (connectionCallback) {
        connectionCallback(false, "");
    }
    return true;
}

bool TransportSimpleBLE::isConnected() {
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
    if (!isConnected()) return false;

    try {
        out = peripheral.read(characteristic.serviceUUid, characteristic.characteristicUuid);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool TransportSimpleBLE::write(const Characteristic& characteristic, const uint8_t* data, size_t length, bool withResponse) {
    if (!isConnected()) return false;

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
    if (!isConnected()) return false;

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
    if (!isConnected()) return false;

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

        adapter = adapters[0];

        adapter.set_callback_on_scan_found([this](SimpleBLE::Peripheral scannedPeripheral) {
            DeviceInfo device;
            device.address = scannedPeripheral.address();
            device.name = scannedPeripheral.identifier();
            device.rssi = static_cast<int16_t>(scannedPeripheral.rssi());

            for (const auto& md : scannedPeripheral.manufacturer_data()) {
                device.manufacturerData[md.first] = md.second;
            }

            std::lock_guard<std::mutex> lock(stateMutex);
            auto it = std::find_if(scanResults.begin(), scanResults.end(), [&](const DeviceInfo& x) {
                return x.address == device.address;
            });

            if (it == scanResults.end()) {
                scanResults.push_back(std::move(device));
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
