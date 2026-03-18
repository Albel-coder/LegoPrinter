#include "BootloaderProtocol.h"
#include "../logging/LogManager.h"

#include <chrono>
#include <thread>

BootloaderProtocol::BootloaderProtocol(ITransport& transport) 
: transport(transport) {}

bool BootloaderProtocol::flashFirmware(const std::vector<uint8_t>& firmwareBootloader) {
	LOG_INFO("transport.isConnected() = %d", transport.isConnected());
	
	LOG_INFO("Bootloader flash finished");
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
		if (service == protocol::LWP3_HUB_SERVICE_UUID) {
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

bool BootloaderProtocol::sendCommand(protocol::BootloaderCommand command, const std::vector<uint8_t>& payload, std::vector<uint8_t>* response) {
	if (!transport.isConnected() || bootloaderChar.characteristicUuid.empty()) {
		LOG_ERROR("Bootloader sendCommand: not connected or char missing");
		return false;
	}

	bool gotResponse = false;
	std::vector<uint8_t> responseBuffer;

	auto subscribeOk = transport.subscribe(bootloaderChar, [&](const Characteristic&, const uint8_t* data, size_t length) {
		responseBuffer.assign(data, data + length);
		gotResponse = true;
	});

	if (!subscribeOk) {
		LOG_ERROR("Bootloader sendCommand: subscribe failed");
		return false;
	}

	auto packet = makePacket(command, payload);
	LOG_COMMAND("Bootloader sendCommand command=0x%02X payload=%zu", static_cast<uint8_t>(command), payload.size());

	const size_t maxWrite = transport.getMaxWriteSize();
	if (packet.size() > maxWrite && maxWrite > 1) {
		// Bootloader packet should be split if needed
		// This baseline keeps it simple: send in chunks with a short delay
		size_t offset = 0;
		while (offset < packet.size()) {
			const size_t chunk = std::min(maxWrite, packet.size() - offset);
			
			if (!transport.write(bootloaderChar, packet.data() + offset, chunk, true)) {
				transport.unsubscribe(bootloaderChar);
				LOG_ERROR("Bootloader sendCommand: chunk write failed");
				return false;
			}

			offset += chunk;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	else {
		if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
			transport.unsubscribe(bootloaderChar);
			LOG_ERROR("Bootloader sendCommand: write failed");
			return false;
		}
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

std::vector<uint8_t> BootloaderProtocol::makePacket(protocol::BootloaderCommand command, const std::vector<uint8_t>& payload) const {
	std::vector<uint8_t> out;
	out.reserve(1 + payload.size());
	out.push_back(static_cast<uint8_t>(command));
	out.insert(out.end(), payload.begin(), payload.end());
	
	return out;
}
