#include "PrinterDriver.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <thread>

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transport) 
    : transport(std::move(transport)),
    motorManager(std::make_unique<MotorManager>(*transport)) {

    gLog.setLogCategories(LOG_CATEGORY_ALL);

    bootloader = std::make_unique<BootloaderProtocol>(*transport);
    printerProtocol = std::make_unique<PrinterProtocol>(*transport);
    runtime = std::make_unique<RuntimeSession>(*transport);

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
    LOG_BLUETOOTH("scan(timeoutSeconds=%d, legoOnly=%d)", timeoutSeconds, legoOnly ? "true" : "false");

    if (!transport->startScan(timeoutSeconds)) {
        LOG_ERROR("Failed to start scan");
        return {};
    }

    std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds));
    transport->stopScan();

    auto rawData = transport->getScanResults();
    scanResults = filterAndSortHubs(rawData, legoOnly);

    LOG_BLUETOOTH("Scan finished, %zu candidate(s)", scanResults.size());
    return scanResults;
}

bool PrinterDriver::connectAuto(int timeoutMs) {
    LOG_BLUETOOTH("connectAuto(timeoutMs=%d)", timeoutMs);

    if (transport->isConnected()) {
        LOG_BLUETOOTH("connectAuto: already connected to %s", transport->getConnectedAddress());
        return true;
    }

    const int scanSeconds = std::max(1, (timeoutMs + 999) / 1000);
    if (!transport->startScan(scanSeconds)) {
        LOG_ERROR("Failed to start auto-scan");
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::vector<std::string> tried;

    while (std::chrono::steady_clock::now() < deadline) {
        auto current = filterAndSortHubs(transport->getScanResults(), true);

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

            LOG_BLUETOOTH("connectAuto: trying %s [%s] rssi=%d", device.name.c_str(), device.address.c_str(), device.rssi);

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
    LOG_ERROR("No LEGO hub found");
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
    LOG_BLUETOOTH("disconnect()");
    disconnectRuntime();

    if (transport) {
        transport->disconnect();
    }

    connectedAddress.clear();
    return true;
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
    if (address.empty()) {
        LOG_ERROR("detectHubMode: empty address");
        return HubMode::Unknown;
    }

    LOG_BLUETOOTH("detectHubMode(%s)", address.c_str());

    if (!transport->connect(address)) {
        LOG_WARNING("detectHubMode: connect failed");
        return HubMode::Unknown;
    }

    const auto services = transport->getServices();
    HubMode mode = HubMode::Unknown;

    for (const auto& service : services) {
        if (service == protocol::LWP3_BOOTLOADER_CHAR_UUID) {
            mode = HubMode::Bootloader;
        }
        if (service == protocol::PYBRICKS_SERVICE_UUID) {
            mode = HubMode::PybricksRuntime;
        }
        if (service == protocol::LWP3_HUB_SERVICE_UUID) {
            mode = HubMode::LegoOfficial;
        }
    }

    transport->disconnect();
    LOG_INFO("detectHubMode: %d", static_cast<int>(mode));
    return mode;
}

bool PrinterDriver::probeRuntime(const std::string& address, int timeoutMs) {
    if (address.empty()) {
        LOG_ERROR("probeRuntime: empty address");
        return false;
    }

    LOG_BLUETOOTH("probeRuntime(%s, %dms)", address.c_str(), timeoutMs);

    if (!runtime->connect(address)) {
        LOG_WARNING("probeRuntime: runtime connect failed");
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

    runtime->disconnect();

    if (!ready) {
        LOG_WARNING("probeRuntime: no ready marker");
    }
    else {
        LOG_INFO("probeRuntime: ready");
    }

    return ready;
}

bool PrinterDriver::flashFirmware(const std::filesystem::path& firmwareBootloaderPath, const std::string& address) {
    const std::string target = resolveAddress(address);
    if (target.empty()) {
        LOG_ERROR("No target address for firmware flash");
        return false;
    }

    LOG_INFO("flashFirmware^ %s -> %s", firmwareBootloaderPath.string().c_str(), target.c_str());

    // Make sure runtime is not holding the transport open
    disconnectRuntime();

    if (!transport->connect(target)) {
        LOG_ERROR("Failed to connect before flash");
        return false;
    }

    // NOTE:
    // firmwareBootloaderPath should point to extracted firmware bootloader / bin
    // if we pass a .zip, extract the bootloader first before calling this method
    std::vector<uint8_t> firmwareBootloader;
    {
        FILE* file = nullptr;
#ifdef _WIN32
        fopen_s(&file, firmwareBootloaderPath.string().c_str(), "rb");
#else
        file = fopen(firmwareBootloaderPath.string(),c_str(), "rb");
#endif // _WIN32

        if (!file) {
            LOG_ERROR("Failed to open firmware bootloader");
            transport->disconnect();
            return false;
        }

        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (size <= 0) {
            fclose(file);
            LOG_ERROR("Empty firmware bootloader");
            return false;
        }

        firmwareBootloader.resize(static_cast<size_t>(size));
        fread(firmwareBootloader.data(), 1, firmwareBootloader.size(), file);
        fclose(file);
    }

    const bool result = bootloader->flashFirmware(firmwareBootloader);
    transport->disconnect();

    if (result) {
        LOG_ERROR("flashFirmware failed");
    }
    else {
        LOG_INFO("flashFirmware succeeded");
    }

    return result;
}

bool PrinterDriver::uploadProgram(const std::filesystem::path& scriptFile, const std::string& address) {
    const std::string target = resolveAddress(address);
    if (target.empty()) {
        LOG_ERROR("No target address for program upload");
        return false;
    }

    LOG_INFO("uploadProgram: %s -> %s", scriptFile.string().c_str(), target.c_str());

    disconnectRuntime();

    if (!transport->connect(target)) {
        LOG_ERROR("Failed to connect before upload");
        return false;
    }

    std::vector<uint8_t> scriptBootloader;
    {
        FILE* file = nullptr;
#ifdef _WIN32
        fopen_s(&file, scriptFile.string().c_str(), "rb");
#else
        fopen(scriptFile.string().c_str(), "rb");
#endif // _WIN32

        if (!file) {
            LOG_ERROR("Failed to open script file");
            transport->disconnect();
            return false;
        }

        fseek(file, 0, SEEK_END);
        const long size = ftell(file);
        fseek(file, 0, SEEK_SET);

        if (size <= 0) {
            fclose(file);
            LOG_ERROR("Empty script file");
            transport->disconnect();
            return false;
        }

        scriptBootloader.resize(static_cast<size_t>(size));
        fread(scriptBootloader.data(), 1, scriptBootloader.size(), file);
        fclose(file);
    }

    const bool result = printerProtocol->uploadProgram(scriptBootloader);
    transport->disconnect();

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

bool PrinterDriver::connectRuntime(const std::string& address) {
    const std::string target = resolveAddress(address);
    if (target.empty()) {
        LOG_ERROR("No target address for runtime connect");
        return false;
    }

    LOG_BLUETOOTH("connectRuntime(%s)", target.c_str());

    if (!runtime->connect(target)) {
        LOG_ERROR("connectRuntime failed");
        return false;
    }

    connectedAddress = target;
    lastKnownAddress = target;
    return true;
}

void PrinterDriver::disconnectRuntime() {
    if (runtime) {
        runtime->disconnect();
    }
}

bool PrinterDriver::sendRuntime(const uint8_t* data, size_t length) {
    if (!runtime || !runtime->isConnected()) {
        LOG_ERROR("Runtime not connected");
        return false;
    }

    const bool result = runtime->send(data, length, false);
    if (result) {
        LOG_ERROR("sendRuntime failed");
    }

    return result;
}

bool PrinterDriver::sendMotorCommands(const MotorCommand* commands, int count) {
    if (!commands || count <= 0) {
        LOG_ERROR("Invalid motor commands");
        return false;
    }

    if (!runtime || !runtime->isConnected()) {
        LOG_ERROR("Runtime not connected");
        return false;
    }

    std::vector<uint8_t> packet;
    packet.reserve(2 + static_cast<size_t>(count) * 10);

    packet.push_back(0x01); // COMMAND_MOVE for executor
    packet.push_back(static_cast<uint8_t>(count));

    for (int i = 0; i < count; ++i) {
        const auto& command = commands[i];

        LOG_MOTOR("command[%d]: port=%u speed=%d angle=%d flags=0x%02X", i, command.port, command.speed);


    }

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
