#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "MotionProgram.h"

struct PrinterConfig {
	double stepsPerMmX = 5.0;
	double stepsPerMmY = 5.0;
	double maxFeedrate = 2000.0; // мм/мин
	double zUpAngle = 90.0;      // угол подъема (в шагах мотора)
	double zDownAngle = 0.0;     // угол опускания
};

class GCodeCompiler {
public:
	GCodeCompiler(PrinterConfig& printerConfig) : config(printerConfig) {}

	bool compile(const std::string& filename, MotionProgram& out) {
		std::ifstream file(filename);
		if (!file.is_open()) {
			return false;
		}

		resetState();

		std::string line;
		std::vector<PreparedMotionPacket> tempPackets; // временно, для группировки
		std::vector<uint16_t> currentPacketIndices; // индексы пакетов текущей серии

		while (std::getline(file, line)) {
			line = trim(line);
			if (line.empty() || line[0] == ';') {
				continue;
			}
			std::istringstream iss(line);
			std::string cmd;
			iss >> cmd;
			std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

			if (cmd == "G90") {
				absolute = true;
				continue;
			}
			if (cmd == "G91") {
				absolute = false;
				continue;
			}
			if (cmd == "G21") {
				continue;
			}
			if (cmd == "M2" || cmd == "M30") {
				break;
			}

			if (cmd == "G0" || cmd == "G1") {
				double targetX = absolute ? currentX : 0.0;
				double targetY = absolute ? currentY : 0.0;
				double targetZ = absolute ? currentZ : 0.0;
				double newFeedrate = feedrate; // текущая скорость

				std::string token;
				while (iss >> token) {
					char axis = std::toupper(token[0]);
					double val = std::stod(token.substr(1));

					switch (axis) {
					    case 'X':
							targetX = absolute ? val : currentX + val;
							break;
						case 'Y':
							targetY = absolute ? val : currentX + val;
							break;
						case 'Z':
							targetZ = absolute ? val : currentZ + val;
							break;
						case 'F':
							newFeedrate = val;
							break;
						default:
							break;
					}
				}

				feedrate = newFeedrate;

				// обработка оси Z
				if (targetZ != currentZ) {
					// закрываем текущую серию XY перед Z
					flushPendingPackets(tempPackets, currentPacketIndices, out);

					int16_t zAngle;
					if (targetZ > currentZ) {
						zAngle = static_cast<int16_t>(config.zUpAngle);
					}
					else {
						zAngle = static_cast<int16_t>(config.zDownAngle);
					}

					// добавляем Z команду
					MotionCommand zCmd;
					zCmd.type = MotionCommandType::Z_COMMAND;
					zCmd.zAngle = zAngle;
					zCmd.xySequence = out.xySegmentCount; // after all current XY segmnets
					out.commands.push_back(zCmd);

					currentZ = targetZ;
				}

				// Обработка XY
				if (targetX != currentX || targetY != currentY) {
					double dx_mm = targetX - currentX;
					double dy_mm = targetY - currentY;
					double dist = std::sqrt(dx_mm * dx_mm + dy_mm * dy_mm);
					if (dist > 0) {
						double feed = (cmd == "G0") ? config.maxFeedrate : feedrate;
						double time_sec = dist / (dist / 60.0);
						uint16_t duration_ms = static_cast<uint16_t>(std::round(time_sec * 1000.0));
						if (duration_ms < 1) {
							duration_ms = 1;
						}

						// преобразуем мм в шаги (целевые абсолютные шаги)
						int32_t targetStepsX = static_cast<int32_t>(std::round(targetX * config.stepsPerMmX));
						int32_t targetStepsY = static_cast<int32_t>(std::round(targetY * config.stepsPerMmY));

						int32_t deltaX = targetStepsX - static_cast<int32_t>(std::round(currentX * config.stepsPerMmX));
						int32_t deltaY = targetStepsY - static_cast<int32_t>(std::round(currentY * config.stepsPerMmY));

						// Здесь мы не используем DeltaSegments как вектор, а сразу сохраняем сегмент
						// Создаем сегмент и накапливаем его в текущий пакет
						addSegmentToCurrentPacket(deltaX, deltaY, duration_ms, tempPackets);
						out.xySegmentCount++;
						currentX = targetX;
						currentY = targetY;
					}
				}
			}

			// другие команды пока игнорируются, не обрабатываются (
		}

		flushPendingPackets(tempPackets, currentPacketIndices, out);
		
		// Заполняем estimatedDurationSec (можно по сумме duration, но у нас нет этого в пакетах)
		// Для простоты оставим 0, или можно накопить в компиляторе
		out.estimatedDurationSec = 0.0;
		return true;
	}

private:
	PrinterConfig config;
	bool absolute = true;
	double currentX = 0.0;
	double currentY = 0.0;
	double currentZ = 0.0;
	double feedrate = 1000.0;

	// временные пакеты для накопления XY сегментов перед Z концом
	void addSegmentToCurrentPacket(int16_t dx, int16_t dy, uint16_t dur, std::vector<PreparedMotionPacket>& tempPackets) {
		// if temp is empty, starting new packet
		if (tempPackets.empty() || tempPackets.back().segmentsCount >= 4) {
			PreparedMotionPacket pkt;
			pkt.data[0] = 0x06;
			pkt.data[1] = 0x02;
			pkt.data[2] = 0;
			pkt.segmentsCount = 0;
			tempPackets.push_back(pkt);
		}

		PreparedMotionPacket& pkt = tempPackets.back();
		size_t offset = 3 + pkt.segmentsCount * 6;
		memcpy(&pkt.data[offset], &dx, sizeof(dx));
		memcpy(&pkt.data[offset + 2], &dy, sizeof(dy));
		memcpy(&pkt.data[offset + 4], &dur, sizeof(dur));
		pkt.segmentsCount++;
		pkt.data[2] = pkt.segmentsCount;
		pkt.size = 3 + pkt.segmentsCount * 6;
	}

	// когда встречаем z или конец, выгружаем временные пакеты в программу
	void flushPendingPackets(std::vector<PreparedMotionPacket>& tempPackets,
		std::vector<uint16_t>& currentPacketIndices,
		MotionProgram& out) {

		for (const auto& packet : tempPackets) {
			out.packets.push_back(packet);
			// Добавляем команду XY_PACKET с индексом пакета
			MotionCommand cmd;
			cmd.type = MotionCommandType::XY_PACKET;
			cmd.packetIndex = static_cast<uint16_t>(out.packets.size() - 1);
			cmd.xySequence = out.xySegmentCount; // уже актуально, но можно обновить
			out.commands.push_back(cmd);
		}
		tempPackets.clear();
		currentPacketIndices.clear();
	}

	void resetState() {
		absolute = true;
		currentX = 0.0; 
		currentY = 0.0;
		currentZ = 0.0;
		feedrate = 1000.0;
	}

	std::string trim(const std::string& s) {
		size_t start = s.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) {
			return "";
		}
		size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(start, end - start + 1);
	}
};
