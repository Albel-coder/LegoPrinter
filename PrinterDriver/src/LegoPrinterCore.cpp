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
    std::thread CurrentOperation;
    std::mutex Mutex;

public:

    PrinterImplementation() :
        OperationInProgress(false),
        StopRequested(false),
        IsValid(true),
        Status(0)
    {
    }

    ~PrinterImplementation()
    {
        IsValid = false;
        StopRequested = true;

        // Safely stop the flow
        {
            std::lock_guard<std::mutex> lock(Mutex);
            if (CurrentOperation.joinable())
            {
                if (CurrentOperation.get_id() != std::this_thread::get_id())
                {
                    CurrentOperation.detach();
                }
            }
        }

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
    
    // Internal helper methods

    void SendCommandVector(std::vector<uint8_t> Command)
    {
        if (!IsValid)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(Mutex);

        try
        {

            // Check connection
            if (!peripheral.is_connected())
            {
                std::cout << "Peripheral is not connected!\n";
                return;
            }

            std::cout << "Sending: ";
            for (auto b : Command)
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(b) << " ";
            }
            std::cout << std::dec << "\n";

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, Command);
            std::cout << "Command sent successfully!" << "\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "Error sending command: " << e.what() << "\n";
            return;
        }
    }

public:

    // Basic methods
    bool Connect()
    {
        if (!IsValid)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(Mutex);

        try
        {
            // Checking Bluetooth Status
            std::cout << "Checking Bluetooth status:\n";

            bool ble_enabled = SimpleBLE::Adapter::bluetooth_enabled();
            std::cout << "  - SimpleBLE::Adapter::bluetooth_enabled(): "
                << std::boolalpha << ble_enabled << "\n";

            // Getting a list of adapters
            auto adapters = SimpleBLE::Adapter::get_adapters();
            std::cout << "  - Adapters found: " << adapters.size() << "\n";

            if (adapters.empty())
            {
                std::cout << "\nBluetooth adapters not found! Possible reasons:\n"
                    << "1. The Bluetooth adapter is disabled or not working\n"
                    << "2. Drivers not installed\n"
                    << "3. Hardware problem\n";
                return false;
            }

            // We use the first adapter
            SimpleBLE::Adapter& adapter = adapters[0];
            std::cout << "\nAdapter used: " << adapter.identifier()
                << " [" << adapter.address() << "]\n";

            // Setting up callbacks
            adapter.set_callback_on_scan_start([]() {
                std::cout << "Scanning started...\n";
                });

            std::cout << "Scanning started...\n";

            adapter.set_callback_on_scan_stop([]() {
                });

            std::cout << "Scanning stopped\n";

            adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral)
                {
                    std::string name = peripheral.identifier();
                    std::string address = peripheral.address();
                    int rssi = peripheral.rssi();

                    // Convert the name to uppercase for universality
                    std::transform(name.begin(), name.end(), name.begin(), ::toupper);

                    std::cout << "\nDevice found: " << name << " [" << address << "], RSSI: " << rssi << " dBm";
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
                            std::cout << "[LEGO Manufacturer Data Found]";
                        }
                    }

                    if (isLego)
                    {
                        std::cout << "<<< LEGO HUB DISCOVERED!";
                    }
                });

            // Start scanning
            adapter.scan_start();
            std::cout << "Scanning for 10 seconds...\n";
            std::this_thread::sleep_for(10s);
            adapter.scan_stop();

            // We get a list of found devices
            auto peripherals = adapter.scan_get_results();
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

                    std::cout << "\n>>> LEGO HUB detected:" << ScannedPeripheral.identifier()
                        << " [" << ScannedPeripheral.address() << "], RSSI: " << ScannedPeripheral.rssi() << " dBm";

                    // Connection attempt
                    try 
                    {
                        std::cout << "\nTrying to connect...";
                        ScannedPeripheral.connect();

                        // In the main function, after connection:               
                        if (ScannedPeripheral.is_connected()) 
                        {
                            std::cout << "\nConnecting LEGO Hub!\n";

                            peripheral = std::move(ScannedPeripheral);

                            ScannedPeripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                                [](const std::vector<uint8_t>& data) 
                                {
                                    std::cout << "Notification: ";
                                    for (auto b : data) 
                                    {
                                        std::cout << std::hex << std::setw(2) << std::setfill('0')
                                            << static_cast<int>(b) << " ";
                                    }
                                    std::cout << std::dec << "\n";
                                }
                            );

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
                        }
                    }
                    catch (const std::exception& e) 
                    {
                        std::cout << "\nConnection error: " << e.what();
                    }
                }
            }
            return false;
        }
        catch (const std::exception& e) 
        {
            std::cout << "\n!!! ERROR: " << e.what() << "\n";
            return false;
        }
    }

    bool Disconnect()
    {
        std::lock_guard<std::mutex> contextLock(Mutex);

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

    void RotateMotor(const MotorCommand Commands, int Count)
    {
        if (!IsValid || Count <= 0)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(Mutex);

        // Check connection
        if (!peripheral.is_connected())
        {
            std::cout << "Printer is not connected!\n";
            return;
        }

        OperationInProgress = true;

        std::vector<uint8_t> PortsVector(Commands.Port, Commands.Port + Count);
        std::vector<int8_t> SpeedVector(Commands.Speed, Commands.Speed + Count);
        std::vector<double> RevolutionsVector(Commands.Revolutions, Commands.Revolutions + Count);

        // Convert revolutions to absolute degrees (1 revolution = 360 degrees)
        int32_t Degrees = 0;
        for (double revolutions : RevolutionsVector)
        {
            Degrees = static_cast<int32_t>(std::round(revolutions * 360.0));
        }
        
        // We form a team according to the LEGO Wireless Protocol 3.0
        for (int i = 0; i < PortsVector.size(); i++)
        {
            std::vector<uint8_t> Payload = {
            0x0F,       // Message length (15 bytes)
            0x00,       // Message counter
            0x81,       // Output control command
            PortsVector[i], // Port or combo port
            0x11,
            0x0B,       // Sub-team
            // Rotation angle (4 bytes little-endian)
            static_cast<uint8_t>(Degrees & 0xFF),
            static_cast<uint8_t>((Degrees >> 8) & 0xFF),
            static_cast<uint8_t>((Degrees >> 16) & 0xFF),
            static_cast<uint8_t>((Degrees >> 24) & 0xFF),
            // Speed (1 byte)
            static_cast<uint8_t>(SpeedVector[i]),
            // Maximum power (usually 100%)
            100,
            // Final state (0 = float/coast, 1 = brake/hold)
            0x01,       // Hold the position after completion
            // Use profile (0 = use acceleration profile)
            0x00
            };

            SendCommandVector(Payload);
        }
        
        OperationInProgress = false;
    }

    void SendCommand(const unsigned char* Command, int Length)
    {
        if (!IsValid)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(Mutex);

        try
        {
            std::vector<uint8_t> command(Command, Command + Length);

            // Check connection
            if (!peripheral.is_connected())
            {
                std::cout << "Peripheral is not connected!\n";
                return;
            }

            std::cout << "Sending: ";
            for (auto b : command)
            {
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(b) << " ";
            }
            std::cout << std::dec << "\n";

            // Sending a command via Bluetooth LE
            peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, command);
            std::cout << "Command sent successfully!" << "\n";
        }
        catch (const std::exception& e)
        {
            std::cout << "Error sending command: " << e.what() << "\n";
            return;
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
        if (!Self) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        std::lock_guard<std::mutex> Lock(ContextsMutex);
        Contexts.erase(Implementation);
    }

    void Printer_RotateMotor(IPrinter* Self, const MotorCommand* Commands, int Count)
    {
        if (!Self || !Self->VirtualTable || !Commands || Count <= 0) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->RotateMotor(Commands[0], Count);
    }

    void Printer_SendCommand(IPrinter* Self, const unsigned char* Command, int Length)
    {
        if (!Self || !Self->VirtualTable || !Command || Length <= 0) return;

        PrinterImplementation* Implementation = reinterpret_cast<PrinterImplementation*>(Self);
        Implementation->SendCommand(Command, Length);
    }
}

// Virtual Method Table - C-INTERFACE
static IPrinterVirtualTable PrinterVTable = {
    Printer_Connect,
    Printer_Disconnect,
    Printer_IsConnected,
    Printer_Destroy,      
    Printer_RotateMotor,  
    Printer_SendCommand 
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

        std::cout << "LegoPrinter instance created\n";
        return &PrinterHandle->Interface;
    }

    PRINTER_DRIVER_API void DestroyPrinter(IPrinter* Printer)
    {
        if (Printer && Printer->VirtualTable && Printer->VirtualTable->Destroy)
        {
            Printer->VirtualTable->Destroy(Printer);
            std::cout << "LegoPrinter instance destroyed\n";
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
}