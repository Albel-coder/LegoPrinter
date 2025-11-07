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
    std::atomic<bool> WasConnected { false };

    struct MotorState
    {
        std::atomic<double> CurrentPosition{0.0}; // В оборотах
        std::atomic<double> CurrentSpeed{0.0}; // В оборотах / секунду
        std::atomic<bool> IsMoving{false};
        std::queue<MotorCommandExe> CommandQueue;
        std::mutex QueueMutex;
        std::atomic<bool> Processing{false};
    };

    std::map<uint8_t, MotorState> MotorStates;
    std::map<uint8_t, std::thread> MotorThreads;
    std::mutex GlobalMutex;

    // Система событий энкодера
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
    };

    std::map<uint8_t, CommandExecution> CommandStatus;

public:

    PrinterImplementation() :
        OperationInProgress(false),
        StopRequested(false),
        IsValid(true),
        Status(0)
    {
        // Init motors status
        for (uint8_t Port = 0x00; Port <= 0x3F; Port++)
        {
            CommandStatus[Port].Completed = true;
            CommandStatus[Port].Waiting = false;
        }

        AddLog("PrinterImplementation created");
    }

    ~PrinterImplementation()
    {
        IsValid = false;
        StopRequested = true;

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
        if (!IsValid || StopRequested || Data.size() < 5 || Data[2] != 0x82)
        {
            return;
        }

        uint8_t Port = Data[3];
        uint8_t Feedback = Data[4];

        // Command ends
        if (Feedback == 0x0A)
        {
            std::lock_guard<std::mutex> lock(CompletionMutex);

            if (CommandStatus[Port].Waiting && !CommandStatus[Port].Completed)
            {
                CommandStatus[Port].Completed = true;
                CompletionCV.notify_all();
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

    // Обработчик уведомлений от энкодера
    void HandleEncoderNotification(const std::vector<uint8_t>& Data)
    {
        if (!IsValid)
        {
            AddLog("HandleEncoderNotification: Printer implementation is not valid");
            return;
        }
        if (Data.size() < 8 || Data[2] != 0x04)
        {
            AddLog("HandleEncoderNotification: invalid data params");
            return;
        }

        uint8_t Port = Data[3];
        int32_t PositionRaw =
            (Data[4] & 0xFF) |
            ((Data[5] & 0xFF) << 8) |
            ((Data[6] & 0xFF) << 16) |
            ((Data[7] & 0xFF) << 24);

        double PositionRevolutions = static_cast<double>(PositionRaw) / 360.0;

        // Обновляем состояние мотора
        if (MotorStates.count(Port))
        {
            MotorStates[Port].CurrentPosition = PositionRevolutions;
        }

        // Проверяем события для этого порта
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
                if (std::abs(CurrentPosition - Item->TargetPosition) <= Item->Tolerance)
                {
                    Triggered = true;
                }
                break;
            case ENCODER_SEGMENT_COMPLETED:
                if (std::abs(CurrentPosition - Item->TargetPosition) <= Item->Tolerance)
                {
                    Triggered = true;
                }
                break;
            case ENCODER_MOVEMENT_FINISHED:
                if (std::abs(CurrentPosition - Item->TargetPosition) <= Item->Tolerance)
                {
                    Triggered = true;
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
            Payload = { 0x06, 0x00, 0x81, Port, 0x09 };
            break;
        case CONST_SPEED:
            Payload =
            {
                0x09, 0x00, 0x81, Port, 0x07,
                static_cast<uint8_t>(Command.Speed),
                0x64, 0x00
            };
            break;
        case POSITION:
            int32_t Degrees = static_cast<int32_t>(std::round(Command.TargetRevolutions * 360.0));
            Payload =
            {
                0x0F, 0x00, 0x81, Port, 0x11, 0x0B,
                static_cast<uint8_t>(Degrees & 0xFF),
                static_cast<uint8_t>((Degrees >> 8) & 0xFF),
                static_cast<uint8_t>((Degrees >> 16) & 0xFF),
                static_cast<uint8_t>((Degrees >> 24) & 0xFF),
                static_cast<uint8_t>(Command.Speed),
                100, 0x01, 0x00
            };
            break;
        case PROFILE:
            ExecuteSpeedProfile(Port, Command);
            return;
        }

        SendCommandVector(Payload);
    }

    void ExecuteSpeedProfile(uint8_t Port, const MotorCommandExe Command)
    {
        const int SEGMENTS = std::max(10, static_cast<int>(Command.Profile.Distance * 10));
        double SegmentDistance = Command.Profile.Distance / SEGMENTS;

        double Progress;
        for (int i = 0; i < SEGMENTS + 1 && !StopRequested; i++)
        {
            Progress = static_cast<double>(i) / SEGMENTS;

            // Плавное изменение скорости
            signed char CurrentSpeed = static_cast<signed char>(
                Command.Profile.StartSpeed + (Command.Profile.EndSpeed - Command.Profile.StartSpeed) * Progress
                );

            // Отправляем команду скорости
            std::vector<uint8_t> Payload = 
            {
                0x09, 0x00, 0x81, Port, 0x07,
                static_cast<uint8_t>(CurrentSpeed),
                0x64, 0x00
            };

            SendCommandVector(Payload);

            // Ждем завершения сегмента по энкодеру
            if (i < SEGMENTS && EncoderEvents.count(Port))
            {
                double TargetPosition = MotorStates[Port].CurrentPosition + SegmentDistance;
                WaitForEncoderEventInternal(Port, ENCODER_POSITION_REACHED, TargetPosition, 0.01, 10000);
            }
        }
    }

    // Внутренняя функция ожидания события энкодера
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

        // Удаляем временное событие
        auto Item = std::find(EventState.ActiveEvents.begin(),
            EventState.ActiveEvents.end(), TempEvent);

        if (Item != EventState.ActiveEvents.end())
        {
            EventState.ActiveEvents.erase(Item);
        }

        EventState.EventTriggered = false;

        return Success;
    }

    // Поток непрерывного выполнения команд для каждого мотора
    void MotorCommandProcessor(uint8_t Port)
    {
        MotorState& State = MotorStates[Port];

        while (!StopRequested && IsValid)
        {
            std::unique_lock<std::mutex> Lock(State.QueueMutex);

            if (State.CommandQueue.empty())
            {
                Lock.unlock();
                std::this_thread::sleep_for(1ms);
                continue;
            }

            MotorCommandExe Command = State.CommandQueue.front();
            State.CommandQueue.pop();
            Lock.unlock();

            State.IsMoving = true;
            State.Processing = true;

            // Выполняем команду
            SendMotorCommand(Port, Command);

            // Обновляем состояние
            if (Command.Mode == POSITION)
            {
                State.CurrentPosition = Command.TargetRevolutions;
            }

            State.Processing = false;

            // Если очередь пуста, отмечаем, что мотор остановился
            Lock.lock();
            if (State.CommandQueue.empty())
            {
                State.IsMoving = false;
            }
            Lock.unlock();
        }
    }

    // Настройка уведомлений для энкодера
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

    // Новый метод: запуск потока команд
    void StartCommandStream(const CommandStream* Stream)
    {
        if (!IsValid || !Stream || Stream->Count < 1)
        {
            return;
        }

        for (int i = 0; i < Stream->Count; i++)
        {
            const MotorCommandExe& Command = Stream->Commands[i];
            MotorState& State = MotorStates[Stream->Port];

            std::lock_guard<std::mutex> Lock(State.QueueMutex);
            State.CommandQueue.push(Command);
        }

        AddLog("Started command stream with " + std::to_string(Stream->Count) + " commands on port " +
        std::to_string(Stream->Port));
    }

    // Новый метод: обновление потока команд
    void UpdateCommandStream(const CommandStream* Stream)
    {
        if (!IsValid || !Stream)
        {
            return;
        }

        // Очищаем очереди для указанных портов
        for (int i = 0; i < Stream->Count; i++)
        {
            uint8_t Port = Stream->Port;
            MotorState& State = MotorStates[Port];

            std::lock_guard<std::mutex> Lock(State.QueueMutex);

            // Очищаем очередь и добавляем новые команды
            std::queue<MotorCommandExe> Empty;
            std::swap(State.CommandQueue, Empty);

            // Добавляем все команды из потока
            for (int j = 0; j < Stream->Count; j++)
            {
                State.CommandQueue.push(Stream->Commands[j]);
            }
        }

        AddLog("Updated command stream for port " + std::to_string(Stream->Port));
    }

    void StopCommandStream()
    {
        for (auto& [Port, State] : MotorStates)
        {
            std::lock_guard<std::mutex> Lock(State.QueueMutex);
            std::queue<MotorCommandExe> Empty;
            std::swap(State.CommandQueue, Empty);
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
        {
            for (int i = 0; i < Count; i++)
            {
                CommandStatus[Commands[i].Port].Completed = false;
                CommandStatus[Commands[i].Port].Waiting = true;
            }
        }

        // Send all commands
        for (int i = 0; i < Count; i++)
        {
            SendSingleMotorCommand(Commands[i]);
        }

        WaitForCommandsCompletion(Commands, Count);
        AddLog("RotateMotor completed");
    }

    void SendRawCommand(const unsigned char* Command, int Length)
    {
        if (!IsValid || !Command || Length < 1)
        {
            std::vector<uint8_t> command(Command, Command + Length);
            SendMotorCommand(command);
        }
    }

    void RotateMotorExe(const MotorCommandExe* Commands, int Count)
    {
        if (!IsValid || Count < 1 || !Commands)
        {
            return;
        }

        // Берем временный поток команд
        CommandStream Stream;
        Stream.Commands = const_cast<MotorCommandExe*>(Commands);
        Stream.Count = Count;

        StartCommandStream(&Stream);
    }

    // Система событий энкодера
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

    // Мониторинг
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

    void Printer_RotateMotor(IPrinter* Self, const MotorCommand* Commands, int Count)
    {
        if (!Self || !Self->VirtualTable || !Commands || Count <= 0)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->RotateMotor(Commands, Count);
    }

    void Printer_RotateMotorExe(IPrinter* Self, const MotorCommandExe* Command, int Count)
    {
        if (!Self || !Self->VirtualTable || !Command || Count < 1)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->RotateMotorExe(Command, Count);
    }

    void Printer_SendRawCommand(IPrinter* Self, const unsigned char* Command, int Length)
    {
        if (!Self || !Self->VirtualTable || !Command || Length <= 0)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->SendRawCommand(Command, Length);
    }

    bool Printer_SubscribeToEncoderEvents(IPrinter* Self, const EncoderEvent* Events, int Count)
    {
        if (!Self || !Self->VirtualTable || !Events || Count <= 0)
        {
            return;
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
    Printer_StartCommandStream,
    Printer_UpdateCommandStream,
    Printer_StopCommandStream,
    Printer_RotateMotorExe,
    Printer_SendRawCommand,
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

    PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* Printer)
    {
        if (Printer)
        {
            Printer->VirtualTable->PrinterConnectionInfo(Printer);
        }
    }
}