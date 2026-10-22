#pragma once

#include "../motion/MotionPlanner.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <array>

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

enum class MotionCommandType {
	XY_PACKET,
	Z_COMMAND,
};

struct MotionCommand {
	MotionCommandType type = MotionCommandType::XY_PACKET;
	uint16_t packetIndex = 0;
	int16_t  zAngle = 0;
	uint32_t afterXY = 0;
};

struct MotionProgram {
	std::vector<PreparedMotionPacket> packets;
	std::vector<MotionCommand>        commands;
	size_t   xySegmentCount = 0;
	size_t   startThreshold = 0;
	double   estimatedDurationSec = 0.0;
};

// ============================================================
// Конфигурация станка
// ============================================================

struct PrinterConfig {
	// Кинематика
	double stepsPerMmX = 5.0;
	double stepsPerMmY = 5.0;

	// Подачи из G-кода
	double maxFeedrate = 2000.0; // mm/min — для G0
	double defaultFeedrate = 1000.0; // mm/min — если F не задан

	// Z
	int zUpAngle = 90;
	int zDownAngle = 0;

	// Порог старта и BLE
	size_t maxStartThreshold = 900;
	size_t startThresholdSafetyMargin = 5;

	// ============================================
	// НОВОЕ: физика планировщика
	// ============================================
	double maxAccel = 8000.0; // шаги/с²
	double junctionDeviation = 4.0;    // шагов
	double maxVelocitySteps = 2200.0; // шагов/с
	double junctionMinFactor = 0.0;    // 0 = не занижать
};

// ============================================================
// Компилятор
// ============================================================

class GCodeCompiler {
public:
	explicit GCodeCompiler(const PrinterConfig& printerConfig);

	bool compile(const std::string& filename, MotionProgram& output);

private:
	struct RawParsedSegment {
		struct { int32_t x; int32_t y; } startPoint;
		struct { int32_t x; int32_t y; } endPoint;
		double  requestedVelocity = 0.0;
		bool    isZMove = false;
		int16_t zAngle = 0;
	};

	PrinterConfig config;

	// Состояние парсера
	bool   absolute = true;
	double currentX = 0.0;
	double currentY = 0.0;
	double currentZ = 0.0;
	int32_t currentStepsX = 0;
	int32_t currentStepsY = 0;
	double feedrate = 0.0;

	double feedrateG0 = 24000.0;   // rapid (модальный для G0)
	double feedrateG1 = 1000.0;    // draw (модальный для G1)

	// Счётчик для сборки команд
	size_t compiledCommandPacketCount = 0;

	void resetState();
	static std::string trim(const std::string& value);

	bool addXYSegment(int32_t dx, int32_t dy, uint16_t durationMs,
		MotionProgram& program);
	bool appendSegmentToPacket(int16_t dx, int16_t dy, uint16_t durationMs,
		MotionProgram& program);
	bool addZCommand(int16_t angle, MotionProgram& program);
	void finalizeCommands(MotionProgram& program);
	size_t calculateStartThreshold(size_t segmentCount) const;

	void logPlannerDiagnostics(
		const std::vector<motion::MoveSegment>& moves,
		const std::vector<motion::PlannedSegment>& planned) const;
};
