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
class PrinterImplementation
{
public:
    IPrinter Interface;

private:

    // Printer Status
    SimpleBLE::Peripheral peripheral;
    std::string LastError;
    std::atomic<bool> OperationInProgress;
    std::atomic<bool> StopRequested;
    std::atomic<bool> IsValid;
    std::atomic<int> Status;
    std::atomic<bool> WasConnected {false};

    struct MotorState
    {
        std::atomic<double> AbsolutePosition{ 0.0 };
        std::atomic<double> SegmentAccumulator{ 0.0 };
        std::atomic<double> RelativePosition{ 0.0 };
        std::atomic<double> CurrentPosition{ 0.0 };
        std::atomic<double> CurrentSpeed{ 0.0 };
        std::atomic<bool> IsMoving{ false };
        std::atomic<bool> ThreadRunning{ false };

        std::atomic<double> SegmentTarget{ 0.0 };

        std::atomic<double> SegmentStartPosition{ 0.0 };
        std::atomic<double> SegmentStartRelative{ 0.0 };
        std::atomic<double> SegmentTargetDistance{ 0.0 };
        std::atomic<double> CurrentSegmentTraveled{ 0.0 };
        std::atomic<int> CurrentSegmentIndex{ 0 };
        std::atomic<bool> ProfileActive{ false };

        std::vector<SpeedProfilePoint> ActiveProfile;
        int ProfileTimeoutMs = 60000;

        // Default constructor
        MotorState() = default;

        // Move constructor
        MotorState(MotorState&& other) noexcept
            : CurrentPosition(other.CurrentPosition.load())
            , RelativePosition(other.RelativePosition.load())
            , SegmentStartPosition(other.SegmentStartPosition.load())
            , CurrentSegmentTraveled(other.CurrentSegmentTraveled.load())
            , SegmentStartRelative(other.SegmentStartRelative.load())
            , CurrentSegmentIndex(other.CurrentSegmentIndex.load())
            , ThreadRunning(other.ThreadRunning.load())
            , ProfileActive(other.ProfileActive.load())
            , ActiveProfile(std::move(other.ActiveProfile))
            , ProfileTimeoutMs(other.ProfileTimeoutMs)
        {
        }

        // Move assignment
        MotorState& operator=(MotorState&& other) noexcept {
            if (this != &other) {
                CurrentPosition.store(other.CurrentPosition.load());
                RelativePosition.store(other.RelativePosition.load());
                SegmentStartPosition.store(other.SegmentStartPosition.load());
                CurrentSegmentTraveled.store(other.CurrentSegmentTraveled.load());
                SegmentStartRelative.store(other.SegmentStartRelative.load());
                CurrentSegmentIndex.store(other.CurrentSegmentIndex.load());
                ThreadRunning.store(other.ThreadRunning.load());
                ProfileActive.store(other.ProfileActive.load());
                ActiveProfile = std::move(other.ActiveProfile);
                ProfileTimeoutMs = other.ProfileTimeoutMs;
            }
            return *this;
        }

        // Remove copy
        MotorState(const MotorState&) = delete;
        MotorState& operator=(const MotorState&) = delete;
    };

    std::mutex SendCommandMutex;

    std::map<uint8_t, MotorState> MotorStates;
    std::map<uint8_t, std::thread> MotorThreads;
    std::condition_variable MotorStatesCV;

    // Logging system
    std::vector<std::string> LogEntries;
    std::mutex LogMutex;
    const size_t MAX_LOG_ENTRIES = 10000;

    // Simple synchronization system
    std::mutex OperationMutex;
    std::mutex CompletionMutex;
    std::condition_variable CompletionCV;

    struct CommandExecution
    {
        std::atomic<bool> Completed{ false };
        std::atomic<bool> Waiting{ false };

        CommandExecution()
        {
            Completed = true;
            Waiting = false;
        }

        CommandExecution(const CommandExecution&) = delete;
        CommandExecution& operator=(const CommandExecution&) = delete;

        CommandExecution(CommandExecution&& Other) noexcept
            : Completed(Other.Completed.load()), Waiting(Other.Waiting.load())
        {}
    };

    std::map<uint8_t, CommandExecution> CommandStatus;

    struct SpeedControlState 
    {
        std::atomic<bool> Active{false};
        std::atomic<size_t> CurrentPointIndex{0};
        std::thread ControlThread;

        std::vector<SpeedProfilePoint> ProfilePoints;
        int TimeoutMs;
    };

    std::map<uint8_t, SpeedControlState> SpeedControlStates;
    std::mutex SpeedControlMutex;

    std::mutex MotorStatesMutex;

public:

    PrinterImplementation() :
        OperationInProgress(false),
        StopRequested(false),
        IsValid(true),
        Status(0)
    {

        AddLog("PrinterImplementation created");
    }

    ~PrinterImplementation()
    {      
        try
        {
            std::lock_guard<std::mutex> Lock(CompletionMutex);
            CompletionCV.notify_all();
            for (auto& [Port, State] : SpeedControlStates)
            {
                State.Active = false;
                if (State.ControlThread.joinable())
                {
                    State.ControlThread.join();
                }
            }
        }
        catch (...)
        {
            // Ignore condition variable errors
        }

        // Automatic shutdown when variable is destroyed
        if (WasConnected)
        {
            IsValid = false;
            StopRequested = true;
            // Stop all motor threads
            for (auto& [Port, State] : MotorStates)
            {
                State.ThreadRunning = false;
            }

            for (auto& [Port, Thread] : MotorThreads)
            {
                if (Thread.joinable())
                {
                    Thread.join();
                }
            }

            if (peripheral.is_connected())
            {
                try
                {
                    if (!peripheral.address().empty() && peripheral.is_connected())
                    {
                        peripheral.disconnect();
                    }
                }
                catch (const std::exception& ex)
                {
                    // Ignore all errors
                }
            }
        }
    }

private:

    // Set notification handler
    void SetupNotificationHandler()
    {
        try
        {
            if (!peripheral.is_connected()) 
            {
                AddLog("ERROR: Peripheral not connected for encoder notifications");
                return;
            }

            peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                [this](const std::vector<uint8_t>& Data) 
                {
                    this->HandleHubNotification(Data);
                });

            AddLog("Encoder notifications setup completed - SUBSCRIBED");
        }
        catch (const std::exception& ex)
        {
            AddLog("Error setting up encoder notifications: %s", ex.what());
        }
    }

    void HandleHubNotification(const std::vector<uint8_t>& Data)
    {
        if (!IsValid || Data.empty()) return;

        const double ENCODER_TICKS_PER_REVOLUTION = 360.0;
        const double INCREMENTAL_FACTOR = 1.0 / 360.0;

        // For encoder type 0x45 (incremental)
        if (Data.size() >= 5 && Data[2] == 0x45) 
        {
            uint8_t Port = Data[3];
            InitializeMotorState(Port);

            uint8_t positionByte = Data[4];
            int8_t signedPosition = static_cast<int8_t>(positionByte);

            // Conversion for incremental encoder
            double positionDelta = static_cast<double>(signedPosition) * INCREMENTAL_FACTOR;

            UpdateMotorPosition(Port, positionDelta);

            if (std::abs(positionDelta) > 0.001) 
            {
                AddLog("ENCODER 0x45: Port=0x%02X, Raw=%d, Delta=%.4f rev",
                    Port, signedPosition, positionDelta);
            }
        }
        // For encoder type 0x04 (absolute)
        else if (Data.size() >= 8 && Data[2] == 0x04) 
        {
            uint8_t Port = Data[3];
            InitializeMotorState(Port);

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

            if (lastAbsolutePositions.count(Port)) 
            {
                positionDelta = absolutePosition - lastAbsolutePositions[Port];
                // Adjustment for overflow
                if (positionDelta > 180.0) positionDelta -= 360.0;
                else if (positionDelta < -180.0) positionDelta += 360.0;
            }
            lastAbsolutePositions[Port] = absolutePosition;

            UpdateMotorPosition(Port, positionDelta);

            if (std::abs(positionDelta) > 0.001) 
            {
                AddLog("ENCODER 0x04: Port=0x%02X, Raw=%d, Abs=%.3f, Delta=%.4f rev",
                    Port, SignedPosition, absolutePosition, positionDelta);
            }
        }
    }

    const double EMPIRICAL_CALIBRATION = 3.0 / 2.0;

    // In the UpdateMotorPosition function:
    void UpdateMotorPosition(uint8_t Port, double positionDelta)
    {
        auto& state = MotorStates[Port];

        // We apply empirical calibration
        double calibratedDelta = positionDelta * EMPIRICAL_CALIBRATION;

        double currentAbs = state.AbsolutePosition.load(std::memory_order_relaxed);
        double newAbs = currentAbs + calibratedDelta;
        state.AbsolutePosition.store(newAbs, std::memory_order_relaxed);

        double currentSeg = state.SegmentAccumulator.load(std::memory_order_relaxed);
        double newSeg = currentSeg + calibratedDelta;
        state.SegmentAccumulator.store(newSeg, std::memory_order_relaxed);

        if (std::abs(calibratedDelta) > 0.001) 
        {
            AddLog("POS_UPDATE: Port=0x%02X, RawDelta=%.4f, CalibratedDelta=%.4f, NewAbs=%.3f, NewSeg=%.3f",
                Port, positionDelta, calibratedDelta, newAbs, newSeg);
        }
    }

    void ProcessEncoderUpdate(uint8_t Port, int32_t rawValue, int bytes)
    {
        auto& state = MotorStates[Port];

        // Convert to revolutions depending on the data size
        double positionDelta = 0.0;
        if (bytes == 1) 
        {
            int8_t signedPosition = static_cast<int8_t>(rawValue & 0xFF);
            positionDelta = static_cast<double>(signedPosition) / 360.0;
        }
        else if (bytes == 4) 
        {
            positionDelta = static_cast<double>(rawValue) / 360.0;
        }

        // Updating the segment drive
        double currentAccumulator = state.SegmentAccumulator.load();
        double newAccumulator = currentAccumulator + positionDelta;
        state.SegmentAccumulator.store(newAccumulator);

        // Updating the absolute position
        double oldAbsolute = state.AbsolutePosition.load();
        state.AbsolutePosition.store(oldAbsolute + positionDelta);

        // We log only significant changes
        if (std::abs(positionDelta) > 0.001) 
        {
            AddLog("ENCODER: Port=0x%02X, Delta=%.3f, Accumulator=%.3f, Absolute=%.3f",
                Port, positionDelta, newAccumulator, oldAbsolute + positionDelta);
        }
    }

    // Internal helper methods
    void AddLog(const std::string& Message)
    {
        std::lock_guard<std::mutex> lock(LogMutex);

        // Get current time
        auto Now = std::chrono::system_clock::now();
        auto Time = std::chrono::system_clock::to_time_t(Now);
        auto Milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch()) % 1000;

        std::stringstream String;
        String << "[" << std::put_time(std::localtime(&Time), "%H:%M:%S");
        String << "." << std::setfill('0') << std::setw(3) << Milliseconds.count() << "] " << Message;

        // Adding message to log
        LogEntries.push_back(String.str());

        if (LogEntries.size() > MAX_LOG_ENTRIES)
        {
            LogEntries.erase(LogEntries.begin());
        }
    }

    void AddLog(const char* Format, ...)
    {
        char Buffer[1024 * 16];
        va_list Args;
        va_start(Args, Format);
        vsnprintf(Buffer, sizeof(Buffer), Format, Args);
        va_end(Args);

        AddLog(std::string(Buffer));
    }

    void SendCommandVector(std::vector<uint8_t> Command)
    {
        std::lock_guard<std::mutex> Lock(SendCommandMutex);

        if (!IsValid)
        {
            AddLog("SendCommandVector: Printer implementation is not valid");
            return;
        }
        if (!peripheral.is_connected())
        {
            AddLog("SendCommandVector: Printer is not connect");
            return;
        }

        try
        {

            // Check connection
            if (!peripheral.is_connected())
            {
                AddLog("SendCommandVector: Peripheral not connected");
                return;
            }

            // Logging sending command
            std::string HexCommand = "Command bytes: ";
            for (auto b : Command)
            {
                char Hex[4];
                snprintf(Hex, sizeof(Hex), "%02X", b);
                HexCommand += Hex;
            }
            //AddLog("%s", HexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, Command);
            //AddLog("Command sent successfully!");
        }
        catch (const std::exception& e)
        {
            AddLog("Error sending command: %s", e.what());
            LastError = e.what();
        }
    }

public:

    // Access to log from C-interface
    int GetLogCount()
    {
        std::lock_guard<std::mutex> lock(LogMutex);
        return static_cast<int>(LogEntries.size());
    }

    const char* GetLogEntry(int Index)
    {
        std::lock_guard<std::mutex> lock(LogMutex);
        if (Index < 0 || Index >= static_cast<int>(LogEntries.size()))
        {
            return "";
        }

        return LogEntries[Index].c_str();
    }

    void ClearLog()
    {
        std::lock_guard<std::mutex> lock(LogMutex);
        LogEntries.clear();
        AddLog("Log cleared");
    }

    const char* GetLastErrorMessage()
    {
        return LastError.empty() ? "" : LastError.c_str();
    }

    // Basic methods
    bool Connect()
    {
        if (!IsValid)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(OperationMutex);

        try
        {
            // Checking Bluetooth Status
            AddLog("Checking Bluetooth status:");

            bool ble_enabled = SimpleBLE::Adapter::bluetooth_enabled();
            AddLog("  - SimpleBLE::Adapter::bluetooth_enabled(): %s" + ble_enabled ? "true" : "false");

            // Getting a list of adapters
            auto adapters = SimpleBLE::Adapter::get_adapters();
            AddLog("  - Adapters found: %zu" + adapters.size());

            if (adapters.empty())
            {
                AddLog("Bluetooth adapters not found! Possible reasons:");
                AddLog("1. The Bluetooth adapter is disabled or not working");
                AddLog("2. Drivers not installed");
                AddLog("3. Hardware problem");
                return false;
            }

            // We use the first adapter
            SimpleBLE::Adapter& adapter = adapters[0];
            AddLog("Adapter used: %s [%s]",
                adapter.identifier().c_str(),
                adapter.address().c_str());

            // Setting up callbacks
            adapter.set_callback_on_scan_start([]() { });

            AddLog("Scanning started...");

            adapter.set_callback_on_scan_stop([]() { });

            AddLog("Scanning stopped");

            adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral)
                {
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
                    for (const auto& data : manufacturer_data)
                    {
                        // LEGO Company ID: 0x0397 (little-endian: 97 03)
                        if (data.first == 0x0397)
                        {
                            isLego = true;
                            AddLog("[LEGO Manufacturer Data Found]");
                        }
                    }

                    if (isLego)
                    {
                        AddLog("LEGO HUB DISCOVERED!");
                    }
                });

            // Start scanning
            adapter.scan_start();
            AddLog("Starting Bluetooth scan for 10 seconds...");
            std::this_thread::sleep_for(10s);
            adapter.scan_stop();

            // We get a list of found devices
            auto peripherals = adapter.scan_get_results();
            AddLog("Scan completed. Found %d devices", peripherals.size());
            std::cout << "\n\nDevices found: " << peripherals.size() << "\n";

            // Search LEGO Hub
            for (auto& ScannedPeripheral : peripherals)
            {
                std::string name = ScannedPeripheral.identifier();
                std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                if (name.find("LEGO") != std::string::npos ||
                    name.find("HUB") != std::string::npos ||
                    name.find("CONTROL") != std::string::npos)
                {

                    AddLog("Attempting to connect to LEGO Hub: %s", name.c_str());

                    // Connection attempt
                    try 
                    {
                        ScannedPeripheral.connect();

                        // In the main function, after connection:               
                        if (ScannedPeripheral.is_connected()) 
                        {
                            AddLog("Successfully connected to LEGO Hub");

                            peripheral = std::move(ScannedPeripheral);

                            SetupNotificationHandler();

                            // Looking for LEGO Hub service and features
                            SimpleBLE::Service lego_service;
                            SimpleBLE::Characteristic lego_char;

                            for (auto& service : ScannedPeripheral.services()) 
                            {
                                if (service.uuid() == LEGO_HUB_SERVICE_UUID) 
                                {
                                    lego_service = service;
                                    for (auto& characteristic : service.characteristics()) 
                                    {
                                        if (characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) 
                                        {
                                            lego_char = characteristic;
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }

                            if (lego_char.uuid().empty()) 
                            {
                                std::cout << "LEGO Hub has not found!" << std::endl;
                                return false;
                            }

                            WasConnected = true;
                            return true;
                        }
                    }
                    catch (const std::exception& e) 
                    {
                        AddLog("Connection error: %s", e.what());
                        LastError = e.what();
                    }
                }
            }

            AddLog("No LEGO Hub found or connection failed");
            return false;
        }
        catch (const std::exception& e) 
        {
            AddLog("Exception in Connect: %s", e.what());
            LastError = e.what();
            return false;
        }
    }

    bool Disconnect()
    {
        std::lock_guard<std::mutex> contextLock(OperationMutex);

        if (!IsValid || !peripheral.initialized() || peripheral.address().empty())
        {
            return true; // Disconnect already completed
        }

        if (peripheral.is_connected())
        {
            try
            {
                peripheral.disconnect();
            }
            catch (...)
            {
                // Ignoring disconnection errors
                return false;
            }
        }
        return true;
    }

    bool IsConnected()
    {
        return peripheral.is_connected();
    }

    void PrintConnectionInfo()
    {
        std::lock_guard<std::mutex> lock(OperationMutex);

        AddLog("=== CONNECTION INFORMATION ===");

        if (!peripheral.is_connected())
        {
            AddLog("NOT CONNECTED to any device");
            return;
        }

        AddLog("Device: %s", peripheral.identifier().c_str());
        AddLog("Address: %s", peripheral.address().c_str());
        AddLog("RSSI: %d", peripheral.rssi());
        AddLog("Connected: %s", peripheral.is_connected() ? "true" : "false");

        auto Services = peripheral.services();
        AddLog("Services count: %zu", Services.size());

        for (auto Service : Services)
        {
            AddLog("Service UUID: %s", Service.uuid().c_str());
            
            if (Service.uuid() == LEGO_HUB_SERVICE_UUID)
            {
                AddLog(" >>> LEGO SERVICE FOUND!");
                for (auto Characteristic : Service.characteristics())
                {
                    AddLog("    Characteristic: %s", Characteristic.uuid().c_str());
                    if (Characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID)
                    {
                        AddLog("    >>> LEGO CHARACTERISTIC FOUND!");
                    }
                }
            }
        }

        AddLog("========================================");
    }

    void RotateMotor(const MotorCommand* Commands, int Count)
    {      
        // Check parameters
        if (!IsValid || Count <= 0 || !Commands)
        {
            AddLog("RotateMotor: Invalid parameters");
            return;
        }

        AddLog("RotateMotor called with %d commands", Count);

        // Check connection
        if (!peripheral.is_connected())
        {
            AddLog("Printer is not connected!");
            return;
        }

        std::lock_guard<std::mutex> OperationLock(OperationMutex);        

        // Prepare command tracking
        for (int i = 0; i < Count; i++)
        {
            CommandStatus[Commands[i].Port].Completed = false;
            CommandStatus[Commands[i].Port].Waiting = true;
        }

        // Send all commands
        for (int i = 0; i < Count; i++)
        {
            SendSingleMotorCommand(Commands[i]);
        }

        WaitForCommandsCompletion(Commands, Count);
        AddLog("RotateMotor completed");
    }

    // Monitoring
    bool IsMotorMoving(unsigned char Port)
    {
        if (MotorStates.count(Port))
        {
            return MotorStates[Port].IsMoving;
        }

        return false;
    }

    double GetMotorPosition(uint8_t Port)
    {
        if (MotorStates.count(Port)) 
        {
            // We return AbsolutePosition since it is relevant
            return MotorStates[Port].AbsolutePosition.load();
        }
        return 0.0;
    }

    void SetMotorSpeed(uint8_t Port, int8_t Speed)
    {
        if (!IsValid || !peripheral.is_connected())
        {
            return;
        }

        AddLog("Setting motor speed: Port=0x%02X, Speed=%d", Port, Speed);

        // First command: Activate mode
        std::vector<uint8_t> SetupCommand = 
        {
            0x09,       // Package length
            0x00,       // Hub ID
            0x41,       // Port configuration command
            Port,       // Motor port
            0x01,       // Mode: Power (1)
            0x00,       // Data Format
            0x00,       // Unit
            0x00,       // Range min
            0x00        // Range max
        };

        SendCommandVector(SetupCommand);

        // Second Team: motor control
        std::vector<uint8_t> MotorCommand = 
        {
            0x08,       // Package length
            0x00,       // Hub ID
            0x81,       // Output control command
            Port,       // Motor port
            0x02,       // Subcommand: WriteDirectModeData
            0x01,       // Mode: Power (1)
            static_cast<uint8_t>(Speed) // Speed
        };

        SendCommandVector(MotorCommand);
    }

private:

    void SendSingleMotorCommand(const MotorCommand& Command)
    {
        AddLog("Motor command - Port: 0x%02X, Speed: %d, Revolutions: %.2f",
            Command.Port, Command.Speed, Command.Revolutions);

        // Convert revolutions to absolute degrees (1 revolution = 360 degrees)
        int32_t Degrees = static_cast<int32_t>(std::round(Command.Revolutions * 360.0));
        AddLog("Calculated degrees: %d", Degrees);

        std::vector<uint8_t> Payload = 
        {
        0x0F,       // Message length (15 bytes)
        0x00,       // Message counter
        0x81,       // Output control command
        Command.Port, // Port or combo port
        0x11,
        0x0B,       // Sub-team
        // Rotation angle (4 bytes little-endian)
        static_cast<uint8_t>(Degrees & 0xFF),
        static_cast<uint8_t>((Degrees >> 8) & 0xFF),
        static_cast<uint8_t>((Degrees >> 16) & 0xFF),
        static_cast<uint8_t>((Degrees >> 24) & 0xFF),
        // Speed (1 byte)
        static_cast<uint8_t>(Command.Speed),
        // Maximum power (usually 100%)
        100,
        // Final state (0 = float/coast, 1 = brake/hold)
        0x01,       // Hold the position after completion
        // Use profile (0 = use acceleration profile)
        0x00
        };

        AddLog("Sending motor command to port 0x%02X", Command.Port);
        SendCommandVector(Payload);
    }

    void WaitForCommandsCompletion(const MotorCommand* Commands, int Count)
    {
        std::unique_lock<std::mutex> lock(CompletionMutex);

        // Wait while condition variable gets notification about completing all commands
        bool AllCompleted = CompletionCV.wait_for(lock, std::chrono::seconds(30),
            [this, Commands, Count]()
            {
                for (int i = 0; i < Count; i++)
                {
                    if (!CommandStatus[Commands[i].Port].Completed)
                    {
                        return false;
                    }
                }
            });

        if (!AllCompleted)
        {
            // For timeout - end all
            for (int i = 0; i < Count; i++)
            {
                CommandStatus[Commands[i].Port].Waiting = false;
            }
        }

        // Delete waiting status
        for (int i = 0; i < Count; i++)
        {
            CommandStatus[Commands[i].Port].Waiting = false;
        }
    }

    bool WaitForCommandCompletion(uint8_t Port, int TimeoutMs = 15000)
    {
        std::unique_lock<std::mutex> lock(CompletionMutex);

        // Make sure the element exists in the map
        if (CommandStatus.find(Port) == CommandStatus.end())
        {
            return false;
        }

        // Setting the wait state
        CommandStatus[Port].Completed = false;
        CommandStatus[Port].Waiting = true;

        // We are waiting for notification of completion
        bool success = CompletionCV.wait_for(lock, std::chrono::milliseconds(TimeoutMs),
            [this, Port]() {
                auto it = CommandStatus.find(Port);
                if (it != CommandStatus.end())
                {
                    return it->second.Completed.load();
                }

                return true; // If there is no port, we consider it complete.
            });

        if (CommandStatus.find(Port) != CommandStatus.end())
        {
            CommandStatus[Port].Waiting = false;
        }

        if (!success)
        {
            AddLog("WaitForCommandCompletion timeout for port 0x%02X", Port);
        }
        else
        {
            AddLog("WaitForCommandCompletion success for port 0x%02X", Port);
        }

        return success;
    }

public:

    void SendCommand(const unsigned char* Command, int Length)
    {
        if (!IsValid)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(OperationMutex);

        try
        {
            std::vector<uint8_t> command(Command, Command + Length);

            // Check connection
            if (!peripheral.is_connected())
            {
                AddLog("Peripheral is not connected");
                return;
            }

            // Logging sending command
            std::string HexCommand = "Command bytes: ";
            for (auto b : command)
            {
                char Hex[4];
                snprintf(Hex, sizeof(Hex), "%02X", b);
                HexCommand += Hex;
            }
            AddLog(HexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, command);
            AddLog("Command sent successfully!");
        }
        catch (const std::exception& e)
        {
            AddLog("Error sending command: %s", e.what());
            LastError = e.what();
        }
    }

    void SafeShutdown()
    {
        IsValid = false;
        StopRequested = true;

        // Safe breaking all operations
        try
        {
            std::lock_guard<std::mutex> Lock(CompletionMutex);
            CompletionCV.notify_all();
        }
        catch (...)
        {
            // Ignoring all errors
        }
    }

  //----- Execute speed profile methods -----

public:

    bool ExecuteSpeedProfile(const SpeedProfile* Profile)
    {
        if (!IsValid || !Profile || Profile->Count < 1) {
            AddLog("Error: Invalid profile parameters");
            return false;
        }

        AddLog("=== EXECUTE SPEED PROFILE ===");
        AddLog("Port: 0x%02X, Segments: %d, Timeout: %d ms",
            Profile->Port, Profile->Count, Profile->TimeoutMs);

        uint8_t Port = Profile->Port;

        // Activate the encoder
        ActivateEncoderMode(Port);
        std::this_thread::sleep_for(200ms);

        // Setting up notifications
        SetupNotificationHandler();
        std::this_thread::sleep_for(200ms);

        // Logging your profile
        for (int i = 0; i < Profile->Count; i++) 
        {
            AddLog("Segment %d: Distance=%.3f rev, Speed=%d, Tolerance=%.3f",
                i, Profile->Points[i].Distance, Profile->Points[i].Speed,
                Profile->Points[i].Tolerance);
        }

        // Preparing profile points
        std::vector<SpeedProfilePoint> profilePoints;
        for (int i = 0; i < Profile->Count; i++) 
        {
            profilePoints.push_back(Profile->Points[i]);
        }

        return StartRelativeProfileController(Port, profilePoints, Profile->TimeoutMs);
    }

private:

    bool StartRelativeProfileController(uint8_t Port, const std::vector<SpeedProfilePoint>& ProfilePoints, int TimeoutMs)
    {
        InitializeMotorState(Port);
        auto& state = MotorStates[Port];

        // Stop the previous profile
        state.ProfileActive = false;
        std::this_thread::sleep_for(100ms);

        // We reset the drive and set up a new profile
        state.SegmentAccumulator.store(0.0);
        state.ActiveProfile = ProfilePoints;
        state.ProfileTimeoutMs = TimeoutMs;
        state.CurrentSegmentIndex.store(0);

        if (!ProfilePoints.empty()) 
        {
            state.SegmentTarget.store(ProfilePoints[0].Distance);
        }

        state.ProfileActive.store(true);

        AddLog("Starting relative profile: Segments=%zu, Timeout=%dms",
            ProfilePoints.size(), TimeoutMs);

        // We launch the controller in a separate thread
        std::thread controllerThread([this, Port]() 
            {
            this->RelativeProfileController(Port);
            });
        controllerThread.detach();

        return true;
    }

    void InitializeMotorState(uint8_t Port)
    {
        if (!MotorStates.count(Port)) 
        {
            // The correct way to initialize
            MotorStates[Port] = MotorState();
            AddLog("Initialized motor state for port 0x%02X", Port);
        }
    }

    void RelativeProfileController(uint8_t Port)
    {
        auto& state = MotorStates[Port];
        AddLog("=== STARTING PRECISE PROFILE CONTROLLER ===");

        // Full state reset
        state.AbsolutePosition.store(0.0);
        state.CurrentSegmentIndex.store(0);
        state.ProfileActive.store(true);

        // We start polling the encoder
        StartContinuousEncoderPolling(Port);

        auto profileStartTime = std::chrono::steady_clock::now();
        auto lastControlTime = profileStartTime;
        bool profileCompleted = false;

        // Launching the first segment
        if (!state.ActiveProfile.empty()) 
        {
            StartSegment(Port, 0);
        }

        while (!profileCompleted && state.ProfileActive && IsValid) 
        {
            auto currentTime = std::chrono::steady_clock::now();
            auto timeSinceLastControl = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastControlTime);

            // Precise control with a fixed interval
            lastControlTime = currentTime;

            int currentSegment = state.CurrentSegmentIndex.load();

            // We check the completion of all segments
            if (currentSegment >= state.ActiveProfile.size()) 
            {
                SetMotorSpeed(Port, 0);
                AddLog("PROFILE COMPLETED: All segments finished");
                profileCompleted = true;
                break;
            }

            const auto& segment = state.ActiveProfile[currentSegment];
            double traveled = state.SegmentAccumulator.load(std::memory_order_relaxed); // Use absolute position

            // Логируем прогресс каждые 500ms
            static auto lastLogTime = profileStartTime;
            auto timeSinceLastLog = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - lastLogTime);
            if (timeSinceLastLog.count() > 500) 
            {
                AddLog("SEGMENT %d: Traveled=%.3f/%.3f rev (%.1f%%), Speed=%d",
                    currentSegment, traveled, segment.Distance, (traveled / segment.Distance) * 100, segment.Speed);
                lastLogTime = currentTime;
            }

            // Checking the completion of the current segment
            if (traveled >= segment.Distance - segment.Tolerance) 
            {
                AddLog("=== SEGMENT %d COMPLETED ===", currentSegment);
                AddLog("Traveled %.3f of %.3f revolutions", traveled, segment.Distance);

                // Let's move on to the next segment
                int nextSegment = currentSegment + 1;
                state.CurrentSegmentIndex.store(nextSegment);

                if (nextSegment < state.ActiveProfile.size()) 
                {
                    // RESET position for a new segment
                    state.SegmentAccumulator.store(0.0, std::memory_order_relaxed);
                    StartSegment(Port, nextSegment);
                }
                else 
                {
                    // All segments are complete
                    SetMotorSpeed(Port, 0);
                    AddLog("=== PROFILE COMPLETED ===");
                    profileCompleted = true;
                }
            }

            // Timeout check
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - profileStartTime);
            if (elapsed.count() > state.ProfileTimeoutMs) 
            {
                AddLog("PROFILE TIMEOUT: %d ms elapsed", elapsed.count());

                AddLog("Current segment: %d, Traveled: %.3f/%.3f",
                    currentSegment, traveled, segment.Distance);
                SetMotorSpeed(Port, 0);
                state.ProfileActive = false;
                break;
            }
        }

        // Shutdown
        state.ProfileActive = false;
        StopEncoderPolling(Port);

        if (MotorThreads.count(Port) && MotorThreads[Port].joinable())
        {
            MotorThreads[Port].join();
            MotorThreads.erase(Port);
        }

        AddLog("Profile controller stopped for port 0x%02X", Port);
    }

    void StartSegment(uint8_t Port, int segmentIndex)
    {
        auto& state = MotorStates[Port];
        const auto& segment = state.ActiveProfile[segmentIndex];

        // Reset ONLY the segment drive
        state.SegmentAccumulator.store(0.0, std::memory_order_relaxed);

        // Logging initial values ​​for debugging
        double initialAbs = state.AbsolutePosition.load(std::memory_order_relaxed);
        double initialSeg = state.SegmentAccumulator.load(std::memory_order_relaxed);

        // Setting the motor speed
        SetMotorSpeed(Port, segment.Speed);

        AddLog(">>> STARTING SEGMENT %d: Target=%.3f rev, Speed=%d",
            segmentIndex, segment.Distance, segment.Speed);
        AddLog(">>> INITIAL VALUES: AbsPos=%.3f, SegAcc=%.3f", initialAbs, initialSeg);
    }

    void StartContinuousEncoderPolling(uint8_t Port)
    {
        StopEncoderPolling(Port);

        AddLog("Starting optimized encoder polling for port 0x%02X", Port);

        MotorThreads[Port] = std::thread([this, Port]() 
            {
            auto& state = MotorStates[Port];
            auto lastRequestTime = std::chrono::steady_clock::now();
            const std::chrono::milliseconds requestInterval(5);

            while (IsValid && state.ProfileActive.load(std::memory_order_relaxed)) 
            {
                auto currentTime = std::chrono::steady_clock::now();
                auto elapsed = currentTime - lastRequestTime;

                if (elapsed >= requestInterval) 
                {
                    PollEncoderPosition(Port);
                    lastRequestTime = currentTime;
                }
            }
            AddLog("Encoder polling thread finished for port 0x%02X", Port);
            });
    }

    void ResetEncoderPosition(uint8_t Port)
    {
        AddLog("=== MANUAL POSITION RESET ===");

        auto& state = MotorStates[Port];
        double currentAbsolute = state.AbsolutePosition.load();

        // We reset ALL positions
        state.AbsolutePosition.store(0.0);
        state.SegmentAccumulator.store(0.0);
        state.RelativePosition.store(0.0); // if this variable is still in use

        AddLog("Reset: Port=0x%02X, Was=%.3f, Now=0.000", Port, currentAbsolute);
    }

    void PollEncoderPosition(uint8_t Port)
    {
        // Encoder position query command
        std::vector<uint8_t> requestCmd = 
        {
            0x05,       // Length
            0x00,       // Hub ID
            0x21,       // Port Information Request
            Port,       // Port
            0x00        // Mode: position
        };

        SendCommandVector(requestCmd);
    }

    // New try

public:

    // Updated method for testing
    bool TestEncoderFunctionality(IPrinter* Printer)
    {
        if (!Printer)
        {
            return false;
        }

        AddLog("FUNCTION do not do everything");
    }

private:          

    void StartEncoderPolling(uint8_t Port)
    {
        // We stop the previous survey if there was one
        StopEncoderPolling(Port);

        AddLog("Encoder polling started for port 0x%02X", Port);

        // Запускаем новый поток опроса
        MotorThreads[Port] = std::thread([this, Port]() 
            {
            while (IsValid && !StopRequested) // Limit the number of requests
            { 
                PollEncoderPosition(Port);
                std::this_thread::sleep_for(50ms); // Request every 50ms
            }
            AddLog("Encoder polling stopped for port 0x%02X after %d requests", Port);
            });
    }    

    bool QuickEncoderTest(uint8_t Port)
    {
        AddLog("=== QUICK ENCODER TEST (REAL-TIME) ===");

        // Resetting the position
        ResetEncoderPosition(Port);

        // We get the initial position
        double startPos = GetMotorPosition(Port);
        AddLog("Start position: %.3f", startPos);

        // We start polling the encoder to activate updates
        StartEncoderPolling(Port);

        // We'll wait a bit to get some initial data.
        std::this_thread::sleep_for(100ms);

        // We receive a position after activating the survey
        startPos = GetMotorPosition(Port);
        AddLog("Position after polling start: %.3f", startPos);

        // We start the engine for a short time
        SetMotorSpeed(Port, 40);
        AddLog("Rotating motor at speed 40...");

        // We wait and measure the change in position
        auto startTime = std::chrono::steady_clock::now();
        double maxPosition = startPos;
        int measurements = 0;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count() < 300) 
        {

            double currentPos = GetMotorPosition(Port);
            if (currentPos > maxPosition) 
            {
                maxPosition = currentPos;
            }

            // Logging progress
            if (measurements % 5 == 0) // Every 5 measurements
            { 
                AddLog("Test loop %d: position=%.3f", measurements, currentPos);
            }

            measurements++;
            std::this_thread::sleep_for(50ms);
        }

        // We stop the engine
        SetMotorSpeed(Port, 0);
        AddLog("Setting motor speed: Port=0x%02X, Speed=0", Port);

        // Let the engine stop
        std::this_thread::sleep_for(100ms);

        // Final Dimension
        double finalPos = GetMotorPosition(Port);
        double positionChange = finalPos - startPos;

        AddLog("Position after 300ms: %.3f (change: %.3f), measurements: %d",
            finalPos, positionChange, measurements);

        bool success = (positionChange > 0.05); // Minimum expected change
        AddLog(success ? "SUCCESS: Encoder working! Position changed from %.3f to %.3f" :
            "FAILED: Encoder not responding to motor movement",
            startPos, finalPos);

        // Stopping the survey
        StopEncoderPolling(Port);

        if (!success) 
        {
            AddLog("Final position: %.3f", finalPos);
            AddLog("Last received encoder data analysis:");
            AddLog("  - Check if 0x45 notifications are being received");
            AddLog("  - Check if position bytes are changing in 0x45 messages");
        }

        return success;
    }

    void StopEncoderPolling(uint8_t Port)
    {
        if (MotorThreads.count(Port)) 
        {
            // Setting stop flags
            if (MotorStates.count(Port)) 
            {
                MotorStates[Port].ProfileActive.store(false, std::memory_order_relaxed);
            }

            // Wait for the thread to complete with a timeout
            if (MotorThreads[Port].joinable()) 
            {
                auto& thread = MotorThreads[Port];
                if (thread.get_id() != std::this_thread::get_id()) 
                {
                    // We give the thread 500ms to complete
                    for (int i = 0; i < 50 && thread.joinable(); i++) 
                    {
                        std::this_thread::sleep_for(10ms);
                    }
                    if (thread.joinable()) 
                    {
                        thread.detach(); // Forced detachment as a last resort
                        AddLog("WARNING: Encoder polling thread for port 0x%02X had to be detached", Port);
                    }
                }
            }

            MotorThreads.erase(Port);
            AddLog("Encoder polling stopped for port 0x%02X", Port);
        }
    }

    void ActivateEncoderMode(uint8_t Port)
    {
        if (!IsValid || !peripheral.is_connected())
        {
            return;
        }

        AddLog("Activating encoder mode for port 0x%02X", Port);

        // Basic encoder activation command
        std::vector<uint8_t> setupCmd = 
        {
            0x09,       // Length
            0x00,       // Hub ID  
            0x41,       // Port Configuration Command
            Port,       // Motor port
            0x00,       // Mode: Position (absolute position)
            0x00,       // Data Format
            0x01,       // Unit: degrees
            0x00,       // Range min
            0x00        // Range max
        };

        SendCommandVector(setupCmd);

        // Command to enable notifications
        std::vector<uint8_t> subscribeCmd = 
        {
            0x08,       // Length
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Port  
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag
            0x00        // Padding
        };

        SendCommandVector(subscribeCmd);

        std::this_thread::sleep_for(200ms);
        AddLog("Encoder mode activated for port 0x%02X", Port);
    }

};

// Main context and virtual table
namespace
{
    std::mutex ContextsMutex;
    std::map<PrinterImplementation*, std::unique_ptr<PrinterImplementation>> Contexts;

    // Virtual table functions - a bridge between C++ and C-INTERFACE

    bool Printer_Connect(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }
        
        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->Connect();
    }

    bool Printer_Disconnect(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->Disconnect();
    }

    bool Printer_IsConnected(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->IsConnected();
    }

    void Printer_Destroy(IPrinter* Self)
    {
        if (!Self)
        {
            return;
        }

        try
        {
            PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
            
            Implementation->SafeShutdown();
            
            std::lock_guard<std::mutex> Lock(ContextsMutex);
            
            if (Contexts.find(Implementation) != Contexts.end())
            {
                Contexts.erase(Implementation);
            }
        }
        catch (...)
        {
            // Ignore all errors
        }
    }

    void Printer_SetMotorSpeed(IPrinter* Self, unsigned char Port, signed char Speed)
    {
        if (!Self || !Self->VirtualTable)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->SetMotorSpeed(Port, Speed);
    }

    void Printer_RotateMotor(IPrinter* Self, const MotorCommand* Commands, int Count)
    {
        if (!Self || !Self->VirtualTable || !Commands || Count <= 0)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->RotateMotor(Commands, Count);
    }

    bool Printer_PrinterExecuteSpeedProfile(IPrinter* Self, const SpeedProfile* Profile)
    {
        if (!Self || !Self->VirtualTable || !Profile)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->ExecuteSpeedProfile(Profile);
    }

    void Printer_SendCommand(IPrinter* Self, const unsigned char* Command, int Length)
    {
        if (!Self || !Self->VirtualTable || !Command || Length <= 0)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->SendCommand(Command, Length);
    }

    bool Printer_IsMotorMoving(IPrinter* Self, unsigned char Port)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->IsMotorMoving(Port);
    }

    double Printer_GetMotorPosition(IPrinter* Self, unsigned char Port)
    {
        if (!Self || !Self->VirtualTable)
        {
            return 0.0;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->GetMotorPosition(Port);
    }
    
    int Printer_GetLogCount(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return 0;
        }
        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->GetLogCount();
    }

    const char* Printer_GetLogEntry(IPrinter* Self, int Index)
    {
        if (!Self || !Self->VirtualTable)
        {
            return nullptr;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->GetLogEntry(Index);
    }

    void Printer_PrinterConnectionInfo(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->PrintConnectionInfo();
    }

    void Printer_ClearLog(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->ClearLog();
    }

    const char* Printer_GetLastError(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return "";
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->GetLastErrorMessage();
    }

    bool Printer_TestEncoderFunctionality(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->TestEncoderFunctionality(Self);
    }
}

// Virtual Method Table - C-INTERFACE
static IPrinterVirtualTable PrinterVTable = {
    Printer_Connect,
    Printer_Disconnect,
    Printer_IsConnected,
    Printer_Destroy,
    Printer_RotateMotor,
    Printer_SetMotorSpeed,
    Printer_SendCommand,
    Printer_PrinterExecuteSpeedProfile,
    Printer_IsMotorMoving,
    Printer_GetMotorPosition,
    Printer_GetLogCount,
    Printer_GetLogEntry,
    Printer_ClearLog,
    Printer_GetLastError,
    Printer_PrinterConnectionInfo,
    Printer_TestEncoderFunctionality
};

// Tested function - remove after deep testing

bool TestSpeedProfileAdvanced(IPrinter* Printer)
{
    SpeedProfilePoint Points[] = {
        {3.0, 20, 0.0005},
        {3.0, 30, 0.0005},
        {0.0, 0, 1.0}
    };

    SpeedProfile Profile;
    Profile.Port = 0x00;
    Profile.Points = Points;
    Profile.Count = 3;
    Profile.TimeoutMs = 30000;

    bool Result = PrinterExecuteSpeedProfile(Printer, &Profile);
    return Result;
}

// C-INTERFACE functions are exported to DLL
extern "C"
{

    PRINTER_DRIVER_API IPrinter* CreatePrinter()
    {
        auto Printer = std::make_unique<PrinterImplementation>();
        Printer->Interface.VirtualTable = &PrinterVTable;

        PrinterImplementation* PrinterHandle = Printer.get();
        std::lock_guard<std::mutex> Lock(ContextsMutex);

        Contexts[PrinterHandle] = std::move(Printer);
        return &PrinterHandle->Interface;
    }

    PRINTER_DRIVER_API void DestroyPrinter(IPrinter* Printer)
    {
        if (!Printer)
        {
            return;
        }

        try
        {
            if (Printer->VirtualTable && Printer->VirtualTable->Destroy)
            {
                Printer->VirtualTable->Destroy(Printer);
            }
        }
        catch (...)
        {
            // Ignore all errors
        }
    }

    PRINTER_DRIVER_API bool PrinterConnect(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->Connect)
        {
            return false;
        }

        return Printer->VirtualTable->Connect(Printer);
    }

    PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->Disconnect)
        {
            return false;
        }

        return Printer->VirtualTable->Disconnect(Printer);
    }

    PRINTER_DRIVER_API bool IsConnected(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->IsConnected)
        {
            return false;
        }

        return Printer->VirtualTable->IsConnected(Printer);
    }

    PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* Printer, MotorCommand* Commands, int Count)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->RotateMotor)
        {
            return;
        }

        return Printer->VirtualTable->RotateMotor(Printer, Commands, Count);
    }

    PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* Printer, const unsigned char* Command, int Length)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->SendCommand)
        {
            return;
        }

        return Printer->VirtualTable->SendCommand(Printer, Command, Length);
    }

    PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* Printer, unsigned char Port, signed char Speed)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->SetMotorSpeed)
        {
            return;
        }

        return Printer->VirtualTable->SetMotorSpeed(Printer, Port, Speed);
    }

    PRINTER_DRIVER_API int GetLogCount(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->GetLogCount)
        {
            return 0;
        }

        return Printer->VirtualTable->GetLogCount(Printer);
    }

    PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* Printer, int Index)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->GetLogEntry)
        {
            return "";
        }

        return Printer->VirtualTable->GetLogEntry(Printer, Index);
    }

    PRINTER_DRIVER_API void ClearLog(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->ClearLog)
        {
            return;
        }

        return Printer->VirtualTable->ClearLog(Printer);
    }

    PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->GetLastError)
        {
            return "";
        }

        return Printer->VirtualTable->GetLastError(Printer);
    }

    PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* Printer, int Count)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->IsMotorMoving)
        {
            return false;
        }

        return Printer->VirtualTable->IsMotorMoving(Printer, Count);
    }

    PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* Printer, unsigned char Port)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->GetMotorPosition)
        {
            return 0.0;
        }

        return Printer->VirtualTable->GetMotorPosition(Printer, Port);
    }

    // Test function
    PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* Printer, const char* TestName)
    {
        if (!Printer || !TestName)
        {
            return false;
        }

        std::string Name(TestName);

        if (Name == "SpeedProfileAdvanced")
        {
            return TestSpeedProfileAdvanced(Printer);
        }
        else
        {
            return false;
        }
    }

    PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* Printer)
    {
        if (Printer)
        {
            Printer->VirtualTable->PrintConnectionInfo(Printer);
        }
    }

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* Printer, const SpeedProfile* Profile)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->PrinterExecuteSpeedProfile)
        {
            return false;
        }

        return Printer->VirtualTable->PrinterExecuteSpeedProfile(Printer, Profile);
    }    
}
