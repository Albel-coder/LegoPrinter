#pragma once
#include <cstdint>
#include <array>
#include <vector>

constexpr size_t MAX_BLE_PACKET_SIZE = 32;

struct PreparedMotionPacket {
	std::array<uint8_t, MAX_BLE_PACKET_SIZE> data{};
	uint8_t size = 0;
	uint8_t segmentsCount = 0;
};

enum class MotionCommandType : uint8_t {
	XY_PACKET,
	Z_COMMAND,
};

struct MotionCommand {
	MotionCommandType type;
	uint16_t packetIndex = 0; // для XY_PACKET
	int16_t zAngle = 0;       // для Z_COMMAND
	uint32_t xySequence = 0;  // после скольких XY выполнять для Z
};

struct MotionProgram {
	std::vector<PreparedMotionPacket> packets;
	std::vector<MotionCommand> commands;
	size_t xySegmentCount = 0;
	double estimatedDurationSec = 0.0;
};
