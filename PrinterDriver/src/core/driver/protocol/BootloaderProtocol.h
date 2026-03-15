#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "Constants.h"

#include <functional>
#include <vector>

class BootloaderProtocol {
public:
	using ProgressCallback = std::function<void(int percent, const std::string& stage)>;
	using LogCallback = std::function<void(const std::string& message)>;

	explicit BootloaderProtocol(ITransport& transport);

	bool flashFirmware(const std::vector<uint8_t>& firmwareBootloader,
		ProgressCallback progress = nullptr,
		LogCallback log = nullptr);

private:
	ITransport& transport;
	Characteristic bootloaderChar;

	bool discover();
	bool sendCommand(protocol::BootloaderCommand command,
		const std::vector<uint8_t>& payload, std::vector<uint8_t>* response = nullptr);

	std::vector<uint8_t> makePacket(protocol::BootloaderCommand command,
		const std::vector<uint8_t>& payload) const;
};