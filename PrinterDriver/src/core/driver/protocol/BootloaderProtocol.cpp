#include "BootloaderProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <regex>
#include <sstream>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

    constexpr uint8_t commandEraseFlash = 0x11;
    constexpr uint8_t commandProgramFlash = 0x22;
    constexpr uint8_t commandStartApplication = 0x33;
    constexpr uint8_t commandInitializeLoader = 0x44;
    constexpr uint8_t commandGetInfo = 0x55;
    constexpr uint8_t commandGetChecksum = 0x66;
    constexpr uint8_t commandDisconnect = 0x88;

    constexpr uint32_t defaultTechnicFlashStart = 0x800800;
    constexpr size_t programChunkSize = 32; // matches the common max write size on Technic control+ hubs

    struct BootloaderFrame {
        uint8_t command{};
        std::vector<uint8_t> payload;
    };

    struct BootloaderInfo {
        int32_t version{};
        uint32_t startAddress{};
        uint8_t endAddress{};
        uint8_t typeId{};
    };

    static void logHex(const char* prefix, const std::vector<uint8_t>& data) {
        std::ostringstream oss;
        oss << prefix << " [";
        for (size_t i = 0; i < data.size(); ++i) {
            oss << "0x" << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0')
                << static_cast<int>(data[i]);
            if (i + 1 < data.size()) {
                oss << ' ';
            }
        }
        oss << "]";
        LOG_BLUETOOTH("%s", oss.str().c_str());
    }

    static std::optional<BootloaderFrame> parseFrame(const std::vector<uint8_t>& rawData) {
        if (rawData.size() < 3) {
            return std::nullopt;
        }

        BootloaderFrame frame;
        frame.command = rawData[0];
        frame.payload.assign(rawData.begin() + 1, rawData.end() + 1);
        return frame;
    }

    static std::vector<uint8_t> readBinaryFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            LOG_ERROR("Failed to open file: %s", path.string().c_str());
        }

        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }

    static std::string readTextFile(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            LOG_ERROR("Failed to open file: ", path.string().c_str());
        }

        std::ostringstream string;
        string << file.rdbuf();
        return string.str();
    }

    static std::string trim(std::string string) {
        auto notSpace = [](unsigned char character) {
            return !std::isspace(character);
        };
        string.erase(string.begin(), std::find_if(string.begin(), string.end(), notSpace));
        string.erase(std::find_if(string.rbegin(), string.rend(), notSpace).base(), string.end());
        return string;
    }

    static std::optional<std::string> jsonStringField(const std::string& json, const std::string& key) {
        std::regex regex("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch match;
        if (!std::regex_search(json, match, regex) || match.size() < 2) {
            return std::nullopt;
        }
        return match[1].str();
    }

    static std::optional<int64_t> jsonIntegerField(const std::string& json, const std::string& key) {
        std::regex regex("\"" + key + "\"\\s*:\\s*(-?[0-9]+)");
        std::smatch match;
        if (!std::regex_search(json, match, regex)) {
            return std::nullopt;
        }

        return std::stoll(match[1].str());
    }

} // namespace

// Calculating CRC-16-CCITT
static uint16_t crr16CCITT(const uint16_t* data, size_t length) {
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < length; ++i) {
		crc ^= (data[i] << 8);

		for (int j = 0; j < 8; ++j) {
			if (crc & 0x8000) {
				crc = (crc << 1) ^ 0x1021;
			}
			else {
				crc <<= 1;
			}
		}
	}

	return crc;
}

BootloaderProtocol::BootloaderProtocol(ITransport& transportPointer) 
: transport(transportPointer) {}

bool BootloaderProtocol::flashFirmware(const std::vector<uint8_t>& firmware) {
    if (!transport.isConnected() || !discover()) {
        LOG_ERROR("Bootloader flash: transport not connected or discover failed");
        return false;
    }

    std::mutex firmwareMutex;
    std::condition_variable conditionVariable;

    std::vector<uint8_t> lastRawData;
    bool gotNotification = false;
    bool bootloaderError = false;
    uint8_t bootloaderErrorCommand = 0xFF;
    uint8_t bootloaderErrorCode = 0xFF;

    auto callback = [&](const Characteristic&, const uint8_t* data, size_t length) {
        std::lock_guard<std::mutex> lock(firmwareMutex);
        lastRawData.assign(data, data + length);
        gotNotification = true;

        if (lastRawData.size() >= 2 && lastRawData[0] == 0x05) {
            bootloaderError = true;
            bootloaderErrorCommand = lastRawData[0];
            bootloaderErrorCode = lastRawData[1];
        }

        logHex("RX", lastRawData);
        conditionVariable.notify_all();
    };

    if (!transport.subscribe(bootloaderChar, callback)) {
        LOG_ERROR("Failed to subscribe to bootloader characteristic");
        return false;
    }

    auto cleanup = [&]() {
        transport.unsubscribe(bootloaderChar);
    };

    auto waitForNotification = [&](std::chrono::milliseconds timeout, std::vector<uint8_t>& out) -> bool {
        std::unique_lock<std::mutex> lock(firmwareMutex);
        if (!conditionVariable.wait_for(lock, timeout, [&] { return gotNotification || bootloaderError; })) {
            return false;
        }
        if (bootloaderError) return false;
        out = lastRawData;
        gotNotification = false;
        return true;
    };

    auto sendCommand = [&](uint8_t command, const std::vector<uint8_t>& payload, std::chrono::milliseconds timeout = 5s) -> bool {
        {
            std::lock_guard<std::mutex> lock(firmwareMutex);
            lastRawData.clear();
            gotNotification = false;
            bootloaderError = false;
        }

        auto packet = makePacket(command, payload);
        logHex("TX", packet);

        if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
            LOG_ERROR("Write failed for command 0x%02X", command);
            return false;
        }

        std::vector<uint8_t> rawData;
        if (!waitForNotification(timeout, rawData)) {
            std::lock_guard<std::mutex> lock(firmwareMutex);
            if (bootloaderError) {
                LOG_ERROR("Bootloader error: code=0x%02X", bootloaderErrorCode);
            }
            else {
                LOG_ERROR("Timeout waiting for response to command 0x%02X", command);
            }
            return false;
        }

        if (rawData.size() < 2) {
            LOG_ERROR("Response too short");
            return false;
        }

        uint8_t respCmd = rawData[0];
        uint8_t status = rawData[1];

        if (respCmd == 0x05) {
            LOG_ERROR("Bootloader error: code=0x%02X", status);
            return false;
        }

        if (respCmd != command) {
            LOG_ERROR("Unexpected response command 0x%02X", respCmd);
            return false;
        }

        if (status != 0x00) {
            LOG_ERROR("Command 0x%02X failed with status 0x%02X", command, status);
            return false;
        }

        return true;
    };

    // Erase Flash
    uint32_t firmwareSize = static_cast<uint32_t>(firmware.size());
    uint32_t alignedSize = ((firmwareSize + 1023) / 1024) * 1024;
    uint32_t eraseAddress = 0x08008000;

    std::vector<uint8_t> erasePayload;
    erasePayload.push_back(eraseAddress & 0xFF);
    erasePayload.push_back((eraseAddress >> 8) & 0xFF);
    erasePayload.push_back((eraseAddress >> 16) & 0xFF);
    erasePayload.push_back((eraseAddress >> 24) & 0xFF);
    erasePayload.push_back(alignedSize & 0xFF);
    erasePayload.push_back((alignedSize >> 8) & 0xFF);
    erasePayload.push_back((alignedSize >> 16) & 0xFF);
    erasePayload.push_back((alignedSize >> 24) & 0xFF);

    if (!sendCommand(0x11, erasePayload, 5s)) {
        LOG_ERROR("Erase Flash failed");
        cleanup();
        return false;
    }
    std::this_thread::sleep_for(200ms);

    // Initiate Loader
    std::vector<uint8_t> sizePayload;
    sizePayload.push_back(static_cast<uint8_t>(firmwareSize & 0xFF));
    sizePayload.push_back(static_cast<uint8_t>((firmwareSize >> 8) & 0xFF));
    sizePayload.push_back(static_cast<uint8_t>((firmwareSize >> 16) & 0xFF));
    sizePayload.push_back(static_cast<uint8_t>((firmwareSize >> 24) & 0xFF));

    if (!sendCommand(0x44, sizePayload, 5s)) {
        LOG_ERROR("Initiate Loader failed");
        cleanup();
        return false;
    }

    // Program Flash
    const size_t FLASH_CHUNK_SIZE = 16;
    const uint32_t baseAddress = 0x08008000;
    size_t sent = 0;
    while (sent < firmware.size()) {
        const size_t chunk = std::min(FLASH_CHUNK_SIZE, firmware.size() - sent);
        std::vector<uint8_t> payload;
        payload.reserve(4 + chunk);
        const uint32_t offset = baseAddress + static_cast<uint32_t>(sent);
        payload.push_back(static_cast<uint8_t>(offset & 0xFF));
        payload.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>((offset >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((offset >> 24) & 0xFF));
        payload.insert(payload.end(),
            firmware.begin() + static_cast<std::ptrdiff_t>(sent),
            firmware.begin() + static_cast<std::ptrdiff_t>(sent + chunk));
        while (payload.size() < 4 + FLASH_CHUNK_SIZE) {
            payload.push_back(0xFF);
        }
        if (!sendCommand(0x22, payload, 200ms)) {
            LOG_ERROR("ProgramFlash failed at offset %zu", sent);
            cleanup();
            return false;
        }
        sent += chunk;
        LOG_INFO("Progress: %zu / %zu", sent, firmware.size());
        std::this_thread::sleep_for(10ms);
    }

    // Start Application (without answer)
    auto startAppPacket = makePacket(0x33, {});
    logHex("TX", startAppPacket);
    if (!transport.write(bootloaderChar, startAppPacket.data(), startAppPacket.size(), true)) {
        LOG_ERROR("StartApp write failed");
        cleanup();
        return false;
    }

    cleanup();
    LOG_INFO("Firmware flashed successfully");
    return true;
}


bool BootloaderProtocol::discover() {
    if (!transport.isConnected()) {
        LOG_BLUETOOTH("Bootloader discover: transport not connected");
        return false;
    }

    LOG_BLUETOOTH("Bootloader discover: checking services");
    for (const auto& service : transport.getServices()) {
        LOG_BLUETOOTH(" Service: %s", service.c_str());

        if (service != protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
            continue;
        }

        for (const auto& character : transport.getCharacteristics(service)) {
            LOG_BLUETOOTH("    Char: %s", character.characteristicUuid.c_str());

            if (character.characteristicUuid == protocol::LWP3_BOOTLOADER_CHAR_UUID) {
                bootloaderChar = character;
                LOG_BLUETOOTH("Bootloader characteristic found");
                return true;
            }
        }
    }

    LOG_ERROR("Bootloader discover failed");
    return false;
}

std::vector<uint8_t> BootloaderProtocol::makePacket(uint8_t command, const std::vector<uint8_t>& payload) const {
    std::vector<uint8_t> packet;
    packet.reserve(2 + payload.size());
    packet.push_back(command);
    packet.push_back(0x00);
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}

