#include "PrinterDriver.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <fstream>
#include <thread>
#include <iterator>

namespace {
    static std::vector<uint8_t> readBinaryFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }

        return {
            std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
        };
    }

    static bool isLikelyMpy(const std::vector<uint8_t>& data) {
        return data.size() >= 3 && data[0] == 'M' && data[1] == 'P' && data[2] == 'Y';
    }
} // namespace

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transportPointer) 
    : transport(std::move(transportPointer)) {

    motorManager = std::make_unique<MotorManager>(*transport);
    bootloaderProtocol = std::make_unique<BootloaderProtocol>(*transport);
    runtime = std::make_unique<RuntimeSession>(*transport);
    printerProtocol = std::make_unique<PrinterProtocol>(*transport);
    
    gLog.setLogCategories(LOG_CATEGORY_ALL);

    LOG_INFO("PrinterImplementation created");
}

PrinterDriver::~PrinterDriver() {
    LOG_INFO("PrinterImplementation destroyed");
}

bool PrinterDriver::isLegoHub(const DeviceInfo& device) {
    if (device.manufacturerData.find(0x0397) != device.manufacturerData.end()) {
        return true;
    }

    const std::string name = device.name;
    return name.find("LEGO") != std::string::npos ||
        name.find("Hub") != std::string::npos ||
        name.find("Technic") != std::string::npos ||
        name.find("CONTROL") != std::string::npos ||
        name.find("Boost") != std::string::npos ||
        name.find("SPIKE") != std::string::npos ||
        name.find("Move") != std::string::npos;
}

std::vector<DeviceInfo> PrinterDriver::filterAndSortHubs(const std::vector<DeviceInfo>& input, bool legoOnly) const {
    std::vector<DeviceInfo> out;
    out.reserve(input.size());

    for (const auto& device : input) {
        if (!legoOnly || isLegoHub(device)) {
            out.push_back(device);
        }
    }

    std::sort(out.begin(), out.end(), [](const DeviceInfo& firstDevice, const DeviceInfo secondDevice) {
        return firstDevice.rssi > secondDevice.rssi; // strongest first
    });

    return out;
}

void PrinterDriver::rememberHub(const DeviceInfo& device) {
    if (device.address.empty()) {
        return;
    }

    auto it = std::find_if(recentHubs.begin(), recentHubs.end(), [&](const DeviceInfo& otherDevice) {
        return otherDevice.address == device.address;
    });

    if (it != recentHubs.end()) {
        *it = device;
        DeviceInfo front = device;
        recentHubs.erase(it);
        recentHubs.insert(recentHubs.begin(), std::move(front));
    }
    else {
        recentHubs.insert(recentHubs.begin(), device);
    }

    if (recentHubs.size() > 10) {
        recentHubs.resize(10);
    }
}

std::string PrinterDriver::resolveAddress(const std::string& address) const {
    if (!address.empty()) {
        return address;
    }
    if (!lastKnownAddress.empty()) {
        return lastKnownAddress;
    }

    return {};
}

std::vector<DeviceInfo> PrinterDriver::scan(int timeoutSeconds, bool legoOnly) {
    scanResults.clear();
    LOG_BLUETOOTH("scan(timeoutSeconds=%d, legoOnly=%s)", timeoutSeconds, legoOnly ? "true" : "false");

    if (!transport->startScan(timeoutSeconds)) {
        LOG_ERROR("Failed to start scan");
        return {};
    }

    std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));
    transport->stopScan();

    auto rawData = transport->getScanResults();
    scanResults = filterAndSortHubs(rawData, legoOnly);

    LOG_BLUETOOTH("Scan finished, %zu candidate(s)", scanResults.size());
    
    for (const auto& device : scanResults) {
        LOG_BLUETOOTH("  %s [%s] rssi = %d", device.name.c_str(), device.address.c_str(), device.rssi);
    }
    
    return scanResults;
}

bool PrinterDriver::connectAuto(int timeoutMs, bool legoOnly) {
    LOG_BLUETOOTH("connectAuto(timeoutMs=%d)", timeoutMs);

    if (transport->isConnected()) {
        LOG_BLUETOOTH("connectAuto: already connected to %s", transport->getConnectedAddress().c_str());
        bool disconnectResult = transport->disconnect();
        return true;
    }

    const int scanSeconds = std::max(1, (timeoutMs + 999) / 1000);
    if (!transport->startScan(scanSeconds)) {
        LOG_ERROR("Failed to start auto-scan");
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<std::string> tried;

    auto raw = transport->getScanResults();
    LOG_BLUETOOTH("connectAuto: raw scan results = %zu", raw.size());

    for (const auto& device : raw) {
        LOG_BLUETOOTH("  device: name = '%s', address = %s, rssi = '%s', lego = %s",
            device.name.c_str(),
            device.address.c_str(),
            device.rssi, isLegoHub(device) ? "true" : "false");
    }

    while (std::chrono::steady_clock::now() < deadline) {
        auto current = filterAndSortHubs(transport->getScanResults(), legoOnly);
            
        if (current.empty() && legoOnly) {
            LOG_BLUETOOTH("connectAuto: no LEGO hubs found, trying all devices");

            current = filterAndSortHubs(transport->getScanResults(), false);
        }

        // Prefer last known hub if it is present
        if (!lastKnownAddress.empty()) {
            auto it = std::find_if(current.begin(), current.end(), [&](const DeviceInfo& device) {
                return device.address == lastKnownAddress;
            });

            if (it != current.end()) {
                LOG_BLUETOOTH("connectAuto: trying last known hub: %s", it->address.c_str());
                transport->stopScan();
                if (connect(it->address)) {
                    return true;
                }
                tried.push_back(it->address);
            }
        }

        for (const auto& device : current) {
            if (std::find(tried.begin(), tried.end(), device.address) != tried.end()) {
                continue;
            }

            LOG_BLUETOOTH("connectAuto: trying %s [%s] rssi=%d", 
                device.name.c_str(), device.address.c_str(), device.rssi);

            transport->stopScan();
            if (connect(device.address)) {
                return true;
            }
            tried.push_back(device.address);

            // Restart scan after a failed attempt
            if (!transport->startScan(scanSeconds)) {
                LOG_ERROR("Failed to restart scan during connectAuto");
                return false;
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    transport->stopScan();
    LOG_BLUETOOTH("connectAuto finished: tried = %zu", tried.size());
    LOG_ERROR("No hub found");
    return false;
}

bool PrinterDriver::connect(const std::string& address) {
    if (address.empty()) {
        LOG_ERROR("Empty address");
        return false;
    }

    LOG_BLUETOOTH("connect(%s)", address.c_str());

    // Generate transport connection: good for official LEGO firmware too
    if (!transport->connect(address)) {
        LOG_ERROR("Transport connect failed");
        return false;
    }

    connectedAddress = address;
    lastKnownAddress = address;

    // Try remember full info if we saw this device in the last scan
    auto it = std::find_if(scanResults.begin(), scanResults.end(), [&](const DeviceInfo& device) {
        return device.address == address;
    });

    if (it != scanResults.end()) {
        rememberHub(*it);
    }
    else {
        DeviceInfo minimal;
        minimal.address = address;
        minimal.name = address;
        minimal.rssi = 0;
        rememberHub(minimal);
    }

    LOG_INFO("Connected to %s", address.c_str());
    return true;
}

bool PrinterDriver::reconnectLast() {
    if (lastKnownAddress.empty()) {
        LOG_ERROR("No last known hub");
        return false;
    }

    LOG_BLUETOOTH("reconnectLast -> %s", lastKnownAddress.c_str());
    return connect(lastKnownAddress);
}

bool PrinterDriver::disconnect() {
    LOG_BLUETOOTH("disconnect (%s)",transport->getConnectedAddress().c_str());    
    bool result = true;

    if (transport->isConnected()) {
        result = transport->disconnect();
    }

    connectedAddress.clear();
    return result;
}

bool PrinterDriver::isConnected() {
    return transport && transport->isConnected();
}

std::string PrinterDriver::getConnectedAddress() const {
    if (transport && transport->isConnected()) {
        return transport->getConnectedAddress();
    }

    return connectedAddress;
}

std::vector<DeviceInfo> PrinterDriver::getRecentHubs() const {
    return recentHubs;
}

std::vector<DeviceInfo> PrinterDriver::getLastScanResults() const {
    return scanResults;
}

HubMode PrinterDriver::detectHubMode(const std::string& address) {
    if (!transport->isConnected() || transport->getConnectedAddress() != address) {
        return HubMode::Unknown;
    }

    LOG_INFO("detectHubMode (%s)", address.c_str());

    const std::vector<std::string> services = transport->getServices();
    
    LOG_INFO("Services (%zu):", services.size());
    for (const auto& service : services) {
        LOG_INFO("   %s", service.c_str());
    }
    
    const bool hasBootloader = std::find(services.begin(), services.end(), protocol::LWP3_BOOTLOADER_SERVICE_UUID) != services.end();
    const bool hasPybricks = std::find(services.begin(), services.end(), protocol::PYBRICKS_SERVICE_UUID) != services.end();
    const bool hasLwp3Hub = std::find(services.begin(), services.end(), protocol::LWP3_HUB_SERVICE_UUID) != services.end();

    LOG_INFO("Flags: bootloader = %d, pybricks = %d, lwp3hub = %d", hasBootloader, hasPybricks, hasLwp3Hub);

    HubMode result = hasPybricks ? HubMode::PybricksRuntime :
        (hasBootloader ? HubMode::Bootloader :
            (hasLwp3Hub ? HubMode::LegoOfficial : HubMode::Unknown));
    LOG_INFO("detectHubMode result = %d", static_cast<int>(result));

    if (hasPybricks) {
        return HubMode::PybricksRuntime;
    }
    if (hasBootloader) {
        return HubMode::Bootloader;
    }    
    if (hasLwp3Hub) {
        return HubMode::LegoOfficial;
    }

    return HubMode::Unknown;
}

bool PrinterDriver::probeRuntime(const std::string& address, int timeoutMs) {
    if (address.empty()) {
        LOG_ERROR("probeRuntime: empty address");
        return false;
    }

    LOG_BLUETOOTH("probeRuntime(%s, %dms)", address.c_str(), timeoutMs);

    if (transport->isConnected() && transport->getConnectedAddress() != address) {
        bool disconnectResult = transport->disconnect();
    }

    if (!transport->isConnected()) {
        if (!transport->connect(address)) {
            LOG_WARNING("probeRuntime: connect failed");
            return false;
        }
    }

    if (!runtime->connect(address)) {
        LOG_WARNING("probeRuntime: runtime connect failed");
        bool disconnectResult = transport->disconnect();
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    bool ready = false;

    runtime->setCallback([&](const uint8_t* data, size_t length) {
        std::string message(reinterpret_cast<const char*>(data), length);
        if (message.find("ready") != std::string::npos) {
            ready = true;
        }
    });

    while (std::chrono::steady_clock::now() < deadline) {
        if (ready) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!ready) {
        LOG_WARNING("probeRuntime: no ready marker");
        runtime->disconnect();
        bool disconnectResult = transport->disconnect();
        return false;
    }

    LOG_INFO("probeRuntime: received ready marker");
    return true;
}

bool PrinterDriver::flashFirmware(const std::string& firmwareBootloaderPath, const std::string& address) {
    const std::string target = resolveAddress(address);
    if (target.empty()) {
        LOG_ERROR("No target address for firmware flash");
        return false;
    }    

    if (!transport->isConnected() || transport->getConnectedAddress() != target) {
        LOG_ERROR("Transport not connected to the target hub. Please connect first");
        return false;
    }

    LOG_BLUETOOTH("flashFirmware: discovering services...");
    auto services = transport->getServices();
    LOG_BLUETOOTH("flashFirmware: service = %zu", services.size());

    for (const auto& service : services) {
        LOG_BLUETOOTH("   service: %s", service.c_str());
    }

    auto mode = detectHubMode(target);
    if (mode != HubMode::Bootloader) {
        LOG_ERROR("Hub is not in bootloader mode. Please restart it with the button to enter bootloader");
        return false;
    }

    LOG_INFO("flashFirmware: %s -> %s", firmwareBootloaderPath.c_str(), target.c_str());

    // firmwareBlobPath should point to firmware.blob produced from firmware.zip

    std::vector<uint8_t> firmware = readBinaryFile(firmwareBootloaderPath);
    if (firmware.empty()) {
        LOG_ERROR("Failed to read firmware file or file is empty: %s", firmwareBootloaderPath.c_str());
        bool disconnectResult = transport->disconnect();
        return false;
    }

    const bool result = bootloaderProtocol->flashFirmware(firmware);

    if (!result) {
        LOG_ERROR("flashFirmware failed");
    }
    else {
        LOG_INFO("flashFirmware succeeded");
    }

    return result;
}

std::vector<uint8_t> createMultiFileBlob(const std::vector<uint8_t>& scriptData, const std::string& moduleName = "__main__") {
    std::vector<uint8_t> blob;

    uint32_t size = static_cast<uint32_t>(scriptData.size());
    for (int i = 0; i < 4; ++i) {
        blob.push_back(static_cast<uint8_t>((size >> (i * 8)) & 0xFF));
    }

    blob.insert(blob.end(), moduleName.begin(), moduleName.end());
    blob.push_back(0x00);

    blob.insert(blob.end(), scriptData.begin(), scriptData.end());

    return blob;
}

bool PrinterDriver::uploadProgram(const std::string& scriptFile, const std::string& address) {
    const std::string target = resolveAddress(address);
    if (target.empty()) {
        LOG_ERROR("No target address for program upload");
        return false;
    }

    if (!transport->isConnected() || transport->getConnectedAddress() != target) {
        LOG_ERROR("Transport not connected to the target hub. Please connect first");
        return false;
    }

    auto mode = detectHubMode(target);
    if (mode != HubMode::PybricksRuntime) {
        LOG_ERROR("Hub is not in Pybricks runtime mode");
        return false;
    }

    LOG_INFO("uploadProgram: %s -> %s", scriptFile.c_str(), target.c_str());

    std::vector<uint8_t> scriptData = readBinaryFile(scriptFile);
    if (scriptData.empty()) {
        LOG_ERROR("Failed to read script file or file is empty: %s", scriptFile.c_str());
        return false;
    }

    std::vector<uint8_t> blob = createMultiFileBlob(scriptData);
    const bool result = printerProtocol->uploadProgram(blob);

    if (!result) {
        LOG_ERROR("uploadProgram failed");
    }
    else {
        LOG_INFO("uploadProgram succeeded");
    }

    return result;
}

bool PrinterDriver::startUserProgram() {
    if (!transport || !transport->isConnected()) {
        if (!lastKnownAddress.empty()) {
            LOG_BLUETOOTH("startUserProgram: connecting to last known address %s", lastKnownAddress.c_str());
            if (!transport->connect(lastKnownAddress)) {
                LOG_ERROR("Failed to connect for startUserProgram");
                return false;
            }
        }
        else {
            LOG_ERROR("No connected hub");
            return false;
        }
    }

    const bool result = printerProtocol->startUserProgram();
    if (!result) {
        LOG_ERROR("startUserProgram failed");
    }

    return result;
}

bool PrinterDriver::stopUserProgram() {
    if (!transport || !transport->isConnected()) {
        LOG_ERROR("No connected hub");
        return false;
    }

    const bool result = printerProtocol->stopUserProgram();
    if (!result) {
        LOG_ERROR("stopUserProgram failed");
    }

    return result;
}

bool PrinterDriver::runtimeRotateMotor(uint8_t port, int32_t speed, int32_t angle, bool hold) {
    return runtime->rotateMotor(port, speed, angle, hold);
}

bool PrinterDriver::runtimePing() {
    return runtime->ping();
}

bool PrinterDriver::connectRuntime(const std::string& address) { 
    LOG_INFO("Starting connectRuntime, address = %s", address.c_str());
    const std::string target = resolveAddress(address);
    if (target.empty()) {
        LOG_ERROR("No target address for runtime connect");
        return false;
    }

    if (!transport->isConnected() || transport->getConnectedAddress() != target) {
        LOG_ERROR("Transport not connected to the target hub. Please connect first");
        return false;
    }

    if (!runtime->connect(target)) {
        LOG_ERROR("connectRuntime failed");
        return false;
    }

    connectedAddress = target;
    lastKnownAddress = target;
    return true;
}

void PrinterDriver::disconnectRuntime() {
    runtime->disconnect();
}

bool PrinterDriver::sendRuntime(const uint8_t* data, size_t length) {
    const bool result = runtime->send(data, length, false);
    if (!result) {
        LOG_ERROR("sendRuntime failed");
    }

    return result;
}

bool PrinterDriver::sendMotorCommands(const MotorCommand* commands, int count) {
    if (!commands || count <= 0) {
        LOG_ERROR("Invalid motor commands");
        return false;
    }

    // TODO implement

    return true;
}

void PrinterDriver::rotateMotor(const MotorCommand* commands, int count) {
    motorManager->rotate(commands, count);
}

void PrinterDriver::setMotorSpeed(uint8_t port, int8_t speed) {
    motorManager->setSpeed(port, speed);
}

int PrinterDriver::getLogCount() {
    return gLog.getLogCount();
}

void PrinterDriver::clearLog() {
    gLog.clearLog();
}

const char* PrinterDriver::getLogEntry(int index) {
    return gLog.getLogEntry(index);
}

const char* PrinterDriver::getLastErrorMessage() {
    return "";
}

void PrinterDriver::printerConnectionInfo() {

}

void PrinterDriver::printerSetLogCategories(unsigned int categories) {

}

unsigned int PrinterDriver::printerGetLogCategories() {
    return 0;
}

bool PrinterDriver::printerExecuteSpeedProfile(const SpeedProfile* profile) {
    return false;
}

bool PrinterDriver::printerExecuteSpeedProfiles(const SpeedProfile* profiles, int count) {
    return false;
}

bool PrinterDriver::printerIsMotorMoving(int count) {
    return false;
}

double PrinterDriver::printerGetMotorPosition(unsigned char port) {
    return motorManager->getPosition(port);
}

bool PrinterDriver::runPrinterTest(const char* testName) {
    return false;
}

bool PrinterDriver::printerRequestBatteryLevel() {
    return false;
}

unsigned char PrinterDriver::printerGetBatteryLevel() {
    return 0;
}

bool PrinterDriver::printerIsBatteryLevelFresh(int maxAgeSeconds) {
    return false;
}
