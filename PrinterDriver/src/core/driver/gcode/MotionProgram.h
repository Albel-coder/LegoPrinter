#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

constexpr size_t MAX_BLE_PACKET_SIZE = 32;

constexpr uint8_t MOTION_TRANSPORT_PREFIX = 0x06;
constexpr uint8_t CMD_MOTION_BLOCK = 0x02;
constexpr uint8_t CMD_Z = 0x05;
constexpr uint8_t CMD_SET_START_THRESHOLD = 0x06;

constexpr size_t MOTION_PACKET_HEADER_SIZE = 3;
constexpr size_t MOTION_SEGMNET_SIZE = 6;
constexpr size_t MAX_SEGMENTS_PER_PACKET =
    (MAX_BLE_PACKET_SIZE - MOTION_PACKET_HEADER_SIZE) / MOTION_SEGMNET_SIZE;

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
	MotionCommandType type = MotionCommandType::XY_PACKET;

	// для XY_PACKET
	uint16_t packetIndex = 0;

	// для Z_COMMAND
	int16_t zAngle = 0;
	uint32_t xySequence = 0;  // сколько XY сегментов должно быть выполнено перед этой Z
};

struct MotionProgram {
	std::vector<PreparedMotionPacket> packets;
	std::vector<MotionCommand> commands;

	size_t xySegmentCount = 0;
	size_t startThreshold = 0; // вычисляется компилятором
	double estimatedDurationSec = 0.0;
};
