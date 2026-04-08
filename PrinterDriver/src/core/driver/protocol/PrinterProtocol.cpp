#include "PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>

using namespace std::chrono_literals;

namespace {

	static std::vector<uint8_t> readBinaryFile(const std::string& path) {
		std::ifstream file(path, std::ios::binary);
		if (file) {
			return {};
		}

		return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	static bool isLikelyMpy(const std::vector<uint8_t>& data) {
		return data.size() >= 3 && data[0] == 'M' && data[1] == 'P' && data[2] == 'Y';
	}

} // namespace

PrinterProtocol::PrinterProtocol(ITransport& transportPointer)
	: transport(transportPointer) { }

bool PrinterProtocol::discover() {
	if (!transport.isConnected()) {
		LOG_BLUETOOTH("PrinterProtocol discover: transport not connected");
		return false;
	}

	commandEvent = {};
	capabilities = {};

	LOG_BLUETOOTH("PrinterProtocol discover: checking Pybricks services");

	for (const auto& service : transport.getServices()) {
		if (service != protocol::PYBRICKS_SERVICE_UUID) {
			continue;
		}

		for (const auto& characteristic : transport.getCharacteristics(service)) {
			if (characteristic.characteristicUuid == protocol::PYBRICKS_COMMAND_EVENT_UUID) {
				commandEvent = characteristic;
			}
			else if (characteristic.characteristicUuid == protocol::PYBRICKS_HUB_CAPABILITIES_UUID) {
				capabilities = characteristic;
			}
		}
	}

	if (commandEvent.characteristicUuid.empty()) {
		LOG_ERROR("PrinterProtocol discover: command/event missing");
		return false;
	}

	LOG_BLUETOOTH("PrinterProtocol command/event found");
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

	LOG_COMMAND("PrinterProtocol sendCommand command=0x%02X payload=%zu withResponse=%s",
		static_cast<uint8_t>(command), payload.size(), withResponse ? "true" : "false");
	
	LOG_COMMAND("Sending packet: %zu bytes", buffer.size());

	return transport.write(commandEvent, buffer.data(), buffer.size(), withResponse);
}

bool PrinterProtocol::uploadProgram(const std::vector<uint8_t>& script) {
	if (!transport.isConnected()) {
		LOG_ERROR("Printer upload: transport not connected");
		return false;
	}
	
	if (!discover()) {
		LOG_ERROR("PrinterProtocol command/event not found");
		return false;
	}

	if (script.empty()) {
		LOG_ERROR("PrinterProtocol upload: compiled blob is empty");
		return false;
	}

	LOG_INFO("PrinterProtocol upload started (%zu bytes)", script.size());

	// Optional: stop running program first
	(void)sendCommand(protocol::PybricksCommand::StopUserProgram, {}, false);
	std::this_thread::sleep_for(100ms);

	// WRITE_USER_PROGRAM_META: size as u32 LE
	const uint32_t programSize = static_cast<uint32_t>(script.size());
	std::vector<uint8_t> meta = {
		static_cast<uint8_t>(programSize & 0xFF),
		static_cast<uint8_t>((programSize >> 8) & 0xFF),
		static_cast<uint8_t>((programSize >> 16) & 0xFF),
		static_cast<uint8_t>((programSize >> 24) & 0xFF),
	};

	if (!sendCommand(protocol::PybricksCommand::WriteUserProgramMeta, meta, false)) {
		LOG_ERROR("WRITE_USER_PROGRAM_META failed");
		return false;
	}

	// max write size includes command bytes + offset(4) + data
	const size_t maxWrite = transport.getMaxWriteSize();

	// Actual useful data size = maxWrite - 1 (command) - 4 (offset) - 3 (ATT header) ~ 24
	const size_t payloadLimit = (maxWrite > 8) ? (maxWrite - 8) : 20;
	
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

		payload.insert(payload.end(), 
			script.begin() + static_cast<std::ptrdiff_t>(sent), 
			script.begin() + static_cast<std::ptrdiff_t>(sent + chunk));

		if (!sendCommand(protocol::PybricksCommand::CommandWriteUserRam, payload, false)) {
			LOG_ERROR("COMMAND_WRITE_USER_RAM failed at offset=%zu", sent);
			return false;
		}

		sent += chunk;
		LOG_INFO("PrinterProtocol upload progress: %zu / %zu", sent, script.size());
	
		std::this_thread::sleep_for(50ms);
	}

	if (!sendCommand(protocol::PybricksCommand::StartUserProgram, {}, false)) {
		LOG_ERROR("START_USER_PROGRAM failed");
		return false;
	}

	LOG_INFO("PrinterProtocol upload finished");
	return true;
}

bool PrinterProtocol::uploadProgramFromFile(const std::string& filePath) {
	auto scriptData = readBinaryFile(filePath);
	if (scriptData.empty()) {
		LOG_ERROR("Failed to read program file or file is empty: %s", filePath.c_str());
		return false;
	}

	if (!isLikelyMpy(scriptData)) {
		LOG_WARNING("Program file does not look like .mpy, continuing anyway");
	}

	return uploadProgram(scriptData);
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
