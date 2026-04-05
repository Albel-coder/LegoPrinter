#pragma once

#include <cstdint>

namespace protocol {

    static constexpr const char* LWP3_HUB_SERVICE_UUID = "00001623-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_HUB_CHARACTERISTIC_UUID = "00001624-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_BOOTLOADER_SERVICE_UUID = "00001625-1212-efde-1623-785feabcd123";
    static constexpr const char* LWP3_BOOTLOADER_CHAR_UUID = "00001626-1212-efde-1623-785feabcd123";

    static constexpr const char* PYBRICKS_SERVICE_UUID = "c5f50001-8280-46da-89f4-6d8051e4aeef";
    static constexpr const char* PYBRICKS_COMMAND_EVENT_UUID = "c5f50002-8280-46da-89f4-6d8051e4aeef";
    static constexpr const char* PYBRICKS_HUB_CAPABILITIES_UUID = "c5f50003-8280-46da-89f4-6d8051e4aeef";

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
        EraseFlash = 0x11,
        ProgramFlash = 0x22,
        StartApp = 0x33,
        InitLoader = 0x44,
        GetInfo = 0x55,
        GetCheckSum = 0x66,
        GetFlashState = 0x77,
        Disconnect = 0x88,
    };

} // namespace protocol
