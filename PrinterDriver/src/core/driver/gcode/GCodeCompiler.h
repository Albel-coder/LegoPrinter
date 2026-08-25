#pragma once

#include "MotionProgram.h"
#include <cstdint>
#include <sstream>
#include <string>

struct PrinterConfig {
	double stepsPerMmX = 5.0;
	double stepsPerMmY = 5.0;

	double maxFeedrate = 2000.0; // мм/мин
	double defaultFeedrate = 1000.0;

	double zUpAngle = 90.0;
	double zDownAngle = 0.0;

	uint16_t maxStartThreshold = 900;
	uint16_t startThresholdSafetyMargin = 5;
};

class GCodeCompiler {
public:
	explicit GCodeCompiler(const PrinterConfig& printerConfig);

	bool compile(const std::string& filename, MotionProgram& output);

private:
	PrinterConfig config;

	bool absolute = true;

	double currentX = 0.0;
	double currentY = 0.0;
	double currentZ = 0.0;

	int32_t currentStepsX = 0;
	int32_t currentStepsY = 0;

	double feedrate = 1000.0;

	size_t compiledCommandPacketCount = 0; // сколько пакетов уже добавлено в commands

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
