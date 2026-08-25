#include "GCodeCompiler.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>

namespace {
	constexpr double EPSILON = 1e-9;

	bool nearlyEqual(double a, double b) {
		return std::abs(a - b) < EPSILON;
	}

	bool toUpperCommand(std::string& command) {
		if (command.empty()) {
			return false;
		}

		for (char& character : command) {
			character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
		}
		return true;
	}
} // namespace

GCodeCompiler::GCodeCompiler(const PrinterConfig& printerConfig) : config(printerConfig) {
	resetState();
}

void GCodeCompiler::resetState() {
	absolute = true;

	currentX = 0.0;
	currentY = 0.0;
	currentZ = 0.0;

	currentStepsX = 0;
	currentStepsY = 0;

	feedrate = config.defaultFeedrate;
	compiledCommandPacketCount = 0;
}

std::string GCodeCompiler::trim(const std::string& value) {
	const size_t start = value .find_first_not_of(" \t\r\n");
	if (start == std::string::npos) {
		return {};
	}
	const size_t end = value.find_last_not_of(" \t\r\n");
	return value.substr(start, end - start + 1);
}

bool GCodeCompiler::compile(const std::string& filename, MotionProgram& output) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		return false;
	}

	output = MotionProgram{};
	resetState();

	std::string line;
	while (std::getline(file, line)) {
		if (!processLine(line, output)) {
			return false;
		}
	}

	finalizeCommands(output);
	output.startThreshold = calculateStartThreshold(output.xySegmentCount);

	return true;
}

bool GCodeCompiler::processLine(const std::string& rawLine, MotionProgram& program) {
	std::string line = rawLine;

	// ”дал€ем комментарий
	size_t commentPos = line.find(';');
	if (commentPos != std::string::npos) {
		line.erase(commentPos);
	}

	line = trim(line);
	if (line.empty()) {
		return true;
	}

	std::istringstream stream(line);
	std::string command;
	if (!(stream >> command)) {
		return true;
	}

	toUpperCommand(command);

	if (command == "G90") {
		absolute = true;
		return true;
	}
	if (command == "G91") {
		absolute = false;
		return true;
	}
	if (command == "G21") {
		// millimeters
		return true;
	}
	if (command == "G20") {
		return false; // inches is not available for now
	}
	if (command == "M2" || command == "M30") {
		return true; // end of program
	}
	if (command == "G0" || command == "G1") {
		return processMovement(command, stream, program);
	}
	if (command.size() > 1 && command[0] == 'F') {
		try {
			double value = std::stoi(command.substr(1));
			if (value <= 0) {
				return false;
			}
			return true;
		}
		catch (...) {
			return false;
		}
	}

	// неизвестные команды игнорируютс€
	return true;
}

// G0 / G1 commands
bool GCodeCompiler::processMovement(const std::string& command, std::istringstream& stream, MotionProgram& program) {
	double targetX = absolute ? currentX : 0.0;
	double targetY = absolute ? currentY : 0.0;
	double targetZ = absolute ? currentZ : 0.0;
	double newFeedrate = feedrate;

	std::string token;
	while (stream >> token) {
		if (token.size() < 2) {
			return false;
		}

		char axis = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
		double value = 0.0;

		try {
			value = std::stod(token.substr(1));
		}
		catch (...) {
			return false;
		}

		switch (axis) {
			case 'X':
				targetX = absolute ? value : currentX + value;
				break;
			case 'Y':
				targetY = absolute ? value : currentY + value;
				break;
			case 'Z':
				targetZ = absolute ? value : currentZ + value;
				break;
			case 'F':
				if (value <= 0) {
					return false;
				}

				newFeedrate = value;
				break;
			default:
				return false;
		}
	}

	feedrate = newFeedrate;

	// Z
	if (!nearlyEqual(targetZ, currentZ)) {
		const int16_t zAngle = targetZ > currentZ ? config.zUpAngle : config.zDownAngle;

		if (!addZCommand(zAngle, program)) {
			return false;
		}

		currentZ = targetZ;
	}

	// Z всегда перед XY
	if (!nearlyEqual(targetX, currentX) || !nearlyEqual(targetY, currentY)) {
		double dxMm = targetX - currentX;
		double dyMm = targetY - currentY;
		double distance = std::sqrt(dxMm * dxMm + dyMm * dyMm);
		if (distance <= EPSILON) {
			currentX = targetX;
			currentY = targetY;
			return true;
		}

		double moveFeedrate = (command == "G0") ? config.maxFeedrate : feedrate;
		if (moveFeedrate <= 0) {
			return false;
		}

		// простое врем€ движени€, позже сюда перенесем проверенный velocity planner

		double durationSec = distance / (moveFeedrate / 60.0);
		double durationMsDouble = durationSec * 1000.0;
		if (durationMsDouble < 0.0 || durationMsDouble > static_cast<double>(std::numeric_limits<uint16_t>::max())) {
			return false;
		}
		uint16_t durationMs = static_cast<uint16_t>(std::lround(durationMsDouble));
		if (durationMs < 1) {
			durationMs = 1;
		}

		// convert mm -> steps

		const int32_t targetStepsX = static_cast<int32_t>(std::lround(targetX * config.stepsPerMmX));
		const int32_t targetStepsY = static_cast<int32_t>(std::lround(targetY * config.stepsPerMmY));

		const int64_t deltaX64 = static_cast<int64_t>(targetStepsX) - static_cast<int64_t>(currentStepsX);
		const int64_t deltaY64 = static_cast<int64_t>(targetStepsY) - static_cast<int64_t>(currentStepsY);

		if (deltaX64 < INT16_MIN || deltaX64 > INT16_MAX ||
			deltaY64 < INT16_MIN || deltaY64 > INT16_MAX) {
			return false;
		}

		if (!addXYSegment(
			static_cast<int32_t>(deltaX64), 
			static_cast<int32_t>(deltaY64), 
			durationMs, 
			program)) {
			return false;
		}

		// “олько после успешного добавлени€ измен€ем current state
		currentStepsX = targetStepsX;
		currentStepsY = targetStepsY;

		currentX = targetX;
		currentY = targetY;
	}

	return true;
}

bool GCodeCompiler::addXYSegment(int32_t dx, int32_t dy, uint16_t durationMs, MotionProgram& program) {
	if (dx < INT16_MIN || dx > INT16_MAX ||
		dy < INT16_MIN || dy > INT16_MAX) {
		return false;
	}
	
	if (!appendSegmentToPacket(
		static_cast<int16_t>(dx), 
		static_cast<int16_t>(dy), 
		durationMs, 
		program)) {
		return false;
	}

	program.xySegmentCount++;
	program.estimatedDurationSec += static_cast<double>(durationMs) / 1000.0;
	return true;
}

bool GCodeCompiler::appendSegmentToPacket(int16_t dx, int16_t dy, uint16_t durationMs, MotionProgram& program) {
	if (program.packets.empty() || program.packets.back().segmentsCount >= MAX_BLE_PACKET_SIZE) {
		PreparedMotionPacket packet;
		packet.data[0] = MOTION_TRANSPORT_PREFIX;
		packet.data[1] = CMD_MOTION_BLOCK;
		packet.data[2] = 0;
		packet.size = MOTION_PACKET_HEADER_SIZE;
		packet.segmentsCount = 0;
		program.packets.push_back(packet);
	}

	PreparedMotionPacket& packet = program.packets.back();
	const size_t offset = MOTION_PACKET_HEADER_SIZE + packet.segmentsCount * MOTION_SEGMNET_SIZE;
	std::memcpy(packet.data.data() + offset, &dx, sizeof(dx));
	std::memcpy(packet.data.data() + offset + 2, &dy, sizeof(dy));
	std::memcpy(packet.data.data() + offset + 4, &durationMs, sizeof(durationMs));

	packet.segmentsCount++;
	packet.data[2] = packet.segmentsCount;
	packet.size = static_cast<uint8_t>(MOTION_PACKET_HEADER_SIZE + packet.segmentsCount * MOTION_SEGMNET_SIZE);
	return true;
}

bool GCodeCompiler::addZCommand(int16_t angle, MotionProgram& program) {
	// добавл€ем все нокопленные, но еще не добавл€ем пакеты в commands
	while (compiledCommandPacketCount < program.packets.size()) {
		MotionCommand cmd;
		cmd.type = MotionCommandType::XY_PACKET;
		cmd.packetIndex = static_cast<uint16_t>(compiledCommandPacketCount);
		program.commands.push_back(cmd);
		compiledCommandPacketCount++;
	}

	MotionCommand zCmd;
	zCmd.type = MotionCommandType::Z_COMMAND;
	zCmd.zAngle = angle;
	zCmd.afterXY = static_cast<uint32_t>(program.xySegmentCount);
	program.commands.push_back(zCmd);
	return true;
}

void GCodeCompiler::finalizeCommands(MotionProgram& program) {
	while (compiledCommandPacketCount < program.packets.size()) {
		MotionCommand cmd;
		cmd.type = MotionCommandType::XY_PACKET;
		cmd.packetIndex = static_cast<uint16_t>(compiledCommandPacketCount);
		program.commands.push_back(cmd);
		compiledCommandPacketCount++;
	}
}

size_t GCodeCompiler::calculateStartThreshold(size_t segmentCount) const {
	if (segmentCount == 0) {
		return 0;
	}

	const size_t safetyMargin = config.startThresholdSafetyMargin;
	size_t threshold = (segmentCount > safetyMargin) ? segmentCount - safetyMargin : segmentCount;
	threshold = std::min(threshold, static_cast<size_t>(config.maxStartThreshold));
	return threshold;
}
