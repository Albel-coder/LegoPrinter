#pragma once

#include "../core/driver/interfaces/ITransport.h"
#include "gcode/MotionProgram.h"
#include "gcode/GCodeCompiler.h"

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <array>

#pragma pack(push, 1)
struct MotionSegmentDelta {
	int16_t dx;
	int16_t dy;
	uint16_t duration_ms;
};
#pragma pack(pop)

static_assert(
	sizeof(MotionSegmentDelta) == 6,
	"MotionSegmentDelta must be 6 bytes"
);

class Controller {
public:
	explicit Controller(ITransport& transportPointer);

	bool connect(const std::string& address);
	void disconnect();

	bool send(const uint8_t* data, size_t length, bool withResponse = false);

	bool sendAngles(int32_t angleX, int32_t angleY);

	bool isConnected() const;

	bool runMotionTest();

	bool sendMotionBlock(const std::vector<MotionSegmentDelta>& segments);

	bool sendPreparedMotionPackets(const std::vector<PreparedMotionPacket>& packets);

	bool sendMotionProgram(const MotionProgram& program);
	bool sendStartThreshold(uint16_t threshold);
	bool sendZCommand(int16_t angle, uint32_t afterXY);

	bool runGCodeTest(const std::string& filename);

private:
	ITransport& transport;

	Characteristic pybricksCommandEvent;
	Characteristic pybricksCapabilities;
	std::string connectedAddress;

	std::atomic<bool> connected{ false };
	std::atomic<bool> subscribed{ false };

	std::atomic<bool> remoteBufferFull{ false };

	bool discover();
	void onData(const uint8_t* data, size_t length);

	template<typename T>
	bool sendCommand(uint8_t axis, uint8_t cmd, const T& payload);

	bool sendLineSegment(int tx, int ty, uint16_t dur);
};
