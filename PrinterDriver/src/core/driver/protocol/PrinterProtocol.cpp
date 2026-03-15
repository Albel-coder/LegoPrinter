#include "PrinterProtocol.h"

#include <algorithm>
#include <cstdint>

PrinterProtocol::PrinterProtocol(ITransport& transport)
	: transport(transport) { }

bool PrinterProtocol::discover() {
	if (!transport.isConnected()) {
		return false;
	}

	commandEvent = {};
	capabilities = {};

	for (const auto& service : transport.getServices()) {
		if (service == protocol::PYBRICKS_SERVICE_UUID) {

			for (const auto& characteristic : transport.getCharacteristics(service)) {
				if (characteristic.characteristicUuid == protocol::PYBRICKS_COMMAND_UUID) {
					commandEvent = characteristic;
				}
				else if (characteristic.characteristicUuid == protocol::PYBRICKS_CAPABILITIES_UUID) {
					capabilities = characteristic;
				}
			}
		}
	}

	return !commandEvent.characteristicUuid.empty();
}

bool PrinterProtocol::sendCommand(protocol::PybricksCommand command, const std::vector<uint8_t>& payload, bool withResponse) {
	if (commandEvent.characteristicUuid.empty() || !transport.isConnected()) {
		return false;
	}

	std::vector<uint8_t> buffer;
	buffer.reserve(1 + payload.size());
	buffer.push_back(static_cast<uint8_t>(command));
	buffer.insert(buffer.end(), payload.begin(), payload.end());

	return transport.write(commandEvent, buffer.data(), buffer.size(), withResponse);
}

bool PrinterProtocol::uploadProgram(const std::vector<uint8_t>& script, ProgressCallback progress, LogCallback log) {
	if (!discover()) {
		if (log) {
			log("Printer command characteristic not found.");
		}
		return false;
	}

	if (progress) {
		progress(5, "Preparing upload");
	}

	// Meta: [size u32 little-endian]
	std::vector<uint8_t> meta;
	const uint32_t size = static_cast<uint32_t>(script.size());
	meta.push_back(static_cast<uint8_t>(size & 0xFF));
	meta.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
	meta.push_back(static_cast<uint8_t>((size >> 16) & 0xFF));
	meta.push_back(static_cast<uint8_t>((size >> 24) & 0xFF));

	if (!sendCommand(protocol::PybricksCommand::WriteUserProgramMeta, meta, true)) {
		if (log) {
			log("WRITE_USER_PROGRAM_META failed.");
		}

		return false;
	}

	if (progress) {
		progress(15, "Uploading script");
	}

	// Payload for WRITE_USER_RAM in this baseline:
	// [offset u32 little-endian][chunk bytes...]
	constexpr size_t kChunkSize = 180;
	size_t sent = 0;

	while (sent < script.size()) {
		const size_t chunk = std::min(kChunkSize, script.size() - sent);

		std::vector<uint8_t> payload;
		payload.reserve(4 + chunk);

		const uint32_t offset = static_cast<uint32_t>(sent);
		payload.push_back(static_cast<uint8_t>(offset & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 16) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 24) & 0xFF));
		payload.insert(payload.end(), script.begin() + sent, script.begin() + sent + chunk);

		if (!sendCommand(protocol::PybricksCommand::CommandWriteUserRam, payload, false)) {
			if (log) {
				log("COMMAND_WRITE_USER_RAM failed.");
			}
			return false;
		}

		sent += chunk;
		if (progress) {
			const int percent = 15 + static_cast<int>((sent * 75) / script.size());
			progress(percent, "Uploading script");
		}
	}

	if (progress) {
		progress(95, "Stopping old program");
	}
	sendCommand(protocol::PybricksCommand::StopUserProgram, {}, true);

	if (progress) {
		progress(100, "Program upload");
	}
	return true;
}

bool PrinterProtocol::startUserProgram() {
	return discover() && sendCommand(protocol::PybricksCommand::StartUserProgram, {}, true);
}

bool PrinterProtocol::stopUserProgram() {
	return discover() && sendCommand(protocol::PybricksCommand::StopUserProgram, {}, true);
}

bool PrinterProtocol::rebootToUpdateMode() {
	return discover() && sendCommand(protocol::PybricksCommand::RebootToUpdateMode, {}, true);
}