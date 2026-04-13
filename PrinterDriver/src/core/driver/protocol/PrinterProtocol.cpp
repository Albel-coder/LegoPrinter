#include "PrinterProtocol.h"
#include "../logging/LogManager.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <thread>
#include <cstdint>
#include <cstring>

using namespace std::chrono_literals;

namespace {

	uint32_t calculateCrc32(const uint8_t* data, size_t length) {
		uint32_t crc = 0xFFFFFFFF;

		for (size_t i = 0; i < length; ++i) {
			crc ^= data[i];

			for (int j = 0; j < 8; ++j) {
				crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
			}
		}
		return ~crc;
	}

	static std::vector<uint8_t> readBinaryFile(const std::string& path) {
		std::ifstream file(path, std::ios::binary);
		if (!file) {
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

bool PrinterProtocol::waitForProgramStop(std::chrono::milliseconds timeout) {
	auto start = std::chrono::steady_clock::now();
	bool stopped = false;

	while (!stopped && std::chrono::steady_clock::now() - start < timeout) {
		
		{
			std::lock_guard<std::mutex> lock(responseMutex);
			waitingForResponse = true;
			expectedCommand = 0x00;
			lastResponse.reset();
		}

		std::unique_lock<std::mutex> lock(responseMutex);
		bool received = responseConditionVariable.wait_for(lock, std::chrono::seconds(1), [this] {
			return !waitingForResponse;
		});

		if (received && lastResponse && lastResponse->size() >= 2) {
			uint8_t flags = (*lastResponse)[1];
			if ((flags & 0x02) == 0) {
				stopped = true;
				LOG_INFO("Program stopped, flags=0x%02X", flags);
				break;
			}
			else {
				LOG_COMMAND("Program still running, flags=0x%02X", flags);
			}
		}

		waitingForResponse = false;
		responseConditionVariable.notify_all();
	}

	return stopped;
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

	LOG_COMMAND("PrinterProtocol sendCommand command = 0x%02X payload = %zu withResponse = %s",
		static_cast<uint8_t>(command), payload.size(), withResponse ? "true" : "false");
	
	LOG_COMMAND("Sending packet: %zu bytes", buffer.size());

	{
		std::lock_guard<std::mutex> lock(responseMutex);
		if (withResponse) {
			waitingForResponse = true;
			expectedCommand = static_cast<uint8_t>(command);
			lastResponse.reset();
		}
	}

	bool writeCommandResult = transport.write(commandEvent, buffer.data(), buffer.size(), false);
	if (!writeCommandResult) {
		std::lock_guard<std::mutex> lock(responseMutex);
		waitingForResponse = false;
		return false;
	}

	if (withResponse) {
		std::unique_lock<std::mutex> lock(responseMutex);

		bool received = responseConditionVariable.wait_for(lock, std::chrono::seconds(2), [this] {
			return !waitingForResponse;
		});

		waitingForResponse = false;
		responseConditionVariable.notify_all();

		if (lastResponse && lastResponse->size() >= 2) {
			uint8_t status = (*lastResponse)[1];
			if (status != 0x00) {
				LOG_ERROR("Command 0x%02X failed with status 0x%02X", command, status);
			}
			else {
				LOG_COMMAND("Command 0x%02X succeeded", command);
			}
		}

		if (!received) {
			LOG_ERROR("Timeout waiting for response to command 0x%02X", static_cast<uint8_t>(command));
			return false;
		}

		// Check the response status (second byte is 0x00 = success)
		if (lastResponse && lastResponse->size() >= 2 && (*lastResponse)[1] != 0x00) {
			LOG_ERROR("Command 0x%02X failed with status 0x%02X", command, (*lastResponse)[1]);
			return false;
		}
	}
	
	return true;
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

	// A subscription is required to use Pybricks (the hub is waiting for an active listener)

	bool subscribed = transport.subscribe(commandEvent, [this](const Characteristic&, const uint8_t* data, size_t length) {

		std::string hexData;
		for (size_t i = 0; i < length; ++i) {
			char buffer[4];
			snprintf(buffer, sizeof(buffer), "%02X ", data[i]);
			hexData += buffer;
		}
		LOG_COMMAND("Upload RX: %zu bytes [%s]", length, hexData.c_str());

		std::lock_guard<std::mutex> lock(responseMutex);
		if (waitingForResponse && length > 0) {

			if (data[0] == expectedCommand) {
				lastResponse = std::vector<uint8_t>(data, data + length);
				waitingForResponse = false;
				responseConditionVariable.notify_one();
			}
			else {
				LOG_COMMAND("Ignored event with command=0x%02X while waiting for 0x%02X", data[0], expectedCommand);
			}
		}
		});

	if (!subscribed) {
		LOG_ERROR("Failed to subscribe to Pybricks command/event characteristic");
		return false;
	}

	LOG_INFO("PrinterProtocol upload started (%zu bytes)", script.size());

	// Optional: stop running program first
	(void)sendCommand(protocol::PybricksCommand::StopUserProgram, {}, true);

	if (!waitForProgramStop(30s)) {
		LOG_ERROR("Timeout waiting for program to stop");
		transport.unsubscribe(commandEvent);
		return false;
	}	

	LOG_INFO("Sending WRITE_USER_PROGRAM_META with size = 0");
	std::vector<uint8_t> metaZero = {0, 0, 0, 0};

	if (!sendCommand(protocol::PybricksCommand::WriteUserProgramMeta, metaZero, false)) {
		LOG_ERROR("WRITE_USER_PROGRAM_META (size = 0) failed");
		transport.unsubscribe(commandEvent);
		return false;
	}

	std::this_thread::sleep_for(2000ms);

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

		payload.insert(payload.end(), script.begin() + sent, script.begin() + sent + chunk);

		if (!sendCommand(protocol::PybricksCommand::CommandWriteUserRam, payload, false)) {
			LOG_ERROR("COMMAND_WRITE_USER_RAM failed at offset=%zu", sent);
			transport.unsubscribe(commandEvent);
			return false;
		}

		sent += chunk;
		LOG_INFO("PrinterProtocol upload progress: %zu / %zu", sent, script.size());
	
		std::this_thread::sleep_for(200ms);
	}

	const uint32_t programSize = static_cast<uint32_t>(script.size());
	std::vector<uint8_t> meta = {
		static_cast<uint8_t>(programSize & 0xFF),
		static_cast<uint8_t>((programSize >> 8) & 0xFF),
		static_cast<uint8_t>((programSize >> 16) & 0xFF),
		static_cast<uint8_t>((programSize >> 24) & 0xFF)
	};

	if (!sendCommand(protocol::PybricksCommand::WriteUserProgramMeta, meta, false)) {
		LOG_ERROR("WRITE_USER_PROGRAM_META (final size) failed");
		transport.unsubscribe(commandEvent);
		return false;
	}

	//sendCommand(protocol::PybricksCommand::StartUserProgram, {}, false);

	transport.unsubscribe(commandEvent);
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
	return discover() && sendCommand(protocol::PybricksCommand::StartUserProgram, {}, false);
}

bool PrinterProtocol::stopUserProgram() {
	LOG_COMMAND("PrinterProtocol stopUserProgram");
	return discover() && sendCommand(protocol::PybricksCommand::StopUserProgram, {}, true);
}

bool PrinterProtocol::rebootToUpdateMode() {
	LOG_COMMAND("PrinterProtocol rebootToUpdateMode");
	return discover() && sendCommand(protocol::PybricksCommand::RebootToUpdateMode, {}, true);
}
