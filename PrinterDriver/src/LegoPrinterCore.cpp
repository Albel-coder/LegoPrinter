#define LEGOPRINTERCORE_EXPORTS
#include "LegoPrinterCore.h"


using namespace std::chrono_literals;

const std::string LEGO_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
const std::string LEGO_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";

void SendCommand(SimpleBLE::Peripheral& peripheral, const std::vector<uint8_t>& command)
{
    try
    {
        std::cout << "Sending: ";
        for (auto b : command) {
            std::cout << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(b) << " ";
        }
        std::cout << std::dec << "\n";

        peripheral.write_command(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID, command);
        std::cout << "Command sent successfully!" << "\n";
    }
    catch (const std::exception& e) {
        std::cout << "Error sending command: " << e.what() << "\n";
        return;
    }
}

void UnlockAndSetLedColor(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b) {
    std::cout << "\n=== Trying to unlock LED control ===\n";

    // 1. Initialization sequence (may be needed for "unlocking")
    std::vector<std::vector<uint8_t>> init_sequence = {
        {0x05, 0x00, 0x02, 0x01, 0x00},  // Probably the initialization command
        {0x06, 0x00, 0x82, 0x0A, 0x00, 0x01},  // System command
        {0x06, 0x00, 0x82, 0x0B, 0x00, 0x01},  // Another system command
        {0x07, 0x00, 0x82, 0x10, 0x00, 0x01, 0x00},  // Configuration command
    };

    for (const auto& cmd : init_sequence) {
        SendCommand(peripheral, cmd);
        std::this_thread::sleep_for(500ms);
    }

    // 2. Activating the LED Port (Trying Different Approaches)
    std::vector<std::vector<uint8_t>> activation_commands = {
        {0x06, 0x00, 0x81, 0x32, 0x01, 0x01},  // Standard activation
        {0x08, 0x00, 0x41, 0x32, 0x02, 0x00, 0x00, 0x00},  // Port configuration
        {0x06, 0x00, 0x81, 0x3A, 0x01, 0x01},  // Alternative port
        {0x08, 0x00, 0x41, 0x3A, 0x02, 0x00, 0x00, 0x00},  // Alternate port configuration
    };

    for (const auto& cmd : activation_commands) {
        SendCommand(peripheral, cmd);
        std::this_thread::sleep_for(500ms);
    }

    // 3.Let's try special "unlocking" commands
    std::vector<std::vector<uint8_t>> unlock_commands = {
        {0x07, 0x00, 0x82, 0x51, 0x00, 0x01, 0x00},  // Possible unlock command
        {0x08, 0x00, 0x82, 0x52, 0x00, 0x01, 0x00, 0x00},  // Another unlock command
        {0x06, 0x00, 0x82, 0x53, 0x00, 0x01},  // Another option
        {0x09, 0x00, 0x82, 0x54, 0x00, 0x01, 0x00, 0x00, 0x00},  // Extended Team
    };

    for (const auto& cmd : unlock_commands) {
        SendCommand(peripheral, cmd);
        std::this_thread::sleep_for(500ms);
    }

    // 4. Let's try setting the color in different ways
    std::vector<std::vector<uint8_t>> color_commands = {
        // Standard color commands
        {0x08, 0x00, 0x81, 0x32, 0x11, 0x00, r, g, b},
        {0x08, 0x00, 0x81, 0x3A, 0x11, 0x00, r, g, b},

        // Commands with mode
        {0x09, 0x00, 0x81, 0x32, 0x11, 0x00, 0x02, r, g, b},
        {0x09, 0x00, 0x81, 0x3A, 0x11, 0x00, 0x02, r, g, b},

        // Alternative subcommands
        {0x08, 0x00, 0x81, 0x32, 0x51, 0x00, r, g, b},
        {0x08, 0x00, 0x81, 0x3A, 0x51, 0x00, r, g, b},

        // System color commands
        {0x08, 0x00, 0x82, 0x11, 0x00, r, g, b},
        {0x08, 0x00, 0x82, 0x12, 0x00, r, g, b},
        {0x08, 0x00, 0x82, 0x13, 0x00, r, g, b},

        // Commands with different lengths
        {0x0A, 0x00, 0x81, 0x32, 0x11, 0x00, 0x02, 0x00, r, g, b},
        {0x0A, 0x00, 0x81, 0x3A, 0x11, 0x00, 0x02, 0x00, r, g, b},
    };

    for (const auto& cmd : color_commands) {
        SendCommand(peripheral, cmd);
        std::this_thread::sleep_for(1s);
    }

    std::this_thread::sleep_for(2s);
}

void TestSpecialSequences(SimpleBLE::Peripheral& peripheral) {
    std::cout << "\n=== Testing Special Unlock Sequences ===\n";

    // Sequences that can "unlock" LED control
    std::vector<std::vector<std::vector<uint8_t>>> sequences = {
        // Sequence 1: Simulate the launch of the official application
        {
            {0x05, 0x00, 0x02, 0x01, 0x00},
            {0x06, 0x00, 0x82, 0x0A, 0x00, 0x01},
            {0x08, 0x00, 0x41, 0x32, 0x02, 0x00, 0x00, 0x00},
            {0x06, 0x00, 0x81, 0x32, 0x01, 0x01},
            {0x08, 0x00, 0x81, 0x32, 0x11, 0x00, 0xFF, 0x00, 0x00},
        },

        // Sequence 2: Alternative Approach
        {
            {0x06, 0x00, 0x82, 0x0B, 0x00, 0x01},
            {0x08, 0x00, 0x41, 0x3A, 0x02, 0x00, 0x00, 0x00},
            {0x06, 0x00, 0x81, 0x3A, 0x01, 0x01},
            {0x07, 0x00, 0x82, 0x51, 0x00, 0x01, 0x00},
            {0x08, 0x00, 0x81, 0x3A, 0x51, 0x00, 0x00, 0xFF, 0x00},
        },

        // Sequence 3: Combination of system commands
        {
            {0x05, 0x00, 0x02, 0x01, 0x00},
            {0x07, 0x00, 0x82, 0x10, 0x00, 0x01, 0x00},
            {0x08, 0x00, 0x82, 0x52, 0x00, 0x01, 0x00, 0x00},
            {0x08, 0x00, 0x82, 0x11, 0x00, 0x00, 0x00, 0xFF},
        },

        // Sequence 4: Long sequence with pauses
        {
            {0x05, 0x00, 0x02, 0x01, 0x00},
            {0x06, 0x00, 0x82, 0x0A, 0x00, 0x01},
            {0x06, 0x00, 0x82, 0x0B, 0x00, 0x01},
            {0x07, 0x00, 0x82, 0x10, 0x00, 0x01, 0x00},
            {0x08, 0x00, 0x41, 0x32, 0x02, 0x00, 0x00, 0x00},
            {0x08, 0x00, 0x41, 0x3A, 0x02, 0x00, 0x00, 0x00},
            {0x06, 0x00, 0x81, 0x32, 0x01, 0x01},
            {0x06, 0x00, 0x81, 0x3A, 0x01, 0x01},
            {0x07, 0x00, 0x82, 0x51, 0x00, 0x01, 0x00},
            {0x08, 0x00, 0x82, 0x52, 0x00, 0x01, 0x00, 0x00},
            {0x09, 0x00, 0x82, 0x54, 0x00, 0x01, 0x00, 0x00, 0x00},
            {0x08, 0x00, 0x81, 0x32, 0x11, 0x00, 0xFF, 0x00, 0x00},
        }
    };

    for (size_t i = 0; i < sequences.size(); i++) {
        std::cout << "\n--- Testing Sequence " << (i + 1) << " ---\n";

        for (const auto& cmd : sequences[i]) {
            SendCommand(peripheral, cmd);
            std::this_thread::sleep_for(800ms);
        }

        std::this_thread::sleep_for(3s);

        // After each sequence, we try different colors.
        std::vector<std::vector<uint8_t>> test_colors = {
            {0xFF, 0x00, 0x00},  // Red
            {0x00, 0xFF, 0x00},  // Green
            {0x00, 0x00, 0xFF},  // Blue
        };

        for (const auto& color : test_colors) {
            std::vector<uint8_t> color_cmd = { 0x08, 0x00, 0x81, 0x32, 0x11, 0x00, color[0], color[1], color[2] };
            SendCommand(peripheral, color_cmd);
            std::this_thread::sleep_for(2s);

            std::vector<uint8_t> color_cmd_alt = { 0x08, 0x00, 0x81, 0x3A, 0x11, 0x00, color[0], color[1], color[2] };
            SendCommand(peripheral, color_cmd_alt);
            std::this_thread::sleep_for(2s);
        }

        std::this_thread::sleep_for(3s);
    }
}

void TestAllLedCommands(SimpleBLE::Peripheral& peripheral) {
    std::vector<uint8_t> ports = { 0x32, 0x3A, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    std::vector<uint8_t> subcommands = { 0x01, 0x02, 0x11, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57 };
    std::vector<uint8_t> modes = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };
    std::vector<uint8_t> flags = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };

    std::vector<std::vector<uint8_t>> colors = {
        {255, 0, 0},   // Red
        {0, 255, 0},   // Green
        {0, 0, 255},   // Blue
        {255, 255, 0}, // Yellow
        {255, 0, 255}, // Purple
        {0, 255, 255}, // Light blue
        {255, 255, 255} // White
    };

    int test_count = 1;

    for (auto port : ports) {
        for (auto subcommand : subcommands) {
            for (auto mode : modes) {
                for (auto flag : flags) {
                    for (auto& color : colors) {
                        std::cout << "\n=== Test " << test_count++ << " ===\n";
                        std::cout << "Port: 0x" << std::hex << static_cast<int>(port)
                            << ", Subcommand: 0x" << static_cast<int>(subcommand)
                            << ", Mode: 0x" << static_cast<int>(mode)
                            << ", Flag: 0x" << static_cast<int>(flag) << std::dec << "\n";

                        // Various command formats
                        std::vector<std::vector<uint8_t>> test_commands;

                        // Short commands (6-8 bytes)
                        if (subcommand == 0x01) {
                            test_commands.push_back({ 0x06, 0x00, 0x81, port, subcommand, 0x01 });
                        }
                        else {
                            test_commands.push_back({ 0x08, 0x00, 0x81, port, subcommand, flag, color[0], color[1], color[2] });
                        }

                        // Commands with mode (9-10 bytes)
                        test_commands.push_back({ 0x09, 0x00, 0x81, port, subcommand, flag, mode, color[0], color[1], color[2] });
                        test_commands.push_back({ 0x0A, 0x00, 0x81, port, subcommand, flag, mode, 0x00, color[0], color[1], color[2] });

                        // System commands
                        test_commands.push_back({ 0x08, 0x00, 0x82, 0x11, flag, color[0], color[1], color[2] });
                        test_commands.push_back({ 0x09, 0x00, 0x82, 0x11, flag, mode, color[0], color[1], color[2] });

                        // Alternative system commands
                        test_commands.push_back({ 0x08, 0x00, 0x82, 0x12, flag, color[0], color[1], color[2] });

                        test_commands.push_back({ 0x08, 0x00, 0x82, 0x13, flag, color[0], color[1], color[2] });

                        // Commands with different lengths
                        test_commands.push_back({ 0x07, 0x00, 0x81, port, subcommand, flag, color[0], color[1] });
                        test_commands.push_back({ 0x0B, 0x00, 0x81, port, subcommand, flag, mode, 0x00, 0x00, color[0], color[1], color[2] });

                        for (auto& cmd : test_commands) {
                            SendCommand(peripheral, cmd);
                            std::this_thread::sleep_for(1s);
                        }

                        // Taking a break between tests
                        std::this_thread::sleep_for(2s);
                    }
                }
            }
        }
    }
}

void SetLedColor(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b) {

    // System command to change the color of the LED
    std::vector<uint8_t> system_command = {
        0x08,
        0x00,
        0x81,
        0x32,
        0x11,
        0x00,
        r, g, b
    };
    SendCommand(peripheral, system_command);
    std::this_thread::sleep_for(500ms);
}

void SetLedColorSystem(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b) {
    // System command to change the color of the LED
    std::vector<uint8_t> system_command = {
        0x06,
        0x00,
        0x81,
        0x32,
        0x01,
        0x01,
        r, g, b
    };
    SendCommand(peripheral, system_command);
    std::this_thread::sleep_for(500ms);
}

void SetLedColorAltPort(SimpleBLE::Peripheral& peripheral, uint8_t r, uint8_t g, uint8_t b) {
    // System command to change the color of the LED
    std::vector<uint8_t> system_command = {
        0x08,
        0x00,
        0x82,
        0x11,
        0x00,
        r, g, b
    };
    SendCommand(peripheral, system_command);
    std::this_thread::sleep_for(500ms);
}

// Motor control function (updated)
void SetMotorSpeed(SimpleBLE::Peripheral& peripheral, uint8_t port, int8_t speed) {
    // Command 1: Activate mode
    std::vector<uint8_t> setup_command = {
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
    SendCommand(peripheral, setup_command);

    // Team 2: Motor Control
    std::vector<uint8_t> motor_command = {
        0x08,       // Package length
        0x00,       // Hub ID
        0x81,       // Output control command
        port,       // Motor port
        0x02,       // Subcommand: WriteDirectModeData
        0x01,       // Mode: Power (1)
        static_cast<uint8_t>(speed) // Speed
    };
    SendCommand(peripheral, motor_command);
}

void RotateMotor(SimpleBLE::Peripheral& peripheral, const uint8_t port, int8_t speed, double revolutions)
{
    // Convert revolutions to absolute degrees (1 revolution = 360 degrees)
    int32_t degrees = static_cast<int32_t>(std::round(revolutions * 360.0));

    // We form a team according to the LEGO Wireless Protocol 3.0
    std::vector<uint8_t> payload = {
        0x0F,       // Message length (15 bytes)
        0x00,       // Message counter
        0x81,       // Output control command
        port, // Port or combo port
        0x11,
        0x0B,       // Sub-team
        // Rotation angle (4 bytes little-endian)
        static_cast<uint8_t>(degrees & 0xFF),
        static_cast<uint8_t>((degrees >> 8) & 0xFF),
        static_cast<uint8_t>((degrees >> 16) & 0xFF),
        static_cast<uint8_t>((degrees >> 24) & 0xFF),
        // Speed ​​(1 byte)
        static_cast<uint8_t>(speed),
        // Maximum power (usually 100%)
        100,
        // Final state (0 = float/coast, 1 = brake/hold)
        0x01,       // Hold the position after completion
        // Use profile (0 = use acceleration profile)
        0x00
    };

    SendCommand(peripheral, payload);
}

void Connect()
{

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
            return;
        }

        // We use the first adapter
        SimpleBLE::Adapter& adapter = adapters[0];
        std::cout << "\nAdapter used: " << adapter.identifier()
            << " [" << adapter.address() << "]\n";

        // Setting up callbacks
        adapter.set_callback_on_scan_start([]() {
            //std::cout << "Scanning started...\n";
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
        for (auto& peripheral : peripherals)
        {
            std::string name = peripheral.identifier();
            std::transform(name.begin(), name.end(), name.begin(), ::toupper);

            if (name.find("LEGO") != std::string::npos ||
                name.find("HUB") != std::string::npos ||
                name.find("CONTROL") != std::string::npos)
            {

                std::cout << "\n>>> LEGO HUB detected:" << peripheral.identifier()
                    << " [" << peripheral.address() << "], RSSI: " << peripheral.rssi() << " dBm";

                // Connection attempt
                try {
                    std::cout << "\nTrying to connect...";
                    peripheral.connect();

                    // In the main function, after connection:               
                    if (peripheral.is_connected()) {
                        std::cout << "\nConnecting LEGO Hub!\n";

                        peripheral.notify(LEGO_HUB_SERVICE_UUID, LEGO_HUB_CHARACTERISTIC_UUID,
                            [](const std::vector<uint8_t>& data) {
                                std::cout << "Notification: ";
                                for (auto b : data) {
                                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                                        << static_cast<int>(b) << " ";
                                }
                                std::cout << std::dec << "\n";
                            }
                        );

                        // Looking for LEGO Hub service and features
                        SimpleBLE::Service lego_service;
                        SimpleBLE::Characteristic lego_char;

                        for (auto& service : peripheral.services()) {
                            if (service.uuid() == LEGO_HUB_SERVICE_UUID) {
                                lego_service = service;
                                for (auto& characteristic : service.characteristics()) {
                                    if (characteristic.uuid() == LEGO_HUB_CHARACTERISTIC_UUID) {
                                        lego_char = characteristic;
                                        break;
                                    }
                                }
                                break;
                            }
                        }

                        if (lego_char.uuid().empty()) {
                            std::cout << "LEGO Hub has not found!" << std::endl;
                            return;
                        }
                        std::this_thread::sleep_for(1s);

                        SetLedColor(peripheral, 128, 0, 128);
                        std::this_thread::sleep_for(1s);

                        SetLedColor(peripheral, 255, 0, 0);
                        std::this_thread::sleep_for(1s);

                        SetLedColor(peripheral, 0, 255, 0);
                        std::this_thread::sleep_for(1s);

                        SetLedColor(peripheral, 0, 0, 255);
                        std::this_thread::sleep_for(5s);

                        Test(peripheral);

                        // We stop the engine
                        std::this_thread::sleep_for(2s);

                        peripheral.disconnect();

                    }
                }
                catch (const std::exception& e) {
                    std::cout << "\nConnection error: " << e.what();
                }
            }
        }
        return;
    }
    catch (const std::exception& e) {
        std::cout << "\n!!! ERROR: " << e.what() << "\n";
        return;
    }
}

void Test(SimpleBLE::Peripheral peripheral)
{
    RotateMotor(peripheral, 0x02, 100, 3);

    RotateMotor(peripheral, 0x03, 100, 3);
    std::this_thread::sleep_for(2s);

    RotateMotor(peripheral, 0x03, -100, 3);

    RotateMotor(peripheral, 0x02, -100, 3);
    std::this_thread::sleep_for(2s);

    RotateMotor(peripheral, 0x00, 100, 3);
    std::this_thread::sleep_for(2s);

    RotateMotor(peripheral, 0x00, -100, 3);
    std::this_thread::sleep_for(2s);

    RotateMotor(peripheral, 0x01, +100, 0.5);
    std::this_thread::sleep_for(2s);

    RotateMotor(peripheral, 0x01, -100, 0.5);
    std::this_thread::sleep_for(2s);
}
