#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Motion transport protocol

constexpr size_t MAX_BLE_PACKET_SIZE = 32;

constexpr uint8_t MOTION_TRANSPORT_PREFIX = 0x06;
constexpr uint8_t CMD_MOTION_BLOCK = 0x02;
constexpr uint8_t CMD_Z = 0x05;
constexpr uint8_t CMD_SET_START_THRESHOLD = 0x06;

constexpr size_t MOTION_PACKET_HEADER_SIZE = 3;
// [0] = 0x06
// [1] = command
// [2] = segment count

constexpr size_t MOTION_SEGMENT_SIZE = 6;
// int16 dx
// int16 dy
// uint16 duration_ms

constexpr size_t MAX_SEGMENTS_PER_PACKET =
    (MAX_BLE_PACKET_SIZE - MOTION_PACKET_HEADER_SIZE) / MOTION_SEGMENT_SIZE;

static_assert(MAX_SEGMENTS_PER_PACKET == 4,
	"MAX_SEGMENTS_PER_PACKET must match hub-side parser");

static_assert(
	MOTION_PACKET_HEADER_SIZE + MAX_SEGMENTS_PER_PACKET * MOTION_SEGMENT_SIZE
	<= MAX_BLE_PACKET_SIZE,
	"packet layout must fit into MAX_BLE_PACKET_SIZE");

struct PreparedMotionPacket {
	std::array<uint8_t, MAX_BLE_PACKET_SIZE> data{};

	uint8_t size = 0;
	uint8_t segmentsCount = 0;
};

// Command sequence

enum class MotionCommandType : uint8_t {
	XY_PACKET,
	Z_COMMAND,
};

struct MotionCommand {
	MotionCommandType type = MotionCommandType::XY_PACKET;

	// for XY_PACKET
	uint16_t packetIndex = 0;

	// for Z_COMMAND
	int16_t zAngle = 0;
	uint32_t afterXY = 0;  // сколько XY сегментов должно быть выполнено перед этой Z
};

// Fully compiled motion program

struct MotionProgram {
	std::vector<PreparedMotionPacket> packets;
	std::vector<MotionCommand> commands;

	size_t xySegmentCount = 0;
	size_t startThreshold = 0; // вычисляется компилятором
	double estimatedDurationSec = 0.0;
};
