#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "../logging/LogManager.h"
#include "Constants.h"

#include <vector>

class BootloaderProtocol {
public:
	explicit BootloaderProtocol(ITransport& transportPointer);

	// transport must already be connected to the hub in bootloader / official mode
	bool flashFirmware(const std::vector<uint8_t>& firmwareBootloader);

	void testBootloader() const {
		LOG_INFO("testBootloader called on %p", this);
	}

private:
	ITransport& transport;
	Characteristic bootloaderChar;

	bool discover();
	bool sendCommand(protocol::BootloaderCommand command,
		const std::vector<uint8_t>& payload, std::vector<uint8_t>* response = nullptr, bool withResponse = true);

	std::vector<uint8_t> makePacket(protocol::BootloaderCommand command,
		const std::vector<uint8_t>& payload) const;
};