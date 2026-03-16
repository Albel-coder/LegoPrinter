#include "PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <chrono>
#include <thread>

PrinterProtocol::PrinterProtocol(ITransport& transport)
	: transport(transport) { }

bool PrinterProtocol::discover() {
	if (!transport.isConnected()) {
		LOG_BLUETOOTH("PrinterProtocol discover: transport not connected");
		return false;
	}

	commandEvent = {};
	capabilities = {};

	LOG_BLUETOOTH("PrinterProtocol discover: checking services");

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

	if (commandEvent.characteristicUuid.empty()) {
		LOG_ERROR("PrinterProtocol discover: command characteristic missing");
		return false;
	}

	LOG_BLUETOOTH("PrinterProtocol command characteristic found");
	return true;
}

bool PrinterProtocol::sendCommand(protocol::PybricksCommand command, const std::vector<uint8_t>& payload, bool withResponse) {
	if (commandEvent.characteristicUuid.empty() || !transport.isConnected()) {
		LOG_ERROR("PrinterProtocol sendCommand: not connected or char missing");
		return false;
	}

	std::vector<uint8_t> buffer;
	buffer.reserve(1 + payload.size());
	buffer.push_back(static_cast<uint8_t>(command));
	buffer.insert(buffer.end(), payload.begin(), payload.end());

	LOG_COMMAND("PrinterProtocol sendCommand command=0x%02X payload=%zu withResponse=%d",
		static_cast<uint8_t>(command), payload.size(), withResponse ? "true" : "false");
	
	return transport.write(commandEvent, buffer.data(), buffer.size(), withResponse);
}

bool PrinterProtocol::uploadProgram(const std::vector<uint8_t>& script) {
	if (!discover()) {
		LOG_ERROR("PrinterProtocol command characteristic not found");
		return false;
	}

	LOG_INFO("PrinterProtocol upload started (%zu bytes)", script.size());

	// Stop any currently running program before upload
	if (!sendCommand(protocol::PybricksCommand::StopUserProgram, {}, true)) {
		LOG_WARNING("Could not stop current program before upload (continuing)");
	}

	// Meta: [size u32 little-endian]
	std::vector<uint8_t> meta;
	const uint32_t size = static_cast<uint32_t>(script.size());
	meta.push_back(static_cast<uint8_t>(size & 0xFF));
	meta.push_back(static_cast<uint8_t>((size >> 8) & 0xFF));
	meta.push_back(static_cast<uint8_t>((size >> 16) & 0xFF));
	meta.push_back(static_cast<uint8_t>((size >> 24) & 0xFF));

	if (!sendCommand(protocol::PybricksCommand::WriteUserProgramMeta, meta, true)) {
		LOG_ERROR("WRITE_USER_PROGRAM_META failed");
		return false;
	}

	// Payload for WRITE_USER_RAM in this baseline:
	// [offset u32 little-endian][chunk bytes...]
	const size_t maxWrite = transport.getMaxWriteSize();
	const size_t payloadLimit = (maxWrite > 8) ? (maxWrite - 4) : 16; // 4 bytes offset
	size_t sent = 0;

	while (sent < script.size()) {
		const size_t chunk = std::min(payloadLimit, script.size() - sent);

		std::vector<uint8_t> payload;
		payload.reserve(4 + chunk);

		const uint32_t offset = static_cast<uint32_t>(sent);
		payload.push_back(static_cast<uint8_t>(offset & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 8) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 16) & 0xFF));
		payload.push_back(static_cast<uint8_t>((offset >> 24) & 0xFF));
		payload.insert(payload.end(), script.begin() + sent, script.begin() + sent + chunk);

		if (!sendCommand(protocol::PybricksCommand::CommandWriteUserRam, payload, false)) {
			LOG_ERROR("COMMAND_WRITE_USER_RAM failed at offset=%zu", sent);
			return false;
		}

		sent += chunk;
		LOG_INFO("PrinterProtocol upload progress: %zu / %zu", sent, script.size());
	}

	LOG_INFO("PrinterProtocol upload finished");
	return true;
}

bool PrinterProtocol::startUserProgram() {
	LOG_COMMAND("PrinterProtocol startUserProgram");
	return discover() && sendCommand(protocol::PybricksCommand::StartUserProgram, {}, true);
}

bool PrinterProtocol::stopUserProgram() {
	LOG_COMMAND("PrinterProtocol stopUserProgram");
	return discover() && sendCommand(protocol::PybricksCommand::StopUserProgram, {}, true);
}

bool PrinterProtocol::rebootToUpdateMode() {
	LOG_COMMAND("PrinterProtocol rebootToUpdateMode");
	return discover() && sendCommand(protocol::PybricksCommand::RebootToUpdateMode, {}, true);
}