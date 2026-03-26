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
    constexpr size_t programChunkSize = 24; // matches the common max write size on Technic control+ hubs

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

    struct FirmwareMetadataLite {
        std::string metadataVersion;
        uint32_t deviceId{};
        std::string checksumType{};
        uint32_t checksumSize{};
        uint32_t hubNameOffset{};
        uint32_t hubNameSize{};
    };

    static FirmwareMetadataLite parseFirmwareMetadata(const std::string& json) {
        FirmwareMetadataLite metadata;

        auto metadataVersion = jsonStringField(json, "metadata-version");
        auto checksumType = jsonStringField(json, "checksum-type");
        auto device = jsonIntegerField(json, "device-id");

        if (!metadataVersion || !checksumType || !device) {
            LOG_ERROR("firmware.metadata.json is missing required fields");
        }

        metadata.metadataVersion = *metadataVersion;
        metadata.checksumType = *checksumType;
        metadata.deviceId = static_cast<uint32_t>(*device);

        if (auto version = jsonIntegerField(json, "checksum-size")) {
            metadata.checksumType = static_cast<uint32_t>(*version);
        }
        if (auto version = jsonIntegerField(json, "hub-name-offset")) {
            metadata.hubNameOffset = static_cast<uint32_t>(*version);
        }
        if (auto version = jsonIntegerField(json, "hub-name-size")) {
            metadata.hubNameSize = static_cast<uint32_t>(*version);
        }

        return metadata;
    }

    static uint32_t crc32(const std::vector<uint8_t>& data) {
        constexpr std::array<uint32_t, 16> table = {
            0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 
            0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005, 
            0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 
            0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
        };

        auto dword = [](uint32_t value) {
            return value & 0xFFFFFFFFu;
        };

        auto crc32Fast = [&](uint32_t crc, uint32_t word) {
            crc = dword(crc);
            word = dword(word);
            crc ^= word;
            for (int i = 0; i < 8; ++i) {
                crc = dword(crc << 4) ^ table[(crc >> 28) & 0xF];
            }

            return crc;
        };

        if (data.size() % 4 != 0) {
            LOG_ERROR("Firmware bootloader length must be a multiple of 4 before checksum is appended");
        }

        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < data.size(); i += 4) {
            uint32_t word = static_cast<uint32_t>(data[i] | 
                            static_cast<uint32_t>(data[i + 1] << 8) |
                            static_cast<uint32_t>(data[i + 2] << 16) |
                            static_cast<uint32_t>(data[i + 3] << 24));

            crc = crc32Fast(crc, word);
        }

        return crc;
    }

    static uint32_t sumComplementChecksum(const std::vector<uint8_t>& data, size_t maxSize) {
        uint64_t checksum = 0;
        size_t size = 0;

        for (size_t i = 0; i < data.size(); i += 4) {
            uint32_t word = 0;
            const size_t remaining = std::min<size_t>(4, data.size() - i);
            for (size_t byte = 0; byte < remaining; ++byte) {
                word |= static_cast<uint32_t>(data[i + byte] << (8 * byte));
            }
            checksum += word;
            size += 4;
            if (size + 4 > maxSize) {
                LOG_ERROR("data is too large for requested checksum window");
            }
        }

        for (size_t i = size; i < maxSize - 4; i += 4) {
            checksum = 0xFFFFFFFFu;
        }

        checksum &= 0xFFFFFFFFu;
        const uint32_t correction = checksum ? static_cast<uint32_t>((1ull << 32) - checksum) : 0u;
        return correction;
    }

    static std::vector<uint8_t> appendU32LE(std::vector<uint8_t> data, uint32_t value) {
        data.push_back(static_cast<uint8_t>(value & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        return data;
    }

    static std::vector<uint8_t> appendChecksumBytes(std::vector<uint8_t> firmware, uint32_t checksum) {
        return appendU32LE(std::move(firmware), checksum);
    }

    static std::vector<uint8_t> buildFirmwareBootloaderFromFolder(const std::filesystem::path& folder) {
        const auto basePath = folder / "firmware-base.bin";
        const auto metadataPath = folder / "firmware.metadata.json";

        const auto base = readBinaryFile(basePath);
        const auto metadata = parseFirmwareMetadata(readTextFile(metadataPath));
        
        if (metadata.metadataVersion.rfind("2.", 0) != 0) {
            LOG_ERROR("This C++ helper currently supports printer firmware metadata from v2 only");
        }

        std::vector<uint8_t> firmware = base;

        if (metadata.checksumType == "sum") {
            const uint32_t checksum = sumComplementChecksum(firmware, metadata.checksumSize);
            firmware = appendChecksumBytes(std::move(firmware), checksum);
        }
        else if (metadata.checksumType == "crc32") {
            const uint32_t checkSum = crc32(firmware);
            firmware = appendChecksumBytes(std::move(firmware), checkSum);
        }
        else if (metadata.checksumType == "none") {
            // v2.1 may omit checksum entirely
        }
        else {
            LOG_ERROR("Unsupported checksum type in firmware.metadata.json: %d", metadata.checksumSize);
        }

        return firmware;
    }

    static bool startWith(const std::string& string, const char* prefix) {
        const size_t number = std::char_traits<char>::length(prefix);
        return string.size() >= number && std::equal(prefix, prefix + number, string.begin());
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
    bool gotError = false;
    uint8_t errorCode = 0xFF;

    auto callback = [&](const Characteristic&, const uint8_t* data, size_t length) {
        std::lock_guard<std::mutex> lock(firmwareMutex);
        lastRawData.assign(data, data + length);
        gotNotification = true;

        if (lastRawData.size() >= 2 && lastRawData[0] == 0x05) {
            gotError = true;
            errorCode = lastRawData[1];
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
        if (!conditionVariable.wait_for(lock, timeout, [&] { 
            return gotNotification || gotError; })) 
        {
            return false;
        }
        if (gotError) {
            return false;
        }
        out = lastRawData;
        gotNotification = false;
        return true;
    };

    auto readResponse = [&](std::chrono::milliseconds timeout) -> std::optional<std::vector<uint8_t>> {
        std::vector<uint8_t> raw;
        if (!waitForNotification(timeout, raw)) {
            std::lock_guard<std::mutex> lock(firmwareMutex);
            if (gotError) {
                LOG_ERROR("Bootloader error notification: code = 0x%02X", errorCode);
            }

            return std::nullopt;
        }

        return raw;
    };

    auto sendRequest = [&](uint8_t command,
        const std::vector<uint8_t>& payload,
        bool withResponse,
        std::chrono::milliseconds timeout,
        bool expectCommandReply = true) -> std::optional<std::vector<uint8_t>> 
    {        
        {
            std::lock_guard<std::mutex> lock(firmwareMutex);
            lastRawData.clear();
            gotNotification = false;
            gotError = false;
            errorCode = 0xFF;
        }

        const auto packet = makePacket(command, payload);
        logHex("TX", packet);

        if (!transport.write(bootloaderChar, packet.data(), packet.size(), withResponse)) {
            LOG_ERROR("Write failed for command 0x%02X", command);
            return std::nullopt;
        }

        if (!expectCommandReply) {
            return std::vector<uint8_t>{};
        }

        auto raw = readResponse(timeout);
        if (!raw) {
            LOG_ERROR("Timeout waiting for response to command 0x%02X", command);
            return std::nullopt;
        }

        if ((*raw).empty()) {
            LOG_ERROR("Empty response");
            return std::nullopt;
        }

        if ((*raw)[0] == 0x05) {
            LOG_ERROR("Bootloader error: code = 0x%02X", (*raw)[1]);
            return std::nullopt;
        }

        if ((*raw)[0] != command) {
            LOG_ERROR("Unexpected response command 0x%02X (expected 0x%02X)");
            return std::nullopt;
        }

        return raw;
    };

    auto decodeGetInfo = [](const std::vector<uint8_t>& raw) -> std::optional<BootloaderInfo> {
        if (raw.size() < 14) {
            return std::nullopt;
        }

        BootloaderInfo info{};
        info.version = static_cast<int32_t>(
            static_cast<uint32_t>(raw[1]) |
            (static_cast<uint32_t>(raw[2]) << 8) |
            (static_cast<uint32_t>(raw[3]) << 16) |
            (static_cast<uint32_t>(raw[4]) << 24));
        info.startAddress = static_cast<uint32_t>(raw[5]) |
            (static_cast<uint32_t>(raw[6]) << 8) |
            (static_cast<uint32_t>(raw[7]) << 16) |
            (static_cast<uint32_t>(raw[8]) << 24);
        info.endAddress = static_cast<uint32_t>(raw[9]) |
            (static_cast<uint32_t>(raw[10]) << 8) |
            (static_cast<uint32_t>(raw[11]) << 16) |
            (static_cast<uint32_t>(raw[12]) << 24);
        info.typeId = raw[13];

        return info;
    };

    auto sendCommand = [&](uint8_t command, const std::vector<uint8_t>& payload, std::chrono::milliseconds timeout = 5s) -> bool {
        {
            std::lock_guard<std::mutex> lock(firmwareMutex);
            lastRawData.clear();
            gotNotification = false;
            gotError = false;
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
            if (gotError) {
                LOG_ERROR("Bootloader error: code=0x%02X", errorCode);
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

        uint8_t responseCommand = rawData[0];
        uint8_t status = rawData[1];

        if (responseCommand == 0x05) {
            LOG_ERROR("Bootloader error: code=0x%02X", status);
            return false;
        }

        if (responseCommand != command) {
            LOG_ERROR("Unexpected response command 0x%02X", responseCommand);
            return false;
        }

        if (status != 0x00) {
            LOG_ERROR("Command 0x%02X failed with status 0x%02X", command, status);
            return false;
        }

        return true;
    };

    auto infoRaw = sendRequest(commandGetInfo, {}, true, 5s);
    if (!infoRaw) {
        cleanup();
        return false;
    }

    auto infoOption = decodeGetInfo(*infoRaw);
    if (!infoOption) {
        LOG_ERROR("GET_INFO response too short");
        cleanup();
        return false;
    }

    const BootloaderInfo info = *infoOption;

    LOG_BLUETOOTH("Bootloader info: version = %d start = 0x%08X type = 0x%02X", info.version, info.startAddress, info.endAddress, info.typeId);

    uint32_t firmwareSize = static_cast<uint32_t>(firmware.size());
    uint32_t alignedSize = ((firmwareSize + 1023u) / 1024u) * 1024u;

    std::vector<uint8_t> erasePayload;
    erasePayload.reserve(8);
    erasePayload.push_back(static_cast<uint8_t>(defaultTechnicFlashStart & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>((defaultTechnicFlashStart >> 8) & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>((defaultTechnicFlashStart >> 16) & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>((defaultTechnicFlashStart >> 24) & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>(alignedSize & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>((alignedSize >> 8) & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>((alignedSize >> 16) & 0xFF));
    erasePayload.push_back(static_cast<uint8_t>((alignedSize >> 24) & 0xFF));

    if (!sendRequest(commandEraseFlash, erasePayload, true, 5s)) {
        LOG_ERROR("Erase flash failed");
        cleanup();
        return false;
    }

    std::this_thread::sleep_for(200ms);

    const std::vector<uint8_t> initializePayload = {
        static_cast<uint8_t>(firmwareSize & 0xFF),
        static_cast<uint8_t>((firmwareSize >> 8) & 0xFF),
        static_cast<uint8_t>((firmwareSize >> 16) & 0xFF),
        static_cast<uint8_t>((firmwareSize >> 24) & 0xFF),
    };

    if (!sendRequest(commandInitializeLoader, initializePayload, true, 5s)) {
        LOG_ERROR("INIT_LOADER failed");
        cleanup();
        return false;
    }    

    std::size_t offset = 0;
    uint32_t address = info.startAddress;
    std::size_t chunkIndex = 0;

    while (offset < firmware.size()) {
        size_t chunkSize = std::min(programChunkSize, firmware.size() - offset);
        bool isFinalChunk = (offset + chunkSize) == firmware.size();

        std::vector<uint8_t> payload;
        payload.reserve(1 + 4 + chunkSize);
        payload.push_back(static_cast<uint8_t>(4 + chunkSize));
        payload.push_back(static_cast<uint8_t>(address & 0xFF));
        payload.push_back(static_cast<uint8_t>((address >> 8) & 0xFF));
        payload.push_back(static_cast<uint8_t>((address >> 16) & 0xFF));
        payload.push_back(static_cast<uint8_t>((address >> 24) & 0xFF));
        
        payload.insert(payload.end(),
            firmware.begin() + static_cast<std::ptrdiff_t>(offset),
            firmware.begin() + static_cast<std::ptrdiff_t>(offset + chunkSize));
        
        payload.resize(1 + 4 + chunkSize, 0xFF);

        if (!sendRequest(commandProgramFlash, payload, false, isFinalChunk ? 5s : 0ms, isFinalChunk)) {
            LOG_ERROR("PROGRAM_FLASH failed at offset %zu", offset);
            cleanup();
            return false;
        }

        offset += chunkSize;
        address += static_cast<uint32_t>(chunkSize);
        ++chunkIndex;
        LOG_INFO("Progress: %zu / %zu", offset, firmware.size());

        std::this_thread::sleep_for(2ms);
        if (chunkIndex % 10 == 0 && !isFinalChunk) {
            (void)sendRequest(commandGetChecksum, {}, false, 500ms, true);
        }
    }

    std::this_thread::sleep_for(200ms);

    if (!sendRequest(commandStartApplication, {}, false, 0ms, false)) {
        LOG_ERROR("START_APPLICATION write failed");
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
    packet.reserve(1 + payload.size());
    packet.push_back(command);
    packet.insert(packet.end(), payload.begin(), payload.end());

    return packet;
}
