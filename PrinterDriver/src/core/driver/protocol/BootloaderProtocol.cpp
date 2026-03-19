#include "BootloaderProtocol.h"
#include "../logging/LogManager.h"

#include <chrono>
#include <thread>

BootloaderProtocol::BootloaderProtocol(ITransport& transportPointer) 
: transport(transportPointer) {}

bool BootloaderProtocol::flashFirmware(const std::vector<uint8_t>& firmware) {
	if (!transport.isConnected()) {
		LOG_ERROR("Bootloader flash: transport not connected");
		return false;
	}

	if (!discover()) {
		LOG_ERROR("Bootloader characteristic not found");
		return false;
	}

	LOG_INFO("Bootloader flash started (%zu bytes)", firmware.size());

	if (!sendCommand(protocol::BootloaderCommand::InitLoader, {}, nullptr, true)) {
		LOG_ERROR("INIT_LOADER failed");
		return false;
	}

	std::vector<uint8_t> info;
	if (!sendCommand(protocol::BootloaderCommand::GetInfo, {}, &info, true)) {
		LOG_WARNING("GET_INFO failed");
	}

	// Payload format in this baseline:
	// [offset u32 little-endian][chunk bytes...]
	const size_t maxWrite = transport.getMaxWriteSize();
	const size_t payloadLimit = (maxWrite > 5) ? (maxWrite - 5) : 1; // 5 points - command and offset
	size_t sent = 0;

	while (sent < firmware.size()) {
		const size_t chunk = std::min(payloadLimit, firmware.size() - sent);

		std::vector<uint8_t> payload;
		payload.reserve(4 + chunk);

		const uint32_t offset = static_cast<uint32_t>(sent);
		payload.push_back(static_cast<uint8_t>(offset & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 16) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 24) & 0xFF));
		payload.insert(payload.end(), firmware.begin() + sent, firmware.begin() + sent + chunk);

		// PROGRAM_FLASH send without waiting answer
		if (!sendCommand(protocol::BootloaderCommand::ProgramFlash, payload, nullptr, false)) {
			LOG_ERROR("PROGRAM_FLASH failed at offset=%zu", sent);
			return false;
		}

		sent += chunk;
		LOG_INFO("Bootloader progress: %zu / %zu", sent, firmware.size());
	}

	LOG_INFO("Bootloader flash finished, sending START_APPLICATION");

	if (!sendCommand(protocol::BootloaderCommand::StartApp, {}, nullptr, false)) {
		LOG_WARNING("START_APPLICATION command send failed (hub may be rebooting)");
	}
	else {
		LOG_INFO("START_APPLICATION sent, waiting for reboot...");
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(2000));

	return true;
}

bool BootloaderProtocol::discover() {
	if (!transport.isConnected()) {
		LOG_BLUETOOTH("Bootloader discover: transport not connected");
		return false;
	}

	bootloaderChar = {};

	LOG_BLUETOOTH("Bootloader discover: checking services");

	for (const auto& service : transport.getServices()) {
		if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
			const auto chars = transport.getCharacteristics(service);

			for (const auto& characteristic : chars) {
				if (characteristic.characteristicUuid == protocol::LWP3_BOOTLOADER_CHAR_UUID) {
					bootloaderChar = characteristic;
					LOG_BLUETOOTH("Bootloader characteristic found");
					return true;
				}
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
	LOG_COMMAND("Bootloader sendCommand command=0x%02X payload=%zu withResponse=%d",
		static_cast<uint8_t>(command), payload.size(), withResponse);

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
	std::vector<uint8_t> out;
	out.reserve(1 + payload.size());
	out.push_back(static_cast<uint8_t>(command));
	out.insert(out.end(), payload.begin(), payload.end());
	
	return out;
}
