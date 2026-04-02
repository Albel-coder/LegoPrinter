#pragma once

#include <cstdint>

namespace protocol {

    static constexpr const char* LWP3_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_BOOTLOADER_SERVICE_UUID = "00001625-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_BOOTLOADER_CHAR_UUID = "00001626-1212-efde-1623-785feabcd123";

    static constexpr const char* PRINTER_SERVICE_UUID = "c5f50001-8280-46da-89f4-6d8051e4aeef";
    static constexpr const char* PRINTER_COMMAND_EVENT_UUID = "c5f50002-8280-46da-89f4-6d8051e4aeef";
    static constexpr const char* PRINTER_CAPABILITIES_UUID = "c5f50003-8280-46da-89f4-6d8051e4aeef";

    static constexpr const char* LWP3_COMMAND_CHAR_UUID = "00001624-1212-efde-1623-785feabcd123";

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
        CheckSum = 0x11,
        InitLoader = 0x44,
        ProgramFlash = 0x22,
        StartApp = 0x33,
        GetInfo = 0x55
    };

} // namespace protocol