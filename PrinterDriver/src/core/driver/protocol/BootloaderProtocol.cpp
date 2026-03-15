#include "BootloaderProtocol.h"

#include <chrono>
#include <thread>

BootloaderProtocol::BootloaderProtocol(ITransport& transport) 
	: transport(transport) {}

bool BootloaderProtocol::flashFirmware(const std::vector<uint8_t>& firmwareBootloader, ProgressCallback progress, LogCallback log) {
	if (!discover) {
		if (log) {
			log("Bootloader characteristic not found.");
		}
		return false;
	}

	if (progress) {
		progress(5, "Bootloader initialized");
	}

	if (!sendCommand(protocol::BootloaderCommand::InitLoader, {}, nullptr)) {
		if (log) {
			log("INIT_LOADER failed");
		}
		return false;
	}

	if (progress) {
		progress(10, "Querying hub info");
	}

	std::vector<uint8_t> info;
	if (!sendCommand(protocol::BootloaderCommand::GetInfo, {}, &info)) {
		if (log) {
			log("GET_INFO failed");
			return false;
		}
	}

	// Payload format in this baseline:
	// [offset u32 little-endian][chunk bytes...]
	constexpr size_t kChunkSize = 180;
	size_t sent = 0;

	while (sent < firmwareBootloader.size()) {
		const size_t chunk = std::min(kChunkSize, firmwareBootloader.size() - sent);

		std::vector<uint8_t> payload;
		payload.reserve(4 + chunk);

		const uint32_t offset = static_cast<uint32_t>(sent);
		payload.push_back(static_cast<uint8_t>(offset & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 16) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 24) & 0xFF));
		payload.insert(payload.end(), firmwareBootloader.begin() + sent, firmwareBootloader.begin() + sent + chunk);
	
		if (!sendCommand(protocol::BootloaderCommand::ProgramFlash, payload, nullptr)) {
			if (log) {
				log("PROGRAM_FLASH failed");
				return false;
			}
		}

		sent += chunk;
		if (progress) {
			const int percent = 15 + static_cast<int>((sent * 80) / firmwareBootloader.size());
			progress(percent, "Uploading firmware");
		}
	}

	if (progress) {
		progress(95, "Starting application");
	}
	if (!sendCommand(protocol::BootloaderCommand::StartApp, {}, nullptr)) {
		if (log) {
			log("START_APP failed.");
		}
		return false;
	}
	
	if (progress) {
		progress(100, "Firmware flashed");
	}
	return true;
}

bool BootloaderProtocol::discover() {
	if (!transport.isConnected()) return false;

	for (const auto& service : transport.getServices()) {
		if (service == protocol::LWP3_HUB_SERVICE_UUID) {
			const auto chars = transport.getCharacteristics(service);

			for (const auto& characteristic : chars) {
				if (characteristic.characteristicUuid == protocol::LWP3_BOOTLOADER_CHAR_UUID) {
					bootloaderChar = characteristic;
					return true;
				}
			}
		}
	}

	return false;
}

bool BootloaderProtocol::sendCommand(protocol::BootloaderCommand command, const std::vector<uint8_t>& payload, std::vector<uint8_t>* response) {
	if (!transport.isConnected() || bootloaderChar.characteristicUuid.empty()) return false;

	bool gotResponse = false;
	std::vector<uint8_t> responseBuffer;

	auto subscribeOk = transport.subscribe(bootloaderChar, [&](const Characteristic&, const uint8_t* data, size_t length) {
		responseBuffer.assign(data, data + length);
		gotResponse = true;
	});

	if (!subscribeOk) return false;

	auto packet = makePacket(command, payload);
	if (!transport.write(bootloaderChar, packet.data(), packet.size(), true)) {
		transport.unsubscribe(bootloaderChar);
		return false;
	}

	const auto start = std::chrono::steady_clock::now();
	while (!gotResponse) {
		if (std::chrono::steady_clock::now() - start > std::chrono::seconds(5)) {
			transport.unsubscribe(bootloaderChar);
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
