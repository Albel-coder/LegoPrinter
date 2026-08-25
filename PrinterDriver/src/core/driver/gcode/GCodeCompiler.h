#pragma once

#include "MotionProgram.h"

#include <cstdint>
#include <sstream>
#include <string>

struct PrinterConfig {
	double stepsPerMmX = 5.0;
	double stepsPerMmY = 5.0;

	// mm/min
	double maxFeedrate = 2000.0;
	double defaultFeedrate = 1000.0;

	// Углы Z-мотора
	double zUpAngle = 90.0;
	double zDownAngle = 0.0;

	// Защита от попытки накопить в hub больше чем способен вместить его XY ring buffer
	uint16_t maxStartThreshold = 900;
	uint16_t startThresholdSafetyMargin = 5; // Сколько сегментов оставляем как небольшой запас
};

class GCodeCompiler {
public:
	explicit GCodeCompiler(const PrinterConfig& printerConfig);

	bool compile(const std::string& filename, MotionProgram& output);

private:
	PrinterConfig config;

	// G90 / G91
	bool absolute = true;

	// Текущие координаты g-code, mm
	double currentX = 0.0;
	double currentY = 0.0;
	double currentZ = 0.0;

	// Текущие координаты уже в шагах
	int32_t currentStepsX = 0;
	int32_t currentStepsY = 0;

	// Текущий feedrate, mm/min
	double feedrate = 1000.0;

	// сколько пакетов XY уже добавлено в MotionProgram::commands
	size_t compiledCommandPacketCount = 0;

private:
	void resetState();
	bool processLine(const std::string& line, MotionProgram& program);
	bool processMovement(const std::string& command, std::istringstream& stream, MotionProgram& program);

	bool addXYSegment(int32_t dx, int32_t dy, uint16_t durationMs, MotionProgram& program);
	bool appendSegmentToPacket(int16_t dx, int16_t dy, uint16_t durationMs, MotionProgram& program);

	bool addZCommand(int16_t angle, MotionProgram& program);
	void finalizeCommands(MotionProgram& program);

	std::string trim(const std::string& value);
	size_t calculateStartThreshold(size_t segmentCount) const;
};
