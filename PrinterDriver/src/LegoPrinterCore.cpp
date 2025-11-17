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
        bool HasFirstNotification = false;
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
        if (!IsValid || Data.empty()) {
            return;
        }

        // Логируем ВСЕ уведомления для отладки
        std::string hexData = "RAW NOTIFICATION: ";
        for (size_t i = 0; i < Data.size() && i < 12; i++) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X", Data[i]);
            hexData += hex;
        }
        AddLog(hexData.c_str());

        // Обрабатываем данные энкодера (тип 0x04)
        if (Data.size() >= 8 && Data[2] == 0x04) {
            uint8_t Port = Data[3];

            // Декодируем позицию - исправленный код
            int32_t PositionRaw =
                (static_cast<int32_t>(Data[4]) << 0) |
                (static_cast<int32_t>(Data[5]) << 8) |
                (static_cast<int32_t>(Data[6]) << 16) |
                (static_cast<int32_t>(Data[7]) << 24);

            // Преобразуем в знаковое число (дополнение до двух)
            if (PositionRaw & 0x80000000) {
                PositionRaw |= 0xFFFFFFFF00000000;
            }

            double PositionRevolutions = static_cast<double>(PositionRaw) / 360.0;

            AddLog("ENCODER UPDATE FIXED: Port=0x%02X, Raw=%d, Rev=%.3f",
                Port, PositionRaw, PositionRevolutions);

            // Обновляем состояние мотора
            if (MotorStates.count(Port)) {
                MotorStates[Port].CurrentPosition.store(PositionRevolutions);

                // Логируем значимые изменения
                static std::map<uint8_t, double> lastLogged;
                if (!lastLogged.count(Port) ||
                    std::abs(PositionRevolutions - lastLogged[Port]) > 0.01) {
                    AddLog("POSITION CHANGED: Port=0x%02X, Position=%.3f",
                        Port, PositionRevolutions);
                    lastLogged[Port] = PositionRevolutions;
                }
            }

            // Проверка событий энкодера
            CheckEncoderEvents(Port, PositionRevolutions);
        }
        // Обрабатываем фидбэк команд (0x82)
        else if (Data.size() >= 5 && Data[2] == 0x82) {
            uint8_t Port = Data[3];
            uint8_t Feedback = Data[4];
            AddLog("COMMAND FEEDBACK: Port=0x%02X, Feedback=0x%02X", Port, Feedback);

            // Command completion
            if (Feedback == 0x0A) {
                std::lock_guard<std::mutex> lock(CompletionMutex);
                if (CommandStatus[Port].Waiting && !CommandStatus[Port].Completed) {
                    CommandStatus[Port].Completed = true;
                    CompletionCV.notify_all();
                    AddLog("Command completed for port 0x%02X", Port);
                }
            }
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

        // Декодируем позицию
        int32_t PositionRaw =
            (static_cast<int32_t>(Data[4]) & 0xFF) |
            ((static_cast<int32_t>(Data[5]) & 0xFF) << 8) |
            ((static_cast<int32_t>(Data[6]) & 0xFF) << 16) |
            ((static_cast<int32_t>(Data[7]) & 0xFF) << 24);

        double PositionRevolutions = static_cast<double>(PositionRaw) / 360.0;

        AddLog("Encoder raw: Port=0x%02X, Data=%02X%02X%02X%02X, Raw=%d, Rev=%.3f",
            Port, Data[4], Data[5], Data[6], Data[7],
            PositionRaw, PositionRevolutions);

        // Обновляем состояние мотора
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

        // Логируем значимые изменения (реже чтобы не засорять логи)
        static std::map<uint8_t, double> lastLoggedPositions;
        if (!lastLoggedPositions.count(Port) ||
            std::abs(PositionRevolutions - lastLoggedPositions[Port]) > 0.05) 
        {
            AddLog("Encoder: Port=0x%02X, Position=%.3f", Port, PositionRevolutions);
            lastLoggedPositions[Port] = PositionRevolutions;
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

        AddLog("Setting motor speed: Port=0x%02X, Speed=%d", Port, Speed);

        // First command: Activate mode
        std::vector<uint8_t> SetupCommand = {
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
        std::vector<uint8_t> MotorCommand = {
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
        if (!IsValid || !Profile || Profile->Count < 1) {
            AddLog("Error in ExecuteSpeedProfile: invalid parameters");
            return false;
        }

        uint8_t Port = Profile->Port;

        // Используем непрерывный контроль скорости
        bool started = StartSpeedProfileFromCurrentPosition(Profile);

        if (!started) {
            return false;
        }

        // Ждем завершения профиля
        auto startTime = std::chrono::steady_clock::now();

        while (IsValid && !StopRequested) {
            // Проверяем завершение
            {
                std::lock_guard<std::mutex> lock(SpeedControlMutex);
                if (!SpeedControlStates.count(Port) || !SpeedControlStates[Port].Active) {
                    break;
                }
            }

            // Проверяем таймаут
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);

            if (elapsed.count() > Profile->TimeoutMs) {
                AddLog("Speed profile timeout in ExecuteSpeedProfile");
                StopContinuousSpeedControl(Port);
                return false;
            }

            std::this_thread::sleep_for(100ms);
        }

        // Проверяем успешное завершение
        bool success = false;
        {
            std::lock_guard<std::mutex> lock(MotorStatesMutex);
            double OldPosition = MotorStates[Port].CurrentPosition;
            if (SpeedControlStates.count(Port)) 
            {
                success = SpeedControlStates[Port].CurrentPointIndex >= Profile->Count;
            }
        }

        AddLog("Speed profile %s", success ? "completed successfully" : "failed");
        return success;
    }

private:

    void ResetEncoderPosition(uint8_t Port)
    {
        AddLog("=== Resetting encoder position for port 0x%02X ===", Port);
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

        std::this_thread::sleep_for(500ms);

        // Сброс в внутреннем состоянии
        if (MotorStates.count(Port))
        {
            MotorStates[Port].CurrentPosition = 0.0;
            AddLog("Encoder position set to 0.0 internally for port 0x%02X", Port);
        }

        std::this_thread::sleep_for(300ms);
        AddLog("=== Encoder reset complete ===");
    }

    void ActivateEncoder(uint8_t Port)
    {
        AddLog("=== Activate encoder for port 0x%02X ===", Port);

        // Стандартная команда активации энкодера для LEGO Technic Hub
        std::vector<uint8_t> ActivateCommand = {
            0x0A,       // Длина пакета
            0x00,       // Hub ID  
            0x41,       // Port configuration command
            Port,       // Порт
            0x00,       // Mode: position (0x00 для абсолютной позиции)
            0x00,       // Delta interval
            0x01,       // Unit: градусы
            0x00,       // Notifications enabled
            0x00,       // Padding
            0x00        // Padding
        };

        SendCommandVector(ActivateCommand);
        AddLog("Encoder activated for port 0x%02X", Port);

        // Дополнительная команда для включения обновлений
        std::vector<uint8_t> SubscribeCommand = {
            0x08,       // Длина пакета
            0x00,       // Hub ID
            0x47,       // Hub Attached IO
            Port,       // Порт  
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

    // New try

public:

    bool StartSpeedProfileFromCurrentPosition(const SpeedProfile* Profile)
    {
        if (!IsValid || !Profile || Profile->Count < 1) {
            AddLog("Invalid profile for speed control");
            return false;
        }

        uint8_t Port = Profile->Port;

        AddLog("=== STARTING SPEED PROFILE FROM CURRENT POSITION ===");

        // АКТИВИРУЕМ ЭНКОДЕР ПЕРЕД НАЧАЛОМ
        ActivateEncoder(Port);
        ResetEncoderPosition(Port);

        // Ждем немного для инициализации
        std::this_thread::sleep_for(200ms);

        // Запоминаем начальную позицию
        double startPosition = MotorStates[Port].CurrentPosition;
        AddLog("Starting from position: %.3f", startPosition);

        // Пересчитываем целевые позиции относительно текущей позиции
        std::vector<SpeedProfilePoint> relativePoints;
        for (int i = 0; i < Profile->Count; i++) {
            SpeedProfilePoint point = Profile->Points[i];
            point.Position = startPosition + point.Position; // Делаем абсолютными относительно старта
            relativePoints.push_back(point);
            AddLog("Target %d: absolute position=%.3f, speed=%d",
                i, point.Position, point.Speed);
        }

        // Запускаем контроль скорости
        return StartSpeedControlInternal(Port, relativePoints, Profile->TimeoutMs);
    }

    bool StartSpeedControlInternal(uint8_t Port,
        const std::vector<SpeedProfilePoint>& Points, int TimeoutMs)
    {
        StopContinuousSpeedControl(Port);

        std::lock_guard<std::mutex> lock(SpeedControlMutex);
        auto& State = SpeedControlStates[Port];

        State.Active = true;
        State.CurrentPointIndex = 0;
        State.ProfilePoints = Points;

        // Запускаем поток контроля
        State.ControlThread = std::thread([this, Port, TimeoutMs]() {
            this->PreciseSpeedControlProcessor(Port, TimeoutMs);
            });

        AddLog("Precise speed control started for port 0x%02X", Port);
        return true;
    }

    void StopContinuousSpeedControl(uint8_t Port)
    {
        std::lock_guard<std::mutex> lock(SpeedControlMutex);

        if (SpeedControlStates.count(Port)) 
        {
            auto& State = SpeedControlStates[Port];
            State.Active = false;

            if (State.ControlThread.joinable()) 
            {
                State.ControlThread.join();
            }

            // Останавливаем мотор
            SetMotorSpeed(Port, 0);

            AddLog("Continuous speed control stopped for port 0x%02X", Port);
        }
    }

    bool TestEncoderFunctionality(IPrinter* Printer)
    {
        if (!Printer) return false;

        AddLog("=== Comprehensive encoder diagnostics ===");

        // Проверяем тип мотора
        CheckMotorType(0x00);
        std::this_thread::sleep_for(1000ms);

        // Проверяем физическое подключение
        TestPhysicalConnection(0x00);
        std::this_thread::sleep_for(1000ms);

        // Тестируем все порты
        TestAllMotorPorts();
        std::this_thread::sleep_for(1000ms);

        AddLog("=== Final recommendation ===");
        AddLog("If encoder don`t work, use positioning commands instead");
    }

private:
    
    void CheckMotorType(uint8_t Port)
    {
        AddLog("=== Checking motor type for port 0x%02X ===", Port);

        // Команда для запроса информации об устройстве
        std::vector<uint8_t> InfoCommand = {
            0x05,
            0x00,
            0x21,
            Port,
            0x01
        };

        SendCommandVector(InfoCommand);
        std::this_thread::sleep_for(500ms);

        // Дополнительная команда для запроса типа устройства
        std::vector<uint8_t> TypeCommand = {
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

        // 1. Сначала деактивируем порт полностью
        std::vector<uint8_t> deactivateCmd = {
            0x05,       // Длина
            0x00,       // Hub ID
            0x41,       // Port Configuration
            Port,       // Порт  
            0x00        // Deactivate
        };
        SendCommandVector(deactivateCmd);
        std::this_thread::sleep_for(200ms);

        // 2. Активируем порт в режиме энкодера с разными настройками
        std::vector<std::vector<uint8_t>> activationAttempts = {
            // Попытка 1: Стандартная активация
            {0x09, 0x00, 0x41, Port, 0x00, 0x00, 0x01, 0x01, 0x00},
            // Попытка 2: Альтернативные настройки
            {0x09, 0x00, 0x41, Port, 0x00, 0x01, 0x01, 0x01, 0x00},
            // Попытка 3: Другие параметры
            {0x09, 0x00, 0x41, Port, 0x00, 0x00, 0x02, 0x01, 0x00}
        };

        for (size_t i = 0; i < activationAttempts.size(); i++) {
            AddLog("Activation attempt %zu", i + 1);
            SendCommandVector(activationAttempts[i]);
            std::this_thread::sleep_for(300ms);

            // Проверяем, появились ли обновления
            double pos = MotorStates[Port].CurrentPosition.load();
            if (pos != 0.0) {
                AddLog("SUCCESS: Position updated to %.3f", pos);
                return true;
            }
        }

        return false;
    }

    void TestPhysicalConnection(uint8_t Port)
    {
        AddLog("=== PHYSICAL CONNECTION TEST ===");

        // Тест 1: Проверяем, вращается ли мотор вообще
        AddLog("1. Testing motor rotation without encoder");
        SetMotorSpeed(Port, 50);
        std::this_thread::sleep_for(2000ms);

        // Останавливаем и слушаем звук/наблюдаем вращение
        SetMotorSpeed(Port, 0);
        std::this_thread::sleep_for(1000ms);

        // Тест 2: Проверяем разные скорости
        AddLog("2. Testing different speeds");
        for (int speed = 30; speed <= 80; speed += 20) {
            AddLog("   Speed %d", speed);
            SetMotorSpeed(Port, speed);
            std::this_thread::sleep_for(1000ms);

            double pos = MotorStates[Port].CurrentPosition.load();
            AddLog("   Position: %.3f", pos);

            if (pos != 0.0) {
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

        std::vector<uint8_t> motorPorts = { 0x00, 0x01, 0x02, 0x03 };

        for (uint8_t port : motorPorts) {
            AddLog("--- Testing port 0x%02X ---", port);

            // Активируем энкодер
            ActivateEncoderAlternative(port);
            std::this_thread::sleep_for(500ms);

            // Сбрасываем позицию
            std::vector<uint8_t> resetCmd = { 0x08, 0x00, 0x81, port, 0x51, 0x02, 0x00, 0x00 };
            SendCommandVector(resetCmd);
            std::this_thread::sleep_for(500ms);

            // Вращаем и проверяем
            double initialPos = MotorStates[port].CurrentPosition.load();
            AddLog("Initial position: %.3f", initialPos);

            SetMotorSpeed(port, 40);
            std::this_thread::sleep_for(2000ms);
            SetMotorSpeed(port, 0);

            double finalPos = MotorStates[port].CurrentPosition.load();
            AddLog("Final position: %.3f", finalPos);

            bool encoderWorking = (std::abs(finalPos - initialPos) > 0.1);
            AddLog("Encoder on port 0x%02X: %s", port, encoderWorking ? "WORKING" : "NOT WORKING");

            if (encoderWorking) {
                AddLog("*** FOUND WORKING ENCODER ON PORT 0x%02X ***", port);
                return;
            }

            std::this_thread::sleep_for(1000ms);
        }

        AddLog("*** NO WORKING ENCODERS FOUND ON ANY PORT ***");
    }

    void SpeedControlProcessor(uint8_t Port, const SpeedProfile* Profile)
    {
        AddLog("Speed control processor started for port 0x%02X", Port);

        // Получаем начальную позицию ИЗ MOTORSTATES
        double startPosition = MotorStates[Port].CurrentPosition;
        AddLog("Starting from position: %.3f", startPosition);

        size_t currentPointIndex = 0;
        auto& State = SpeedControlStates[Port];

        // Устанавливаем начальную скорость
        if (Profile->Count > 0) {
            const SpeedProfilePoint& firstPoint = Profile->Points[0];
            SetMotorSpeed(Port, firstPoint.Speed);
            AddLog("Initial speed set to %d for target position %.3f",
                firstPoint.Speed, firstPoint.Position);
        }

        auto startTime = std::chrono::steady_clock::now();

        while (State.Active && IsValid && !StopRequested) {
            // Получаем текущую позицию ИЗ MOTORSTATES
            double currentPosition = MotorStates[Port].CurrentPosition;

            // Проверяем достижение текущей целевой точки
            if (currentPointIndex < Profile->Count) {
                const SpeedProfilePoint& targetPoint = Profile->Points[currentPointIndex];
                double distanceToTarget = std::abs(currentPosition - targetPoint.Position);

                // Логируем прогресс каждую секунду
                auto currentTime = std::chrono::steady_clock::now();
                static auto lastLogTime = startTime;
                if (std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastLogTime).count() > 1000) {
                    AddLog("Progress: current=%.3f, target=%.3f, distance=%.3f, speed=%d",
                        currentPosition, targetPoint.Position, distanceToTarget,
                        currentPointIndex < Profile->Count ?
                        Profile->Points[currentPointIndex].Speed : 0);
                    lastLogTime = currentTime;
                }

                if (distanceToTarget <= targetPoint.Tolerance) {
                    // Достигли точки - переходим к следующей
                    currentPointIndex++;
                    State.CurrentPointIndex = currentPointIndex;

                    if (currentPointIndex < Profile->Count) {
                        // Устанавливаем новую скорость для следующей точки
                        const SpeedProfilePoint& nextPoint = Profile->Points[currentPointIndex];
                        SetMotorSpeed(Port, nextPoint.Speed);
                        AddLog("Point %zu reached: position=%.3f, new speed=%d",
                            currentPointIndex - 1, currentPosition, nextPoint.Speed);
                    }
                    else {
                        // Все точки пройдены
                        AddLog("All profile points completed");
                        SetMotorSpeed(Port, 0); // Останавливаем мотор
                        State.Active = false;
                        break;
                    }
                }
            }

            // Проверяем общий таймаут
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
            if (elapsed.count() > Profile->TimeoutMs) {
                AddLog("Speed profile timeout after %d ms", elapsed.count());
                SetMotorSpeed(Port, 0);
                State.Active = false;
                break;
            }

            // Небольшая пауза
            std::this_thread::sleep_for(10ms);
        }

        // Освобождаем копию профиля
        delete Profile;

        AddLog("Speed control processor stopped for port 0x%02X", Port);
    }

    void PreciseSpeedControlProcessor(uint8_t Port, int TimeoutMs)
    {
        auto& State = SpeedControlStates[Port];
        AddLog("Precise speed control processor started for port 0x%02X", Port);

        double startPosition = MotorStates[Port].CurrentPosition.load();
        AddLog("Starting position: %.3f", startPosition);

        // Устанавливаем начальную скорость
        if (State.ProfilePoints.size() > 0) {
            SetMotorSpeed(Port, State.ProfilePoints[0].Speed);
            AddLog("Initial speed set to %d", State.ProfilePoints[0].Speed);
        }

        auto startTime = std::chrono::steady_clock::now();
        size_t currentPointIndex = 0;

        while (State.Active && IsValid && !StopRequested) {
            // Получаем текущую позицию
            double currentPosition = MotorStates[Port].CurrentPosition.load();

            // Улучшенное логирование - каждые 10 итераций
            static int logCounter = 0;
            if (++logCounter % 10 == 0) {
                AddLog("Control Loop: current=%.3f, target=%.3f, point=%zu/%zu",
                    currentPosition,
                    currentPointIndex < State.ProfilePoints.size() ?
                    State.ProfilePoints[currentPointIndex].Position : 0.0,
                    currentPointIndex,
                    State.ProfilePoints.size());
            }

            // Проверяем достижение текущей целевой точки
            if (currentPointIndex < State.ProfilePoints.size()) {
                const auto& targetPoint = State.ProfilePoints[currentPointIndex];
                double distanceToTarget = targetPoint.Position - currentPosition;

                // Проверяем, достигли ли мы целевой позиции
                bool positionReached = std::abs(distanceToTarget) <= targetPoint.Tolerance;

                if (positionReached) {
                    // Достигли точки - устанавливаем новую скорость
                    currentPointIndex++;

                    if (currentPointIndex < State.ProfilePoints.size()) {
                        const auto& nextPoint = State.ProfilePoints[currentPointIndex];
                        SetMotorSpeed(Port, nextPoint.Speed);
                        AddLog("=== TARGET REACHED: position=%.3f, new speed=%d ===",
                            currentPosition, nextPoint.Speed);
                    }
                    else {
                        // Все точки пройдены - останавливаемся
                        SetMotorSpeed(Port, 0);
                        AddLog("=== ALL TARGETS COMPLETED ===");
                        State.Active = false;
                        break;
                    }
                }
            }

            // Проверяем таймаут
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
            if (elapsed.count() > TimeoutMs) {
                AddLog("=== CONTROL TIMEOUT ===");
                SetMotorSpeed(Port, 0);
                State.Active = false;
                break;
            }

            std::this_thread::sleep_for(5ms);
        }

        AddLog("Precise speed control processor stopped for port 0x%02X", Port);
    }

    bool TestWorkingPort(uint8_t Port)
    {
        AddLog("=== TESTING WORKING PORT 0x%02X ===", Port);

        // 1. Активируем энкодер
        if (!ActivateEncoderAlternative(Port)) {
            AddLog("Failed to activate encoder");
            return false;
        }

        // 2. Сбрасываем позицию
        std::vector<uint8_t> resetCmd = { 0x08, 0x00, 0x81, Port, 0x51, 0x02, 0x00, 0x00 };
        SendCommandVector(resetCmd);
        std::this_thread::sleep_for(500ms);

        // 3. Запускаем тестовый профиль
        SpeedProfilePoint points[] = {
            {0.5, 30, 0.05},   // 0.5 оборота на скорости 30
            {1.0, 50, 0.05},   // 1.0 оборот на скорости 50  
            {1.5, 30, 0.05},   // 1.5 оборотов на скорости 30
            {2.0, 0, 0.02}     // 2.0 оборота - остановка
        };

        SpeedProfile profile = { Port, points, 4, 30000 };

        AddLog("Starting test profile on working port 0x%02X", Port);
        return ExecuteSpeedProfile(&profile);
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

bool TestMode(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }

    MotorCommand* Command = new MotorCommand[1];
    Command[0].Port = 0x00;
    Command[0].Revolutions = 20;
    Command[0].Speed = 100;

    PrinterRotateMotor(Printer, Command, 1);

    std::this_thread::sleep_for(2s);
    PrinterSetMotorSpeed(Printer, Command[0].Port, 40);

    std::this_thread::sleep_for(4s);
    PrinterSetMotorSpeed(Printer, Command[0].Port, 0);

    delete[] Command;
}

bool EncoderTests(IPrinter* Printer)
{
    if (!Printer)
    {
        return false;
    }
    else
    {
        return Printer_TestEncoderFunctionality(Printer);
    }
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
        else if (Name == "Test")
        {
            return TestMode(Printer);
        }
        else if (Name == "EncoderTests")
        {
            return EncoderTests(Printer);
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
