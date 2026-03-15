#pragma once

#include <cstdint>

namespace protocol {

    static constexpr const char* LWP3_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_BOOTLOADER_SERVICE_UUID = "00001625-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_BOOTLOADER_CHAR_UUID = "00001626-1212-efde-1623-785feabcd123";

    static constexpr const char* PYBRICKS_SERVICE_UUID = "c5f50001-8280-46da-89f4-6d8051e4aeef";
    static constexpr const char* PYBRICKS_COMMAND_UUID = "c5f50002-8280-46da-89f4-6d8051e4aeef";
    static constexpr const char* PYBRICKS_CAPABILITIES_UUID = "c5f50003-8280-46da-89f4-6d8051e4aeef";

    static constexpr const char* NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
    static constexpr const char* NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

    enum class PybricksCommand : uint8_t {
        StopUserProgram = 0x00,
        StartUserProgram = 0x01,
        StartRepl = 0x02,
        WriteUserProgramMeta = 0x03,
        CommandWriteUserRam = 0x04,
        RebootToUpdateMode = 0x05,
        WriteStdin = 0x06
    };

    enum class BootloaderCommand : uint8_t {
        InitLoader = 0x44,
        ProgramFlash = 0x22,
        StartApp = 0x33,
        GetInfo = 0x55
    };

} // namespace protocol