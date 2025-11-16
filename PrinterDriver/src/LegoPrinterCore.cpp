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
        std::atomic<double> CurrentPosition{0.0}; // In revolutions
        std::atomic<double> CurrentSpeed{0.0}; // In revolutions / second
        std::atomic<bool> IsMoving{false};        
        std::atomic<bool> Processing{false};
        std::atomic<bool> ThreadRunning{false};
        std::queue<MotorCommandExe> CommandQueue;
        std::mutex QueueMutex;
        unsigned char ActivePort{0}; // Current active port
    };

    std::mutex SendCommandMutex;

    std::map<uint8_t, MotorState> MotorStates;
    std::map<uint8_t, std::thread> MotorThreads;

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
    const size_t MAX_LOG_ENTRIES = 1000;

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

    // Start motor thread for a specific port
    void StartMotorThread(uint8_t Port)
    {
        if (MotorThreads.count(Port) && MotorThreads[Port].joinable())
        {
            return; // Thread already running
        }

        MotorStates[Port].ThreadRunning = true;
        MotorThreads[Port] = std::thread(&PrinterImplementation::MotorCommandProcessor, this, Port);
        AddLog("Started motor thread for port 0x%02X", Port);
    }

    // Stop motor thread for a specific port
    void StopMotorThread(uint8_t Port)
    {
        if (MotorStates.count(Port))
        {
            MotorStates[Port].ThreadRunning = false;
        }

        if (MotorThreads.count(Port) && MotorThreads[Port].joinable())
        {
            MotorThreads[Port].join();
            MotorThreads.erase(Port);
            AddLog("Stopped motor thread for port 0x%02X", Port);
        }
    }

    // Set notification handler
    void SetupNotificationHandler()
    {
        try
        {
            peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                [this](const std::vector<uint8_t>& Data)
                {
                    this->HandleHubNotification(Data);
                }
            );
            AddLog("Notification handler setup completed");
        }
        catch (const std::exception& ex)
        {
            AddLog("Error setting up notification handler: %s", ex.what());
        }
    }

    void HandleHubNotification(const std::vector<uint8_t>& Data)
    {
        if (!IsValid || StopRequested || Data.size() < 5)
        {
            return;
        }

        // Handle different notification types
        if (Data[2] == 0x82) // Command feedback
        {
            uint8_t Port = Data[3];
            uint8_t Feedback = Data[4];

            AddLog("Command feedback: Port=0x%02X, Feedback=0x%02X", Port, Feedback);

            // Command completion
            if (Feedback == 0x0A) // Command completed successfully
            {
                std::lock_guard<std::mutex> lock(CompletionMutex);

                auto it = CommandStatus.find(Port);
                if (it != CommandStatus.end() && it->second.Waiting.load() && !it->second.Completed.load())
                {
                    it->second.Completed = true;
                    CompletionCV.notify_all();
                    AddLog("Command completed notification for port 0x%02X", Port);
                }
            }
        }
        else if (Data[2] == 0x04) // Encoder data
        {
            HandleEncoderNotification(Data);
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
        char Buffer[1024];
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
            AddLog("%s", HexCommand.c_str());

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, Command);
            AddLog("Command sent successfully!");
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

        // Правильное декодирование позиции энкодера
        int32_t PositionRaw =
            (static_cast<int32_t>(Data[4]) & 0xFF) |
            ((static_cast<int32_t>(Data[5]) & 0xFF) << 8) |
            ((static_cast<int32_t>(Data[6]) & 0xFF) << 16) |
            ((static_cast<int32_t>(Data[7]) & 0xFF) << 24);

        // Конвертация в обороты (360° = 1 оборот)
        double PositionRevolutions = static_cast<double>(PositionRaw) / 360.0;

        // Обновление состояния мотора
        if (MotorStates.count(Port))
        {
            double oldPos = MotorStates[Port].CurrentPosition;
            MotorStates[Port].CurrentPosition = PositionRevolutions;

            // Логируем ВСЕ обновления энкодера для отладки
            AddLog("ENCODER: Port=0x%02X, Raw=%d, Rev=%.3f (delta=%.3f)",
                Port, PositionRaw, PositionRevolutions, PositionRevolutions - oldPos);
        }

        // Проверка событий энкодера
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

    void SendMotorCommand(uint8_t Port, const MotorCommandExe& Command)
    {
        std::vector<uint8_t> Payload;

        switch (Command.Mode)
        {
        case STOP:
        {
            Payload = 
            { 
                0x06, 
                0x00, 
                0x81, 
                Port, 
                0x09 
            };
            break;
        }
        case CONST_SPEED:
        {
            Payload =
            {
                0x09, 
                0x00, 
                0x81, 
                Port, 
                0x07,
                static_cast<uint8_t>(static_cast<int8_t>(Command.Speed)),
                0x64, 
                0x00
            };

            AddLog("CONST_SPEED command: Port=0x%02X, Speed=%d, Payload: 0%02X 0%02X 0%02X 0%02X 0%02X 0%02X 0%02X 0%02X",
                Port, Command.Speed,
                Payload[0], Payload[1], Payload[2], Payload[3],
                Payload[4], Payload[5], Payload[6], Payload[7]);
            break;
        }
        case POSITION:
        {
            int32_t Degrees = static_cast<int32_t>(std::round(Command.TargetRevolutions * 360.0));
            Payload =
            {
                0x0F, 
                0x00, 
                0x81, 
                Port, 
                0x11, 
                0x0B,
                static_cast<uint8_t>(Degrees & 0xFF),
                static_cast<uint8_t>((Degrees >> 8) & 0xFF),
                static_cast<uint8_t>((Degrees >> 16) & 0xFF),
                static_cast<uint8_t>((Degrees >> 24) & 0xFF),
                static_cast<uint8_t>(static_cast<int8_t>(Command.Speed)),
                100, // Maximum speed
                0x01, // Last state
                0x00 // Use profile
            };
            break;
        }
        case PROFILE:
        {
            ExecuteSpeedProfile(Port, Command);
            return;
        }
        }

        if (!Payload.empty())
        {
            SendCommandVector(Payload);
        }
    }

    void ExecuteSpeedProfile(uint8_t Port, const MotorCommandExe Command)
    {
        const int SEGMENTS = std::max(10, static_cast<int>(Command.Profile.Distance * 10));
        double SegmentDistance = Command.Profile.Distance / SEGMENTS;

        double Progress;
        for (int i = 0; i < SEGMENTS + 1 && !StopRequested; i++)
        {
            Progress = static_cast<double>(i) / SEGMENTS;

            signed char CurrentSpeed = static_cast<signed char>(
                Command.Profile.StartSpeed + (Command.Profile.EndSpeed - Command.Profile.StartSpeed) * Progress
                );

            std::vector<uint8_t> Payload = 
            {
                0x09, 0x00, 0x81, Port, 0x07,
                static_cast<uint8_t>(CurrentSpeed),
                0x64, 0x00
            };

            SendCommandVector(Payload);

            // Wait for segment completion
            if (i < SEGMENTS && EncoderEvents.count(Port))
            {
                double TargetPosition = MotorStates[Port].CurrentPosition + SegmentDistance;
                WaitForEncoderEventInternal(Port, ENCODER_POSITION_REACHED, TargetPosition, 0.01, 10000);
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
            [&]() {
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

    void MotorCommandProcessor(uint8_t Port)
    {
        MotorState& State = MotorStates[Port];

        while (!StopRequested && IsValid)
        {
            std::unique_lock<std::mutex> Lock(State.QueueMutex);

            if (State.CommandQueue.empty())
            {
                Lock.unlock();

                // We notify about the completion of all commands
                if (State.IsMoving)
                {
                    State.IsMoving = false;
                }

                std::this_thread::sleep_for(1ms);
                continue;
            }

            MotorCommandExe Command = State.CommandQueue.front();
            State.CommandQueue.pop();
            Lock.unlock();

            State.IsMoving = true;
            State.Processing = true;

            // Execute command
            SendMotorCommand(Port, Command);

            // Update state
            if (Command.Mode == POSITION && EncoderEvents.count(Port))
            {
                WaitForEncoderEventInternal(Port, ENCODER_POSITION_REACHED,
                    Command.TargetRevolutions, 0, 10000);
            }
            else if (Command.Mode == CONST_SPEED)
            {
                std::this_thread::sleep_for(50ms);
            }

            State.Processing = false;

            if (Command.Mode == POSITION)
            {
                State.CurrentPosition = Command.TargetRevolutions;
            }

            // Check if motor should stop
            Lock.lock();
            if (State.CommandQueue.empty())
            {
                State.IsMoving = false;
            }
            Lock.unlock();
        }

        State.IsMoving = false;
        State.Processing = false;
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

    void StartCommandStream(const CommandStream* Stream)
    {
        if (!IsValid || !Stream || Stream->Count < 1)
        {
            return;
        }

        uint8_t Port = Stream->Port;
        StartMotorThread(Port); // Turn on thread

        //Update commands
        UpdateCommandStream(Stream);

        AddLog("Started command stream with " + std::to_string(Stream->Count) + " commands on port " +
        std::to_string(Stream->Port));
    }

    void UpdateCommandStream(const CommandStream* Stream)
    {
        if (!IsValid || !Stream)
        {
            return;
        }

        uint8_t Port = Stream->Port;
        MotorState& State = MotorStates[Port];
        
        {
            std::lock_guard<std::mutex> Lock(State.QueueMutex);

            // Clear queue and add new commands
            std::queue<MotorCommandExe> Empty;
            std::swap(State.CommandQueue, Empty);

            for (int i = 0; i < Stream->Count; i++)
            {
                State.CommandQueue.push(Stream->Commands[i]);
            }

            AddLog("Updated command stream for port " + std::to_string(Stream->Port));
        }

        // Start the stream if it is not running
        if (!State.ThreadRunning)
        {
            StartMotorThread(Port);
        }

        AddLog("Command stream update for port 0x%02X", Port);
    }

    void StopCommandStream()
    {
        for (auto& [Port, State] : MotorStates)
        {
            std::lock_guard<std::mutex> Lock(State.QueueMutex);
            std::queue<MotorCommandExe> Empty;
            std::swap(State.CommandQueue, Empty);
            State.IsMoving = false;
        }

        AddLog("Stopped all commands streams");
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

    double GetMotorPosition(unsigned char Port)
    {
        if (MotorStates.count(Port))
        {
            return MotorStates[Port].CurrentPosition;
        }

        return 0.0;
    }

    void SetMotorSpeed(uint8_t Port, int8_t Speed)
    {
        if (!IsValid || !peripheral.is_connected())
        {
            return;
        }

        MotorCommandExe Command;
        Command.Mode = CONST_SPEED;
        Command.Speed = Speed;
        SendMotorCommand(Port, Command);
    }

private:

    void SendSingleMotorCommand(const MotorCommand& Command)
    {
        AddLog("Motor command - Port: 0x%02X, Speed: %d, Revolutions: %.2f",
            Command.Port, Command.Speed, Command.Revolutions);

        // Convert revolutions to absolute degrees (1 revolution = 360 degrees)
        int32_t Degrees = static_cast<int32_t>(std::round(Command.Revolutions * 360.0));
        AddLog("Calculated degrees: %d", Degrees);

        std::vector<uint8_t> Payload = {
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

    bool WaitForCommandStreamCompletion(uint8_t Port, int TimeoutMs = 120000)
    {
        if (!MotorStates.count(Port))
        {
            return true; // No active commands
        }

        MotorState& State = MotorStates[Port];

        auto StartTime = std::chrono::steady_clock::now();

        while (!StopRequested && IsValid)
        {
            // Condition 1: The command queue is empty and there is no active processing
            std::unique_lock<std::mutex> Lock(State.QueueMutex);
            bool ConditionsMet = State.CommandQueue.empty() &&
                !State.Processing &&
                !State.IsMoving;
            Lock.unlock();
            if (ConditionsMet)
            {
                // Condition 2: Additional check - the motor has actually stopped
                // Let's wait a bit and check if the position has changed
                double InitialPosition = State.CurrentPosition;
                std::this_thread::sleep_for(50ms);

                if (std::abs(State.CurrentSpeed - InitialPosition) < 0.000001)
                {
                    return true; // Movement completed
                }
            }

            // If the conditions are not met, we use smart waiting.
            if (EncoderEvents.count(Port))
            {
                // Get the target position from the queue (if any)
                double NextTarget = GetNextTargetPosition(Port);
                if (NextTarget > 0)
                {
                    // We are waiting for the next target position
                    WaitForEncoderEventInternal(Port, ENCODER_POSITION_REACHED,
                        NextTarget, 0.02, 100);
                }
                else
                {
                    // If there is no target position, pause briefly.
                    std::this_thread::sleep_for(20ms);
                }
            }
            else
            {
                std::this_thread::sleep_for(20ms);
            }

            // Check timeout
            auto CurrentTime = std::chrono::steady_clock::now();
            auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                CurrentTime - StartTime);

            if (Elapsed.count() > TimeoutMs)
            {
                AddLog("Timeout waiting for command stream completion on port 0x%02X", Port);
                return false;
            }            
        }

        return false;
    }

    bool WaitForCommandCompletion(uint8_t Port, int TimeoutMs = 15000)
    {
        std::unique_lock<std::mutex> lock(CompletionMutex);

        // Убедитесь, что элемент существует в map
        if (CommandStatus.find(Port) == CommandStatus.end())
        {
            return false;
        }

        // Устанавливаем состояние ожидания
        CommandStatus[Port].Completed = false;
        CommandStatus[Port].Waiting = true;

        // Ждем уведомления о завершении
        bool success = CompletionCV.wait_for(lock, std::chrono::milliseconds(TimeoutMs),
            [this, Port]() {
                auto it = CommandStatus.find(Port);
                if (it != CommandStatus.end())
                {
                    return it->second.Completed.load();
                }
                return true; // Если порта нет, считаем завершенным
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

    double GetNextTargetPosition(uint8_t Port)
    {
        if (!MotorStates.count(Port))
        {
            return 0.0;
        }

        MotorState& State = MotorStates[Port];
        std::lock_guard<std::mutex> Lock(State.QueueMutex);

        if (!State.CommandQueue.empty())
        {
            const MotorCommandExe& NextCommand = State.CommandQueue.front();
            if (NextCommand.Mode == POSITION)
            {
                return NextCommand.TargetRevolutions;
            }
        }

        return 0.0;
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
        if (!IsValid || !Profile || Profile->Count < 1)
        {
            AddLog("Error in ExecuteSpeedProfile function");
            return false;
        }

        uint8_t Port = Profile->Port;

        // Сбрасываем позиции перед началом профиля
        ResetEncoderPosition(Port);

        // Starting motor thread if it not on
        if (!MotorStates[Port].ThreadRunning)
        {
            StartMotorThread(Port);
        }

        // Cleaning current queue
        {
            std::lock_guard<std::mutex> Lock(MotorStates[Port].QueueMutex);
            std::queue<MotorCommandExe> Empty;
            std::swap(MotorStates[Port].CommandQueue, Empty);
        }

        MotorCommandExe InitialCommand;
        InitialCommand.Mode = CONST_SPEED;
        InitialCommand.Speed = Profile->Points[0].Speed;

        {
            std::lock_guard<std::mutex> Lock(MotorStates[Port].QueueMutex);
            MotorStates[Port].CommandQueue.push(InitialCommand);
        }

        return ProcessSpeedProfile(Profile);
    }

private:

    bool ProcessSpeedProfile(const SpeedProfile* Profile)
    {
        uint8_t Port = Profile->Port;

        if (!MotorStates.count(Port))
        {
            AddLog("Motor state not found for port 0x%02X", Port);
            return false;
        }

        AddLog("Using ABSOLUTE POSITIONING approach");

        for (int i = 0; i < Profile->Count && IsValid && !StopRequested; i++)
        {
            const SpeedProfilePoint& Point = Profile->Points[i];

            // Используем абсолютное позиционирование как в работающем RotateMotor
            MotorCommand cmd;
            cmd.Port = Port;
            cmd.Speed = Point.Speed;
            cmd.Revolutions = Point.Position;

            AddLog("Moving to position %.3f with speed %d", Point.Position, Point.Speed);

            // Используем старый работающий метод
            SendSingleMotorCommand(cmd);

            // Ждем завершения команды через работающий механизм
            if (!WaitForCommandCompletion(Port, 15000))
            {
                AddLog("Timeout waiting for position %.3f", Point.Position);
                return false;
            }

            AddLog("Successfully reached position %.3f", Point.Position);

            // Небольшая пауза между командами
            std::this_thread::sleep_for(100ms);
        }

        AddLog("Speed profile completed successfully");
        return true;
    }

    void ResetEncoderPosition(uint8_t Port)
    {
        // Команда сброса позиции энкодера для LEGO Hub
        std::vector<uint8_t> ResetCommand = {
            0x08,
            0x00,
            0x81,
            Port,
            0x51,
            0x02,
            0x00,
            0x00,
        };

        SendCommandVector(ResetCommand);
        AddLog("Encoder position reset for port 0x%02X", Port);

        // Сброс в внутреннем состоянии
        if (MotorStates.count(Port))
        {
            MotorStates[Port].CurrentPosition = 0.0;
        }

        std::this_thread::sleep_for(100ms);
    }

    void ActivateEncoder(uint8_t Port)
    {
        // Команда активации энкодера для ЛЕГО хаба
        std::vector<uint8_t> ActivateCommand = {
            0x08,
            0x00,
            0x81,
            Port,
            0x11,
            0x00,
            0x00,
            0x00,
            0x00
        };

        SendCommandVector(ActivateCommand);
        AddLog("Encoder activated for port 0x%02X", Port);

        std::this_thread::sleep_for(200ms);
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

    void Printer_StartCommandStream(IPrinter* Self, const CommandStream* Stream)
    {
        if (!Self || !Self->VirtualTable || !Stream)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->StartCommandStream(Stream);
    }

    void Printer_UpdateCommandStream(IPrinter* Self, const CommandStream* Stream)
    {
        if (!Self || !Self->VirtualTable || !Stream)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->UpdateCommandStream(Stream);
    }

    void Printer_StopCommandStream(IPrinter* Self)
    {
        if (!Self || !Self->VirtualTable)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->StopCommandStream();
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

}

// Virtual Method Table - C-INTERFACE
static IPrinterVirtualTable PrinterVTable = {
    Printer_Connect,
    Printer_Disconnect,
    Printer_IsConnected,
    Printer_Destroy,
    Printer_RotateMotor,
    Printer_SetMotorSpeed,
    Printer_StartCommandStream,
    Printer_UpdateCommandStream,
    Printer_StopCommandStream,
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
    Printer_PrinterConnectionInfo
};

// Tested function - remove after deep testing
bool TestConstantSpeed(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    // Test 1: Forward speed
    PrinterSetMotorSpeed(Printer, 0x00, 50);
    std::this_thread::sleep_for(2s);

    // Test 2: Reverse Speed
    PrinterSetMotorSpeed(Printer, 0x00, -30);
    std::this_thread::sleep_for(2s);

    // Test 3: Stop
    PrinterSetMotorSpeed(Printer, 0x00, 0);
    std::this_thread::sleep_for(1s);

    return true;
}

bool TestSpeedProfileBasic(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    SpeedProfilePoint Points[] = {
        {0.0, 30, 0.1}, // Start at 30%
        {1.0, 60, 0.05}, // Accelerate to 60% at 1 revolution
        {3.0, 30, 0.05}, // Slow down to 30% at 3 revolutions
        {5.0, 0, 0.02} // Stop at 5 revolutions
    };

    SpeedProfile Profile;
    Profile.Port = 0x00;
    Profile.Points = Points;
    Profile.Count = 4;
    Profile.TimeoutMs = 30000;

    bool Result = PrinterExecuteSpeedProfile(Printer, &Profile);
    return Result;
}

bool TestSpeedProfileAdvanced(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    SpeedProfilePoint Points[] = {
        {0.0, 20, 0.1}, // Gentle start
        {0.5, 40, 0.05},
        {1.0, 60, 0.05}, // Medium speed
        {2.0, 80, 0.05}, // High speed
        {4.0, 60, 0.05}, // Slow speed
        {6.0, 40, 0.05},
        {7.0, 20, 0.05},
        {8.0, 0, 0.02}, // Precise stop
    };

    SpeedProfile Profile;
    Profile.Port = 0x00;
    Profile.Points = Points;
    Profile.Count = 8;
    Profile.TimeoutMs = 60000;

    bool Result = PrinterExecuteSpeedProfile(Printer, &Profile);
    return Result;
}

bool TestEncoderEvents(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    std::atomic<int> EventCount{ 0 };

    // Create encoder event callback
    auto EncoderCallback = [](unsigned char Port, EncoderEventType Event,
        double Position, void* UserData)
        {
            std::atomic<int>* Count = static_cast<std::atomic<int>*>(UserData);
            (*Count)++;
        };

    // Subscribe to encoder events
    EncoderEvent events[2];
    events[0].Port = 0x00;
    events[0].Type = ENCODER_POSITION_REACHED;
    events[0].TargetPosition = 1.0;
    events[0].Tolerance = 0.05;
    events[0].Callback = EncoderCallback;
    events[0].UserData = &EventCount;

    events[1].Port = 0x00;
    events[1].Type = ENCODER_POSITION_REACHED;
    events[1].TargetPosition = 2.0;
    events[1].Tolerance = 0.05;
    events[1].Callback = EncoderCallback;
    events[1].UserData = &EventCount;

    PrinterSubscribeToEncoderEvents(Printer, events, 2);

    // Move motor through the positions
    SpeedProfilePoint Points[] = {
        {0.0, 40, 0.1},
        {2.5, 0, 0.02}
    };

    SpeedProfile Profile;
    Profile.Port = 0x00;
    Profile.Points = Points;
    Profile.Count = 2;
    Profile.TimeoutMs = 600000;

    bool Result = PrinterExecuteSpeedProfile(Printer, &Profile);

    // Wait a bit more for events
    std::this_thread::sleep_for(1s);

    // Cleanup
    PrinterUnsubscribeFromEncoderEvents(Printer, Profile.Port);

    return Result && (EventCount >= 2);
}

bool TestMultipleMotors(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    auto StartTime = std::chrono::steady_clock::now();

    // Create a profile that should take about 10 seconds
    SpeedProfilePoint Points[] = {
        {0.0, 50, 0.1},
        {2.0, 70, 0.05},
        {5.0, 30, 0.05},
        {8.0, 0, 0.02},
    };

    SpeedProfile Profile;
    Profile.Port = 0x00;
    Profile.Points = Points;
    Profile.Count = 4;
    Profile.TimeoutMs = 200000;

    bool Result = PrinterExecuteSpeedProfile(Printer, &Profile);

    auto EndTime = std::chrono::steady_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);

    // Verify it took reasonable time (not too fast, not too slow)
    bool TimeReasonable = (Duration.count() > 5000) && (Duration.count() < 25000);
    return Result && TimeReasonable;
}

bool TestErrorConditions(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    // Test two motors simultaneously
    std::atomic<bool> FirstMotorDone{ false };
    std::atomic<bool> SecondMotorDone{ false };

    auto FirstMotorThread = std::thread([&]() {
        SpeedProfilePoint Points[] = {
            {0.0, 40, 0.1},
            {3.0, 0, 0.02},
        };

        SpeedProfile Profile = { 0x00, Points, 2, 20000 };
        FirstMotorDone = PrinterExecuteSpeedProfile(Printer, &Profile);
        });

    auto SecondMotorThread = std::thread([&]() {
        SpeedProfilePoint Points[] = {
            {0.0, 30, 0.1},
            {2.0, 0, 0.02},
        };

        SpeedProfile Profile = { 0x01, Points, 2, 20000 };
        SecondMotorDone = PrinterExecuteSpeedProfile(Printer, &Profile);
        });

    FirstMotorThread.join();
    SecondMotorThread.join();

    return FirstMotorDone && SecondMotorDone;;
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

    // ===========================

    PRINTER_DRIVER_API void PrinterStartCommandStream(IPrinter* Printer, const CommandStream* Stream)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->StartCommandStream || !Stream)
        {
            return;
        }

        return Printer->VirtualTable->StartCommandStream(Printer, Stream);
    }

    PRINTER_DRIVER_API void PrinterUpdateCommandStream(IPrinter* Printer, const CommandStream* Stream)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->UpdateCommandStream || !Stream)
        {
            return;
        }

        return Printer->VirtualTable->UpdateCommandStream(Printer, Stream);
    }

    PRINTER_DRIVER_API void PrinterStopCommandStream(IPrinter* Printer)
    {
        if (!Printer || !Printer->VirtualTable || !Printer->VirtualTable->StopCommandStream)
        {
            return;
        }

        return Printer->VirtualTable->StopCommandStream(Printer);
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

        if (Name == "ConstantSpeed")
        {
            return TestConstantSpeed(Printer);
        }
        else if (Name == "SpeedProfileBasic")
        {
            return TestSpeedProfileBasic(Printer);
        }
        else if (Name == "SpeedProfileAdvanced")
        {
            return TestSpeedProfileAdvanced(Printer);
        }
        else if (Name == "EncoderEvents")
        {
            return TestEncoderEvents(Printer);
        }
        else if (Name == "MultipleMotors")
        {
            return TestMultipleMotors(Printer);
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
