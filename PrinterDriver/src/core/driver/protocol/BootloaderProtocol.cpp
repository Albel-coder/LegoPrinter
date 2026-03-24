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
        if (frame && frame->messageType == 0x05) {

            bootloaderError = true;
            if (frame->payload.size() > 0) {
                bootloaderErrorCommand = frame->payload[0];
            }
            if (frame->payload.size() > 1) {
                bootloaderErrorCode = frame->payload[1];
            }
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

        const bool result = conditionVariable.wait_for(lock, timeout, [&] {
            return gotNotification || bootloaderError;
        });

        if (!result) {
            return false;
        }

        if (bootloaderError) {
            return false;
        }

        out = lastRawData;
        gotNotification = false;
        return true;
    };

    auto sendAndWait = [&](protocol::BootloaderCommand command,
        const std::vector<uint8_t>& payload,
        std::chrono::milliseconds timeout = 5s) -> bool
        {
            {
                std::lock_guard<std::mutex> lock(firmwareMutex);
                lastRawData.clear();
                gotNotification = false;
                bootloaderError = false;
                bootloaderErrorCommand = 0xFF;
                bootloaderErrorCode = 0xFF;
            }

            auto packet = makePacket(command, payload);
            logHex("TX", packet);

            if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
                LOG_ERROR("Write failed for command 0x%02X", static_cast<uint8_t>(command));
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
                    LOG_ERROR("Timeout waiting for response to command 0x%02X",
                        static_cast<uint8_t>(command));
                }
                
                return false;
            }

            auto frame = parseFrame(rawData);
            if (!frame) {
                LOG_ERROR("Invalid frame");
                
                return false;
            }

            if (frame->messageType == 0x05) {
                uint8_t commandError = frame->payload.size() > 0 ? frame->payload[0] : 0xFF;
                uint8_t error = frame->payload.size() > 1 ? frame->payload[1] : 0xFF;
                LOG_ERROR("Bootloader error: cmd=0x%02X err=0x%02X", commandError, error);
                
                return false;
            }

            if (frame->messageType != static_cast<uint8_t>(command)) {
                LOG_ERROR("Unexpected reply type 0x%02X for command 0x%02X",
                    frame->messageType, static_cast<uint8_t>(command));
                
                return false;
            }

            return true;
    };

    auto sendFireAndForget = [&](protocol::BootloaderCommand command,
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

            auto packet = makePacket(command, payload);
            logHex("TX", packet);

            if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
                LOG_ERROR("Write failed for command 0x%02X", static_cast<uint8_t>(command));
                return false;
            }

            std::vector<uint8_t> rawData;
            if (waitForNotification(50ms, rawData)) {
                auto frame = parseFrame(rawData);
                if (frame && frame->messageType == 0x05) {
                    uint8_t commandError = frame->payload.size() > 0 ? frame->payload[0] : 0xFF;
                    uint8_t error = frame->payload.size() > 1 ? frame->payload[1] : 0xFF;
                    LOG_ERROR("Bootloader error: command = 0x%02X error = 0x%02X", commandError, error);
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

    if (!sendAndWait(protocol::BootloaderCommand::GetInfo, {}, 5s)) {
        LOG_ERROR("GetInfo failed - bootloader not responding");
        cleanup();
        return false;
    }

    if (!sendAndWait(protocol::BootloaderCommand::InitLoader, {}, 5s)) {
        LOG_ERROR("InitLoader failed");
        cleanup();
        return false;
    }

    if (!sendAndWait(protocol::BootloaderCommand::CheckSum, {}, 5s)) {
        LOG_WARNING("CheckSum failed, continuing");
    }

    LOG_INFO("Bootloader responded");

    const size_t FLASH_CHUNK_SIZE = 11;
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

        if (!sendFireAndForget(protocol::BootloaderCommand::ProgramFlash, payload)) {
            LOG_ERROR("ProgramFlash failed at offset %zu", sent);
            cleanup();
            return false;
        }

        sent += chunk;
        LOG_INFO("Progress: %zu / %zu", sent, firmware.size());
        std::this_thread::sleep_for(10ms);
    }

    LOG_INFO("Verifying checksum...");
    if (!sendAndWait(protocol::BootloaderCommand::CheckSum, {}, 5s)) {
        LOG_WARNING("CheckSum failed, continuing");
    }

    LOG_INFO("Starting application...");
    if (!sendFireAndForget(protocol::BootloaderCommand::StartApp, {})) {
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

bool BootloaderProtocol::sendCommand(protocol::BootloaderCommand command, const std::vector<uint8_t>& payload, std::vector<uint8_t>* response, bool withResponse) {
	if (!transport.isConnected() || bootloaderChar.characteristicUuid.empty()) {
		LOG_ERROR("Bootloader sendCommand: not connected or char missing");
		return false;
	}

	auto packet = makePacket(command, payload);
	//LOG_COMMAND("Bootloader sendCommand command=0x%02X payload=%zu withResponse=%d",static_cast<uint8_t>(command), payload.size(), withResponse);

	const size_t maxWrite = transport.getMaxWriteSize();
	if (packet.size() > maxWrite) {
		LOG_ERROR("Bootloader sendCommand: packet size %zu exceeds MTU %zu", packet.size(), maxWrite);
		return false;
	}

	if (withResponse) {
		bool gotResponse = false;
		std::vector<uint8_t> responseBuffer;

		auto subscribeResult = transport.subscribe(bootloaderChar, [&](const Characteristic&, const uint8_t* data, size_t length) {
			responseBuffer.assign(data, data + length);
			gotResponse = true;
		});

		if (!subscribeResult) {
			LOG_ERROR("Bootloader sendCommand: subscribe failed");
			return false;
		}

		if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
			transport.unsubscribe(bootloaderChar);
			LOG_ERROR("Bootloader sendCommand: write failed");
			return false;
		}

		const auto start = std::chrono::steady_clock::now();
		while (!gotResponse) {
			if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
				transport.unsubscribe(bootloaderChar);
				LOG_ERROR("Bootloader sendCommand: timeout");
				return false;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		transport.unsubscribe(bootloaderChar);
		if (response) {
			*response = std::move(responseBuffer);
		}

		return true;
	}
	else {
		return transport.write(bootloaderChar, packet.data(), packet.size(), false);
	}
}

std::vector<uint8_t> BootloaderProtocol::makePacket(protocol::BootloaderCommand command, const std::vector<uint8_t>& payload) const {
	std::vector<uint8_t> packet;
	packet.reserve(3 + payload.size());

	packet.push_back(static_cast<uint8_t>(3 + payload.size())); // LEGO common header length
	packet.push_back(0x00);                                     // Hub ID
	packet.push_back(static_cast<uint8_t>(command));            // Message Type
	packet.insert(packet.end(), payload.begin(), payload.end());

	return packet;
}
