#include "LegoPrinterCore.h"
#include <simpleble/SimpleBLE.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <queue>
#include <condition_variable>
#include <algorithm>
#include <cstring>
#include <cstdarg>
#include <iomanip>
#include <sstream>
#include <cmath>

using namespace std::chrono_literals;

// Constants for working with LEGO HUB
const std::string LEGO_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
const std::string LEGO_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";

// --Printer implementation class--
// Internal driver implementation, hidden from the outside world
// Contains all the core logic for working with Bluetooth and the LEGO HUB
class PrinterImplementation {
public:
    IPrinter interface;

private:

    // Printer Status
    SimpleBLE::Peripheral peripheral;
    std::string lastError;
    std::atomic<bool> operationInProgress;
    std::atomic<bool> stopRequested;
    std::atomic<bool> isValid;
    std::atomic<int> status;
    std::atomic<bool> wasConnected {false};

    struct MotorState {
        std::atomic<double> absolutePosition { 0.0 };
        std::atomic<double> segmentAccumulator { 0.0 };
        std::atomic<double> relativePosition { 0.0 };
        std::atomic<double> currentPosition { 0.0 };
        std::atomic<double> currentSpeed { 0.0 };
        std::atomic<bool> isMoving { false };
        std::atomic<bool> threadRunning { false };

        std::atomic<double> segmentTarget { 0.0 };

        std::atomic<double> segmentStartPosition { 0.0 };
        std::atomic<double> segmentStartRelative { 0.0 };
        std::atomic<double> segmentTargetDistance { 0.0 };
        std::atomic<double> currentSegmentTraveled { 0.0 };
        std::atomic<int> currentSegmentIndex { 0 };
        std::atomic<bool> profileActive { false };

        std::vector<SpeedProfilePoint> activeProfile;
        int profileTimeoutMs = 60000;

        // Default constructor
        MotorState() = default;

        // Move constructor
        MotorState(MotorState&& other) noexcept
            : currentPosition(other.currentPosition.load())
            , relativePosition(other.relativePosition.load())
            , segmentStartPosition(other.segmentStartPosition.load())
            , currentSegmentTraveled(other.currentSegmentTraveled.load())
            , segmentStartRelative(other.segmentStartRelative.load())
            , currentSegmentIndex(other.currentSegmentIndex.load())
            , threadRunning(other.threadRunning.load())
            , profileActive(other.profileActive.load())
            , activeProfile(std::move(other.activeProfile))
            , profileTimeoutMs(other.profileTimeoutMs)
        {
        }

        // Move assignment
        MotorState& operator=(MotorState&& other) noexcept {
            if (this != &other) {
                currentPosition.store(other.currentPosition.load());
                relativePosition.store(other.relativePosition.load());
                segmentStartPosition.store(other.segmentStartPosition.load());
                currentSegmentTraveled.store(other.currentSegmentTraveled.load());
                segmentStartRelative.store(other.segmentStartRelative.load());
                currentSegmentIndex.store(other.currentSegmentIndex.load());
                threadRunning.store(other.threadRunning.load());
                profileActive.store(other.profileActive.load());
                activeProfile = std::move(other.activeProfile);
                profileTimeoutMs = other.profileTimeoutMs;
            }
            return *this;
        }

        // Remove copy
        MotorState(const MotorState&) = delete;
        MotorState& operator=(const MotorState&) = delete;
    };

    std::mutex sendCommandMutex;

    std::map<uint8_t, MotorState> motorStates;
    std::map<uint8_t, std::thread> motorThreads;
    std::condition_variable motorStatesCV;

    // Logging system
    std::vector<std::string> logEntries;
    std::mutex logMutex;
    const size_t MAX_LOG_ENTRIES = 10000;

    // Simple synchronization system
    std::mutex operationMutex;
    std::mutex completionMutex;
    std::condition_variable completionCV;

    struct CommandExecution {
        std::atomic<bool> completed{ false };
        std::atomic<bool> waiting{ false };

        CommandExecution() {
            completed = true;
            waiting = false;
        }

        CommandExecution(const CommandExecution&) = delete;
        CommandExecution& operator=(const CommandExecution&) = delete;

        CommandExecution(CommandExecution&& Other) noexcept
            : completed(Other.completed.load()), waiting(Other.waiting.load()) {}
    };

    std::map<uint8_t, CommandExecution> commandStatus;

    struct SpeedControlState {
        std::atomic<bool> active {false};
        std::atomic<size_t> currentPointIndex{0};
        std::thread controlThread;

        std::vector<SpeedProfilePoint> profilePoints;
        int timeoutMs;
    };

    std::map<uint8_t, SpeedControlState> speedControlStates;
    std::mutex speedControlMutex;

    std::mutex motorStatesMutex;

public:

    PrinterImplementation() :
        operationInProgress(false),
        stopRequested(false),
        isValid(true),
        status(0) {

        addLog("PrinterImplementation created");
    }

    ~PrinterImplementation() {      
        try {
            std::lock_guard<std::mutex> lock(completionMutex);
            completionCV.notify_all();
            for (auto& [port, state] : speedControlStates) {
                state.active = false;
                if (state.controlThread.joinable()) {
                    state.controlThread.join();
                }
            }
        }
        catch (...) {
            // Ignore condition variable errors
        }

        // Automatic shutdown when variable is destroyed
        if (wasConnected) {
            isValid = false;
            stopRequested = true;
            // Stop all motor threads
            for (auto& [port, State] : motorStates) {
                State.threadRunning = false;
            }

            for (auto& [port, Thread] : motorThreads) {
                if (Thread.joinable()) {
                    Thread.join();
                }
            }

            if (peripheral.is_connected()) {
                try {
                    if (!peripheral.address().empty() && peripheral.is_connected()) {
                        peripheral.disconnect();
                    }
                }
                catch (const std::exception& ex) {
                    // Ignore all errors
                }
            }
        }
    }

private:

    // Set notification handler
    void setupNotificationHandler() {
        try {
            if (!peripheral.is_connected()) {
                addLog("ERROR: Peripheral not connected for encoder notifications");
                return;
            }

            peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                [this](const std::vector<uint8_t>& Data) {
                    this->handleHubNotification(Data);
                });

            addLog("Encoder notifications setup completed - SUBSCRIBED");
        }
        catch (const std::exception& ex) {
            addLog("Error setting up encoder notifications: %s", ex.what());
        }
    }

    void handleHubNotification(const std::vector<uint8_t>& Data) {
        if (!isValid || Data.empty()) return;

        const double ENCODER_TICKS_PER_REVOLUTION = 360.0;
        const double INCREMENTAL_FACTOR = 1.0 / 360.0;

        // For encoder type 0x45 (incremental)
        if (Data.size() >= 5 && Data[2] == 0x45) {
            uint8_t port = Data[3];
            initializeMotorState(port);

            uint8_t positionByte = Data[4];
            int8_t signedPosition = static_cast<int8_t>(positionByte);
            double positionDelta = static_cast<double>(signedPosition) * INCREMENTAL_FACTOR;

            updateMotorPosition(port, positionDelta);

            if (std::abs(positionDelta) > 0.001) {
                addLog("ENCODER 0x45: Port=0x%02X, Raw=%d, Delta=%.4f rev",
                    port, signedPosition, positionDelta);
            }
        }
        // For encoder type 0x04 (absolute)
        else if (Data.size() >= 8 && Data[2] == 0x04) {
            uint8_t port = Data[3];
            initializeMotorState(port);

            int32_t PositionRaw = (static_cast<int32_t>(Data[4])) |
                (static_cast<int32_t>(Data[5]) << 8) |
                (static_cast<int32_t>(Data[6]) << 16) |
                (static_cast<int32_t>(Data[7]) << 24);

            int32_t SignedPosition = static_cast<int32_t>(PositionRaw);

            // Conversion for absolute encoder
            double absolutePosition = static_cast<double>(SignedPosition) / ENCODER_TICKS_PER_REVOLUTION;

            // For an absolute encoder, you need to calculate the delta from the previous value
            static std::map<uint8_t, double> lastAbsolutePositions;
            double positionDelta = 0.0;

            if (lastAbsolutePositions.count(port)) {
                positionDelta = absolutePosition - lastAbsolutePositions[port];
                // Adjustment for overflow
                if (positionDelta > 180.0) positionDelta -= 360.0;
                else if (positionDelta < -180.0) positionDelta += 360.0;
            }
            lastAbsolutePositions[port] = absolutePosition;

            updateMotorPosition(port, positionDelta);

            if (std::abs(positionDelta) > 0.001) {
                addLog("ENCODER 0x04: Port=0x%02X, Raw=%d, Abs=%.3f, Delta=%.4f rev",
                    port, SignedPosition, absolutePosition, positionDelta);
            }
        }
    }

    const double EMPIRICAL_CALIBRATION = 3.0 / 2.0;

    // In the UpdateMotorPosition function:
    void updateMotorPosition(uint8_t port, double positionDelta) {
        auto& state = motorStates[port];

        // We apply empirical calibration
        double calibratedDelta = positionDelta * EMPIRICAL_CALIBRATION;

        double currentAbs = state.absolutePosition.load(std::memory_order_relaxed);
        double newAbs = currentAbs + calibratedDelta;
        state.absolutePosition.store(newAbs, std::memory_order_relaxed);

        double currentSeg = state.segmentAccumulator.load(std::memory_order_relaxed);
        double newSeg = currentSeg + calibratedDelta;
        state.segmentAccumulator.store(newSeg, std::memory_order_relaxed);

        if (std::abs(calibratedDelta) > 0.001) {
            addLog("POS_UPDATE: Port=0x%02X, RawDelta=%.4f, CalibratedDelta=%.4f, NewAbs=%.3f, NewSeg=%.3f",
                port, positionDelta, calibratedDelta, newAbs, newSeg);
        }
    }

    void processEncoderUpdate(uint8_t port, int32_t rawValue, int bytes) {
        auto& state = motorStates[port];

        // Convert to revolutions depending on the data size
        double positionDelta = 0.0;
        if (bytes == 1) {
            int8_t signedPosition = static_cast<int8_t>(rawValue & 0xFF);
            positionDelta = static_cast<double>(signedPosition) / 360.0;
        }
        else if (bytes == 4) {
            positionDelta = static_cast<double>(rawValue) / 360.0;
        }

        // Updating the segment drive
        double currentAccumulator = state.segmentAccumulator.load();
        double newAccumulator = currentAccumulator + positionDelta;
        state.segmentAccumulator.store(newAccumulator);

        // Updating the absolute position
        double oldAbsolute = state.absolutePosition.load();
        state.absolutePosition.store(oldAbsolute + positionDelta);

        // We log only significant changes
        if (std::abs(positionDelta) > 0.001) {
            addLog("ENCODER: Port=0x%02X, Delta=%.3f, Accumulator=%.3f, Absolute=%.3f",
                port, positionDelta, newAccumulator, oldAbsolute + positionDelta);
        }
    }

    // Internal helper methods
    void addLog(const std::string& Message) {
        std::lock_guard<std::mutex> lock(logMutex);

        // Get current time
        auto Now = std::chrono::system_clock::now();
        auto Time = std::chrono::system_clock::to_time_t(Now);
        auto Milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch()) % 1000;

        std::stringstream String;
        String << "[" << std::put_time(std::localtime(&Time), "%H:%M:%S");
        String << "." << std::setfill('0') << std::setw(3) << Milliseconds.count() << "] " << Message;

        // Adding message to log
        logEntries.push_back(String.str());

        if (logEntries.size() > MAX_LOG_ENTRIES) {
            logEntries.erase(logEntries.begin());
        }
    }

    void addLog(const char* Format, ...) {
        char buffer[1024 * 16];
        va_list args;
        va_start(args, Format);
        vsnprintf(buffer, sizeof(buffer), Format, args);
        va_end(args);

        addLog(std::string(buffer));
    }

    void sendCommandVector(std::vector<uint8_t> command) {
        std::lock_guard<std::mutex> Lock(sendCommandMutex);

        if (!isValid) {
            addLog("SendCommandVector: Printer implementation is not valid");
            return;
        }
        if (!peripheral.is_connected()) {
            addLog("SendCommandVector: Printer is not connect");
            return;
        }

        try {
            // Check connection
            if (!peripheral.is_connected()) {
                addLog("SendCommandVector: Peripheral not connected");
                return;
            }

            // Logging sending command
            std::string hexCommand = "Command bytes: ";
            for (auto byte : command) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X", byte);
                hexCommand += hex;
            }
            //addLog("%s", hexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, command);
            //addLog("Command sent successfully!");
        }
        catch (const std::exception& e) {
            addLog("Error sending command: %s", e.what());
            lastError = e.what();
        }
    }

public:

    // Access to log from C-interface
    int GetLogCount() {
        std::lock_guard<std::mutex> lock(logMutex);
        return static_cast<int>(logEntries.size());
    }

    const char* GetLogEntry(int index) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (index < 0 || index >= static_cast<int>(logEntries.size())) {
            return "";
        }

        return logEntries[index].c_str();
    }

    void ClearLog() {
        std::lock_guard<std::mutex> lock(logMutex);
        logEntries.clear();
        addLog("Log cleared");
    }

    const char* GetLastErrorMessage() {
        return lastError.empty() ? "" : lastError.c_str();
    }

    // Basic methods
    bool connect() {
        if (!isValid) return false;

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
            // Checking Bluetooth Status
            addLog("Checking Bluetooth status:");

            bool bleEnabled = SimpleBLE::Adapter::bluetooth_enabled();
            addLog("  - SimpleBLE::Adapter::bluetooth_enabled(): %s" + bleEnabled ? "true" : "false");

            // Getting a list of adapters
            auto adapters = SimpleBLE::Adapter::get_adapters();
            addLog("  - Adapters found: %zu" + adapters.size());

            if (adapters.empty()) {
                addLog("Bluetooth adapters not found! Possible reasons:");
                addLog("1. The Bluetooth adapter is disabled or not working");
                addLog("2. Drivers not installed");
                addLog("3. Hardware problem");
                return false;
            }

            // We use the first adapter
            SimpleBLE::Adapter& adapter = adapters[0];
            addLog("Adapter used: %s [%s]",
                adapter.identifier().c_str(),
                adapter.address().c_str());

            // Setting up callbacks
            adapter.set_callback_on_scan_start([]() { });

            addLog("Scanning started...");

            adapter.set_callback_on_scan_stop([]() { });

            addLog("Scanning stopped");

            adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral) {
                    std::string name = peripheral.identifier();
                    std::string address = peripheral.address();
                    int rssi = peripheral.rssi();

                    // Convert the name to uppercase for universality
                    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                    // Check by name
                    bool isLego = (name.find("LEGO") != std::string::npos) ||
                        (name.find("HUB") != std::string::npos) ||
                        (name.find("CONTROL") != std::string::npos);

                    // Check with manufacturer data (LEGO Company ID: 0x0397)
                    auto manufacturer_data = peripheral.manufacturer_data();
                    for (const auto& data : manufacturer_data) {
                        // LEGO Company ID: 0x0397 (little-endian: 97 03)
                        if (data.first == 0x0397) {
                            isLego = true;
                            addLog("[LEGO Manufacturer Data Found]");
                        }
                    }

                    if (isLego) {
                        addLog("LEGO HUB DISCOVERED!");
                    }
                });

            // Start scanning
            adapter.scan_start();
            addLog("Starting Bluetooth scan for 10 seconds...");
            std::this_thread::sleep_for(10s);
            adapter.scan_stop();

            // We get a list of found devices
            auto peripherals = adapter.scan_get_results();
            addLog("Scan completed. Found %d devices", peripherals.size());
            std::cout << "\n\nDevices found: " << peripherals.size() << "\n";

            // Search LEGO Hub
            for (auto& scannedPeripheral : peripherals) {
                std::string name = scannedPeripheral.identifier();
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                if (name.find("LEGO") != std::string::npos ||
                    name.find("HUB") != std::string::npos ||
                    name.find("CONTROL") != std::string::npos) {

                    addLog("Attempting to connect to LEGO Hub: %s", name.c_str());

                    // Connection attempt
                    try {
                        scannedPeripheral.connect();

                        // In the main function, after connection:               
                        if (scannedPeripheral.is_connected()) {
                            addLog("Successfully connected to LEGO Hub");

                            peripheral = std::move(scannedPeripheral);

                            setupNotificationHandler();

                            // Looking for LEGO Hub service and features
                            SimpleBLE::Service legoService;
                            SimpleBLE::Characteristic legoChar;

                            for (auto& service : scannedPeripheral.services()) {
                                if (service.uuid() == LEGO_HUB_SERVICE_UUID) {
                                    legoService = service;
                                    for (auto& characteristic : service.characteristics()) {
                                        if (characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) {
                                            legoChar = characteristic;
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }

                            if (legoChar.uuid().empty()) return false;

                            wasConnected = true;
                            return true;
                        }
                    }
                    catch (const std::exception& e) {
                        addLog("Connection error: %s", e.what());
                        lastError = e.what();
                    }
                }
            }

            addLog("No LEGO Hub found or connection failed");
            return false;
        }
        catch (const std::exception& e) {
            addLog("Exception in Connect: %s", e.what());
            lastError = e.what();
            return false;
        }
    }

    bool disconnect() {
        std::lock_guard<std::mutex> contextLock(operationMutex);

        // Disconnect already completed
        if (!isValid || !peripheral.initialized() || peripheral.address().empty()) return false;

        if (peripheral.is_connected()) {
            try {
                peripheral.disconnect();
            }
            catch (...) {
                // Ignoring disconnection errors
                return false;
            }
        }
        return true;
    }

    bool isConnected() {
        return peripheral.is_connected();
    }

    void printConnectionInfo() {
        std::lock_guard<std::mutex> lock(operationMutex);

        addLog("=== CONNECTION INFORMATION ===");

        if (!peripheral.is_connected()) {
            addLog("NOT CONNECTED to any device");
            return;
        }

        addLog("Device: %s", peripheral.identifier().c_str());
        addLog("Address: %s", peripheral.address().c_str());
        addLog("RSSI: %d", peripheral.rssi());
        addLog("Connected: %s", peripheral.is_connected() ? "true" : "false");

        auto Services = peripheral.services();
        addLog("Services count: %zu", Services.size());

        for (auto Service : Services) {
            addLog("Service UUID: %s", Service.uuid().c_str());
            
            if (Service.uuid() == LEGO_HUB_SERVICE_UUID) {
                addLog(" >>> LEGO SERVICE FOUND!");
                for (auto Characteristic : Service.characteristics()) {
                    addLog("    Characteristic: %s", Characteristic.uuid().c_str());
                    if (Characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) {
                        addLog("    >>> LEGO CHARACTERISTIC FOUND!");
                    }
                }
            }
        }

        addLog("========================================");
    }

    void rotateMotor(const MotorCommand* commands, int count) {      
        // Check parameters
        if (!isValid || count <= 0 || !commands) {
            addLog("RotateMotor: Invalid parameters");
            return;
        }

        addLog("RotateMotor called with %d commands", count);

        // Check connection
        if (!peripheral.is_connected()) {
            addLog("Printer is not connected!");
            return;
        }

        std::lock_guard<std::mutex> operationLock(operationMutex);        

        // Prepare command tracking
        for (int i = 0; i < count; i++) {
            commandStatus[commands[i].port].completed = false;
            commandStatus[commands[i].port].waiting = true;
        }

        // Send all commands
        for (int i = 0; i < count; i++) {
            sendSingleMotorCommand(commands[i]);
        }

        waitForCommandsCompletion(commands, count);
        addLog("RotateMotor completed");
    }

    // Monitoring
    bool isMotorMoving(unsigned char port) {
        if (motorStates.count(port)) {
            return motorStates[port].isMoving;
        }
        return false;
    }

    double getMotorPosition(uint8_t port) {
        if (motorStates.count(port)) {
            // We return AbsolutePosition since it is relevant
            return motorStates[port].absolutePosition.load();
        }
        return 0.0;
    }

    void setMotorSpeed(uint8_t port, int8_t speed)
    {
        if (!isValid || !peripheral.is_connected()) return;

        addLog("Setting motor speed: Port=0x%02X, Speed=%d", port, speed);

        // First command: Activate mode
        std::vector<uint8_t> setupCommand = {
            0x09,       // Package length
            0x00,       // Hub ID
            0x41,       // Port configuration command
            port,       // Motor port
            0x01,       // Mode: Power (1)
            0x00,       // Data Format
            0x00,       // Unit
            0x00,       // Range min
            0x00        // Range max
        };

        sendCommandVector(setupCommand);

        // Second Team: motor control
        std::vector<uint8_t> motorCommand = {
            0x08,       // Package length
            0x00,       // Hub ID
            0x81,       // Output control command
            port,       // Motor port
            0x02,       // Subcommand: WriteDirectModeData
            0x01,       // Mode: Power (1)
            static_cast<uint8_t>(speed) // Speed
        };

        sendCommandVector(motorCommand);
    }

private:

    void sendSingleMotorCommand(const MotorCommand& command) {
        addLog("Motor command - Port: 0x%02X, Speed: %d, Revolutions: %.2f",
            command.port, command.speed, command.revolutions);

        // Convert revolutions to absolute degrees (1 revolution = 360 degrees)
        int32_t degrees = static_cast<int32_t>(std::round(command.revolutions * 360.0));
        addLog("Calculated degrees: %d", degrees);

        std::vector<uint8_t> payload = {
            0x0F,       // Message length (15 bytes)
            0x00,       // Message counter
            0x81,       // Output control command
            command.port, // Port or combo port
            0x11,
            0x0B,       // Sub-team
            // Rotation angle (4 bytes little-endian)
            static_cast<uint8_t>(degrees & 0xFF),
            static_cast<uint8_t>((degrees >> 8) & 0xFF),
            static_cast<uint8_t>((degrees >> 16) & 0xFF),
            static_cast<uint8_t>((degrees >> 24) & 0xFF),
            // Speed (1 byte)
            static_cast<uint8_t>(command.speed),
            // Maximum power (usually 100%)
            100,
            // Final state (0 = float/coast, 1 = brake/hold)
            0x01,       // Hold the position after completion
            // Use profile (0 = use acceleration profile)
            0x00
        };

        addLog("Sending motor command to port 0x%02X", command.port);
        sendCommandVector(payload);
    }

    void waitForCommandsCompletion(const MotorCommand* Commands, int count) {
        std::unique_lock<std::mutex> lock(completionMutex);

        // Wait while condition variable gets notification about completing all commands
        bool allCompleted = completionCV.wait_for(lock, std::chrono::seconds(30),
            [this, Commands, count]() {
                for (int i = 0; i < count; i++) {
                    if (!commandStatus[Commands[i].port].completed) return false;
                }
            });

        if (!allCompleted) {
            // For timeout - end all
            for (int i = 0; i < count; i++) {
                commandStatus[Commands[i].port].waiting = false;
            }
        }

        // Delete waiting status
        for (int i = 0; i < count; i++) {
            commandStatus[Commands[i].port].waiting = false;
        }
    }

    bool waitForCommandCompletion(uint8_t port, int timeoutMs = 15000) {
        std::unique_lock<std::mutex> lock(completionMutex);

        // Make sure the element exists in the map
        if (commandStatus.find(port) == commandStatus.end()) return false;

        // Setting the wait state
        commandStatus[port].completed = false;
        commandStatus[port].waiting = true;

        // We are waiting for notification of completion
        bool success = completionCV.wait_for(lock, std::chrono::milliseconds(timeoutMs),
            [this, port]() {
                auto it = commandStatus.find(port);
                if (it != commandStatus.end()) {
                    return it->second.completed.load();
                }

                return true; // If there is no port, we consider it complete.
            });

        if (commandStatus.find(port) != commandStatus.end()) {
            commandStatus[port].waiting = false;
        }

        if (!success) {
            addLog("WaitForCommandCompletion timeout for port 0x%02X", port);
        }
        else {
            addLog("WaitForCommandCompletion success for port 0x%02X", port);
        }

        return success;
    }

public:

    void sendCommand(const unsigned char* command, int length) {
        if (!isValid || length < 1) return;

        std::lock_guard<std::mutex> lock(operationMutex);

        try {
            std::vector<uint8_t> command(command, command + length);

            // Check connection
            if (!peripheral.is_connected()) {
                addLog("Peripheral is not connected");
                return;
            }

            // Logging sending command
            std::string hexCommand = "Command bytes: ";
            for (auto byte : command) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X", byte);
                hexCommand += hex;
            }
            addLog(hexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, command);
            addLog("Command sent successfully!");
        }
        catch (const std::exception& e) {
            addLog("Error sending command: %s", e.what());
            lastError = e.what();
        }
    }

    void safeShutdown() {
        isValid = false;
        stopRequested = true;

        // Safe breaking all operations
        try {
            std::lock_guard<std::mutex> Lock(completionMutex);
            completionCV.notify_all();
        }
        catch (...) {
            // Ignoring all errors
        }
    }

  //----- Execute speed profile methods -----

public:

    bool executeSpeedProfile(const SpeedProfile* profile)
    {
        if (!isValid || !profile || profile->count < 1) {
            addLog("Error: Invalid profile parameters");
            return false;
        }

        addLog("=== EXECUTE SPEED PROFILE ===");
        addLog("Port: 0x%02X, Segments: %d, Timeout: %d ms",
            profile->port, profile->count, profile->timeoutMs);

        uint8_t port = profile->port;

        // Activate the encoder
        ActivateEncoderMode(port);
        std::this_thread::sleep_for(200ms);

        // Setting up notifications
        setupNotificationHandler();
        std::this_thread::sleep_for(200ms);

        // Logging your profile
        for (int i = 0; i < profile->count; i++) {
            addLog("Segment %d: Distance=%.3f rev, Speed=%d, Tolerance=%.3f",
                i, profile->points[i].distance, profile->points[i].speed,
                profile->points[i].tolerance);
        }

        // Preparing profile points
        std::vector<SpeedProfilePoint> profilePoints;
        for (int i = 0; i < profile->count; i++) {
            profilePoints.push_back(profile->points[i]);
        }

        return startRelativeProfileController(port, profilePoints, profile->timeoutMs);
    }

private:

    bool startRelativeProfileController(uint8_t port, const std::vector<SpeedProfilePoint>& profilePoints, int timeoutMs) {
        initializeMotorState(port);
        auto& state = motorStates[port];

        // Stop the previous profile
        state.profileActive = false;
        std::this_thread::sleep_for(100ms);

        // We reset the drive and set up a new profile
        state.segmentAccumulator.store(0.0);
        state.activeProfile = profilePoints;
        state.profileTimeoutMs = timeoutMs;
        state.currentSegmentIndex.store(0);

        if (!profilePoints.empty()) {
            state.segmentTarget.store(profilePoints[0].distance);
        }

        state.profileActive.store(true);

        addLog("Starting relative profile: Segments=%zu, Timeout=%dms",
            profilePoints.size(), timeoutMs);

        // We launch the controller in a separate thread
        std::thread controllerThread([this, port]() {
            this->relativeProfileController(port);
            });
        controllerThread.detach();

        return true;
    }

    void initializeMotorState(uint8_t port) {
        if (!motorStates.count(port)) {
            // The correct way to initialize
            motorStates[port] = MotorState();
            addLog("Initialized motor state for port 0x%02X", port);
        }
    }

    void relativeProfileController(uint8_t port)
    {
        auto& state = motorStates[port];
        addLog("=== STARTING PRECISE PROFILE CONTROLLER ===");

        // Full state reset
        state.absolutePosition.store(0.0);
        state.currentSegmentIndex.store(0);
        state.profileActive.store(true);

        // We start polling the encoder
        startContinuousEncoderPolling(port);

        auto profileStartTime = std::chrono::steady_clock::now();
        auto lastControlTime = profileStartTime;
        bool profileCompleted = false;

        // Launching the first segment
        if (!state.activeProfile.empty()) {
            startSegment(port, 0);
        }

        while (!profileCompleted && state.profileActive && isValid) {
            auto currentTime = std::chrono::steady_clock::now();
            auto timeSinceLastControl = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastControlTime);

            // Precise control with a fixed interval
            lastControlTime = currentTime;

            int currentSegment = state.currentSegmentIndex.load();

            // We check the completion of all segments
            if (currentSegment >= state.activeProfile.size()) {
                setMotorSpeed(port, 0);
                addLog("PROFILE COMPLETED: All segments finished");
                profileCompleted = true;
                break;
            }

            const auto& segment = state.activeProfile[currentSegment];
            double traveled = state.segmentAccumulator.load(std::memory_order_relaxed); // Use absolute position

            // We log progress every 500ms
            static auto lastLogTime = profileStartTime;
            auto timeSinceLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastLogTime);
            if (timeSinceLastLog.count() > 500) {
                addLog("SEGMENT %d: Traveled=%.3f/%.3f rev (%.1f%%), Speed=%d",
                    currentSegment, traveled, segment.distance, (traveled / segment.distance) * 100, segment.speed);
                lastLogTime = currentTime;
            }

            // Checking the completion of the current segment
            if (traveled >= segment.distance - segment.tolerance) {
                addLog("=== SEGMENT %d COMPLETED ===", currentSegment);
                addLog("Traveled %.3f of %.3f revolutions", traveled, segment.distance);

                // Let's move on to the next segment
                int nextSegment = currentSegment + 1;
                state.currentSegmentIndex.store(nextSegment);

                if (nextSegment < state.activeProfile.size()) {
                    // RESET position for a new segment
                    state.segmentAccumulator.store(0.0, std::memory_order_relaxed);
                    startSegment(port, nextSegment);
                }
                else {
                    // All segments are complete
                    setMotorSpeed(port, 0);
                    addLog("=== PROFILE COMPLETED ===");
                    profileCompleted = true;
                }
            }

            // Timeout check
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - profileStartTime);
            if (elapsed.count() > state.profileTimeoutMs) {
                addLog("PROFILE TIMEOUT: %d ms elapsed", elapsed.count());

                addLog("Current segment: %d, Traveled: %.3f/%.3f",
                    currentSegment, traveled, segment.distance);
                setMotorSpeed(port, 0);
                state.profileActive = false;
                break;
            }
        }

        // Shutdown
        state.profileActive = false;
        stopEncoderPolling(port);

        if (motorThreads.count(port) && motorThreads[port].joinable()) {
            motorThreads[port].join();
            motorThreads.erase(port);
        }

        addLog("Profile controller stopped for port 0x%02X", port);
    }

    void startSegment(uint8_t port, int segmentIndex) {
        auto& state = motorStates[port];
        const auto& segment = state.activeProfile[segmentIndex];

        // Reset ONLY the segment drive
        state.segmentAccumulator.store(0.0, std::memory_order_relaxed);

        // Logging initial values ​​for debugging
        double initialAbs = state.absolutePosition.load(std::memory_order_relaxed);
        double initialSeg = state.segmentAccumulator.load(std::memory_order_relaxed);

        // Setting the motor speed
        setMotorSpeed(port, segment.speed);

        addLog(">>> STARTING SEGMENT %d: Target=%.3f rev, Speed=%d",
            segmentIndex, segment.distance, segment.speed);
        addLog(">>> INITIAL VALUES: AbsPos=%.3f, SegAcc=%.3f", initialAbs, initialSeg);
    }

    void startContinuousEncoderPolling(uint8_t port)
    {
        stopEncoderPolling(port);

        addLog("Starting optimized encoder polling for port 0x%02X", port);

        motorThreads[port] = std::thread([this, port]() {
            auto& state = motorStates[port];
            auto lastRequestTime = std::chrono::steady_clock::now();
            const std::chrono::milliseconds requestInterval(5);

            while (isValid && state.profileActive.load(std::memory_order_relaxed)) {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsed = currentTime - lastRequestTime;

                if (elapsed >= requestInterval) {
                    pollEncoderPosition(port);
                    lastRequestTime = currentTime;
                }
            }
            addLog("Encoder polling thread finished for port 0x%02X", port);
            });
    }

    void resetEncoderPosition(uint8_t port) {
        addLog("=== MANUAL POSITION RESET ===");

        auto& state = motorStates[port];
        double currentAbsolute = state.absolutePosition.load();

        // We reset ALL positions
        state.absolutePosition.store(0.0);
        state.segmentAccumulator.store(0.0);
        state.relativePosition.store(0.0); // if this variable is still in use

        addLog("Reset: Port=0x%02X, Was=%.3f, Now=0.000", port, currentAbsolute);
    }

    void pollEncoderPosition(uint8_t port) {
        // Encoder position query command
        std::vector<uint8_t> requestCmd = 
        {
            0x05,       // Length
            0x00,       // Hub ID
            0x21,       // Port Information Request
            port,       // Port
            0x00        // Mode: position
        };

        sendCommandVector(requestCmd);
    }

    // New try

public:

    // Updated method for testing
    bool testEncoderFunctionality(IPrinter* printer) {
        if (!printer) return false;

        addLog("FUNCTION do not do everything");
    }

private:          

    void startEncoderPolling(uint8_t port)
    {
        // We stop the previous survey if there was one
        stopEncoderPolling(port);

        addLog("Encoder polling started for port 0x%02X", port);

        // Launching a new survey stream
        motorThreads[port] = std::thread([this, port]() {
            while (isValid && !stopRequested) { // Limit the number of requests
                pollEncoderPosition(port);
                std::this_thread::sleep_for(50ms); // Request every 50ms
            }
            addLog("Encoder polling stopped for port 0x%02X after %d requests", port);
            });
    }    

    bool quickEncoderTest(uint8_t port)
    {
        addLog("=== QUICK ENCODER TEST (REAL-TIME) ===");

        // Resetting the position
        resetEncoderPosition(port);

        // We get the initial position
        double startPos = getMotorPosition(port);
        addLog("Start position: %.3f", startPos);

        // We start polling the encoder to activate updates
        startEncoderPolling(port);

        // We'll wait a bit to get some initial data.
        std::this_thread::sleep_for(100ms);

        // We receive a position after activating the survey
        startPos = getMotorPosition(port);
        addLog("Position after polling start: %.3f", startPos);

        // We start the engine for a short time
        setMotorSpeed(port, 40);
        addLog("Rotating motor at speed 40...");

        // We wait and measure the change in position
        auto startTime = std::chrono::steady_clock::now();
        double maxPosition = startPos;
        int measurements = 0;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count() < 300) {

            double currentPos = getMotorPosition(port);
            if (currentPos > maxPosition) {
                maxPosition = currentPos;
            }

            // Logging progress
            if (measurements % 5 == 0) { // Every 5 measurements
                addLog("Test loop %d: position=%.3f", measurements, currentPos);
            }

            measurements++;
            std::this_thread::sleep_for(50ms);
        }

        // We stop the engine
        setMotorSpeed(port, 0);
        addLog("Setting motor speed: Port=0x%02X, Speed=0", port);

        // Let the engine stop
        std::this_thread::sleep_for(100ms);

        // Final Dimension
        double finalPos = getMotorPosition(port);
        double positionChange = finalPos - startPos;

        addLog("Position after 300ms: %.3f (change: %.3f), measurements: %d",
            finalPos, positionChange, measurements);

        bool success = (positionChange > 0.05); // Minimum expected change
        addLog(success ? "SUCCESS: Encoder working! Position changed from %.3f to %.3f" :
            "FAILED: Encoder not responding to motor movement",
            startPos, finalPos);

        // Stopping the survey
        stopEncoderPolling(port);

        if (!success) {
            addLog("Final position: %.3f", finalPos);
            addLog("Last received encoder data analysis:");
            addLog("  - Check if 0x45 notifications are being received");
            addLog("  - Check if position bytes are changing in 0x45 messages");
        }

        return success;
    }

    void stopEncoderPolling(uint8_t port) {
        if (motorThreads.count(port)) {
            // Setting stop flags
            if (motorStates.count(port)) {
                motorStates[port].profileActive.store(false, std::memory_order_relaxed);
            }

            // Wait for the thread to complete with a timeout
            if (motorThreads[port].joinable()) {
                auto& thread = motorThreads[port];
                if (thread.get_id() != std::this_thread::get_id()) {
                    // We give the thread 500ms to complete
                    for (int i = 0; i < 50 && thread.joinable(); i++) {
                        std::this_thread::sleep_for(10ms);
                    }
                    if (thread.joinable()) {
                        thread.detach(); // Forced detachment as a last resort
                        addLog("WARNING: Encoder polling thread for port 0x%02X had to be detached", port);
                    }
                }
            }

            motorThreads.erase(port);
            addLog("Encoder polling stopped for port 0x%02X", port);
        }
    }

    void ActivateEncoderMode(uint8_t port)
    {
        if (!isValid || !peripheral.is_connected()) return;

        addLog("Activating encoder mode for port 0x%02X", port);

        // Basic encoder activation command
        std::vector<uint8_t> setupCommand = {
            0x09,       // Length
            0x00,       // Hub ID  
            0x41,       // Port Configuration Command
            port,       // Motor port
            0x00,       // Mode: Position (absolute position)
            0x00,       // Data Format
            0x01,       // Unit: degrees
            0x00,       // Range min
            0x00        // Range max
        };

        sendCommandVector(setupCommand);

        // Command to enable notifications
        std::vector<uint8_t> subscribeCommand = {
            0x08,       // Length
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            port,       // Port  
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag
            0x00        // Padding
        };

        sendCommandVector(subscribeCommand);

        std::this_thread::sleep_for(200ms);
        addLog("Encoder mode activated for port 0x%02X", port);
    }

};

// Main context and virtual table
namespace
{
    std::mutex contextsMutex;
    std::map<PrinterImplementation*, std::unique_ptr<PrinterImplementation>> contexts;

    // Virtual table functions - a bridge between C++ and C-INTERFACE
    bool printer_connect(IPrinter* self) {
        if (!self || !self->vtable) return false;
        
        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->connect();
    }

    bool printer_disconnect(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->disconnect();
    }

    bool printer_is_connected(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->isConnected();
    }

    void printer_destroy(IPrinter* self) {
        if (!self) return;

        try {
            PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
            
            Implementation->safeShutdown();
            
            std::lock_guard<std::mutex> Lock(contextsMutex);
            
            if (contexts.find(Implementation) != contexts.end()) {
                contexts.erase(Implementation);
            }
        }
        catch (...) {
            // Ignore all errors
        }
    }

    void printer_set_motor_speed(IPrinter* self, unsigned char port, signed char speed) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        Implementation->setMotorSpeed(port, speed);
    }

    void printer_rotate_motor(IPrinter* self, const MotorCommand* commands, int count) {
        if (!self || !self->vtable || !commands || count < -1) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        Implementation->rotateMotor(commands, count);
    }

    bool printer_printer_execute_speed_profile(IPrinter* self, const SpeedProfile* profile) {
        if (!self || !self->vtable || !profile) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->executeSpeedProfile(profile);
    }

    void printer_send_command(IPrinter* self, const unsigned char* command, int length) {
        if (!self || !self->vtable || !command || length < -1) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        Implementation->sendCommand(command, length);
    }

    bool printer_is_motor_moving(IPrinter* self, unsigned char port) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->isMotorMoving(port);
    }

    double printer_get_motor_position(IPrinter* self, unsigned char port) {
        if (!self || !self->vtable) return 0.0;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->getMotorPosition(port);
    }
    
    int printer_get_log_count(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->GetLogCount();
    }

    const char* printer_get_log_entry(IPrinter* self, int index) {
        if (!self || !self->vtable) return nullptr;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->GetLogEntry(index);
    }

    void printer_printer_connection_info(IPrinter* self) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->printConnectionInfo();
    }

    void printer_clear_log(IPrinter* self) {
        if (!self || !self->vtable) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->ClearLog();
    }

    const char* printer_get_last_error(IPrinter* self) {
        if (!self || !self->vtable) return nullptr;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->GetLastErrorMessage();
    }

    bool printer_test_encoder_functionality(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(self);
        return Implementation->testEncoderFunctionality(self);
    }
}

// Virtual Method Table - C-INTERFACE
static IPrinterVirtualTable PrinterVTable = {
    printer_connect,
    printer_disconnect,
    printer_is_connected,
    printer_destroy,
    printer_rotate_motor,
    printer_set_motor_speed,
    printer_send_command,
    printer_printer_execute_speed_profile,
    printer_is_motor_moving,
    printer_get_motor_position,
    printer_get_log_count,
    printer_get_log_entry,
    printer_clear_log,
    printer_get_last_error,
    printer_printer_connection_info,
    printer_test_encoder_functionality
};

// Tested function - remove after deep testing

bool testSpeedProfileAdvanced(IPrinter* Printer) {
    SpeedProfilePoint points[] = {
        {3.0, 20, 0.0005},
        {3.0, 30, 0.0005},
        {0.0, 0, 1.0}
    };

    SpeedProfile profile;
    profile.port = 0x00;
    profile.points = points;
    profile.count = 3;
    profile.timeoutMs = 30000;

    bool result = PrinterExecuteSpeedProfile(Printer, &profile);
    return result;
}

// C-INTERFACE functions are exported to DLL
extern "C"
{

    PRINTER_DRIVER_API IPrinter* CreatePrinter() {
        auto printer = std::make_unique<PrinterImplementation>();
        printer->interface.vtable = &PrinterVTable;

        PrinterImplementation* printerHandle = printer.get();
        std::lock_guard<std::mutex> Lock(contextsMutex);

        contexts[printerHandle] = std::move(printer);
        return &printerHandle->interface;
    }

    PRINTER_DRIVER_API void DestroyPrinter(IPrinter* printer)
    {
        if (!printer) return;

        try {
            if (printer->vtable && printer->vtable->printer_destroy) {
                printer->vtable->printer_destroy(printer);
            }
        }
        catch (...) {
            // Ignore all errors
        }
    }

    PRINTER_DRIVER_API bool PrinterConnect(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_connect) return false;

        return printer->vtable->printer_connect(printer);
    }

    PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_disconnect) return false;

        return printer->vtable->printer_disconnect(printer);
    }

    PRINTER_DRIVER_API bool IsConnected(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_connected) return false;

        return printer->vtable->printer_is_connected(printer);
    }

    PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* printer, MotorCommand* commands, int count) {
        if (!printer || !printer->vtable || !printer->vtable->printer_rotate_motor) return;

        return printer->vtable->printer_rotate_motor(printer, commands, count);
    }

    PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* printer, const unsigned char* command, int length) {
        if (!printer || !printer->vtable || !printer->vtable->printer_send_command) return;

        return printer->vtable->printer_send_command(printer, command, length);
    }

    PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* printer, unsigned char port, signed char speed) {
        if (!printer || !printer->vtable || !printer->vtable->printer_set_motor_speed) return;

        return printer->vtable->printer_set_motor_speed(printer, port, speed);
    }

    PRINTER_DRIVER_API int GetLogCount(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_log_count) return 0;

        return printer->vtable->printer_get_log_count(printer);
    }

    PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* printer, int index) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_log_entry) return nullptr;

        return printer->vtable->printer_get_log_entry(printer, index);
    }

    PRINTER_DRIVER_API void ClearLog(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_clear_log) return;

        return printer->vtable->printer_clear_log(printer);
    }

    PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_last_error) return nullptr;

        return printer->vtable->printer_get_last_error(printer);
    }

    PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* printer, int count) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_motor_moving) return false;

        return printer->vtable->printer_is_motor_moving(printer, count);
    }

    PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* printer, unsigned char port) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_motor_position) return 0.0;

        return printer->vtable->printer_get_motor_position(printer, port);
    }

    // Test function
    PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* printer, const char* testName) {
        if (!printer || !testName) return false;

        std::string name(testName);

        if (name == "SpeedProfileAdvanced") {
            return testSpeedProfileAdvanced(printer);
        }
        else {
            return false;
        }
    }

    PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* printer) {
        if (printer) printer->vtable->printer_printer_connection_info(printer);
    }

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* printer, const SpeedProfile* profile) {
        if (!printer || !printer->vtable || !printer->vtable->printer_printer_execute_speed_profile) return false;

        return printer->vtable->printer_printer_execute_speed_profile(printer, profile);
    }    
}
