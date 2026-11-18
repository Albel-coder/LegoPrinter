#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "gcode/motion/MotionPlanner.h"
#include "gcode/compiler/GCodeCompiler.h"

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <deque>
#include <array>

class Controller {
public:
	explicit Controller(ITransport& transportPointer);

	bool connect(const std::string& address);
	void disconnect();

	bool isConnected() const;

	bool sendMotionProgram(const MotionProgram& program);
	bool sendStartThreshold(uint16_t threshold);
	bool sendZCommand(int16_t angle, uint32_t afterXY);

	bool executeCode(const std::string& filename);

private:
	ITransport& transport;

	Characteristic pybricksCommandEvent;
	Characteristic pybricksCapabilities;
	std::string connectedAddress;

	std::atomic<bool> connected{ false };
	std::atomic<bool> subscribed{ false };

	std::atomic<bool> remoteBufferFull{ false };
	std::atomic<bool> zRemoteFull{ false };

	std::atomic<int64_t> lastHubRxMs{ 0 };
	std::atomic<int64_t> lastFlowBlockMs{ 0 };

	bool discover();
	void onData(const uint8_t* data, size_t length);

	template<typename T>
	bool sendCommand(uint8_t axis, uint8_t cmd, const T& payload);
};
