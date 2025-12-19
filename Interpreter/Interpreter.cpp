#define GCODEINTERPRETER_EXPORTS

#include "Interpreter.h"
#include <map>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdarg>
#include <sstream>
#include <fstream>
#include <future>

const double PI = 3.14159265358979323846;

// Global mutex for thread-safe string access
std::mutex stringCacheMutex;
std::map<int, std::string> stringCache;
int nextStringId = 0;
std::mutex gInterpreterMutex;
std::atomic<int> gActiveInterpreters(0);

// Functions for caching strings
const char* cacheString(const std::string& string) {
	std::lock_guard<std::mutex> lock(stringCacheMutex);
	int id = nextStringId;
	stringCache[id] = string;
	return stringCache[id].c_str();
}

// Function for clearing cache (optional)
void clearStringCache() {
	std::lock_guard<std::mutex> lock(stringCacheMutex);
	stringCache.clear();
	nextStringId = 0;
}

enum GcodeError { // Interpreter error enumeration
	IDENTIFIER_NOT_DEFINED = 0,
	VALUE_NOT_DEFINED = 1,
	OUT_OF_RANGE = 2,
	FILE_ERROR = 3,
	CONFIG_ERROR = 4,
	PRINTER_ERROR = 5,
	SYNTAX_ERROR = 6,
	MOVEMENT_ERROR = 7,
	NO_ERROR = 8
};

enum Status { // Interpreter execution status
	IDLE = 0,
	CHECKING_CODE = 1,
	RUNNING = 2,
	PAUSED = 3,
	COMPLETED = 4,
	ERROR = 5
};

enum class ConfigKey { // Configuration keys
	ROTATE_DISTANCE = 0,
	DISTANCE_TO_PRINT_POSITION = 1,
	GEAR_RATIO = 2,
	DIRECTION = 3,
	PORTS = 4,
	MINIMUM_FEEDRATE = 5,
	MAXIMUM_FEEDRATE = 6,
	UNKNOWN = 7
};

enum class Section { // Configuration sections
	STEPPER_X = 0,
	STEPPER_Y = 1,
	STEPPER_Z = 2,
	UNKNOWN = 3
};

class Interpreter {
private:
	IPrinter* currentPrinter;

	std::atomic<Status> status;
	std::atomic<GcodeError> currentError;
	std::atomic<bool> stopRequested;
	std::atomic<bool> pauseRequested;
	std::atomic<double> progress;
	std::string lastError;
	std::vector<std::string> gCodeErrors;
	std::vector<std::string> executionLog;
	std::atomic<bool> threadRunning;

	// Current coordinates
	double currentX;
	double currentY;
	double currentZ;
	bool absolutePositioning;
	double speed;

	std::unique_ptr<std::thread> executionThread;
	std::mutex mutex;
	std::mutex logMutex;

	struct StepperConfig { // Stepper motor configuration
		double rotationDistance = 0.0;
		double gearRatio;
		bool direction;
		std::vector<uint8_t> ports;
		double minimumFeedrate = 0.0;
		double maximumFeedrate = 0.0;
	};

	StepperConfig stepperX;
	StepperConfig stepperY;
	StepperConfig stepperZ;

	double zDistanceToPrint = 0.0;
		
	struct ArcParameters {
		double centerX;
		double centerY;
		double radius;
		double startAngle;
		double endAngle;
		bool clockwise;
		double startX;
		double startY;
		double endX;
		double endY;
	};

	void addLogEntry(const std::string& entry) {
		std::unique_lock<std::mutex> lock(logMutex, std::try_to_lock);
		if (lock.owns_lock()) {
			// Size limit without recursion
			if (executionLog.size() >= 1000) {
				executionLog.erase(executionLog.begin(), executionLog.begin() + 100);

				static bool cleanupMessageAdded = false;
				if (!cleanupMessageAdded) {
					executionLog.push_back("Log buffer limit reached - old entries are being removed");
					cleanupMessageAdded = true;
				}	
			}

			executionLog.push_back(entry);
		}
	}

	// Add G-code error information
	void addGCodeErrorInfo(const std::string& code, GcodeError errorType = VALUE_NOT_DEFINED) {
		std::unique_lock<std::mutex> lock(logMutex, std::try_to_lock);

		if (gCodeErrors.size() > 999) {
			gCodeErrors.erase(gCodeErrors.begin());
		}

		std::string ErrorInfo;
		currentError = errorType;

		switch (errorType) {
		case IDENTIFIER_NOT_DEFINED:
			ErrorInfo = "Identifier '" + code + "' is not defined";
			break;
		case VALUE_NOT_DEFINED:
			ErrorInfo = "Value '" + code + "' is not defined or invalid";
			break;
		case OUT_OF_RANGE:
			ErrorInfo = "Value '" + code + "' is out of range";
			break;
		case FILE_ERROR:
			ErrorInfo = "File error: " + code;
			break;
		case CONFIG_ERROR:
			ErrorInfo = "Configuration error: " + code;
			break;
		case PRINTER_ERROR:
			ErrorInfo = "Printer error: " + code;
			break;
		case SYNTAX_ERROR:
			ErrorInfo = "Syntax error: " + code;
			break;
		case MOVEMENT_ERROR:
			ErrorInfo = "Movement error: " + code;
			break;
		case NO_ERROR:
			break;
		default:
			ErrorInfo = "Unknown error: " + code;
			break;
		}

		gCodeErrors.push_back(ErrorInfo);
		addLogEntry("[ERROR] " + ErrorInfo);
		lastError = ErrorInfo;
		status = ERROR;

		// Add to the main log without recursion
		if (lock.owns_lock()) {
			if (executionLog.size() > 999) {
				executionLog.erase(executionLog.begin());
			}
			executionLog.push_back("[ERROR] " + ErrorInfo);
		}
	}

	void setError(GcodeError error, const std::string& message) {
		addGCodeErrorInfo(message, error);
	}

	Section stringToSection(const std::string& section) {
		if (section == "stepper_x")	{
			return Section::STEPPER_X;
		}
		if (section == "stepper_y")	{
			return Section::STEPPER_Y;
		}
		if (section == "stepper_z")	{
			return Section::STEPPER_Z;
		}

		addLogEntry("Unknown section in configuration: " + section);
		return Section::UNKNOWN;
	}

	// Convert std::string to configuration key
	ConfigKey stringToKey(const std::string& key) {
		static const std::unordered_map<std::string, ConfigKey> keyMap = {
			{ "rotation_distance", ConfigKey::ROTATE_DISTANCE },
			{ "distance_to_print_position", ConfigKey::DISTANCE_TO_PRINT_POSITION },
			{ "gear_ratio", ConfigKey::GEAR_RATIO },
			{ "direction", ConfigKey::DIRECTION },
			{ "ports", ConfigKey::PORTS },
			{ "minimum_feedrate", ConfigKey::MINIMUM_FEEDRATE },
			{ "maximum_feedrate", ConfigKey::MAXIMUM_FEEDRATE }
		};

		auto it = keyMap.find(key);
		if (it != keyMap.end()) return it->second;

		addLogEntry("Unknown configuration key: " + key);
		return ConfigKey::UNKNOWN;
	}

	double evaluateExpression(const std::string& expression) {
		std::string Processed = expression;
	}

	std::string parseValue(const std::string& value) {
		if (value.find('{') == std::string::npos || value.find('}') == std::string::npos) {
			return value;
		}

		// TODO: Implement expression parsing if needed

		return value;
	}

	void setConfigValue(Section section, ConfigKey key, const std::string& value) {
		StepperConfig* config = nullptr;

		switch (section) {
		case Section::STEPPER_X:
			config = &stepperX;
			break;
		case Section::STEPPER_Y:
			config = &stepperY;
			break;
		case Section::STEPPER_Z:
			config = &stepperZ;
			break;
		default:
			addGCodeErrorInfo("Unknown section in configuration", CONFIG_ERROR);
			return;
		}

		switch (key) {
		case ConfigKey::ROTATE_DISTANCE:
			try	{
				config->rotationDistance = std::stod(parseValue(value));
				addLogEntry("Set rotation_distance: " + std::to_string(config->rotationDistance));
			}
			catch (const std::exception& ex) {				
				addGCodeErrorInfo("Invalid rotation distance value: " + std::to_string(config->rotationDistance), CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::GEAR_RATIO:
			try	{
				config->gearRatio = std::stod(parseValue(value));
				addLogEntry("Set gear_ratio: " + std::to_string(config->gearRatio));
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Invalid gear ratio value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::DIRECTION:
			try	{
				std::string directionString = parseValue(value);
				std::transform(directionString.begin(), directionString.end(), directionString.begin(), ::tolower);

				if (directionString == "clockwise" || directionString == "cw") {
					config->direction = true;
					addLogEntry("Set direction: clockwise (true)");
				}
				else if (directionString == "counterclockwise" || directionString == "ccw") {
					config->direction = false;
					addLogEntry("Set direction: counterclockwise (false)");
				}
				else {
					// Try to convert as number for backward compatibility
					config->direction = std::stoi(directionString) != 0;
					addLogEntry("Set direction: " + std::to_string(config->direction));
				}
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Invalid direction value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;
			
		case ConfigKey::PORTS:
			try	{
				std::vector<uint8_t> ports;
				addLogEntry("Processing ports configuration: " + value);

				std::string processedValue = value;
				processedValue.erase(std::remove(processedValue.begin(), processedValue.end(), ' '), processedValue.end());
				processedValue.erase(std::remove(processedValue.begin(), processedValue.end(), ','), processedValue.end());
				processedValue.erase(std::remove(processedValue.begin(), processedValue.end(), ';'), processedValue.end());

				for (char character : processedValue) {
					uint8_t portValue = 0xFF;
					switch(character) {
					case 'A':
					case 'a':
						portValue = 0x00;
						break;
					case 'B':
					case 'b':
						portValue = 0x01;
						break;
					case 'C':
					case 'c':
						portValue = 0x02;
						break;
					case 'D':
					case 'd':
						portValue = 0x03;
						break;
					default:
						addLogEntry("Warning: unknown port character '" + std::string(1, character) + "'");
						break;
					}

					bool isDuplicate = false;
					for (auto existingPort : ports)	{
						if (existingPort == portValue) {
							isDuplicate = true;
							break;
						}
					}

					if (!isDuplicate) {
						ports.push_back(portValue);
						addLogEntry("Added port " + std::string(1, character));
					}
					else {
						addLogEntry("Duplicate port detected: " + std::string(1, character));
					}
				}				

				config->ports = ports;
				addLogEntry("Ports configuration completed. Total ports: " + std::to_string(config->ports.size()));

				if (config->ports.empty()) {
					addLogEntry("WARNING: No valid ports configured!");
				}
				else {
					std::string portsList = "Configured ports: ";
					for (auto port : config->ports)	{
						portsList += std::to_string(port) + " ";
					}

					addLogEntry(portsList);
				}
			}
			catch (const std::exception& ex)  {
				addGCodeErrorInfo("Invalid ports configuration: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MINIMUM_FEEDRATE:
			try	{
				config->minimumFeedrate = std::stod(parseValue(value));
				addLogEntry("Set miminum_feedrate: " + value);
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Invalid minimum feedrate value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MAXIMUM_FEEDRATE:
			try	{
				config->maximumFeedrate = std::stod(parseValue(value));
				addLogEntry("Set maximum_feedrate: " + value);
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Invalid maximum feedrate value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::UNKNOWN:
			addGCodeErrorInfo("Unknown configuration key", CONFIG_ERROR);
			break;
		}
	}

	// Movement calculation helper
	struct MovementCalculation {
		double revolutions;
		double baseSpeed;
		double time;
	};

	MovementCalculation calculateAxisMovement(const StepperConfig& config, double movement, double defaultSpeed) {
		MovementCalculation result;
		result.revolutions = (std::abs(movement) * config.gearRatio) / config.rotationDistance;

		result.baseSpeed = defaultSpeed;
		if (movement < 0) result.baseSpeed = -result.baseSpeed;
		if (!config.direction) result.baseSpeed = -result.baseSpeed;

		// Apply speed limits
		if (result.baseSpeed > 0) {
			result.baseSpeed = std::min(result.baseSpeed, config.maximumFeedrate);
			result.baseSpeed = std::max(result.baseSpeed, config.minimumFeedrate);
		}
		else {
			result.baseSpeed = std::max(result.baseSpeed, -config.maximumFeedrate);
			result.baseSpeed = std::min(result.baseSpeed, -config.minimumFeedrate);
		}

		result.time = (result.revolutions > 0 && std::abs(result.baseSpeed) > 0)
			? result.revolutions / std::abs(result.baseSpeed)
			: 0.0;

		return result;
	}	

	struct LinearSegment {
		double startX;
		double startY;
		double endX;
		double endY;
		double length;
	};

	// Алгоритм разбиения дуги на линейные сегменты (линейная аппроксимация)
	std::vector<LinearSegment> approximateArcWithLines(
		double centerX, double centerY, double radius,
		double startAngle, double endAngle, bool clockwise,
		int maxSegments = 100, double maxError = 0.01) {

		std::vector<LinearSegment> segments;

		// Корректировка углов для направления
		double totalAngle = endAngle - startAngle;

		// Для полуокружности (180°) totalAngle будет -π для G2 (по часовой)
		// или +π для G3 (против часовой)

		// Определяем количество сегментов на основе угла
		int segmentsCount = static_cast<int>(std::ceil(std::abs(totalAngle) / (PI / 10))); // Каждые 18°
		segmentsCount = std::max(4, std::min(segmentsCount, maxSegments));

		double angleStep = totalAngle / segmentsCount;

		// Начальная точка
		double prevX = centerX + radius * std::cos(startAngle);
		double prevY = centerY + radius * std::sin(startAngle);

		for (int i = 1; i <= segmentsCount; i++) {
			double currentAngle = startAngle + i * angleStep;
			double currentX = centerX + radius * std::cos(currentAngle);
			double currentY = centerY + radius * std::sin(currentAngle);

			LinearSegment segment;
			segment.startX = prevX;
			segment.startY = prevY;
			segment.endX = currentX;
			segment.endY = currentY;

			double dx = currentX - prevX;
			double dy = currentY - prevY;
			segment.length = std::sqrt(dx * dx + dy * dy);

			segments.push_back(segment);

			prevX = currentX;
			prevY = currentY;
		}

		return segments;
	}

	std::vector<SpeedProfile> createSpeedProfile(const StepperConfig& config, const std::vector<SpeedProfilePoint>& movementPoints, int timeoutMs) {
		std::vector<SpeedProfile> profiles;

		if (config.ports.empty() || movementPoints.empty()) {
			addLogEntry("Warning: no ports or movement ports for axis");
			return profiles;
		}

		std::vector<SpeedProfilePoint> pointsWithStop = movementPoints;

		SpeedProfilePoint stopPoint;
		if (!pointsWithStop.empty()) {
			stopPoint.distance = pointsWithStop.back().distance;
		}
		else {
			stopPoint.distance = 0.0;
		}
		stopPoint.speed = 0;
		stopPoint.tolerance = 0.0;

		pointsWithStop.push_back(stopPoint);
		addLogEntry("Added stop point at distance " + std::to_string(stopPoint.distance));

		if (config.ports.size() == 1) {
			SpeedProfile profile;
			profile.port = config.ports[0];
			profile.count = static_cast<int>(pointsWithStop.size());
			profile.timeoutMs = timeoutMs;

			profile.points = new SpeedProfilePoint[pointsWithStop.size()];
			std::copy(pointsWithStop.begin(), pointsWithStop.end(), profile.points);

			profiles.push_back(profile);
			addLogEntry("Created single speed profile with stop for port " + std::to_string(profile.port));
		}
		else {
			for (uint8_t port : config.ports) {
				SpeedProfile profile;
				profile.port = port;
				profile.count = static_cast<int>(pointsWithStop.size());
				profile.timeoutMs = timeoutMs;

				profile.points = new SpeedProfilePoint[pointsWithStop.size()];
				std::copy(pointsWithStop.begin(), pointsWithStop.end(), profile.points);

				profiles.push_back(profile);
				addLogEntry("Created speed profile with stop for port " + std::to_string(port));
			}

			addLogEntry("Created " + std::to_string(profiles.size()) + " speed profiles with stop");
		}

		return profiles;
	}

	void cleanupSpeedProfiles(std::vector<SpeedProfile>& profiles) {
		for (auto& profile : profiles) {
			if (profile.points) {
				delete[] profile.points;
				profile.points = nullptr;
			}
		}
		profiles.clear();
	}

	std::vector<MotorCommand> generateMotorCommands(const StepperConfig& config, double synchronizedSpeed, double revolutions) {
		std::vector<MotorCommand> commands;
		for (uint8_t port : config.ports) {
			MotorCommand command;
			command.port = port;
			command.speed = static_cast<signed char>(synchronizedSpeed);
			command.revolutions = revolutions;
			commands.push_back(command);
		}

		return commands;
	}

	struct ArcSegment {
		double x;
		double y;
		double time;
	};

	// Исправленный метод calculateArcParameters
	ArcParameters calculateArcParameters(double endX, double endY, double i, double j, double r, bool clockwise) {
		ArcParameters arc;
		arc.startX = currentX;
		arc.startY = currentY;
		arc.endX = absolutePositioning ? endX : currentX + endX;
		arc.endY = absolutePositioning ? endY : currentY + endY;
		arc.clockwise = clockwise;

		// Проверка движения
		if (abs(arc.endX - arc.startX) < 0.001 && abs(arc.endY - arc.startY) < 0.001) {
			addLogEntry("Warning: No arc movement - start and end points are the same");
			arc.radius = 0;
			return arc;
		}

		if (r > 0) {
			// Расчет через радиус
			double dx = arc.endX - arc.startX;
			double dy = arc.endY - arc.startY;
			double chordLength = std::sqrt(dx * dx + dy * dy);

			if (chordLength == 0) {
				addGCodeErrorInfo("Chord length is zero", MOVEMENT_ERROR);
				arc.radius = 0;
				return arc;
			}

			if (chordLength > 2 * r) {
				addGCodeErrorInfo("Radius too small. Radius: " +
					std::to_string(r) + ", Chord: " + std::to_string(chordLength), MOVEMENT_ERROR);
				arc.radius = 0;
				return arc;
			}

			double chordHalf = chordLength / 2.0;
			double h = std::sqrt(r * r - chordHalf * chordHalf);

			// Перпендикуляр к хорде
			double dxPerp = -dy / chordLength;
			double dyPerp = dx / chordLength;

			// Середина хорды
			double midX = (arc.startX + arc.endX) / 2.0;
			double midY = (arc.startY + arc.endY) / 2.0;

			if (clockwise) {
				arc.centerX = midX + h * dxPerp;
				arc.centerY = midY + h * dyPerp;
			}
			else {
				arc.centerX = midX - h * dxPerp;
				arc.centerY = midY - h * dyPerp;
			}

			arc.radius = r;
		}
		else {
			// Расчет через смещения I, J
			arc.centerX = arc.startX + i;
			arc.centerY = arc.startY + j;
			arc.radius = std::sqrt(i * i + j * j);

			if (arc.radius < 0.001) {
				addGCodeErrorInfo("Arc radius too small", MOVEMENT_ERROR);
				return arc;
			}
		}

		// Расчет углов
		arc.startAngle = std::atan2(arc.startY - arc.centerY, arc.startX - arc.centerX);
		arc.endAngle = std::atan2(arc.endY - arc.centerY, arc.endX - arc.centerX);

		// Коррекция углов для направления
		if (clockwise) {
			if (arc.endAngle > arc.startAngle) {
				arc.endAngle -= 2 * PI;
			}
		}
		else {
			if (arc.endAngle < arc.startAngle) {
				arc.endAngle += 2 * PI;
			}
		}

		debugArcCalculation(arc, speed);

		return arc;
	}

	// Helper function for generating arc points (slightly simplified)
	std::vector<SpeedProfilePoint> generateArcPointsLinearApproximation(
		const StepperConfig& config,
		const ArcParameters& arc,
		double feedrate) {

		std::vector<SpeedProfilePoint> points;

		if (arc.radius <= 0.001) {
			return points;
		}

		// Если feedrate не задан, используем максимальный из осей X/Y
		if (feedrate <= 0) {
			feedrate = std::max(stepperX.maximumFeedrate, stepperY.maximumFeedrate);
			addLogEntry("Warning: Using default feedrate: " + std::to_string(feedrate) + " mm/min");
		}

		// Определяем, какая это ось (X или Y)
		bool isXAxis = (config.ports[0] == stepperX.ports[0]);

		// Начальная позиция в миллиметрах для этой оси
		double startPosMm = isXAxis ? arc.startX : arc.startY;

		// Разбиваем дугу на сегменты (увеличим количество сегментов для гладкости)
		std::vector<LinearSegment> segments = approximateArcWithLines(
			arc.centerX, arc.centerY, arc.radius,
			arc.startAngle, arc.endAngle, arc.clockwise,
			50, 0.01); // 50 сегментов, погрешность 0.01 мм

		if (segments.empty()) {
			return points;
		}

		// Вычисляем общую длину пути
		double totalPathLength = 0.0;
		for (const auto& segment : segments) {
			totalPathLength += segment.length;
		}

		// Общее время прохождения дуги (секунды)
		double totalTime = totalPathLength / (feedrate / 60.0);

		if (totalTime <= 0) {
			return points;
		}

		// Конвертация максимальной скорости из мм/мин в оборота/сек
		double maxSpeedRevPerSec = (config.maximumFeedrate * config.gearRatio) /
			(config.rotationDistance * 60.0);

		if (maxSpeedRevPerSec <= 0) {
			addLogEntry("Error: maxSpeedRevPerSec is zero or negative");
			return points;
		}

		// Начальная точка с нулевой скоростью
		SpeedProfilePoint startPoint;
		startPoint.distance = 0.0;
		startPoint.speed = 0;
		startPoint.tolerance = 0.1;
		points.push_back(startPoint);

		// Накопленное расстояние в оборотах
		double accumulatedRevolutions = 0.0;

		// Пропускаем первую точку (она уже добавлена)
		for (size_t i = 0; i < segments.size(); i++) {
			const auto& segment = segments[i];

			// Определяем координату для текущей оси
			double axisPos = isXAxis ? segment.endX : segment.endY;

			// Смещение от начальной точки (мм)
			double linearDisplacement = axisPos - startPosMm;

			// Конвертация в обороты шаговика (АБСОЛЮТНОЕ значение, всегда положительное)
			double revolutions = (std::abs(linearDisplacement) * config.gearRatio) / config.rotationDistance;

			// Направление движения (знак смещения)
			double direction = (linearDisplacement >= 0) ? 1.0 : -1.0;

			// Время прохождения этого сегмента
			double segmentTime = segment.length / (feedrate / 60.0);

			if (segmentTime <= 0) {
				continue; // Пропускаем нулевые сегменты
			}

			// Скорость для этого сегмента (мм/сек)
			double segmentSpeedMmPerSec = (axisPos - (isXAxis ? segment.startX : segment.startY)) / segmentTime;

			// Конвертация в обороты/сек
			double segmentSpeedRevPerSec = (std::abs(segmentSpeedMmPerSec) * config.gearRatio) / config.rotationDistance;

			// Процент от максимальной скорости с учетом направления
			double speedPercent = (segmentSpeedRevPerSec / maxSpeedRevPerSec) * 100.0 * direction;

			// Учет направления мотора из конфигурации
			if (!config.direction) {
				speedPercent = -speedPercent;
			}

			// Ограничение скоростей
			double minSpeedPercent = (config.minimumFeedrate / config.maximumFeedrate) * 100.0;

			if (speedPercent > 0) {
				speedPercent = std::max(speedPercent, minSpeedPercent);
				speedPercent = std::min(speedPercent, 100.0);
			}
			else {
				speedPercent = std::min(speedPercent, -minSpeedPercent);
				speedPercent = std::max(speedPercent, -100.0);
			}

			// Абсолютное расстояние в оборотах (никогда не отрицательное!)
			accumulatedRevolutions = revolutions;

			SpeedProfilePoint point;
			point.distance = accumulatedRevolutions; // АБСОЛЮТНОЕ расстояние от начала
			point.speed = static_cast<signed char>(std::round(speedPercent));
			point.tolerance = 0.1;

			points.push_back(point);
		}

		// Добавляем финальную точку с нулевой скоростью
		if (!points.empty()) {
			SpeedProfilePoint stopPoint;
			stopPoint.distance = points.back().distance;
			stopPoint.speed = 0;
			stopPoint.tolerance = 1.0;
			points.push_back(stopPoint);
		}

		// Отладка
		if (isXAxis) {
			addLogEntry("Generated " + std::to_string(points.size()) +
				" points for X axis, total revolutions: " +
				std::to_string(points.back().distance));
		}
		else {
			addLogEntry("Generated " + std::to_string(points.size()) +
				" points for Y axis, total revolutions: " +
				std::to_string(points.back().distance));
		}

		return points;
	}

	bool validateArc(const ArcParameters& arc) {
		if (arc.radius < 0.1) {
			addGCodeErrorInfo("Invalid arc radius: " + std::to_string(arc.radius), MOVEMENT_ERROR);
			return false;
		}

		// Проверка, что конечная точка лежит на окружности
		double distanceToEnd = std::sqrt(
			(arc.endX - arc.centerX) * (arc.endX - arc.centerX) +
			(arc.endY - arc.centerY) * (arc.endY - arc.centerY)
		);

		if (std::abs(distanceToEnd - arc.radius) > 0.1) {
			addGCodeErrorInfo("End point is not on the arc circle. Radius: " +
				std::to_string(arc.radius) +
				", Distance to end: " + std::to_string(distanceToEnd),
				MOVEMENT_ERROR);
			return false;
		}

		return true;
	}

	bool generateArcSpeedProfile(const ArcParameters& arc, int steps, SpeedProfile& profileX, SpeedProfile& profileY) {
		if (!currentPrinter || !currentPrinter->vtable) {
			return false;
		}

		double angleStep = (arc.endAngle - arc.startAngle) / steps;

		profileX.port = stepperX.ports[0];
		profileX.count = steps;
		profileX.timeoutMs = 10000000;
		profileX.points = new SpeedProfilePoint[steps];

		profileY.port = stepperY.ports[0];
		profileY.count = steps;
		profileY.timeoutMs = 10000000;
		profileY.points = new SpeedProfilePoint[steps];

		for (int i = 0; i < steps; i++) {
			double angle = arc.startAngle + i * angleStep;
			double x = arc.centerX + arc.radius * std::cos(angle);
			double y = arc.centerY + arc.radius * std::sin(angle);

			double xDistance = x - arc.startX;
			double yDistance = y - arc.startY;

			double xRevolutions = (std::abs(xDistance) * stepperX.gearRatio) / stepperX.rotationDistance;
			double yRevolutions = (std::abs(yDistance) * stepperY.gearRatio) / stepperY.rotationDistance;

			double segmentLength = arc.radius * std::abs(angleStep);
			double timePerSegment = segmentLength / speed;

			double xSpeed = (xDistance > 0 ? 1 : -1) * xRevolutions / timePerSegment;
			double ySpeed = (yDistance > 0 ? 1 : -1) * yRevolutions / timePerSegment;

			if (!stepperX.direction) xSpeed = -xSpeed;
			if (!stepperY.direction) ySpeed = -ySpeed;

			xSpeed = std::max(std::min(xSpeed, stepperX.maximumFeedrate), -stepperX.maximumFeedrate);
			ySpeed = std::max(std::min(ySpeed, stepperY.maximumFeedrate), -stepperY.maximumFeedrate);

			profileX.points[i].distance = xRevolutions;
			profileX.points[i].speed = static_cast<signed char>(xSpeed * 100);
			profileX.points[i].tolerance = 0.01;

			profileY.points[i].distance = yRevolutions;
			profileY.points[i].speed = static_cast<signed char>(ySpeed * 100);
			profileY.points[i].tolerance = 0.01;
		}

		return true;
	}

	// Улучшенный метод executeArcMovement с линейной аппроксимацией
	void executeArcMovement(const ArcParameters& arc, double feedrate) {
		if (!currentPrinter || !currentPrinter->vtable) {
			return;
		}

		if (!validateArc(arc)) {
			return;
		}

		// Автоматическое определение количества сегментов
		double arcLength = arc.radius * std::abs(arc.endAngle - arc.startAngle);
		int segments = static_cast<int>(std::ceil(arcLength / 1.0)); // 1 мм на сегмент
		segments = std::max(10, std::min(segments, 200));

		// Генерируем точки с помощью линейной аппроксимации
		std::vector<SpeedProfilePoint> pointsX = generateArcPointsLinearApproximation(
			stepperX, arc, feedrate);
		std::vector<SpeedProfilePoint> pointsY = generateArcPointsLinearApproximation(
			stepperY, arc, feedrate);

		if (pointsX.size() != pointsY.size() || pointsX.size() < 3) {
			addGCodeErrorInfo("Failed to generate arc points with linear approximation", MOVEMENT_ERROR);
			return;
		}

		// Отладка
		debugArcPoints(pointsX, "X (Linear)");
		debugArcPoints(pointsY, "Y (Linear)");

		// Создаем профили для всех портов
		std::vector<SpeedProfile> allProfiles;

		// Для оси X
		for (uint8_t port : stepperX.ports) {
			SpeedProfile profile;
			profile.port = port;
			profile.count = static_cast<int>(pointsX.size());
			profile.timeoutMs = static_cast<int>((arcLength / (feedrate / 60.0)) * 1000) + 1000;

			profile.points = new SpeedProfilePoint[pointsX.size()];
			std::copy(pointsX.begin(), pointsX.end(), profile.points);

			allProfiles.push_back(profile);
		}

		// Для оси Y
		for (uint8_t port : stepperY.ports) {
			SpeedProfile profile;
			profile.port = port;
			profile.count = static_cast<int>(pointsY.size());
			profile.timeoutMs = static_cast<int>((arcLength / (feedrate / 60.0)) * 1000) + 1000;

			profile.points = new SpeedProfilePoint[pointsY.size()];
			std::copy(pointsY.begin(), pointsY.end(), profile.points);

			allProfiles.push_back(profile);
		}

		// Выполняем все профили
		bool success = true;
		for (const auto& profile : allProfiles) {
			if (!currentPrinter->vtable->printer_printer_execute_speed_profile(currentPrinter, &profile)) {
				success = false;
				break;
			}
		}

		// Очистка
		for (auto& profile : allProfiles) {
			delete[] profile.points;
		}

		if (success) {
			// Обновляем текущую позицию
			currentX = arc.endX;
			currentY = arc.endY;
			addLogEntry("Arc movement completed with linear approximation");
		}
	}

	void stopMotorsAfterProfile(const StepperConfig& config) {
		if (!currentPrinter || !currentPrinter->vtable) {
			return;
		}

		for (uint8_t port : config.ports) {
			currentPrinter->vtable->printer_set_motor_speed(currentPrinter, port, 0);
			addLogEntry("Stopped motor on port " + std::to_string(static_cast<int>(port)));
		}
	}

	void debugArcPoints(const std::vector<SpeedProfilePoint>& points, const std::string& axisName) {
		double totalDistance = 0.0;
		double totalTime = 0.0;

		for (size_t i = 0; i < points.size(); i++) {
			totalDistance += points[i].distance;
			if (points[i].speed != 0) {
				totalTime += points[i].distance / (std::abs(points[i].speed) / 100.0);
			}
			addLogEntry(axisName + " Point " + std::to_string(i) +
				": dist=" + std::to_string(points[i].distance) +
				", speed=" + std::to_string(static_cast<int>(points[i].speed)));
		}

		addLogEntry(axisName + " Summary: total dist=" + std::to_string(totalDistance) +
			", est time=" + std::to_string(totalTime) + "s");
	}

	void debugArcCalculation(const ArcParameters& arc, double feedrate) {
		addLogEntry("=== DEBUG ARC CALCULATION ===");
		addLogEntry("Start: (" + std::to_string(arc.startX) + ", " + std::to_string(arc.startY) + ")");
		addLogEntry("End: (" + std::to_string(arc.endX) + ", " + std::to_string(arc.endY) + ")");
		addLogEntry("Center: (" + std::to_string(arc.centerX) + ", " + std::to_string(arc.centerY) + ")");
		addLogEntry("Radius: " + std::to_string(arc.radius));
		addLogEntry("Start angle: " + std::to_string(arc.startAngle * 180.0 / PI) + " deg");
		addLogEntry("End angle: " + std::to_string(arc.endAngle * 180.0 / PI) + " deg");
		addLogEntry("Total angle: " + std::to_string((arc.endAngle - arc.startAngle) * 180.0 / PI) + " deg");
		addLogEntry("Feedrate: " + std::to_string(feedrate) + " mm/min");

		// Расчет примерной скорости
		double arcLength = arc.radius * std::abs(arc.endAngle - arc.startAngle);
		addLogEntry("Arc length: " + std::to_string(arcLength) + " mm");
		addLogEntry("Estimated time: " + std::to_string(arcLength / (feedrate / 60.0)) + " sec");
	}


public:	
	Interpreter() {
		try	{
			std::lock_guard<std::mutex> lock(gInterpreterMutex);
			gActiveInterpreters++;
			threadRunning = false;
			status = IDLE;
			currentError = NO_ERROR;
			stopRequested = false;
			progress = 0.0;
			currentX = 0.0;
			currentY = 0.0;
			currentZ = 0.0;
			absolutePositioning = true;
			speed = 0.0;
			currentPrinter = nullptr;
			executionThread = nullptr;

			stepperX = StepperConfig();
			stepperY = StepperConfig();
			stepperZ = StepperConfig();

			addLogEntry("Interpreter initialized successfully");
		}
		catch (const std::exception& ex) {
			addLogEntry("Error in interpreter constructor: " + std::string(ex.what()));
		}
	}

	~Interpreter() {
		std::lock_guard<std::mutex> lock(gInterpreterMutex);
		stopRequested = true;
		threadRunning = false;

		if (executionThread && executionThread->joinable())	{
			// Wait for thread to finish with timeout
			for (int i = 0; i < 50; i++) { // Increased timeout to 5 seconds
				if (!threadRunning) break;

				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			// Use detach instead of join to avoid blocking
			if (executionThread->joinable()) executionThread->detach();
		}

		addLogEntry("Interpreter destroyed");
		gActiveInterpreters--;
	}

	// Execute G-code from file
	bool executeFile(const char* filename, IPrinter* printer) {
		std::lock_guard<std::mutex> lock(mutex);
		addLogEntry("=== ExecuteFile called ===");
		addLogEntry("Current status: " + std::to_string(static_cast<int>(status)));
		addLogEntry("Printer valid: " + std::string(printer && printer->vtable ? "YES" : "NO"));

		if (!filename) {
			addLogEntry("Filename: NULL");
			return false;
		}

		if (strlen(filename) == 0) {
			addLogEntry("Filename: EMPTY STRING");
			return false;
		}

		if (threadRunning) {
			addGCodeErrorInfo("Interpreter is already executing", PRINTER_ERROR);
			return false;
		}

		// Clean up previous thread
		if (executionThread && executionThread->joinable()) executionThread->detach();

		addLogEntry("Filename: " + std::string(filename));
		addLogEntry("Filename length: " + std::to_string(strlen(filename)));

		if (status == RUNNING) {
			addGCodeErrorInfo("Interpreter ia already running", PRINTER_ERROR);
			return false;
		}

		if (!printer || !printer->vtable) {
			addGCodeErrorInfo("Invalid printer instance", PRINTER_ERROR);
			return false;
		}

		// Check if file exists
		std::ifstream testFile(filename);
		if (!testFile.is_open()) {
			addGCodeErrorInfo("File does not exist or cannot be opened: " + std::string(filename), FILE_ERROR);
		}
		testFile.close();

		currentPrinter = printer;
		status = RUNNING;
		stopRequested = false;
		pauseRequested = false;
		threadRunning = true;
		progress = 0.0;

		// Reset coordinates
		currentX = 0.0;
		currentY = 0.0;
		currentZ = 0.0;
		absolutePositioning = true;
		speed = 0.0;

		addLogEntry("Starting execution thread...");
		addLogEntry("Reset coordinates to X = 0; Y = 0; Z = 0");

		std::string filenameCopy = filename;
		executionThread = std::make_unique<std::thread>([this, filenameCopy]() {
				addLogEntry("Execution thread started");
				addLogEntry("Thread filename: " + filenameCopy);
				runFile(filenameCopy);
				addLogEntry("Execution thread finished");
				threadRunning = false;
			});

		addLogEntry("Execution started: " + std::string(filename));
		return true;
	}

	void pause() // Pause execution
	{
		if (status == RUNNING) {
			pauseRequested = true;
			status = PAUSED;
			addLogEntry("Execution paused");
		}
		else {
			addLogEntry("Pause request ignored - interpreter not running");
		}
	}

	void resume() { // Resume execution
		if (status == PAUSED) {
			pauseRequested = false;
			status = RUNNING;
			addLogEntry("Execution resumed");
		}
		else {
			addLogEntry("Resume request ignored - interpreter not paused");
		}
	}

	void stop()	{ // Stop execution
		addLogEntry("Stop requested");
		stopRequested = true;
		if (status == RUNNING || status == PAUSED || status == CHECKING_CODE) {
			status = IDLE;
			addLogEntry("Execution stopped by user request");
		}

		if (executionThread->joinable()) {
			executionThread->join();
			addLogEntry("Execution thread joined");
		}
	}

	Status getStatus() { return status; }

	double getProgress() { return progress; }

	const char* getLastError() {
		std::lock_guard<std::mutex> lock(logMutex);
		return cacheString(lastError);
	}

	int getLastErrorCode() { return static_cast<int>(currentError.load()); }

	double getSpeed() { return speed; }

	int getErrorCount() {
		std::lock_guard<std::mutex> lock(logMutex);
		return static_cast<int>(gCodeErrors.size());
	}

	const char* GetError(int index) {
		std::lock_guard<std::mutex> lock(logMutex);
		if (index >= 0 && index < gCodeErrors.size()) {
			return cacheString(gCodeErrors[index]);
		}

		return cacheString("");
	}

	int getLogCount() {
		std::lock_guard<std::mutex> lock(logMutex);
		return static_cast<int>(executionLog.size());
	}

	const char* GetLogEntry(int index) {
		std::lock_guard<std::mutex> lock(logMutex);
		if (index >= 0 && index < executionLog.size()) {
			return cacheString(executionLog[index]);
		}

		addLogEntry("Invalid log index requested: " + std::to_string(index));
		return cacheString("");
	}

	void clearErrors() {
		std::lock_guard<std::mutex> lock(logMutex);
		gCodeErrors.clear();
		lastError.clear();
		currentError = NO_ERROR;
		addLogEntry("All errors cleared");
	}

	void clearLog()	{
		std::lock_guard<std::mutex> lock(logMutex);
		executionLog.clear();
		addLogEntry("Log cleared");
	}

	bool testFunction(IPrinter* printer) {
		currentPrinter = printer;
		addLogEntry("Test function started");

		if (!currentPrinter || !currentPrinter->vtable)	{
			addGCodeErrorInfo("Printer is not available for test", PRINTER_ERROR);
			return false;
		}

		SpeedProfilePoint points[] = {
		{3.0, 20, 0.0005},
		{3.0, 30, 0.0005},
		{0.0, 0, 1.0}
		};

		std::vector<SpeedProfile> profile1 = {
			{0x01, points, 3, 30000},
		};

		std::vector<SpeedProfile> profile2 = {
			{0x01, points, 3, 30000}
		};

		currentPrinter->vtable->printer_printer_execute_speed_profile(currentPrinter, profile1.data());
		currentPrinter->vtable->printer_printer_execute_speed_profile(currentPrinter, profile2.data());

		addLogEntry("Multi profile test completed");
		return true;
	}

	bool readConfigFile(const std::string& filename) {
		addLogEntry("Reading interpreter config from: " + filename);

		try	{
			std::ifstream file(filename);
			if (!file.is_open()) {
				addGCodeErrorInfo("Cannot open file: " + filename, FILE_ERROR);
				return false;
			}

			std::string line;
			Section currentSection = Section::UNKNOWN;
			int lineNumber = 0;
			int processedSections = 0;

			while (std::getline(file, line)) {
				lineNumber++;
				if (stopRequested) {
					addLogEntry("Config reading interrupted by stop request");
					break;
				}

				// Remove comments and trim
				size_t commentPosition = line.find('#');
				if (commentPosition != std::string::npos) {
					line = line.substr(0, commentPosition);
				}

				// Trim whitespace
				line.erase(0, line.find_first_not_of(" \t"));
				line.erase(line.find_last_not_of(" \t") + 1);

				if (line.empty()) continue;

				// Process sections [section]
				if (line.front() == '[' && line.back() == ']')
				{
					std::string sectionName = line.substr(1, line.length() - 2);
					currentSection = stringToSection(sectionName);

					if (currentSection != Section::UNKNOWN) {
						processedSections++;
						addLogEntry("Found interpreter section: " + sectionName);
					}
					else {
						// Ignore non-interpreter sections
						addLogEntry("Ignoring non-interpreter section: " + sectionName);
					}
					continue;
				}

				// Process only lines in interpreter sections
				if (currentSection == Section::UNKNOWN) continue;

				// Process key=value pairs
				size_t delimiterPosition = line.find('=');
				if (delimiterPosition == std::string::npos)	{
					addLogEntry("Invalid config line in interpreter section: " + line);
					continue;
				}
				else {
					std::string key = line.substr(0, delimiterPosition);
					std::string value = line.substr(delimiterPosition + 1);

					// Trim whitespace around key and value
					key.erase(0, key.find_first_not_of(" \t"));
					key.erase(key.find_last_not_of(" \t") + 1);
					value.erase(0, value.find_first_not_of(" \t"));
					value.erase(value.find_last_not_of(" \t") + 1);

					ConfigKey configKey = stringToKey(key);
					if (configKey == ConfigKey::UNKNOWN) {
						addLogEntry("Unknown interpreter config key: " + key);
					}
					else {
						setConfigValue(currentSection, configKey, value);

						// Check status after setting value
						if (status == ERROR) {
							addLogEntry("Error setting config value for key: " + key);
							file.close();
							return false;
						}
					}
				}
			}
			
			file.close();

			// Validate required settings
			if (!validateConfig()) {
				addGCodeErrorInfo("Configuration validation failed", CONFIG_ERROR);
			}

			if (status != ERROR) {
				addLogEntry("Interpreter config loaded - processed " +
				std::to_string(processedSections) + " sections");
				return true;
			}

			return false;
		}
		catch (const std::exception& ex) {
			addLogEntry("Error with read: " + filename + " config file: " + ex.what());
			lastError = ex.what();
			status = ERROR;
		}
	}	

	bool executeLine(const std::string& line, IPrinter* printer) {
		addLogEntry("ExecuteLine started: " + line);

		if (status == RUNNING) {
			addGCodeErrorInfo("Interpreter ia already running", PRINTER_ERROR);
			return false;
		}

		if (!printer || !printer->vtable) {
			addGCodeErrorInfo("Invalid printer instance", PRINTER_ERROR);
			return false;
		}
		
		currentPrinter = printer;

		if (!currentPrinter || !currentPrinter->vtable) {
			addGCodeErrorInfo("Printer is not available for execution", PRINTER_ERROR);
			status = ERROR;
			return false;
		}

		if (threadRunning) {
			addGCodeErrorInfo("Interpreter is already executing", PRINTER_ERROR);
			return false;
		}

		// Clean up previous thread
		if (executionThread && executionThread->joinable()) executionThread->detach();;

		try	{
			// Try interpret single line to find errors
			status = CHECKING_CODE;
			bool hasErrors = false;
			processLine(line, 1, true);

			if (status == ERROR) {
				addLogEntry("Execution aborted due to errors");
				threadRunning = false;
				return false;
			}

			// Second pass: execution
			status = RUNNING;
			processLine(line, 1, false);

			if (!stopRequested)	{
				addLogEntry("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else {
				addLogEntry("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex) {
			addGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			lastError = ex.what();
			status = ERROR;
		}

		threadRunning = false;
		addLogEntry("Line executed successfully!");
		return true;
	}

private:

	bool validateConfig() { // Validate configuration
		if (stepperX.ports.empty())	{
			addGCodeErrorInfo("Stepper X has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (stepperX.rotationDistance <= 0) {
			addGCodeErrorInfo("Stepper X rotation distance not set", CONFIG_ERROR);
			return false;
		}

		if (stepperY.ports.empty()) {
			addGCodeErrorInfo("Stepper Y has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (stepperY.rotationDistance <= 0) {
			addGCodeErrorInfo("Stepper Y rotation distance not set", CONFIG_ERROR);
			return false;
		}

		if (stepperZ.ports.empty()) {
			addGCodeErrorInfo("Stepper Z has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (stepperZ.rotationDistance <= 0) {
			addGCodeErrorInfo("Stepper Z rotation distance not set", CONFIG_ERROR);
			return false;
		}

		// Validate speed ranges
		if (stepperX.minimumFeedrate >= stepperX.maximumFeedrate) {
			addGCodeErrorInfo("Stepper X feedrate range invalid", CONFIG_ERROR);
			return false;
		}
		if (stepperY.minimumFeedrate >= stepperY.maximumFeedrate) {
			addGCodeErrorInfo("Stepper Y feedrate range invalid", CONFIG_ERROR);
			return false;
		}
		if (stepperZ.minimumFeedrate >= stepperZ.maximumFeedrate) {
			addGCodeErrorInfo("Stepper Z feedrate range invalid", CONFIG_ERROR);
			return false;
		}

		return true;
	}

	// Execute G-code file
	void runFile(const std::string& filename) {
		try	{
			runFileInternal(filename);
		}
		catch (const std::exception& ex) {
			addLogEntry("CRITICAL ERROR in RunFile: " + std::string(ex.what()));
			status = ERROR;
			threadRunning = false;
		}
		catch (...)	{
			addLogEntry("CRITICAL ERROR: Unknown exception in RunFile");
			status = ERROR;
			threadRunning = false;
		}
	}

	void runFileInternal(const std::string& filename) {
		addLogEntry("Runfile started: " + filename);

		if (!currentPrinter || !currentPrinter->vtable)	{
			addGCodeErrorInfo("Printer is not available for execution", PRINTER_ERROR);
			status = ERROR;
			return;
		}

		try	{
			// First pass: syntax checking
			status = CHECKING_CODE;
			std::ifstream file(filename);
			if (!file.is_open()) {
				addGCodeErrorInfo("Cannot open file: " + filename, FILE_ERROR);
				lastError = "Cannot open file: " + filename;
				status = ERROR;
				threadRunning = false;
				return;
			}

			std::string line = "";
			size_t linesCount = 0;
			bool hasErrors = false;

			// Try interpret lines to find errors
			while (std::getline(file, line)) {
				waitIfPaused();
				if (stopRequested) break;

				while (pauseRequested && !stopRequested) {
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}

				if (stopRequested) break;

				processLine(line, linesCount, true);
				linesCount++;

				if (status == ERROR) {
					hasErrors = true;
					break;
				}
			}

			file.close();

			if (hasErrors) {
				addLogEntry("Execution aborted due to errors");
				status = ERROR;
				threadRunning = false;
				return;
			}

			// Second pass: execution
			status = RUNNING;
			std::ifstream file2(filename);
			if (!file2.is_open()) {
				addGCodeErrorInfo("Cannot open file: " + filename, FILE_ERROR);
				lastError = "Cannot open file: " + filename;
				status = ERROR;
				threadRunning = false;
				return;
			}

			linesCount = 0;
			size_t totalLines = 0;

			std::ifstream countFile(filename);
			totalLines = std::count(std::istreambuf_iterator<char>(countFile),
				std::istreambuf_iterator<char>(), '\n');

			countFile.close();

			while (std::getline(file2, line)) {
				waitIfPaused();
				if (stopRequested) break;

				while (pauseRequested && !stopRequested) {
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}

				if (stopRequested) break;

				processLine(line, linesCount, false);
				linesCount++;

				if (totalLines > 0)	{
					progress = static_cast<double>(linesCount) / totalLines * 100.0;
				}
			}

			file2.close();

			if (!stopRequested) {
				addLogEntry("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else {
				addLogEntry("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex) {
			addGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			lastError = ex.what();
			status = ERROR;
		}		

		threadRunning = false;
	}

	void waitIfPaused() {
		while (pauseRequested && !stopRequested) {
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}
	}

	// Process G-code line
	void processLine(const std::string& line, int linesCount, bool isTryingInterpret) {
		// Clean line from comments and whitespace
		std::string cleanLine = line.substr(0, line.find(';'));
		cleanLine.erase(0, cleanLine.find_first_not_of(" \t"));
		cleanLine.erase(cleanLine.find_last_not_of(" \t") + 1);

		if (cleanLine.empty()) return;

		std::istringstream commandStream(cleanLine);
		std::string command;
		commandStream >> command;

		if (command.empty()) {
			addLogEntry("Empty command in line: " + line);
			return;
		}

		if (isTryingInterpret) {
			try {
				addLogEntry("Syntax checking line: " + std::to_string(linesCount) + " : " + cleanLine);
				if (command[0] == 'G' || command[0] == 'g') {
					int gCode = std::stoi(command.substr(1));

					switch (gCode) {
					case 0:
					case 1:
						processMovement(commandStream, linesCount, true);
						break;
					case 2:
						processArc(commandStream, linesCount, true, true);
						break;
					case 3:
						processArc(commandStream, linesCount, true, false);
						break;
					case 4:
					case 28:
					case 90:
					case 91:
						break;
					default:
						addGCodeErrorInfo("Unknown G-code: " + std::to_string(gCode) +
							" at line " + std::to_string(linesCount), VALUE_NOT_DEFINED);
						break;
					}
				}
				else if (command[0] == 'M' || command[0] == 'm') {
					int mCode = std::stoi(command.substr(1));
					switch (mCode) {
					case 30:
						break;
					default:
						addGCodeErrorInfo("Unknown M-code: " + std::to_string(mCode) +
							" at line " + std::to_string(linesCount));
						break;
					}
				}
				else if (command[0] == 'F' || command[0] == 'f') {
					try {
						double newSpeed = std::stoi(command.substr(1));
						if (newSpeed < 0) {
							addGCodeErrorInfo("Negative feedrate not allowed: " + command, VALUE_NOT_DEFINED);
						}
					}
					catch (const std::exception& ex) {
						addGCodeErrorInfo("Invalid feedrate value: " + command, VALUE_NOT_DEFINED);
					}
				}
				else {
					addGCodeErrorInfo("Unknown processing command '" + command + "' " +
						" at line " + std::to_string(linesCount));
				}
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Exception in processLine: " + std::string(ex.what()) + " at line " +
				std::to_string(linesCount), SYNTAX_ERROR);
			}
		}
		else {
			try {
				addLogEntry("Executing line " + std::to_string(linesCount) + " : " + cleanLine);
				if (command[0] == 'G' || command[0] == 'g') {
					int gCode = std::stoi(command.substr(1));

					switch (gCode) {
					case 0:
					case 1:
						processMovement(commandStream, linesCount, false);
						break;
					case 2:
						processArc(commandStream, linesCount, false, true);
						break;
					case 3:
						processArc(commandStream, linesCount, false, false);
						break;
					case 4:
						stopAllMotors();
						break;
					case 28:
						processHoming();
						break;
					case 90:
						absolutePositioning = true;
						break;
					case 91:
						absolutePositioning = false;
						break;
					default:
						break;
					}
				}
				else if (command[0] == 'M' || command[0] == 'm') {
					int mCode = std::stoi(command.substr(1));
					switch (mCode) {
					case 30:
						stopRequested = true;
						break;
					default:
						break;
					}
				}
				else if (command[0] == 'F' || command[0] == 'f') {
					speed = std::stoi(command.substr(1));
				}
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Exception during execution: " + std::string(ex.what()) + " at line " +
				std::to_string(linesCount), MOVEMENT_ERROR);
			}
		}
	}

	// Improved processArc function for proper parsing
	void processArc(std::istringstream& stream, int lineCount, bool isTryingInterpret, bool clockwise) {
		if (isTryingInterpret) {
			// Syntax check
			std::string token;
			bool hasX = false, hasY = false, hasI = false, hasJ = false, hasR = false;

			while (stream >> token) {
				char axis = std::toupper(token[0]);
				try {
					double value = std::stod(token.substr(1));

					switch (axis) {
					case 'X': hasX = true; break;
					case 'Y': hasY = true; break;
					case 'I': hasI = true; break;
					case 'J': hasJ = true; break;
					case 'R': hasR = true; break;
					default:
						addGCodeErrorInfo("Invalid axis: " + std::string(1, axis), SYNTAX_ERROR);
						return;
					}
				}
				catch (...) {
					addGCodeErrorInfo("Invalid number format: " + token, SYNTAX_ERROR);
					return;
				}
			}

			// Minimum requirements: X,Y and (R or I,J)
			if (!hasX || !hasY) {
				addGCodeErrorInfo("Arc requires X and Y coordinates", SYNTAX_ERROR);
				return;
			}

			if (!hasR && (!hasI || !hasJ)) {
				addGCodeErrorInfo("Arc requires either R or I,J parameters", SYNTAX_ERROR);
				return;
			}

			addLogEntry("Arc syntax OK");
		}
		else {
			// Execution
			addLogEntry("Executing " + std::string(clockwise ? "G2" : "G3"));

			std::string token;
			double x = 0, y = 0, i = 0, j = 0, r = -1;
			bool hasR = false;

			while (stream >> token) {
				char axis = std::toupper(token[0]);
				double value = std::stod(token.substr(1));

				switch (axis) {
				case 'X': x = value; break;
				case 'Y': y = value; break;
				case 'I': i = value; break;
				case 'J': j = value; break;
				case 'R':
					r = value;
					hasR = true;
					break;
				}
			}

			// If R is not specified, use I,J
			if (!hasR && (i == 0 && j == 0)) {
				addGCodeErrorInfo("Arc radius not specified", MOVEMENT_ERROR);
				return;
			}

			try {
				ArcParameters arc = calculateArcParameters(x, y, i, j, r, clockwise);

				if (arc.radius <= 0) {
					addGCodeErrorInfo("Invalid arc parameters", MOVEMENT_ERROR);
					return;
				}

				executeArcMovement(arc, speed);
			}
			catch (const std::exception& ex) {
				addGCodeErrorInfo("Arc error: " + std::string(ex.what()), MOVEMENT_ERROR);
			}
		}
	}

	void processMovement(std::istringstream& string, int lineCount, bool isTryingInterpret) {
		std::string token;
		char axis;
		double value;		

		if (isTryingInterpret) {
			if (!currentPrinter || !currentPrinter->vtable) {
				addGCodeErrorInfo("Printer is not available for movement", PRINTER_ERROR);
				return;
			}

			if (stepperX.ports.empty() || stepperY.ports.empty() || stepperZ.ports.empty())	{
				addGCodeErrorInfo("Motor ports are not configured", CONFIG_ERROR);
				return;
			}

			addLogEntry("Checking movement command syntax");
			while (string >> token)	{
				axis = token[0];
				value = std::stof(token.substr(1));

				switch (axis) {
				case 'X':
				case 'x':
				case 'Y':
				case 'y':
				case 'Z':
				case 'z':
					break;
				default:
					addGCodeErrorInfo("Unknown axis: " + std::string(1, axis) + 
					" at line " + std::to_string(lineCount));
					break;
				}
			}
		}
		else {
			addLogEntry("Execute movement command");

			// Initialize target coordinates
			double targetX = absolutePositioning ? currentX : 0.0;
			double targetY = absolutePositioning ? currentY : 0.0;
			double targetZ = absolutePositioning ? currentZ : 0.0;

			// Parse movement commands
			while (string >> token)	{
				axis = token[0];
				value = std::stof(token.substr(1));

				switch (axis) {
				case 'X':
				case 'x':
					if (absolutePositioning) targetX = value;
					else targetX += value;
					break;
				case 'Y':
				case 'y':
					if (absolutePositioning) targetY = value;
					else targetY += value;
					break;
				case 'Z':
				case 'z':
					if (absolutePositioning) targetZ = value;
					else targetZ += value;
					break;
				default:
					break;
				}
			}

			double xMovement = targetX - currentX;
			double yMovement = targetY - currentY;
			double zMovement = targetZ - currentZ;

			addLogEntry("Execute movement command - X:" + std::to_string(xMovement) +
			 " Y: " + std::to_string(yMovement) + " Z:" + std::to_string(zMovement));

			// Process X and Y axis movement
			if (std::abs(xMovement) > 0 || std::abs(yMovement) > 0)	{
				// Initialized final command for XY movement as a vector
				std::vector<MotorCommand> xyCommands;

				// Calculate movement times for each axis
				double timeX = 0.0;
				double timeY = 0.0;

				// ========== X Axis ==========
				if (std::abs(xMovement) > 0) {
					double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

					// Calculate base time for X movement
					double baseSpeedX = speed;
					if (xMovement < 0) baseSpeedX = -baseSpeedX;
					if (!stepperX.direction) baseSpeedX = -baseSpeedX;

					// Apply speed limits
					if (baseSpeedX > 0)	{
						baseSpeedX = std::min(baseSpeedX, stepperX.maximumFeedrate);
						baseSpeedX = std::max(baseSpeedX, stepperX.minimumFeedrate);
					}
					else {
						baseSpeedX = std::max(baseSpeedX, -stepperX.maximumFeedrate);
						baseSpeedX = std::min(baseSpeedX, -stepperX.minimumFeedrate);
					}

					timeX = revolutionsX / std::abs(baseSpeedX);
				}

				// ========== Y Axis ==========
				if (std::abs(yMovement) > 0) {
					double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

					// Calculates base time for Y movement
					double baseSpeedY = speed;
					if (yMovement < 0) baseSpeedY = -baseSpeedY;
					if (!stepperY.direction) baseSpeedY = -baseSpeedY;

					// Apply speed limits
					if (baseSpeedY > 0)	{
						baseSpeedY = std::min(baseSpeedY, stepperY.maximumFeedrate);
						baseSpeedY = std::max(baseSpeedY, stepperY.minimumFeedrate);
					}
					else {
						baseSpeedY = std::max(baseSpeedY, -stepperY.maximumFeedrate);
						baseSpeedY = std::min(baseSpeedY, -stepperY.minimumFeedrate);
					}

					timeY = revolutionsY / std::abs(baseSpeedY);
				}

				// Determine the maximum time needed
				double maxTime = std::max(timeX, timeY);
				if (maxTime == 0.0) { // Avoid division by zero
					maxTime = 1.0;
				}

				// ============ X Axis with synchronized speed =============
				if (std::abs(xMovement) > 0) {
					double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

					// Calculate speed to match the maximum time
					double synchronizedSpeedX = revolutionsX / maxTime;

					for (uint8_t port : stepperX.ports)	{
						MotorCommand command;
						command.port = port;

						double calculatedSpeed = synchronizedSpeedX;
						if (xMovement < 0) calculatedSpeed = -calculatedSpeed;
						if (!stepperX.direction) calculatedSpeed = -calculatedSpeed;

						// Apply speed limits to synchronized speed
						if (calculatedSpeed > 0) {
							calculatedSpeed = std::min(calculatedSpeed, stepperX.maximumFeedrate);
							calculatedSpeed = std::max(calculatedSpeed, stepperX.minimumFeedrate);
						}
						else {
							calculatedSpeed = std::max(calculatedSpeed, -stepperX.maximumFeedrate);
							calculatedSpeed = std::min(calculatedSpeed, -stepperX.minimumFeedrate);
						}

						command.speed = static_cast<signed char>(calculatedSpeed);
						command.revolutions = revolutionsX;

						xyCommands.push_back(command);

						addLogEntry("X axis - Port: " + std::to_string(port) +
							" Speed: " + std::to_string(calculatedSpeed) +
							" Revolutions: " + std::to_string(revolutionsX));
					}
				}

				// ============= Y Axis with synchronized speed ================
				if (std::abs(yMovement) > 0) {
					double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

					// Calculate speed to match the maximum time
					double synchronizedSpeedY = revolutionsY / maxTime;

					for (uint8_t port : stepperY.ports)	{
						MotorCommand command;
						command.port = port;

						double calculatedSpeed = synchronizedSpeedY;
						if (yMovement < 0) calculatedSpeed = -calculatedSpeed;
						if (stepperY.direction) calculatedSpeed = -calculatedSpeed;

						// Apply speed limits to synchronized speed
						if (calculatedSpeed > 0) {
							calculatedSpeed = std::min(calculatedSpeed, stepperY.maximumFeedrate);
							calculatedSpeed = std::max(calculatedSpeed, stepperY.minimumFeedrate);
						}
						else {
							calculatedSpeed = std::max(calculatedSpeed, -stepperY.maximumFeedrate);
							calculatedSpeed = std::min(calculatedSpeed, -stepperY.minimumFeedrate);
						}

						command.speed = static_cast<signed char>(calculatedSpeed);
						command.revolutions = revolutionsY;

						xyCommands.push_back(command);

						addLogEntry("Y axis - Port: " + std::to_string(port) +
						" Speed: " + std::to_string(calculatedSpeed) +
						" Revolutions " + std::to_string(revolutionsY));
					}
				}

				// Send synchronized commands for X and Y axis
				if (!xyCommands.empty()) {
					MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
					std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
					currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, xyCommands.size());
					delete[] finalCommands;

					addLogEntry("XY movement synchronized. Max time: " + std::to_string(maxTime));
				}
			}

			// =================== Z Axis ===================
			if (std::abs(zMovement) > 0) {
				std::vector<MotorCommand> zCommands;
				
				for (uint8_t port : stepperZ.ports)	{
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = speed;
					if (zMovement < 0) calculatedSpeed = -calculatedSpeed;
					if (!stepperZ.direction) calculatedSpeed = -calculatedSpeed;

					if (calculatedSpeed > 0) {
						calculatedSpeed = std::min(calculatedSpeed, stepperZ.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperZ.minimumFeedrate);
					}
					else {
						calculatedSpeed = std::max(calculatedSpeed, -stepperZ.maximumFeedrate);
						calculatedSpeed = std::min(calculatedSpeed, -stepperZ.minimumFeedrate);
					}

					command.speed = static_cast<signed char>(calculatedSpeed);
					command.revolutions = (std::abs(zMovement) * stepperZ.gearRatio) / stepperZ.rotationDistance;

					zCommands.push_back(command);
				}

				MotorCommand* finalCommands = new MotorCommand[zCommands.size()];
				std::copy(zCommands.begin(), zCommands.end(), finalCommands);
				currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, zCommands.size());
				delete[] finalCommands;
			}

			currentX = targetX;
			currentY = targetY;
			currentZ = targetZ;

			addLogEntry("Movement completed. New position: X=" + std::to_string(currentX) +
			" Y=" + std::to_string(currentY) + " Z=" + std::to_string(currentZ));
		}
	}

	void processHoming() {
		addLogEntry("Homing command started");

		double xMovement = -currentX;
		double yMovement = -currentY;
		double zMovement = -currentZ;

		addLogEntry("Execute movement command - X:" + std::to_string(xMovement) +
			" Y: " + std::to_string(yMovement) + " Z:" + std::to_string(zMovement));

		// Process X and Y axis movement
		if (std::abs(xMovement) > 0 || std::abs(yMovement) > 0)	{
			// Initialized final command for XY movement as a vector
			std::vector<MotorCommand> xyCommands;

			// Calculate movement times for each axis
			double timeX = 0.0;
			double timeY = 0.0;

			// ========== X Axis ==========
			if (std::abs(xMovement) > 0) {
				double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

				// Calculate base time for X movement
				double baseSpeedX = speed;
				if (xMovement < 0) baseSpeedX = -baseSpeedX;
				if (!stepperX.direction) baseSpeedX = -baseSpeedX;

				// Apply speed limits
				if (baseSpeedX > 0)	{
					baseSpeedX = std::min(baseSpeedX, stepperX.maximumFeedrate);
					baseSpeedX = std::max(baseSpeedX, stepperX.minimumFeedrate);
				}
				else {
					baseSpeedX = std::max(baseSpeedX, -stepperX.maximumFeedrate);
					baseSpeedX = std::min(baseSpeedX, -stepperX.minimumFeedrate);
				}

				timeX = revolutionsX / std::abs(baseSpeedX);
			}

			// ========== Y Axis ==========
			if (std::abs(yMovement) > 0) {
				double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

				// Calculates base time for Y movement
				double baseSpeedY = speed;
				if (yMovement < 0) baseSpeedY = -baseSpeedY;
				if (!stepperY.direction) baseSpeedY = -baseSpeedY;

				// Apply speed limits
				if (baseSpeedY > 0)	{
					baseSpeedY = std::min(baseSpeedY, stepperY.maximumFeedrate);
					baseSpeedY = std::max(baseSpeedY, stepperY.minimumFeedrate);
				}
				else {
					baseSpeedY = std::max(baseSpeedY, -stepperY.maximumFeedrate);
					baseSpeedY = std::min(baseSpeedY, -stepperY.minimumFeedrate);
				}

				timeY = revolutionsY / std::abs(baseSpeedY);
			}

			// Determine the maximum time needed
			double maxTime = std::max(timeX, timeY);
			if (maxTime == 0.0)	{ // Avoid division by zero
				maxTime = 1.0;
			}


			// ============ X Axis with synchronized speed =============
			if (std::abs(xMovement) > 0) {
				double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

				// Calculate speed to match the maximum time
				double synchronizedSpeedX = revolutionsX / maxTime;

				for (uint8_t port : stepperX.ports)	{
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = synchronizedSpeedX;
					if (xMovement < 0) calculatedSpeed = -calculatedSpeed;
					if (!stepperX.direction) calculatedSpeed = -calculatedSpeed;

					// Apply speed limits to synchronized speed
					if (calculatedSpeed > 0) {
						calculatedSpeed = std::min(calculatedSpeed, stepperX.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperX.minimumFeedrate);
					}
					else {
						calculatedSpeed = std::max(calculatedSpeed, -stepperX.maximumFeedrate);
						calculatedSpeed = std::min(calculatedSpeed, -stepperX.minimumFeedrate);
					}

					command.speed = static_cast<signed char>(calculatedSpeed);
					command.revolutions = revolutionsX;

					xyCommands.push_back(command);

					addLogEntry("X axis - Port: " + std::to_string(port) +
						" Speed: " + std::to_string(calculatedSpeed) +
						" Revolutions: " + std::to_string(revolutionsX));
				}
			}

			// ============= Y Axis with synchronized speed ================
			if (std::abs(yMovement) > 0) {
				double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

				// Calculate speed to match the maximum time
				double synchronizedSpeedY = revolutionsY / maxTime;

				for (uint8_t port : stepperY.ports)	{
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = synchronizedSpeedY;
					if (yMovement < 0) calculatedSpeed = -calculatedSpeed;
					if (stepperY.direction) calculatedSpeed = -calculatedSpeed;

					// Apply speed limits to synchronized speed
					if (calculatedSpeed > 0) {
						calculatedSpeed = std::min(calculatedSpeed, stepperY.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperY.minimumFeedrate);
					}
					else {
						calculatedSpeed = std::max(calculatedSpeed, -stepperY.maximumFeedrate);
						calculatedSpeed = std::min(calculatedSpeed, -stepperY.minimumFeedrate);
					}

					command.speed = static_cast<signed char>(calculatedSpeed);
					command.revolutions = revolutionsY;

					xyCommands.push_back(command);

					addLogEntry("Y axis - Port: " + std::to_string(port) +
						" Speed: " + std::to_string(calculatedSpeed) +
						" Revolutions " + std::to_string(revolutionsY));
				}
			}

			// Send synchronized commands for X and Y axis
			if (!xyCommands.empty()) {
				MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
				std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
				currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, xyCommands.size());
				delete[] finalCommands;

				addLogEntry("XY movement synchronized. Max time: " + std::to_string(maxTime));
			}
		}

		// =================== Z Axis ===================
		if (std::abs(zMovement) > 0) {
			std::vector<MotorCommand> zCommands;

			for (uint8_t port : stepperZ.ports)	{
				MotorCommand command;
				command.port = port;

				double calculatedSpeed = speed;
				if (zMovement < 0) calculatedSpeed = -calculatedSpeed;
				if (!stepperZ.direction) calculatedSpeed = -calculatedSpeed;

				if (calculatedSpeed > 0) {
					calculatedSpeed = std::min(calculatedSpeed, stepperZ.maximumFeedrate);
					calculatedSpeed = std::max(calculatedSpeed, stepperZ.minimumFeedrate);
				}
				else {
					calculatedSpeed = std::max(calculatedSpeed, -stepperZ.maximumFeedrate);
					calculatedSpeed = std::min(calculatedSpeed, -stepperZ.minimumFeedrate);
				}

				command.speed = static_cast<signed char>(calculatedSpeed);
				command.revolutions = (std::abs(zMovement) * stepperZ.gearRatio) / stepperZ.rotationDistance;

				zCommands.push_back(command);
			}

			MotorCommand* finalCommands = new MotorCommand[zCommands.size()];
			std::copy(zCommands.begin(), zCommands.end(), finalCommands);
			currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, zCommands.size());
			delete[] finalCommands;
		}

		currentX = 0;
		currentY = 0;
		currentZ = 0;

		addLogEntry("Movement completed. New position: X=" + std::to_string(currentX) +
			" Y=" + std::to_string(currentY) + " Z=" + std::to_string(currentZ));

		addLogEntry("Homing completed");
	}

	void stopAllMotors() {
		if (!currentPrinter || !currentPrinter->vtable) {
			addGCodeErrorInfo("Printer is not available for movement", PRINTER_ERROR);
			return;
		}

		if (stepperX.ports.empty() || stepperY.ports.empty() || stepperZ.ports.empty()) {
			addGCodeErrorInfo("Motor ports are not configured", CONFIG_ERROR);
			return;
		}

		for (uint8_t port : stepperX.ports) {
			currentPrinter->vtable->printer_set_motor_speed(currentPrinter, port, 0);
		}
		for (uint8_t port : stepperY.ports) {
			currentPrinter->vtable->printer_set_motor_speed(currentPrinter, port, 0);
		}
		for (uint8_t port : stepperZ.ports) {
			currentPrinter->vtable->printer_set_motor_speed(currentPrinter, port, 0);
		}

		addLogEntry("All motors have already stopped");
	}
};

// C API exports
extern "C"
{
	GCODE_API InterpreterHandle CreateInterpreter() {
		return new Interpreter();
	}

	GCODE_API void DestroyInterpreter(InterpreterHandle handle) {
		delete static_cast<Interpreter*>(handle);
	}

	GCODE_API bool TestCode(InterpreterHandle handle, IPrinter* printer) {
		if (!handle || !printer) return false;

		return static_cast<Interpreter*>(handle)->testFunction(printer);
	}

	GCODE_API bool ExecuteGcode(InterpreterHandle handle, const char* filename, IPrinter* printer) {
		printf("C++: ExecuteGcode called\n");
		printf("C++: Handle: %p\n", handle);
		printf("C++: Printer: %p\n", printer);
		if (printer) {
			printf("C++: Printer->VirtualTable: %p\n", printer->vtable);
		}
		printf("C++: Filename: %s\n", filename ? filename : "NULL");

		if (!handle || !printer || !filename) {
			printf("C++: ERROR - Invalid parameters\n");
			return false;
		}

		FILE* testFile = nullptr;
		errno_t err = fopen_s(&testFile, filename, "r");
		if (err != 0 || !testFile) {
			printf("C++: ERROR - Cannot open file: %s, error code: %d\n", filename, err);
			return false;
		}
		fclose(testFile);

		printf("C++: File exists, calling ExecuteFile\n");
		return static_cast<Interpreter*>(handle)->executeFile(filename, printer);
	}

	GCODE_API bool ExecuteLine(InterpreterHandle handle, const char* line, IPrinter* printer) {
		if (printer) {
			printf("C++: Printer->VirtualTable: %p\n", printer->vtable);
		}

		if (!handle || !printer) {
			printf("C++: ERROR - Invalid parameters\n");
			return false;
		}

		return static_cast<Interpreter*>(handle)->executeLine(line, printer);
	}

	GCODE_API void PauseExecution(InterpreterHandle handle)	{
		if (handle)	{
			static_cast<Interpreter*>(handle)->pause();
		}
	}

	GCODE_API void ResumeExecution(InterpreterHandle handle) {
		if (handle)	{
			static_cast<Interpreter*>(handle)->resume();
		}
	}

	GCODE_API int GetStatus(InterpreterHandle handle) {
		if (!handle) return -1;

		return static_cast<Interpreter*>(handle)->getStatus();
	}

	GCODE_API double GetProgress(InterpreterHandle handle) {
		if (!handle) return 0.0;

		return static_cast<Interpreter*>(handle)->getProgress();
	}

	GCODE_API const char* GetLastInterpreterError(InterpreterHandle handle) {
		if (!handle) return nullptr;

		return static_cast<Interpreter*>(handle)->getLastError();
	}

	GCODE_API int GetErrorCount(InterpreterHandle handle) {
		if (!handle) return 0;

		return static_cast<Interpreter*>(handle)->getErrorCount();
	}

	GCODE_API const char* GetError(InterpreterHandle handle, int index) {
		if (!handle) return nullptr;

		return static_cast<Interpreter*>(handle)->GetError(index);
	}

	GCODE_API int GetLogCount(InterpreterHandle handle)	{
		if (!handle) return 0;

		return static_cast<Interpreter*>(handle)->getLogCount();
	}

	GCODE_API const char* GetLogEntry(InterpreterHandle handle, int index) {
		if (!handle) return nullptr;

		return static_cast<Interpreter*>(handle)->GetLogEntry(index);
	}

	GCODE_API void ClearErrors(InterpreterHandle handle) {
		if (handle)	{
			static_cast<Interpreter*>(handle)->clearErrors();
		}
	}

	GCODE_API void ClearLog(InterpreterHandle handle) {
		if (handle)	{
			static_cast<Interpreter*>(handle)->clearLog();
		}
	}

	GCODE_API bool ReadConfig(InterpreterHandle handle, const char* filename) {
		if (!handle || !filename) return false;

		return static_cast<Interpreter*>(handle)->readConfigFile(filename);
	}
}
