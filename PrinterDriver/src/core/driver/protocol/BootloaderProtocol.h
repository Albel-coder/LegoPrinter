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

	std::vector<uint8_t> BootloaderProtocol::makePacket(
		uint8_t subCommand,
		const std::vector<uint8_t>& payload) const;
};
