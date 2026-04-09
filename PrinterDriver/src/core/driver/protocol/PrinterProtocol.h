#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "Constants.h"

#include <mutex>
#include <condition_variable>
#include <optional>
#include <vector>

class PrinterProtocol {
public:
	explicit PrinterProtocol(ITransport& transportPointer);

	bool discover();

	bool uploadProgram(const std::vector<uint8_t>& script);
	bool uploadProgramFromFile(const std::string& filePath);

	bool startUserProgram();
	bool stopUserProgram();
	bool rebootToUpdateMode();

private:
	ITransport& transport;
	Characteristic commandEvent;
	Characteristic capabilities;
	
	std::mutex responseMutex;
	std::condition_variable responseConditionVariable;
	std::optional<std::vector<uint8_t>> lastResponse;
	bool waitingForResponse = false;
	uint8_t expectedCommand = 0;

	bool sendCommand(protocol::PybricksCommand command,
		const std::vector<uint8_t>& payload = {}, bool withResponse = true);
};
