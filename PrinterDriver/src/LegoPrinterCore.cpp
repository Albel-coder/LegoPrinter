#include "LegoPrinterCore.h"
#include <simpleble/SimpleBLE.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>
#include <thread>
#include <algorithm>
#include <functional>
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
        AddLog("PrinterImplementation destroyed");
        IsValid = false;
        StopRequested = true;

        // Wake up all waiting threads
        CompletionCV.notify_all();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Automatic shutdown when variable is destroyed
        if (peripheral.is_connected())
        {
            try
            {
                peripheral.disconnect();
            }
            catch (...)
            {
                // Ignoring errors in the destructor
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
            AddLog(HexCommand.c_str());

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
            return nullptr;
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
        return LastError.c_str();
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
            AddLog("  - SimpleBLE::Adapter::bluetooth_enabled(): " + ble_enabled);

            // Getting a list of adapters
            auto adapters = SimpleBLE::Adapter::get_adapters();
            AddLog("  - Adapters found: " + adapters.size());

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
            AddLog("Adapter used: " + adapter.identifier() + " [" + adapter.address() + "]");

            // Setting up callbacks
            adapter.set_callback_on_scan_start([]() { });

            AddLog("Scanning started...");

            adapter.set_callback_on_scan_stop([]() { });

            AddLog("Scanning stoped");

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

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        std::lock_guard<std::mutex> Lock(ContextsMutex);
        Contexts.erase(Implementation);
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

    void Printer_SendCommand(IPrinter* Self, const unsigned char* Command, int Length)
    {
        if (!Self || !Self->VirtualTable || !Command || Length <= 0)
        {
            return;
        }

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->SendCommand(Command, Length);
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
    Printer_SendCommand,
    Printer_GetLogCount,
    Printer_GetLogEntry,
    Printer_PrinterConnectionInfo,
    Printer_ClearLog,
    Printer_GetLastError
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
        if (Printer && Printer->VirtualTable && Printer->VirtualTable->Destroy)
        {
            Printer->VirtualTable->Destroy(Printer);
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
}