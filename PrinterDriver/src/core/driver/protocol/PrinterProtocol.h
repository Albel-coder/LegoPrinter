#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "Constants.h"

#include <functional>
#include <vector>

class PrinterProtocol {
public:
	using ProgressCallback = std::function<void(int percent, const std::string& stage)>;
	using LogCallback = std::function<void(const std::string& message)>;

	explicit PrinterProtocol(ITransport& transport);

	bool discover();

	bool uploadProgram(const std::vector<uint8_t>& script,
		ProgressCallback progress = nullptr, LogCallback log = nullptr);

	bool startUserProgram();
	bool stopUserProgram();
	bool rebootToUpdateMode();

private:
	ITransport& transport;
	Characteristic commandEvent;
	Characteristic capabilities;

	bool sendCommand(protocol::PybricksCommand command,
		const std::vector<uint8_t>& payload = {}, bool withResponse = true);
};
