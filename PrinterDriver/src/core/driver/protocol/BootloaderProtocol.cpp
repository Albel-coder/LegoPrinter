#include "BootloaderProtocol.h"
#include "../logging/LogManager.h"

#include <chrono>
#include <thread>
#include <mutex>

using namespace std::chrono_literals;

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
	std::condition_variable variable;
	std::vector<uint8_t> response;
	bool gotResponse = false;

	auto callback = [&](const Characteristic&, const uint8_t* data, size_t length) {
		std::lock_guard<std::mutex> lock(firmwareMutex);
		LOG_BLUETOOTH("Bootloader notification: length=%zu", length);
		for (size_t i = 0; i < length && i < 16; ++i) {
			LOG_BLUETOOTH(" data[%zu] = 0x%02X", i, data[i]);
		}
		response.assign(data, data + length);
		gotResponse = true;
		variable.notify_all();
	};

	if (!transport.subscribe(bootloaderChar, callback)) {
		LOG_ERROR("Failed to subscribe to bootloader character");
		return false;
	}

	auto sendAndWait = [&](protocol::BootloaderCommand command, const std::vector<uint8_t>& payload) -> bool {
		auto packet = makePacket(command, payload);
		LOG_BLUETOOTH("Sending LWP3 command 0x%02X, packet size = %zu", static_cast<uint8_t>(command), packet.size());
		if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
			LOG_ERROR("Write failed for command 0x%02X", static_cast<uint8_t>(command));
			return false;
		}

		{
			std::unique_lock<std::mutex> lock(firmwareMutex);
			if (!variable.wait_for(lock, std::chrono::seconds(5), [&] {
				return gotResponse;
			})) {
				LOG_ERROR("Timeout waiting for response to command 0x%02X", static_cast<uint8_t>(command));
				return false;
			}
			gotResponse = false;
		}

		if (response.size() < 2) {
			LOG_ERROR("Response too short");
			return false;
		}

		if (response[0] != 0x01 && response[0] != 0x05) {
			LOG_ERROR("Invalid response header 0x%02X", response[2]);
			return false;
		}

		uint8_t status = response[1];
		if (status != 0x00) {
			LOG_ERROR("Command error status 0x%02X", status);
			return false;
		}

		return true;
	};	

	if (!sendAndWait(protocol::BootloaderCommand::GetInfo, {})) {
		LOG_ERROR("GetInfo failed - bootloader not responding");
		transport.unsubscribe(bootloaderChar);
		return false;
	}
	
	if (!sendAndWait(protocol::BootloaderCommand::InitLoader, {})) {
		LOG_ERROR("InitLoader failed");
		transport.unsubscribe(bootloaderChar);
		return false;
	}

	if (!sendAndWait(protocol::BootloaderCommand::CheckSum, {})) {
		LOG_WARNING("CheckSum failed, but maybe firmware is okay");
	}

	LOG_INFO("Bootloader responded");

	const size_t maxWrite = transport.getMaxWriteSize();

	size_t sent = 0;
	const size_t FLASH_CHUNK_SIZE = 11;
	uint32_t baseAddress = 0x08008000;

	while (sent < firmware.size()) {
		size_t chunk = std::min(FLASH_CHUNK_SIZE, firmware.size() - sent);
		std::vector<uint8_t> payload;
		payload.reserve(4 + FLASH_CHUNK_SIZE);
		uint32_t offset = baseAddress + sent;
		payload.push_back(offset & 0xFF);
		payload.push_back((offset >> 8) & 0xFF);
		payload.push_back((offset >> 16) & 0xFF);
		payload.push_back((offset >> 24) & 0xFF);
		payload.insert(payload.end(), firmware.begin() + sent, firmware.begin() + sent + chunk);

		while (payload.size() < (4 + FLASH_CHUNK_SIZE)) {
			payload.push_back(0xFF);
		}

		{
			std::lock_guard<std::mutex> lock(firmwareMutex);
			gotResponse = false;
			response.clear(); 
		}

		auto packet = makePacket(protocol::BootloaderCommand::ProgramFlash, payload);

		if (!transport.write(bootloaderChar, packet.data(), packet.size(), false)) {
			LOG_ERROR("WriteCommand failed at offset %zu", sent);
			return false;
		}

		{
			std::unique_lock<std::mutex> lock(firmwareMutex);
			bool success = variable.wait_for(lock, 2s, [&]{ 
				return gotResponse; 
			});

			if (!success || response.size() < 2 || (response[0] != 0x01 && response[0] != 0x05) || response[1] != 0x00) {
				LOG_ERROR("Hub did not confirm write at offset %zu (Status: %02X)",
					sent, (response.size() > 1 ? response[1] : 0xFF));
				return false;
			}
			gotResponse = false;
		}

		sent += chunk;
		LOG_INFO("Progress: %zu / %zu", sent, firmware.size());
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	LOG_INFO("Verifying checksum...");
	if (!sendAndWait(protocol::BootloaderCommand::CheckSum, {})) {
		LOG_WARNING("CheckSum failed, but maybe firmware is okay");
	}

	LOG_INFO("Starting application...");
	if (!sendAndWait(protocol::BootloaderCommand::StartApp, {})) {
		LOG_ERROR("Start application failed");
		transport.unsubscribe(bootloaderChar);
		return false;
	}

	transport.unsubscribe(bootloaderChar);
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
		for (const auto& character : transport.getCharacteristics(service)) {
			LOG_BLUETOOTH("    Char: %s", character.characteristicUuid.c_str());

			if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID &&
				character.characteristicUuid == protocol::LWP3_BOOTLOADER_CHAR_UUID) {
				bootloaderChar = character;
				LOG_BLUETOOTH("Bootloader characteristic found (bootloader)");
				return true;
			}

			if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID &&
				character.characteristicUuid == protocol::LWP3_COMMAND_CHAR_UUID) {
				bootloaderChar = character;
				LOG_BLUETOOTH("Bootloader characteristic found (hub command)");
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
	packet.push_back(0x00);		// LWP3 header: request
	packet.push_back(0x00);		// hub_id (usually 0)
	packet.push_back(static_cast<uint8_t>(command));
	packet.insert(packet.end(), payload.begin(), payload.end());

	return packet;
}
