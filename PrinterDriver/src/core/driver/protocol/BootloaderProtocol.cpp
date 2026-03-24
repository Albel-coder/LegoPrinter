#include "BootloaderProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>
#include <iomanip>

using namespace std::chrono_literals;

namespace {

    struct BootloaderFrame {
        uint8_t length{};
        uint8_t hubId{};
        uint8_t messageType{};
        std::vector<uint8_t> payload;
    };

    static void logHex(const char* prefix, const std::vector<uint8_t>& data) {
        std::ostringstream oss;
        oss << prefix << " [";
        for (size_t i = 0; i < data.size(); ++i) {
            oss << "0x" << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0')
                << static_cast<int>(data[i]);
            if (i + 1 < data.size()) oss << ' ';
        }
        oss << "]";
        LOG_BLUETOOTH("%s", oss.str().c_str());
    }

    static std::optional<BootloaderFrame> parseFrame(const std::vector<uint8_t>& rawData) {
        if (rawData.size() < 3) {
            return std::nullopt;
        }

        BootloaderFrame frame;
        frame.length = rawData[0];
        frame.hubId = rawData[1];
        frame.messageType = rawData[2];
        frame.payload.assign(rawData.begin() + 3, rawData.end());
        return frame;
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

        auto frame = parseFrame(lastRawData);
        if (frame && frame->messageType == 0x05 && frame->payload.size() >= 2) {
            bootloaderError = true;
            bootloaderErrorCommand = frame->payload[0];
            bootloaderErrorCode = frame->payload[1];
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

        if (bootloaderError) {
            return false;
        }

        out = lastRawData;
        gotNotification = false;
        return true;
    };

    auto sendAndWait = [&](uint8_t command,
        const std::vector<uint8_t>& payload,
        std::chrono::milliseconds timeout = 5s) -> bool
        {
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
                    LOG_ERROR("Bootloader error: cmd=0x%02X err=0x%02X",
                        bootloaderErrorCommand, bootloaderErrorCode);
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
                LOG_ERROR("Bootloader error: code = 0x%02X", status);
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

    auto sendFireAndForget = [&](uint8_t subCommand,
        const std::vector<uint8_t>& payload) -> bool
        {
            {
                std::lock_guard<std::mutex> lock(firmwareMutex);
                lastRawData.clear();
                gotNotification = false;
                bootloaderError = false;
                bootloaderErrorCommand = 0xFF;
                bootloaderErrorCode = 0xFF;
            }

            auto packet = makePacket(subCommand, payload);
            logHex("TX", packet);

            if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
                LOG_ERROR("Write failed for command 0x%02X", subCommand);
                return false;
            }

            // Start App does not return anything; for other commands we just sniff briefly for an error
            if (subCommand == 0x33) {
                return true;
            }

            std::vector<uint8_t> raw;
            if (waitForNotification(50ms, raw)) {
                auto frame = parseFrame(raw);
                if (frame && frame->messageType == 0x05 && frame->payload.size() >= 2) {
                    LOG_ERROR("Bootloader error: cmd=0x%02X err=0x%02X",
                        frame->payload[0], frame->payload[1]);
                    return false;
                }
            }
            else {
                std::lock_guard<std::mutex> lock(firmwareMutex);
                if (bootloaderError) {
                    LOG_ERROR("Bootloader error: cmd=0x%02X err=0x%02X",
                        bootloaderErrorCommand, bootloaderErrorCode);
                    return false;
                }
            }

            return true;
    };

    uint32_t firmwareSize = static_cast<uint32_t>(firmware.size());
    
    // Erase Flash (0x11)
    uint32_t eraseAddress = 0x08008000;
    uint32_t eraseLength = ((firmwareSize + 1023) / 1024) * 1024; // выровнено по 1KB
    std::vector<uint8_t> erasePayload;
    erasePayload.push_back(eraseAddress & 0xFF);
    erasePayload.push_back((eraseAddress >> 8) & 0xFF);
    erasePayload.push_back((eraseAddress >> 16) & 0xFF);
    erasePayload.push_back((eraseAddress >> 24) & 0xFF);
    erasePayload.push_back(eraseLength & 0xFF);
    erasePayload.push_back((eraseLength >> 8) & 0xFF);
    erasePayload.push_back((eraseLength >> 16) & 0xFF);
    erasePayload.push_back((eraseLength >> 24) & 0xFF);

    if (!sendAndWait(0x11, erasePayload, 5s)) {
        LOG_ERROR("Erase Flash failed");
        cleanup();
        return false;
    }
    
    std::vector<uint8_t> sizePayload;
    sizePayload.push_back(static_cast<uint8_t>(firmwareSize & 0xFF));
    sizePayload.push_back(static_cast<uint8_t>((firmwareSize >> 8) & 0xFF));
    sizePayload.push_back(static_cast<uint8_t>((firmwareSize >> 16) & 0xFF));
    sizePayload.push_back(static_cast<uint8_t>(firmwareSize >> 24) & 0xFF);

    // Initiate Loader = 0x44
    if (!sendAndWait(0x44, sizePayload, 5s)) {
        LOG_ERROR("Initiate Loader failed");
        cleanup();
        return false;
    }

    // Get Info = subcommand 0x55
    if (!sendAndWait(0x55, {}, 5s)) {
        LOG_ERROR("GetInfo failed - bootloader not responding");
        cleanup();
        return false;
    }

    // Checksum = 0x66
    if (!sendAndWait(0x66, {}, 5s)) {
        LOG_WARNING("CheckSum failed, continuing");
    }

    LOG_INFO("Bootloader responded");

    const size_t FLASH_CHUNK_SIZE = 16;
    const uint32_t baseAddress = 0x08008000;

    size_t sent = 0;
    while (sent < firmware.size()) {
        const size_t chunk = std::min(FLASH_CHUNK_SIZE, firmware.size() - sent);

        std::vector<uint8_t> payload;
        payload.reserve(4 + FLASH_CHUNK_SIZE);

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

        if (!sendAndWait(0x22, payload, 200ms)) {
            LOG_ERROR("ProgramFlash failed at offset %zu", sent);
            cleanup();
            return false;
        }

        sent += chunk;
        LOG_INFO("Progress: %zu / %zu", sent, firmware.size());
        std::this_thread::sleep_for(10ms);
    }

    LOG_INFO("Verifying checksum...");
    if (!sendAndWait(0x66, {}, 5s)) {
        LOG_WARNING("CheckSum failed, continuing");
    }

    LOG_INFO("Starting application...");
    auto startApplication = [&]() -> bool {
        auto packet = makePacket(0x33, {});
        logHex("TX", packet);
        return transport.write(bootloaderChar, packet.data(), packet.size());
    };

    if (!startApplication()) {
        LOG_ERROR("StartApplication write failed");
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

