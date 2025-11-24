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

    // Encoder event system
    struct EncoderEventState
    {
        std::vector<EncoderEvent> ActiveEvents;
        std::mutex EventsMutex;
        std::condition_variable EventCV;
        std::atomic<bool> EventTriggered{ false };
    };

    std::map<uint8_t, EncoderEventState> EncoderEvents;

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

    void CalibrateEncoder(uint8_t Port)
    {
        AddLog("=== STARTING ENCODER CALIBRATION ===");

        // Resetting the position
        ResetEncoderPosition(Port);
        double startPos = GetMotorPosition(Port);

        // We rotate the motor at a known angle (for example, 1 full revolution)
        AddLog("Rotating motor 1 full revolution...");

        // Use a physical marker to know exactly when 1 revolution is completed.
        // Or use a known gear ratio

        // Запускаем мотор
        SetMotorSpeed(Port, 50);

        // Wait until the motor makes 1 revolution (determine this physically)
        // For the test, wait 2 seconds at speed 50
        std::this_thread::sleep_for(2000ms);

        // We stop the engine
        SetMotorSpeed(Port, 0);
        std::this_thread::sleep_for(500ms);

        // We measure the distance traveled using the encoder
        double endPos = GetMotorPosition(Port);
        double measuredRevolutions = endPos - startPos;

        AddLog("CALIBRATION RESULTS:");
        AddLog("  Expected: 1.000 revolution");
        AddLog("  Measured: %.3f revolutions", measuredRevolutions);
        AddLog("  Calibration factor needed: %.3f", 1.0 / measuredRevolutions);

        if (std::abs(measuredRevolutions - 1.0) < 0.1) 
        {
            AddLog("CALIBRATION: Good - encoder is accurate");
        }
        else 
        {
            AddLog("CALIBRATION: Poor - encoder needs calibration factor");
            AddLog("  Suggested calibration factor: %.3f", 1.0 / measuredRevolutions);
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

    void HandleEncoderNotification(const std::vector<uint8_t>& Data)
    {
        if (!IsValid || Data.size() < 8 || Data[2] != 0x04) 
        {
            return;
        }

        uint8_t Port = Data[3];

        // Decoding the position
        int32_t PositionRaw =
            (static_cast<int32_t>(Data[4]) & 0xFF) |
            ((static_cast<int32_t>(Data[5]) & 0xFF) << 8) |
            ((static_cast<int32_t>(Data[6]) & 0xFF) << 16) |
            ((static_cast<int32_t>(Data[7]) & 0xFF) << 24);

        double PositionRevolutions = static_cast<double>(PositionRaw) / 360.0;

        AddLog("Encoder raw: Port=0x%02X, Data=%02X%02X%02X%02X, Raw=%d, Rev=%.3f",
            Port, Data[4], Data[5], Data[6], Data[7],
            PositionRaw, PositionRevolutions);

        // Updating the engine status
        if (MotorStates.count(Port)) 
        {
            double OldPosition = MotorStates[Port].CurrentPosition;
            MotorStates[Port].CurrentPosition = PositionRevolutions;

            if (std::abs(PositionRevolutions - OldPosition) > 0)
            {
                AddLog("Encoder update: Port=0x%02X, Position=%3.f (delta=%.3f)",
                    Port, PositionRevolutions, PositionRevolutions - OldPosition);
            }
        }

        // We log significant changes (less frequently to avoid cluttering the logs)
        static std::map<uint8_t, double> lastLoggedPositions;
        if (!lastLoggedPositions.count(Port) ||
            std::abs(PositionRevolutions - lastLoggedPositions[Port]) > 0.05) 
        {
            AddLog("Encoder: Port=0x%02X, Position=%.3f", Port, PositionRevolutions);
            lastLoggedPositions[Port] = PositionRevolutions;
        }

        // Checking encoder events
        CheckEncoderEvents(Port, PositionRevolutions);
    }

    void CheckEncoderEvents(uint8_t Port, double CurrentPosition)
    {
        if (!EncoderEvents.count(Port))
        {
            return;
        }

        auto& EventState = EncoderEvents[Port];
        std::lock_guard<std::mutex> Lock(EventState.EventsMutex);

        bool Triggered;
        for (auto Item = EventState.ActiveEvents.begin(); Item != EventState.ActiveEvents.end();)
        {
            Triggered = false;
            
            switch (Item->Type)
            {
            case ENCODER_POSITION_REACHED:
            case ENCODER_SEGMENT_COMPLETED:
            case ENCODER_MOVEMENT_FINISHED:
                double Difference = std::abs(CurrentPosition - Item->TargetPosition);

                if (Difference <= Item->Tolerance)
                {
                    Triggered = true;
                    AddLog("Encoder event triggered: Port=0x%02X, Type=%d, Current=%.3f, Target=%.3f",
                        Port, Item->Type, CurrentPosition, Item->TargetPosition);
                }
                break;
            }

            if (Triggered)
            {
                if (Item->Callback)
                {
                    Item->Callback(Port, Item->Type, CurrentPosition, Item->UserData);
                }

                Item = EventState.ActiveEvents.erase(Item);
                EventState.EventTriggered = true;
                EventState.EventCV.notify_all();
            }
            else
            {
                ++Item;
            }
        }
    }

    bool WaitForEncoderEventInternal(uint8_t Port, EncoderEventType EventType,
        double TargetPosition, double Tolerance, int TimeoutMs)
    {
        if (!EncoderEvents.count(Port))
        {
            return true;
        }
        
        auto& EventState = EncoderEvents[Port];
        std::unique_lock<std::mutex> Lock(EventState.EventsMutex);

        EncoderEvent TempEvent;
        TempEvent.Port = Port;
        TempEvent.Type = EventType;
        TempEvent.TargetPosition = TargetPosition;
        TempEvent.Tolerance = Tolerance;

        EventState.ActiveEvents.push_back(TempEvent);

        bool Success = EventState.EventCV.wait_for(Lock,
            std::chrono::milliseconds(TimeoutMs),
            [&]() 
            {
                return EventState.EventTriggered.load();
            });

        // Remove temporary event
        auto Item = EventState.ActiveEvents.begin();
        while (Item != EventState.ActiveEvents.end())
        {
            if (Item->Port == TempEvent.Port &&
                Item->Type == TempEvent.Type &&
                std::abs(Item->TargetPosition - TempEvent.TargetPosition) < 1e-9 &&
                std::abs(Item->Tolerance - TempEvent.Tolerance) < 1e-9)
            {
                Item = EventState.ActiveEvents.erase(Item);
                break;
            }
            else
            {
                ++Item;
            }
        }

        if (Item != EventState.ActiveEvents.end())
        {
            EventState.ActiveEvents.erase(Item);
        }

        EventState.EventTriggered = false;

        return Success;
    }

    void SetupEncoderNotification()
    {
        try
        {
            peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                [this](const std::vector<uint8_t>& Data)
                {
                    this->HandleEncoderNotification(Data);
                });

            AddLog("Encoder notification handler setup completed");
        }
        catch (const std::exception& ex)
        {
            AddLog("Error setting up encoder notification: " + std::string(ex.what()));
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

    // Encoder event system
    bool SubscribeToEncoderEvents(const EncoderEvent* Events, int Count)
    {
        if (!Events || Count < 1)
        {
            return false;
        }

        for (int i = 0; i < Count; i++)
        {
            const EncoderEvent& Event = Events[i];
            EncoderEvents[Event.Port].ActiveEvents.push_back(Event);
        }

        AddLog("Subscribed to " + std::to_string(Count) + " encoder events");
        return true;
    }

    bool UnsubscribeFromEncoderEvents(uint8_t Port)
    {
        if (EncoderEvents.count(Port))
        {
            EncoderEvents[Port].ActiveEvents.clear();
            AddLog("Unsubscribed from encoder events on port " + std::to_string(Port));
            return true;
        }

        return false;
    }

    bool WaitForEncoderEvent(uint8_t Port, EncoderEventType EventType,
        double TargetPosition, double Tolerance, int TimeoutMs)
    {
        return WaitForEncoderEventInternal(Port, EventType, TargetPosition, Tolerance, TimeoutMs);
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

    bool StartHybridProfileController(uint8_t Port, const std::vector<SpeedProfilePoint>& ProfilePoints, int TimeoutMs)
    {
        auto& state = MotorStates[Port];

        // Stop the previous profile
        state.ProfileActive = false;
        std::this_thread::sleep_for(100ms);

        // Setting up a new profile
        state.ActiveProfile = ProfilePoints;
        state.ProfileTimeoutMs = TimeoutMs;
        state.CurrentSegmentIndex = 0;
        state.SegmentStartRelative.store(state.RelativePosition.load());
        state.CurrentSegmentTraveled = 0.0;
        state.ProfileActive = true;

        // We launch the hybrid controller in a separate thread
        std::thread controllerThread([this, Port]() 
            {
            this->HybridProfileController(Port);
            });

        controllerThread.detach();

        return true;
    }

    void ActivateEncoderForPort(uint8_t Port)
    {
        AddLog("=== ACTIVATING ENCODER FOR PORT 0x%02X ===", Port);

        // 1. Port deactivation
        std::vector<uint8_t> deactivateCmd = { 0x05, 0x00, 0x41, Port, 0x00 };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Encoder activation
        std::vector<uint8_t> activateCmd = 
        {
            0x09,       // Length
            0x00,       // Hub ID
            0x41,       // Port Configuration
            Port,       // Port
            0x00,       // Mode: position
            0x01,       // Delta interval
            0x01,       // Unit: degrees
            0x01,       // Notifications enabled
            0x00        // Padding
        };
        SendCommandVector(activateCmd);
        std::this_thread::sleep_for(500ms);

        // 3. Subscribe to notifications
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
        std::this_thread::sleep_for(500ms);

        AddLog("Encoder activated for port 0x%02X", Port);
    }

    void CheckPortDevice(uint8_t Port)
    {
        AddLog("=== CHECKING DEVICE ON PORT 0x%02X ===", Port);

        // Device information query command
        std::vector<uint8_t> infoCmd = 
        {
            0x05,       // Length
            0x00,       // Hub ID  
            0x21,       // Port Information Request
            Port,       // Port
            0x01        // Information type
        };

        SendCommandVector(infoCmd);
        std::this_thread::sleep_for(500ms);
    }

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

    void ActivateEncoder(uint8_t Port)
    {
        AddLog("=== Activate encoder for port 0x%02X ===", Port);

        // Standard Encoder Activation Command for LEGO Technic Hub
        std::vector<uint8_t> ActivateCommand = 
        {
            0x0A,       // Package length
            0x00,       // Hub ID  
            0x41,       // Port configuration command
            Port,       // Port
            0x00,       // Mode: position (0x00 for absolute position)
            0x00,       // Delta interval
            0x01,       // Unit: degrees
            0x00,       // Notifications enabled
            0x00,       // Padding
            0x00        // Padding
        };

        SendCommandVector(ActivateCommand);
        AddLog("Encoder activated for port 0x%02X", Port);

        // Additional command to enable updates
        std::vector<uint8_t> SubscribeCommand = 
        {
            0x08,       // Package length
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Port
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag
            0x00        // Padding
        };

        SendCommandVector(SubscribeCommand);
        AddLog("Encoder subscriptions enabled for port 0x%02X", Port);

        std::this_thread::sleep_for(300ms);
        AddLog("=== Encoder activation complete ===");
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

    // Simplified encoder monitoring
    void StartEncoderMonitoring(uint8_t Port)
    {
        if (MotorStates[Port].ThreadRunning) return;

        MotorStates[Port].ThreadRunning = true;

        std::thread monitorThread([this, Port]() 
            {
            AddLog("Encoder monitoring started for port 0x%02X", Port);

            double lastPosition = MotorStates[Port].CurrentPosition.load();
            auto lastLogTime = std::chrono::steady_clock::now();

            while (MotorStates[Port].ThreadRunning && IsValid && !StopRequested) 
            {
                double currentPosition = MotorStates[Port].CurrentPosition.load();

                // Updating the distance traveled for the active segment
                if (MotorStates[Port].ProfileActive) 
                {
                    double segmentStart = MotorStates[Port].SegmentStartPosition.load();
                    double traveled = currentPosition - segmentStart;
                    MotorStates[Port].CurrentSegmentTraveled.store(traveled);

                    // Logging progress
                    auto currentTime = std::chrono::steady_clock::now();
                    if (std::abs(currentPosition - lastPosition) > 0.05 ||
                        std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastLogTime).count() >= 2) 
                    {

                        AddLog("MONITOR: Port=0x%02X, Pos=%.3f, Traveled=%.3f",
                            Port, currentPosition, traveled);
                        lastPosition = currentPosition;
                        lastLogTime = currentTime;
                    }
                }

                std::this_thread::sleep_for(10ms);
            }

            AddLog("Encoder monitoring stopped for port 0x%02X", Port);
            });

        monitorThread.detach();
    }

    bool SetupEncoder(uint8_t Port)
    {
        if (MotorStates[Port].ThreadRunning)
        {
            return true;
        }

        AddLog("Setting up encoder for port 0x%02X", Port);

        // Simple activation command
        std::vector<uint8_t> activateCmd = 
        {
            0x09, 
            0x00, 
            0x41, 
            Port, 
            0x00, 
            0x01, 
            0x01, 
            0x01, 
            0x00
        };
        SendCommandVector(activateCmd);
        std::this_thread::sleep_for(300ms);

        // Let's start monitoring
        StartEncoderMonitoring(Port);

        return true;
    }
    
    void CheckMotorType(uint8_t Port)
    {
        AddLog("=== Checking motor type for port 0x%02X ===", Port);

        // Command to query information about a device
        std::vector<uint8_t> InfoCommand = 
        {
            0x05,
            0x00,
            0x21,
            Port,
            0x01
        };

        SendCommandVector(InfoCommand);
        std::this_thread::sleep_for(500ms);

        // Additional command to query the device type
        std::vector<uint8_t> TypeCommand = 
        {
            0x05,
            0x00,
            0x21,
            Port,
            0x00
        };

        SendCommandVector(TypeCommand);
        std::this_thread::sleep_for(500ms);
    }

    bool ActivateEncoderAlternative(uint8_t Port)
    {
        AddLog("=== ALTERNATIVE ENCODER ACTIVATION ===");

        // 1. First, we deactivate the port completely
        std::vector<uint8_t> deactivateCmd = 
        {
            0x05,       // Length
            0x00,       // Hub ID
            0x41,       // Port Configuration
            Port,       // Port  
            0x00        // Deactivate
        };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Activate the port in encoder mode with different settings
        std::vector<std::vector<uint8_t>> activationAttempts = {
            // Attempt 1: Standard Activation
            {
                0x09,
                0x00,
                0x41,
                Port,
                0x00,
                0x00,
                0x01,
                0x01,
                0x00
            },
            // Attempt 2: Alternative settings
            {
                0x09,
                0x00,
                0x41,
                Port,
                0x00,
                0x01,
                0x01,
                0x01,
                0x00
            },
            // Attempt 3: Other options
            {
                0x09,
                0x00,
                0x41,
                Port,
                0x00,
                0x00,
                0x02,
                0x01,
                0x00
            }
        };

        for (size_t i = 0; i < activationAttempts.size(); i++) 
        {
            AddLog("Activation attempt %zu", i + 1);
            SendCommandVector(activationAttempts[i]);
            std::this_thread::sleep_for(300ms);

            // Checking for updates
            double pos = MotorStates[Port].CurrentPosition.load();
            if (pos != 0.0) 
            {
                AddLog("SUCCESS: Position updated to %.3f", pos);
                return true;
            }
        }

        return false;
    }

    void TestPhysicalConnection(uint8_t Port)
    {
        AddLog("=== PHYSICAL CONNECTION TEST ===");

        // Test 1: Checking if the motor rotates at all
        AddLog("1. Testing motor rotation without encoder");
        SetMotorSpeed(Port, 50);
        std::this_thread::sleep_for(2000ms);

        // Stop and listen to the sound/watch the rotation
        SetMotorSpeed(Port, 0);
        std::this_thread::sleep_for(1000ms);

        // Test 2: Testing different speeds
        AddLog("2. Testing different speeds");
        for (int speed = 30; speed <= 80; speed += 20)
        {
            AddLog("   Speed %d", speed);
            SetMotorSpeed(Port, speed);
            std::this_thread::sleep_for(1000ms);

            double pos = MotorStates[Port].CurrentPosition.load();
            AddLog("   Position: %.3f", pos);

            if (pos != 0.0) 
            {
                AddLog("   ENCODER WORKING AT SPEED %d!", speed);
                SetMotorSpeed(Port, 0);
                return;
            }
        }

        SetMotorSpeed(Port, 0);
        AddLog("3. Motor rotates but encoder doesn't update - likely encoder hardware issue");
    }

    void TestAllMotorPorts()
    {
        AddLog("=== TESTING ALL MOTOR PORTS ===");

        std::vector<uint8_t> motorPorts = 
        { 
            0x00,
            0x01,
            0x02,
            0x03
        };

        for (uint8_t port : motorPorts) 
        {
            AddLog("--- Testing port 0x%02X ---", port);

            // Activate the encoder
            ActivateEncoderAlternative(port);
            std::this_thread::sleep_for(500ms);

            // Resetting the position
            std::vector<uint8_t> resetCmd = 
            { 
                0x08,
                0x00,
                0x81,
                port,
                0x51,
                0x02,
                0x00,
                0x00
            };
            SendCommandVector(resetCmd);
            std::this_thread::sleep_for(500ms);

            // Rotate and check
            double initialPos = MotorStates[port].CurrentPosition.load();
            AddLog("Initial position: %.3f", initialPos);

            SetMotorSpeed(port, 40);
            std::this_thread::sleep_for(2000ms);
            SetMotorSpeed(port, 0);

            double finalPos = MotorStates[port].CurrentPosition.load();
            AddLog("Final position: %.3f", finalPos);

            bool encoderWorking = (std::abs(finalPos - initialPos) > 0.1);
            AddLog("Encoder on port 0x%02X: %s", port, encoderWorking ? "WORKING" : "NOT WORKING");

            if (encoderWorking) 
            {
                AddLog("*** FOUND WORKING ENCODER ON PORT 0x%02X ***", port);
                return;
            }

            std::this_thread::sleep_for(1000ms);
        }

        AddLog("*** NO WORKING ENCODERS FOUND ON ANY PORT ***");
    }

    void RequestEncoderPosition(uint8_t Port)
    {
        if (!IsValid || !peripheral.is_connected()) 
        {
            return;
        }

        // Encoder position information query command
        std::vector<uint8_t> requestCommand = 
        {
            0x05,       // Package length
            0x00,       // Hub ID
            0x21,       // Port Information Request
            Port,       // Port
            0x00        // Mode information
        };

        SendCommandVector(requestCommand);

        // We don't log every request to avoid cluttering the logs.
        static int requestCount = 0;
        if (++requestCount % 50 == 0) // Log every 50 requests
        { 
            AddLog("Encoder position request sent for port 0x%02X (request #%d)",
                Port, requestCount);
        }
    }

    bool ActivateEncoderForPolling(uint8_t Port)
    {
        AddLog("=== ПРАВИЛЬНАЯ АКТИВАЦИЯ ЭНКОДЕРА ДЛЯ LEGO TECHNIC HUB ===");

        // 1. Complete port deactivation
        AddLog("1. Деактивация порта");
        std::vector<uint8_t> deactivateCmd = 
        { 
            0x05,
            0x00,
            0x41,
            Port,
            0x00
        };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(500ms);

        // 2. Activating the port as an input device - CRITICAL!
        AddLog("2. Активация порта как устройства ввода");
        std::vector<uint8_t> setupCmd = 
        {
            0x09,       // Length
            0x00,       // Hub ID
            0x41,       // Port Configuration
            Port,       // Port
            0x00,       // Mode: Position (0x00)
            0x01,       // Delta interval
            0x01,       // Unit: Degrees
            0x01,       // Notifications: ON
            0x00        // Padding
        };
        SendCommandVector(setupCmd);
        std::this_thread::sleep_for(1000ms);

        // 3. Enabling position updates
        AddLog("3. Включение обновлений позиции");
        std::vector<uint8_t> subscribeCmd = 
        {
            0x08,       // Length
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Port
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe: ON
            0x00        // Padding
        };
        SendCommandVector(subscribeCmd);
        std::this_thread::sleep_for(1000ms);

        // 4. Reset position
        AddLog("4. Сброс позиции энкодера");
        std::vector<uint8_t> resetCmd = 
        {
            0x08,       // Length
            0x00,       // Hub ID  
            0x81,       // Output command
            Port,       // Port
            0x51,       // Subcommand: Preset Encoder
            0x02,       // Mode: Preset value
            0x00,       // Value low byte
            0x00        // Value high byte
        };
        SendCommandVector(resetCmd);
        std::this_thread::sleep_for(1000ms);

        // Reset the internal position
        MotorStates[Port].CurrentPosition.store(0.0);

        AddLog("Энкодер активирован для порта 0x%02X", Port);

        // We test immediately
        TestEncoderResponse(Port, "После активации");

        return true;
    }

    bool TestWorkingPort(uint8_t Port)
    {
        AddLog("=== TESTING WORKING PORT 0x%02X ===", Port);

        // 1. Activate the encoder
        if (!ActivateEncoderAlternative(Port)) 
        {
            AddLog("Failed to activate encoder");
            return false;
        }

        // 2. Reset the position
        std::vector<uint8_t> resetCmd = 
        { 
            0x08,
            0x00,
            0x81,
            Port,
            0x51,
            0x02,
            0x00,
            0x00
        };
        SendCommandVector(resetCmd);
        std::this_thread::sleep_for(500ms);

        // 3. Launch a test profile
        SpeedProfilePoint points[] = 
        {
            {0.5, 30, 0.05},   // 0.5 revolutions at 30 speed
            {1.0, 50, 0.05},   // 1.0 rpm at 50
            {1.5, 30, 0.05},   // 1.5 rpm at 30
            {2.0, 0, 0.02}     // 2.0 revolutions - stop
        };

        SpeedProfile profile = { Port, points, 4, 30000 };

        AddLog("Starting test profile on working port 0x%02X", Port);
        return ExecuteSpeedProfile(&profile);
    }

    void DebugMotorAndEncoder(uint8_t Port)
    {
        AddLog("=== DEEP MOTOR/ENCODER DEBUG FOR PORT 0x%02X ===", Port);

        // 1. We check that the motor responds to commands at all.
        AddLog("1. Testing motor basic functionality");
        SetMotorSpeed(Port, 30);
        std::this_thread::sleep_for(1000ms);
        SetMotorSpeed(Port, 0);
        std::this_thread::sleep_for(500ms);

        // 2. Check the initial state of the encoder
        double initialPos = MotorStates[Port].CurrentPosition.load();
        AddLog("2. Initial encoder position: %.3f", initialPos);

        // 3. Sending a port status query command
        AddLog("3. Requesting port status");
        std::vector<uint8_t> statusCmd = 
        { 
            0x05,
            0x00,
            0x21,
            Port,
            0x01
        }; // Port information request
        SendCommandVector(statusCmd);
        std::this_thread::sleep_for(500ms);

        // 4. Trying different methods of encoder activation
        AddLog("4. Trying different encoder activation methods");

        // Method 1: Standard LEGO Activation
        std::vector<uint8_t> activate1 = { 0x09, 0x00, 0x41, Port, 0x00, 0x00, 0x01, 0x01, 0x00 };
        SendCommandVector(activate1);
        std::this_thread::sleep_for(300ms);

        // Method 2: Alternative Activation
        std::vector<uint8_t> activate2 = { 0x09, 0x00, 0x41, Port, 0x02, 0x02, 0x00, 0x01, 0x00 };
        SendCommandVector(activate2);
        std::this_thread::sleep_for(300ms);

        // Method 3: Another Approach
        std::vector<uint8_t> activate3 = { 0x0A, 0x00, 0x41, Port, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00 };
        SendCommandVector(activate3);
        std::this_thread::sleep_for(300ms);

        // 5. Reset the position
        AddLog("5. Resetting encoder position");
        std::vector<uint8_t> resetCmd = { 0x08, 0x00, 0x81, Port, 0x51, 0x02, 0x00, 0x00 };
        SendCommandVector(resetCmd);
        std::this_thread::sleep_for(500ms);

        // 6. Check the position after reset
        double afterReset = MotorStates[Port].CurrentPosition.load();
        AddLog("6. Position after reset: %.3f", afterReset);

        // 7. Rotate the motor and monitor the encoder in real time
        AddLog("7. Real-time encoder monitoring during rotation");
        SetMotorSpeed(Port, 40);

        auto startTime = std::chrono::steady_clock::now();
        bool encoderUpdated = false;

        for (int i = 0; i < 50; i++) // 5 seconds of monitoring
        { 
            std::this_thread::sleep_for(100ms);
            double currentPos = MotorStates[Port].CurrentPosition.load();

            if (std::abs(currentPos - afterReset) > 0.01) 
            {
                AddLog("   ENCODER UPDATED! Position: %.3f at %d ms",
                    currentPos, (i + 1) * 100);
                encoderUpdated = true;
                break;
            }

            // We log every second
            if ((i + 1) % 10 == 0) 
            {
                AddLog("   Still monitoring... position: %.3f", currentPos);
            }
        }

        SetMotorSpeed(Port, 0);

        if (!encoderUpdated) 
        {
            AddLog("8. CRITICAL: Encoder never updated during rotation");
            AddLog("   Possible causes:");
            AddLog("   - Motor doesn't have encoder hardware");
            AddLog("   - Wrong port configuration");
            AddLog("   - Hub firmware issue");
            AddLog("   - Physical connection problem");
        }
        else 
        {
            AddLog("8. SUCCESS: Encoder is working!");
        }
    }

    void HardwareDiagnostics()
    {
        AddLog("=== HARDWARE DIAGNOSTICS ===");

        // Тестируем все основные порты
        std::vector<uint8_t> portsToTest = { 0x00, 0x01, 0x02, 0x03 };

        for (uint8_t port : portsToTest) {
            AddLog("--- Testing port 0x%02X ---", port);

            // Проверяем, есть ли вообще устройство на порту
            AddLog("Checking if device exists on port...");

            // Отправляем команду запроса информации о порту
            std::vector<uint8_t> infoCmd = { 0x05, 0x00, 0x21, port, 0x01 };
            SendCommandVector(infoCmd);
            std::this_thread::sleep_for(500ms);

            // Проверяем базовую функциональность мотора
            AddLog("Testing motor basic function...");
            SetMotorSpeed(port, 25);
            std::this_thread::sleep_for(1000ms);
            SetMotorSpeed(port, 0);
            std::this_thread::sleep_for(500ms);

            // Проверяем энкодер
            DebugMotorAndEncoder(port);

            std::this_thread::sleep_for(1000ms);
        }

        AddLog("=== HARDWARE DIAGNOSTICS COMPLETE ===");
    }

    bool ActivateEncoderWithFeedback(uint8_t Port)
    {
        AddLog("=== ACTIVATING ENCODER WITH FEEDBACK FOR PORT 0x%02X ===", Port);

        // 1. Полностью деактивируем порт
        std::vector<uint8_t> deactivateCmd = { 0x05, 0x00, 0x41, Port, 0x00 };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Активируем с разными настройками и проверяем отклик
        std::vector<std::pair<std::string, std::vector<uint8_t>>> activationMethods = {
            {"Standard Position Mode", {0x09, 0x00, 0x41, Port, 0x00, 0x00, 0x01, 0x01, 0x00}},
            {"Alternative Mode", {0x09, 0x00, 0x41, Port, 0x02, 0x02, 0x00, 0x01, 0x00}},
            {"Extended Mode", {0x0A, 0x00, 0x41, Port, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00}}
        };

        for (const auto& method : activationMethods) {
            AddLog("Trying: %s", method.first.c_str());
            SendCommandVector(method.second);
            std::this_thread::sleep_for(400ms);

            // Проверяем, появились ли обновления энкодера
            double posBefore = MotorStates[Port].CurrentPosition.load();

            // Вращаем мотор немного чтобы проверить обновления
            SetMotorSpeed(Port, 20);
            std::this_thread::sleep_for(500ms);
            SetMotorSpeed(Port, 0);
            std::this_thread::sleep_for(200ms);

            double posAfter = MotorStates[Port].CurrentPosition.load();

            if (std::abs(posAfter - posBefore) > 0.01) {
                AddLog("SUCCESS: %s WORKED! Position changed: %.3f -> %.3f",
                    method.first.c_str(), posBefore, posAfter);
                return true;
            }
        }

        AddLog("FAILED: No activation method worked");
        return false;
    }

    void CheckMotorTypeAndConnection()
    {
        AddLog("=== MOTOR TYPE AND CONNECTION CHECK ===");

        // Вопросы для диагностики:
        AddLog("Please verify:");
        AddLog("1. Is this a Technic Large/Medium Angular Motor?");
        AddLog("2. Is the motor firmly connected to port A (0x00)?");
        AddLog("3. Is the hub battery charged?");
        AddLog("4. Have you tried a different motor?");
        AddLog("5. Have you tried a different port?");

        // Тест для порта 0x00
        AddLog("Running detailed test for port 0x00...");
        DebugMotorAndEncoder(0x00);
    }

    void DeepEncoderDiagnostics(uint8_t Port)
    {
        AddLog("=== ГЛУБОКАЯ ДИАГНОСТИКА ЭНКОДЕРА ПОРТ 0x%02X ===", Port);

        // 1. Проверяем, что мотор вообще вращается
        AddLog("1. Проверка вращения мотора без энкодера");
        SetMotorSpeed(Port, 50);
        std::this_thread::sleep_for(3000ms);
        SetMotorSpeed(Port, 0);
        AddLog("   Мотор должен был вращаться 3 секунды");
        std::this_thread::sleep_for(1000ms);

        // 2. Проверяем тип мотора и подключение
        AddLog("2. Запрос информации об устройстве");
        std::vector<uint8_t> deviceInfoCmd = { 0x05, 0x00, 0x21, Port, 0x01 };
        SendCommandVector(deviceInfoCmd);
        std::this_thread::sleep_for(1000ms);

        // 3. Полная деактивация и переактивация
        AddLog("3. Полный сброс порта");
        std::vector<uint8_t> deactivateCmd = { 0x05, 0x00, 0x41, Port, 0x00 };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(500ms);

        // 4. Попробуем разные методы активации энкодера
        AddLog("4. Тестируем разные методы активации энкодера:");

        // Метод 1: Стандартный LEGO
        AddLog("   Метод 1: Стандартная активация");
        std::vector<uint8_t> activate1 = { 0x09, 0x00, 0x41, Port, 0x00, 0x01, 0x01, 0x01, 0x00 };
        SendCommandVector(activate1);
        std::this_thread::sleep_for(1000ms);

        TestEncoderResponse(Port, "После метода 1");

        // Метод 2: Альтернативный
        AddLog("   Метод 2: Альтернативная активация");
        std::vector<uint8_t> activate2 = { 0x09, 0x00, 0x41, Port, 0x02, 0x01, 0x00, 0x01, 0x00 };
        SendCommandVector(activate2);
        std::this_thread::sleep_for(1000ms);

        TestEncoderResponse(Port, "После метода 2");

        // Метод 3: Расширенный
        AddLog("   Метод 3: Расширенная активация");
        std::vector<uint8_t> activate3 = { 0x0A, 0x00, 0x41, Port, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00 };
        SendCommandVector(activate3);
        std::this_thread::sleep_for(1000ms);

        TestEncoderResponse(Port, "После метода 3");

        AddLog("=== ДИАГНОСТИКА ЗАВЕРШЕНА ===");
    }

    void TestEncoderResponse(uint8_t Port, const char* context)
    {
        double before = MotorStates[Port].CurrentPosition.load();
        AddLog("   %s: позиция до = %.3f", context, before);

        // Короткое вращение
        SetMotorSpeed(Port, 40);
        std::this_thread::sleep_for(2000ms);
        SetMotorSpeed(Port, 0);
        std::this_thread::sleep_for(500ms);

        double after = MotorStates[Port].CurrentPosition.load();
        AddLog("   %s: позиция после = %.3f, изменение = %.3f", context, after, after - before);

        if (std::abs(after - before) > 0.01) {
            AddLog("   *** УСПЕХ: Энкодер работает! ***");
        }
        else {
            AddLog("   *** ОШИБКА: Энкодер не обновляется ***");
        }
    }

    void CheckDeviceType(uint8_t Port)
    {
        AddLog("=== ПРОВЕРКА ТИПА УСТРОЙСТВА НА ПОРТУ 0x%02X ===", Port);

        // Команда запроса информации об устройстве
        std::vector<uint8_t> infoCmd = { 0x05, 0x00, 0x21, Port, 0x01 };
        SendCommandVector(infoCmd);
        std::this_thread::sleep_for(1000ms);

        // Дополнительная команда для запроса capabilities
        std::vector<uint8_t> capCmd = { 0x05, 0x00, 0x21, Port, 0x02 };
        SendCommandVector(capCmd);
        std::this_thread::sleep_for(1000ms);

        AddLog("Если в логах нет информации об устройстве - возможно неправильный порт");
    }

    void TestAllPorts()
    {
        AddLog("=== ТЕСТИРОВАНИЕ ВСЕХ ПОРТОВ ===");

        std::vector<uint8_t> ports = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

        for (uint8_t port : ports) {
            AddLog("--- Тестируем порт 0x%02X ---", port);

            // Проверяем тип устройства
            CheckDeviceType(port);

            // Активируем энкодер
            ActivateEncoderForPolling(port);

            // Тестируем отклик
            TestEncoderResponse(port, "Итоговый тест");

            std::this_thread::sleep_for(2000ms);
        }
    }

    void DebugAllIncomingData()
    {
        AddLog("=== DEBUG ALL INCOMING DATA ===");

        // Временно переустанавливаем обработчик чтобы логировать всё
        peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
            [this](const std::vector<uint8_t>& Data) {
                if (!Data.empty()) {
                    std::string hexStr = "RAW INCOMING: ";
                    for (uint8_t b : Data) {
                        char hex[4];
                        snprintf(hex, sizeof(hex), "%02X", b);
                        hexStr += hex;
                        hexStr += " ";
                    }
                    AddLog("%s", hexStr.c_str());

                    // Также обрабатываем данные в основном обработчике
                    this->HandleHubNotification(Data);
                }
            }
        );
        AddLog("Debug notification handler installed");
    }

    bool ActivateEncoderLikeOldMethod(uint8_t Port)
    {
        AddLog("=== АКТИВАЦИЯ КАК В СТАРЫХ РАБОЧИХ МЕТОДАХ ===");

        // 1. Деактивация
        std::vector<uint8_t> deactivateCmd = { 0x05, 0x00, 0x41, Port, 0x00 };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Активация КАК В СТАРОМ МЕТОДЕ
        std::vector<uint8_t> activateCmd = {
            0x0A,       // Длина пакета - 10 байт!
            0x00,       // Hub ID  
            0x41,       // Port configuration command
            Port,       // Порт
            0x00,       // Mode: position (0x00)
            0x00,       // Delta interval: 0 (важно!)
            0x01,       // Unit: градусы
            0x00,       // Notifications enabled: 0 (важно!)
            0x00,       // Padding
            0x00        // Padding
        };
        SendCommandVector(activateCmd);
        std::this_thread::sleep_for(500ms);

        // 3. Подписка КАК В СТАРОМ МЕТОДЕ
        std::vector<uint8_t> subscribeCmd = {
            0x08,       // Длина пакета
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Порт  
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag: 1
            0x00        // Padding
        };
        SendCommandVector(subscribeCmd);
        std::this_thread::sleep_for(500ms);

        // 4. Сброс позиции
        std::vector<uint8_t> resetCmd = { 0x08, 0x00, 0x81, Port, 0x51, 0x02, 0x00, 0x00 };
        SendCommandVector(resetCmd);
        std::this_thread::sleep_for(500ms);

        MotorStates[Port].CurrentPosition.store(0.0);

        AddLog("Активация завершена по старому методу для порта 0x%02X", Port);
        return true;
    }

    void CheckCurrentNotificationHandler()
    {
        AddLog("=== ПРОВЕРКА ТЕКУЩЕГО ОБРАБОТЧИКА УВЕДОМЛЕНИЙ ===");

        // Временно установим простой обработчик для отладки
        peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
            [this](const std::vector<uint8_t>& Data) {
                if (Data.size() >= 3) {
                    AddLog("NOTIFICATION: Type=0x%02X, Port=0x%02X, Length=%zu",
                        Data[2], Data.size() > 3 ? Data[3] : 0, Data.size());

                    // Если это данные энкодера (0x04)
                    if (Data[2] == 0x04 && Data.size() >= 8) {
                        uint8_t port = Data[3];
                        int32_t position = (Data[4] | (Data[5] << 8) | (Data[6] << 16) | (Data[7] << 24));
                        double revolutions = static_cast<double>(position) / 360.0;
                        AddLog("ENCODER DATA: Port=0x%02X, Raw=%d, Rev=%.3f",
                            port, position, revolutions);
                    }
                }

                // Также вызываем основной обработчик
                this->HandleHubNotification(Data);
            }
        );
        AddLog("Debug notification handler installed");
    }

    void ComprehensiveEncoderTest(uint8_t Port)
    {
        AddLog("=== ПОЛНЫЙ ТЕСТ ЭНКОДЕРА ===");

        // 1. Включаем отладку входящих данных
        DebugAllIncomingData();
        std::this_thread::sleep_for(1000ms);

        // 2. Проверяем текущий обработчик
        CheckCurrentNotificationHandler();
        std::this_thread::sleep_for(1000ms);

        // 3. Тестируем старый рабочий метод

        // 4. Тестируем активацию как в старом методе
        AddLog("--- Тест активации по старому методу ---");
        ActivateEncoderLikeOldMethod(Port);
        TestEncoderResponse(Port, "После активации по старому методу");

        // 5. Проверяем, приходят ли вообще уведомления
        AddLog("--- Проверка уведомлений ---");
        AddLog("Вращаем мотор и следим за уведомлениями...");

        SetMotorSpeed(Port, 30);
        for (int i = 0; i < 20; i++) {
            std::this_thread::sleep_for(100ms);
            double pos = MotorStates[Port].CurrentPosition.load();
            if (pos != 0.0) {
                AddLog("УСПЕХ: Позиция обновилась до %.3f на шаге %d", pos, i);
                break;
            }
        }
        SetMotorSpeed(Port, 0);

        AddLog("=== ТЕСТ ЗАВЕРШЕН ===");
    }

    void ActivateRealTimeEncoderUpdates(uint8_t Port)
    {
        AddLog("=== ACTIVATING REAL-TIME ENCODER UPDATES ===");

        // 1. Деактивация порта
        std::vector<uint8_t> deactivateCmd = { 0x05, 0x00, 0x41, Port, 0x00 };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Активация порта с настройками для реальных обновлений
        std::vector<uint8_t> activateCmd = {
            0x0A,       // Длина
            0x00,       // Hub ID
            0x41,       // Port Configuration
            Port,       // Порт
            0x00,       // Mode: position
            0x01,       // Delta interval: 1 (важно для частых обновлений)
            0x01,       // Unit: degrees
            0x01,       // Notifications enabled
            0x00,       // Padding
            0x00        // Padding
        };
        SendCommandVector(activateCmd);
        std::this_thread::sleep_for(500ms);

        // 3. Подписка на обновления с минимальным интервалом
        std::vector<uint8_t> subscribeCmd = {
            0x08,       // Длина
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Порт
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag
            0x00        // Padding
        };
        SendCommandVector(subscribeCmd);
        std::this_thread::sleep_for(500ms);

        AddLog("Real-time encoder updates activated for port 0x%02X", Port);
    }

    bool ActivateEncoderProperly(uint8_t Port)
    {
        AddLog("=== PROPER ENCODER ACTIVATION FOR PORT 0x%02X ===", Port);

        // 1. Деактивация порта (чистый старт)
        std::vector<uint8_t> deactivateCmd = { 0x05, 0x00, 0x41, Port, 0x00 };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Активация порта как устройства ВВОДА (input) - это критически важно!
        std::vector<uint8_t> activateCmd = {
            0x09,       // Длина
            0x00,       // Hub ID
            0x41,       // Port Configuration
            Port,       // Порт
            0x00,       // Mode: ABSOLUTE position (0x00)
            0x01,       // Delta interval: 1
            0x01,       // Unit: градусы
            0x01,       // Notifications: ВКЛЮЧЕНО (1)
            0x00        // Padding
        };
        SendCommandVector(activateCmd);
        std::this_thread::sleep_for(500ms);

        // 3. Подписка на обновления - используем правильный UUID сервиса
        std::vector<uint8_t> subscribeCmd = {
            0x08,       // Длина
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Порт
            0x02,       // Subcommand: Subscribe
            0x00,       // Mode
            0x01,       // Subscribe flag: 1
            0x00        // Padding
        };
        SendCommandVector(subscribeCmd);
        std::this_thread::sleep_for(500ms);

        // 4. Сброс позиции энкодера
        std::vector<uint8_t> resetCmd = {
            0x08,       // Длина
            0x00,       // Hub ID
            0x81,       // Output command
            Port,       // Порт
            0x51,       // Subcommand: Preset Encoder
            0x02,       // Preset value
            0x00,       // Value low byte
            0x00        // Value high byte
        };
        SendCommandVector(resetCmd);
        std::this_thread::sleep_for(500ms);

        // Сбрасываем позицию в памяти
        MotorStates[Port].CurrentPosition.store(0.0);
        MotorStates[Port].RelativePosition.store(0.0);

        AddLog("Encoder properly activated for port 0x%02X", Port);
        return true;
    }

    void HybridProfileController(uint8_t Port)
    {
        auto& state = MotorStates[Port];
        AddLog("=== STARTING HYBRID PROFILE CONTROLLER ===");

        // Сбрасываем позицию в памяти
        state.RelativePosition.store(0.0);
        state.SegmentStartRelative.store(0.0);
        state.CurrentSegmentTraveled.store(0.0);
        state.CurrentSegmentIndex.store(0);

        auto profileStartTime = std::chrono::steady_clock::now();
        auto segmentStartTime = profileStartTime;
        auto lastUpdateTime = profileStartTime;
        bool profileCompleted = false;

        // Запускаем первый сегмент
        if (state.ActiveProfile.size() > 0) {
            SetMotorSpeed(Port, state.ActiveProfile[0].Speed);
            AddLog("STARTING SEGMENT 0: Distance=%.3f, Speed=%d",
                state.ActiveProfile[0].Distance, state.ActiveProfile[0].Speed);
        }

        while (!profileCompleted && state.ProfileActive && IsValid && !StopRequested) {
            int currentSegment = state.CurrentSegmentIndex.load();

            if (currentSegment >= state.ActiveProfile.size()) {
                SetMotorSpeed(Port, 0);
                AddLog("PROFILE COMPLETED: All segments finished");
                profileCompleted = true;
                break;
            }

            const auto& segment = state.ActiveProfile[currentSegment];

            // ГИБРИДНЫЙ ПОДХОД: используем данные энкодера ИЛИ расчет по времени
            double encoderBasedTravel = 0.0;
            double timeBasedTravel = 0.0;

            // 1. Попытка использовать данные энкодера
            double currentEncoderPos = state.RelativePosition.load();
            double segmentStart = state.SegmentStartRelative.load();
            encoderBasedTravel = currentEncoderPos - segmentStart;

            // 2. Резервный расчет по времени
            auto currentTime = std::chrono::steady_clock::now();
            auto segmentDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                currentTime - segmentStartTime);

            // Расчет пройденного расстояния на основе времени и скорости
            // Предполагаем, что скорость 100% = ~1 оборот в секунду для больших моторов
            double speedFactor = segment.Speed / 100.0;
            timeBasedTravel = speedFactor * (segmentDuration.count() / 1000.0);

            // Выбираем максимальное значение из двух методов
            double traveled = std::max(encoderBasedTravel, timeBasedTravel);

            state.CurrentSegmentTraveled.store(traveled);

            // Логируем прогресс каждую секунду
            if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastUpdateTime).count() > 1000) {
                AddLog("SEGMENT %d: Encoder=%.3f, Time=%.3f, Used=%.3f/%.3f, Speed=%d",
                    currentSegment, encoderBasedTravel, timeBasedTravel, traveled,
                    segment.Distance, segment.Speed);
                lastUpdateTime = currentTime;
            }

            // Проверяем завершение текущего сегмента
            if (traveled >= segment.Distance - segment.Tolerance) {
                AddLog("SEGMENT %d COMPLETED: Traveled %.3f of %.3f",
                    currentSegment, traveled, segment.Distance);

                state.CurrentSegmentIndex++;
                currentSegment = state.CurrentSegmentIndex.load();

                if (currentSegment < state.ActiveProfile.size()) {
                    // Начинаем новый сегмент
                    state.SegmentStartRelative.store(currentEncoderPos);
                    state.CurrentSegmentTraveled.store(0.0);
                    segmentStartTime = std::chrono::steady_clock::now();

                    const auto& nextSegment = state.ActiveProfile[currentSegment];
                    SetMotorSpeed(Port, nextSegment.Speed);
                    AddLog("STARTING SEGMENT %d: Distance=%.3f, Speed=%d",
                        currentSegment, nextSegment.Distance, nextSegment.Speed);
                }
                else {
                    SetMotorSpeed(Port, 0);
                    AddLog("PROFILE COMPLETED: Total segments: %zu", state.ActiveProfile.size());
                    profileCompleted = true;
                }
            }

                // Проверка таймаута
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - profileStartTime);
            if (elapsed.count() > state.ProfileTimeoutMs) {
                AddLog("PROFILE TIMEOUT: %d ms elapsed", elapsed.count());
                SetMotorSpeed(Port, 0);
                state.ProfileActive = false;
                break;
            }

            std::this_thread::sleep_for(10ms);
        }

        state.ProfileActive = false;
        AddLog("Hybrid profile controller stopped for port 0x%02X", Port);
    }       

    void StartEncoderPolling(uint8_t Port)
    {
        // Останавливаем предыдущий опрос если был
        StopEncoderPolling(Port);

        AddLog("Encoder polling started for port 0x%02X", Port);

        // Запускаем новый поток опроса
        MotorThreads[Port] = std::thread([this, Port]() {
            while (IsValid && !StopRequested) { // Ограничим количество запросов
                PollEncoderPosition(Port);
                std::this_thread::sleep_for(50ms); // Запрос каждые 50ms
            }
            AddLog("Encoder polling stopped for port 0x%02X after %d requests", Port);
            });
    }

    void RequestEncoderData(uint8_t Port)
    {
        if (!IsValid || !peripheral.is_connected()) {
            return;
        }

        // Команда запроса данных позиции
        std::vector<uint8_t> requestCmd = {
            0x05,       // Длина  
            0x00,       // Hub ID  
            0x21,       // Port Information Request  
            Port,       // Моторный порт
            0x00        // Mode: Position
        };

        try {
            SendCommandVector(requestCmd);
        }
        catch (const std::exception& ex) {
            AddLog("ERROR sending encoder request: %s", ex.what());
        }
    }

    void CheckEncoderStatus(uint8_t Port)
    {
        AddLog("=== ENCODER STATUS CHECK ===");

        // Запрос статуса порта
        std::vector<uint8_t> statusCmd = {
            0x05,       // Длина
            0x00,       // Hub ID
            0x21,       // Port Information Request
            Port,       // Порт
            0x01        // Mode: configuration
        };

        SendCommandVector(statusCmd);
        AddLog("Sent status request for port 0x%02X", Port);
    }

    bool QuickEncoderTest(uint8_t Port)
    {
        AddLog("=== QUICK ENCODER TEST (REAL-TIME) ===");

        // Сбрасываем позицию
        ResetEncoderPosition(Port);

        // Получаем начальную позицию
        double startPos = GetMotorPosition(Port);
        AddLog("Start position: %.3f", startPos);

        // Запускаем опрос энкодера для активации обновлений
        StartEncoderPolling(Port);

        // Ждем немного чтобы получить начальные данные
        std::this_thread::sleep_for(100ms);

        // Получаем позицию после активации опроса
        startPos = GetMotorPosition(Port);
        AddLog("Position after polling start: %.3f", startPos);

        // Запускаем мотор на короткое время
        SetMotorSpeed(Port, 40);
        AddLog("Rotating motor at speed 40...");

        // Ждем и измеряем изменение позиции
        auto startTime = std::chrono::steady_clock::now();
        double maxPosition = startPos;
        int measurements = 0;

        while (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count() < 300) {

            double currentPos = GetMotorPosition(Port);
            if (currentPos > maxPosition) {
                maxPosition = currentPos;
            }

            // Логируем прогресс
            if (measurements % 5 == 0) { // Каждые 5 измерений
                AddLog("Test loop %d: position=%.3f", measurements, currentPos);
            }

            measurements++;
            std::this_thread::sleep_for(50ms);
        }

        // Останавливаем мотор
        SetMotorSpeed(Port, 0);
        AddLog("Setting motor speed: Port=0x%02X, Speed=0", Port);

        // Даем мотору остановиться
        std::this_thread::sleep_for(100ms);

        // Финальное измерение
        double finalPos = GetMotorPosition(Port);
        double positionChange = finalPos - startPos;

        AddLog("Position after 300ms: %.3f (change: %.3f), measurements: %d",
            finalPos, positionChange, measurements);

        bool success = (positionChange > 0.05); // Минимальное ожидаемое изменение
        AddLog(success ? "SUCCESS: Encoder working! Position changed from %.3f to %.3f" :
            "FAILED: Encoder not responding to motor movement",
            startPos, finalPos);

        // Останавливаем опрос
        StopEncoderPolling(Port);

        if (!success) {
            AddLog("Final position: %.3f", finalPos);
            AddLog("Last received encoder data analysis:");
            AddLog("  - Check if 0x45 notifications are being received");
            AddLog("  - Check if position bytes are changing in 0x45 messages");
        }

        return success;
    }

    void StopEncoderPolling(uint8_t Port)
    {
        if (MotorThreads.count(Port)) {
            // Устанавливаем флаги остановки
            if (MotorStates.count(Port)) {
                MotorStates[Port].ProfileActive.store(false, std::memory_order_relaxed);
            }

            // Ждем завершения потока с таймаутом
            if (MotorThreads[Port].joinable()) {
                auto& thread = MotorThreads[Port];
                if (thread.get_id() != std::this_thread::get_id()) {
                    // Даем потоку 500ms на завершение
                    for (int i = 0; i < 50 && thread.joinable(); i++) {
                        std::this_thread::sleep_for(10ms);
                    }
                    if (thread.joinable()) {
                        thread.detach(); // Принудительное отсоединение в крайнем случае
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
        if (!IsValid || !peripheral.is_connected()) return;

        AddLog("Activating encoder mode for port 0x%02X", Port);

        // Основная команда активации энкодера
        std::vector<uint8_t> setupCmd = {
            0x09,       // Длина
            0x00,       // Hub ID  
            0x41,       // Port Configuration Command
            Port,       // Моторный порт
            0x00,       // Mode: Position (абсолютная позиция)
            0x00,       // Data Format
            0x01,       // Unit: градусы (ВАЖНО!)
            0x00,       // Range min
            0x00        // Range max
        };

        SendCommandVector(setupCmd);

        // Команда для включения уведомлений
        std::vector<uint8_t> subscribeCmd = {
            0x08,       // Длина
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Порт  
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

    bool Printer_SubscribeToEncoderEvents(IPrinter* Self, const EncoderEvent* Events, int Count)
    {
        if (!Self || !Self->VirtualTable || !Events || Count <= 0)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->SubscribeToEncoderEvents(Events, Count);
    }

    bool Printer_UnSubscribeFromEncoderEvents(IPrinter* Self, uint8_t Port)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->UnsubscribeFromEncoderEvents(Port);
    }

    bool Printer_WaitForEncoderEvent(IPrinter* Self, uint8_t Port, EncoderEventType EventType,
        double TatgetPosition, double Tolerance, int TimeoutMs)
    {
        if (!Self || !Self->VirtualTable)
        {
            return false;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        return Implementation->WaitForEncoderEvent(Port, EventType, TatgetPosition, Tolerance, TimeoutMs);
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
    Printer_SubscribeToEncoderEvents,
    Printer_UnSubscribeFromEncoderEvents,
    Printer_WaitForEncoderEvent,
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
    Profile.TimeoutMs = 30000; // Уменьшенный таймаут

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

    PRINTER_DRIVER_API bool PrinterSubscribeToEncoderEvents(IPrinter* Printer, const EncoderEvent* Events, int Count)
    {
        if (!Events || !Printer || !Printer->VirtualTable || !Printer->VirtualTable->SubscribeToEncoderEvents)
        {
            return false;
        }

        return Printer->VirtualTable->SubscribeToEncoderEvents(Printer, Events, Count);
    }

    PRINTER_DRIVER_API bool PrinterUnsubscribeFromEncoderEvents(IPrinter* Printer, unsigned char Port)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->UnsubscribeFromEncoderEvents)
        {
            return false;
        }

        return Printer->VirtualTable->UnsubscribeFromEncoderEvents(Printer, Port);
    }

    PRINTER_DRIVER_API bool PrinterWaitForEncoderEvent(IPrinter* Printer, unsigned char Port, EncoderEventType EventType, double TargetPosition, double Tolerance, int TimeoutMs)
    {
        if (!EventType || !Printer || !Printer->VirtualTable || !Printer->VirtualTable->WaitForEncoderEvent)
        {
            return false;
        }

        return Printer->VirtualTable->WaitForEncoderEvent(Printer, Port, EventType, TargetPosition, Tolerance, TimeoutMs);
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
