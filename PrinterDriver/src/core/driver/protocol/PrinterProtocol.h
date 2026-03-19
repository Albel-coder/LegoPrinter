#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "Constants.h"

#include <vector>

class PrinterProtocol {
public:
	explicit PrinterProtocol(ITransport& transportPointer);

	bool discover();

	bool uploadProgram(const std::vector<uint8_t>& script);

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
