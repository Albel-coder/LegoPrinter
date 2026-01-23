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
#include <chrono>

// Cross-platform macros
#if defined(_WIN32) || defined(_WIN64)
    #define FORCE_INLINE __forceinline
    #define LOCALTIME(tm, time) localtime_s(tm, time)
    #define STRCPY_SAFE(dest, src, size) strcpy_s(dest, size, src)
    #define STRNCAT_SAFE(dest, src, size) strncat_s(dest, size, src, _TRUNCATE)
    #define STRNCMP_SAFE(s1, s2, size) strncmp_s(s1, s2, size)
    #define STRNCASECMP_SAFE(s1, s2, size) _strnicmp(s1, s2, size)
    #define STRNCPY_SAFE(dest, src, destSize, count) strncpy_s(dest, destSize, src, count)
    #define FOPEN(file, filename, mode) fopen_s(&file, filename, mode)
#else
    #define FORCE_INLINE inline __attribute__((always_inline))
    #define LOCALTIME(tm, time) localtime_r(time, tm)
    #define STRCPY_SAFE(dest, src, size) strncpy(dest, src, size)
    #define STRNCAT_SAFE(dest, src, size) strncat(dest, src, size)
    #define STRNCMP_SAFE(s1, s2, size) strncmp(s1, s2, size)
    #define STRNCASECMP_SAFE(s1, s2, size) strncasecmp(s1, s2, size)
    #define STRNCPY_SAFE(dest, src, destSize, count) do { \
        size_t n = (count) < (destSize) ? (count) : (destSize)-1; \
        strncpy(dest, src, n); \
        dest[n] = '\0'; \
    } while(0)
#endif

#ifdef _DEBUG
	#define LOG_ENABLED 1
	#define LOG_DEBUG_ENABLED 1
#else
	#define LOG_ENABLED 1
	#define LOG_DEBUG_ENABLED 0
#endif

// Macros for logging in the interpreter
#define LOG_ERROR(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_ERROR)) \
		addLogInternal(LOG_CATEGORY_ERROR, format, ##__VA_ARGS__)

#define LOG_WARNING(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_WARNING)) \
		addLogInternal(LOG_CATEGORY_WARNING, format, ##__VA_ARGS__)

#define LOG_INFO(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_INFO)) \
		addLogInternal(LOG_CATEGORY_INFO, format, ##__VA_ARGS__)

#define LOG_DEBUG(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_DEBUG)) \
		addLogInternal(LOG_CATEGORY_DEBUG, format, ##__VA_ARGS__)

#define LOG_GCODE(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_GCODE)) \
		addLogInternal(LOG_CATEGORY_GCODE, format, ##__VA_ARGS__)

#define LOG_MOVEMENT(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_MOVEMENT)) \
		addLogInternal(LOG_CATEGORY_MOVEMENT, format, ##__VA_ARGS__)

#define LOG_ARC(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_ARC)) \
		addLogInternal(LOG_CATEGORY_ARC, format, ##__VA_ARGS__)

#define LOG_CONFIG(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_CONFIG)) \
		addLogInternal(LOG_CATEGORY_CONFIG, format, ##__VA_ARGS__)

#define LOG_PARSING(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_PARSING)) \
		addLogInternal(LOG_CATEGORY_PARSING, format, ##__VA_ARGS__)

#define LOG_EXECUTION(format, ...) \
	if (isCategoryEnabled(LOG_CATEGORY_EXECUTION)) \
		addLogInternal(LOG_CATEGORY_EXECUTION, format, ##__VA_ARGS__)

#ifdef _DEBUG
#define LOG_PERFORMANCE_START() auto performanceStartTime = std::chrono::high_resolution_clock::now()
#define LOG_PERFORMANCE_END(category, operation) \
        auto performanceEndTime = std::chrono::high_resolution_clock::now(); \
        auto performanceDuration = std::chrono::duration_cast<std::chrono::microseconds>(performanceEndTime - performanceStartTime); \
        if (isCategoryEnabled(LOG_CATEGORY_PERFORMANCE)) \
            addLogInternal(LOG_CATEGORY_PERFORMANCE, "%s took %lld µs", operation, performanceDuration.count())
#else
#define LOG_PERFORMANCE_START() 
#define LOG_PERFORMANCE_END(category, operation)
#endif

enum LogCategory {
	LOG_CATEGORY_NONE = 0,
	LOG_CATEGORY_ERROR = 1 << 0,
	LOG_CATEGORY_WARNING = 1 << 1,
	LOG_CATEGORY_INFO = 1 << 2,
	LOG_CATEGORY_DEBUG = 1 << 3,
	LOG_CATEGORY_GCODE = 1 << 4,
	LOG_CATEGORY_MOVEMENT = 1 << 5,
	LOG_CATEGORY_ARC = 1 << 6,
	LOG_CATEGORY_CONFIG = 1 << 7,
	LOG_CATEGORY_PARSING = 1 << 8,
	LOG_CATEGORY_EXECUTION = 1 << 9,
	LOG_CATEGORY_PERFORMANCE = 1 << 10,

	LOG_CATEGORY_ALL = 0xFFFFFFFF,
	LOG_CATEGORY_DEFAULT = LOG_CATEGORY_ERROR |
	LOG_CATEGORY_WARNING |
	LOG_CATEGORY_INFO |
	LOG_CATEGORY_MOVEMENT |
	LOG_CATEGORY_CONFIG,

#ifdef _DEBUG
	LOG_CATEGORY_RELEASE = LOG_CATEGORY_ALL,
#else
	LOG_CATEGORY_RELEASE = LOG_CATEGORY_DEFAULT,

#endif
};

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

	std::atomic<uint32_t> enabledCategories;

	struct LogEntry {
		char message[1024];
		LogCategory category;
		std::chrono::system_clock::time_point timestamp;
	};

	static constexpr size_t MAX_LOG_ENTRIES = 5000;
	static constexpr size_t MAX_MESSAGE_LENGTH = 1023;

	std::unique_ptr<LogEntry[]> logBuffer;
	std::atomic<size_t> logWriteIndex{ 0 };
	std::atomic<size_t> logReadIndex{ 0 };
	std::mutex logBufferMutex;

	FORCE_INLINE bool isCategoryEnabled(LogCategory category) {
		return (enabledCategories.load(std::memory_order_relaxed) & category) != 0;
	}

	template<size_t N>
	FORCE_INLINE void formatToBuffer(char(&buffer)[N], const char* format, va_list args) {
		vsnprintf(buffer, N, format, args);
	}

	void addLogInternal(LogCategory category, const char* format, ...) {
		if (!isCategoryEnabled(category)) return;

		char formatted[1024];
		va_list args;
		va_start(args, format);
		vsnprintf(formatted, sizeof(formatted), format, args);
		formatted[sizeof(formatted) - 1] = '\0';
		va_end(args);

		auto now = std::chrono::system_clock::now();
		auto time_t_now = std::chrono::system_clock::to_time_t(now);
		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()) % 1000;

		tm time_info;
		LOCALTIME(&time_info, &time_t_now);

		const char* categoryName = "UNKNOWN";
		switch (category) {
		case LOG_CATEGORY_ERROR:
			categoryName = "ERROR";
			break;
		case LOG_CATEGORY_WARNING:
			categoryName = "WARNING";
			break;
		case LOG_CATEGORY_INFO:
			categoryName = "INFO";
			break;
		case LOG_CATEGORY_DEBUG:
			categoryName = "DEBUG";
			break;
		case LOG_CATEGORY_GCODE:
			categoryName = "GCODE";
			break;
		case LOG_CATEGORY_MOVEMENT:
			categoryName = "MOVEMENT";
			break;
		case LOG_CATEGORY_ARC:
			categoryName = "ARC";
			break;
		case LOG_CATEGORY_CONFIG:
			categoryName = "CONFIG";
			break;
		case LOG_CATEGORY_PARSING:
			categoryName = "PARSING";
			break;
		case LOG_CATEGORY_EXECUTION:
			categoryName = "EXECUTION";
			break;
		case LOG_CATEGORY_PERFORMANCE:
			categoryName = "PERFORMANCE";
			break;
		}

		char finalBuffer[1024];
		snprintf(finalBuffer, sizeof(finalBuffer),
			"[%s][%02d:%02d:%02d.%03d] %s",
			categoryName,
			time_info.tm_hour, time_info.tm_min, time_info.tm_sec,
			(int)milliseconds.count(),
			formatted);

		size_t write_idx = logWriteIndex.load(std::memory_order_relaxed);
		size_t read_idx = logReadIndex.load(std::memory_order_relaxed);

		size_t next_write = (write_idx + 1) % MAX_LOG_ENTRIES;

		if (next_write == read_idx % MAX_LOG_ENTRIES) {
			logReadIndex.store((read_idx + 1) % MAX_LOG_ENTRIES,
				std::memory_order_relaxed);
		}

		size_t buffer_idx = write_idx % MAX_LOG_ENTRIES;
		STRNCPY_SAFE(logBuffer[buffer_idx].message, finalBuffer, sizeof(logBuffer[buffer_idx].message), MAX_MESSAGE_LENGTH);
		logBuffer[buffer_idx].category = category;
		logBuffer[buffer_idx].timestamp = now;

		logWriteIndex.store(next_write, std::memory_order_release);
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

		LOG_WARNING("Unknown section in configuration: %s", section.c_str());
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

		LOG_WARNING("Unknown configuration key: %s", key.c_str());
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
			LOG_ERROR("Unknown section in configuration");
			return;
		}

		if (!config) return;

		switch (key) {
		case ConfigKey::ROTATE_DISTANCE:
			try	{
				config->rotationDistance = std::stod(parseValue(value));
				LOG_CONFIG("Set rotation_distance: %f", config->rotationDistance);
			}
			catch (const std::exception& ex) {				
				LOG_ERROR("Invalid rotation distance value: %f", config->rotationDistance);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::GEAR_RATIO:
			try	{
				config->gearRatio = std::stod(parseValue(value));
				LOG_CONFIG("Set gear_ratio: %s", value.c_str());
			}
			catch (const std::exception& ex) {
				LOG_ERROR("Invalid gear ratio value: %s", value.c_str());
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
					LOG_CONFIG("Set direction: clockwise (true)");
				}
				else if (directionString == "counterclockwise" || directionString == "ccw") {
					config->direction = false;
					LOG_CONFIG("Set direction: counterclockwise (false)");
				}
				else {
					// Try to convert as number for backward compatibility
					config->direction = std::stoi(directionString) != 0;
					LOG_CONFIG("Set direction: %d", config->direction);
				}
			}
			catch (const std::exception& ex) {
				LOG_ERROR("Invalid direction value: %s", value.c_str());
				lastError = ex.what();
				status = ERROR;
			}
			break;
			
		case ConfigKey::PORTS:
			try	{
				std::vector<uint8_t> ports;
				LOG_CONFIG("Processing ports configuration: %s", value.c_str());

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
						LOG_WARNING("Warning: unknown port character '%с'", character);
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
						LOG_CONFIG("Added port '%с'", character);
					}
					else {
						LOG_WARNING("Duplicate port detected: '%с'", character);
					}
				}				

				config->ports = ports;
				LOG_CONFIG("Ports configuration completed. Total ports: %d", config->ports.size());

				if (config->ports.empty()) {
					LOG_WARNING("WARNING: No valid ports configured!");
				}
				else {
					std::string portsList = "Configured ports: ";
					for (auto port : config->ports)	{
						portsList += std::to_string(port) + " ";
					}

					LOG_CONFIG("Ports list: %s", portsList.c_str());
				}
			}
			catch (const std::exception& ex)  {
				LOG_ERROR("Invalid ports configuration: %s", value.c_str());
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MINIMUM_FEEDRATE:
			try	{
				config->minimumFeedrate = std::stod(parseValue(value));
				LOG_CONFIG("Set miminum_feedrate: %s", value.c_str());
			}
			catch (const std::exception& ex) {
				LOG_ERROR("Invalid minimum feedrate value: %s", value.c_str());
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MAXIMUM_FEEDRATE:
			try	{
				config->maximumFeedrate = std::stod(parseValue(value));
				LOG_CONFIG("Set maximum_feedrate: %s", value.c_str());
			}
			catch (const std::exception& ex) {
				LOG_ERROR("Invalid maximum feedrate value: %s", value.c_str());
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::UNKNOWN:
			LOG_WARNING("Unknown configuration key");
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

	// Algorithm for dividing an arc into linear segments (linear approximation)
	std::vector<LinearSegment> approximateArcWithLines(
		double centerX, double centerY, double radius,
		double startAngle, double endAngle, bool clockwise,
		int maxSegments = 100, double maxError = 0.01) {

		std::vector<LinearSegment> segments;

		// Adjusting angles for direction
		double totalAngle = endAngle - startAngle;

		// For a semicircle (180°) totalAngle will be -π for G2 (clockwise)
		// or +π for G3 (counterclockwise)

		// Determine the number of segments based on the angle
		int segmentsCount = static_cast<int>(std::ceil(std::abs(totalAngle) / (PI / 10))); // Every 18°
		segmentsCount = std::max(4, std::min(segmentsCount, maxSegments));

		double angleStep = totalAngle / segmentsCount;

		// Starting point
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
			LOG_WARNING("Warning: no ports or movement ports for axis");
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
		LOG_DEBUG("Added stop point at distance %f", stopPoint.distance);

		if (config.ports.size() == 1) {
			SpeedProfile profile;
			profile.port = config.ports[0];
			profile.count = static_cast<int>(pointsWithStop.size());
			profile.timeoutMs = timeoutMs;

			profile.points = new SpeedProfilePoint[pointsWithStop.size()];
			std::copy(pointsWithStop.begin(), pointsWithStop.end(), profile.points);

			profiles.push_back(profile);
			LOG_DEBUG("Created single speed profile with stop for port %c", profile.port);
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
				LOG_DEBUG("Created speed profile with stop for port %c", port);
			}

			LOG_DEBUG("Created speed profiles with stop: %d", profiles.size());
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

	// Corrected calculateArcParameters method
	ArcParameters calculateArcParameters(double endX, double endY, double i, double j, double r, bool clockwise) {
		ArcParameters arc;
		arc.startX = currentX;
		arc.startY = currentY;
		arc.endX = absolutePositioning ? endX : currentX + endX;
		arc.endY = absolutePositioning ? endY : currentY + endY;
		arc.clockwise = clockwise;

		// Check for movement
		if (abs(arc.endX - arc.startX) < 0.001 && abs(arc.endY - arc.startY) < 0.001) {
			LOG_WARNING("Warning: No arc movement - start and end points are the same");
			arc.radius = 0;
			return arc;
		}

		if (r > 0) {
			// Calculation via radius
			double dx = arc.endX - arc.startX;
			double dy = arc.endY - arc.startY;
			double chordLength = std::sqrt(dx * dx + dy * dy);

			if (chordLength == 0) {
				LOG_ERROR("Chord length is zero");
				arc.radius = 0;
				return arc;
			}

			if (chordLength > 2 * r) {
				LOG_ERROR("Radius too small. Radius: %f, Choord: %f", r, chordLength);
				arc.radius = 0;
				return arc;
			}

			double chordHalf = chordLength / 2.0;
			double h = std::sqrt(r * r - chordHalf * chordHalf);

			// Perpendicular to the chord
			double dxPerp = -dy / chordLength;
			double dyPerp = dx / chordLength;

			// Midpoint of the chord
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
			// Calculation via offsets I, J
			arc.centerX = arc.startX + i;
			arc.centerY = arc.startY + j;
			arc.radius = std::sqrt(i * i + j * j);

			if (arc.radius < 0.001) {
				LOG_ERROR("Arc radius too small");
				return arc;
			}
		}

		// Calculate angles
		arc.startAngle = std::atan2(arc.startY - arc.centerY, arc.startX - arc.centerX);
		arc.endAngle = std::atan2(arc.endY - arc.centerY, arc.endX - arc.centerX);

		// Correcting angles for direction
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

		// If feedrate is not specified, use the maximum of the X/Y axes
		if (feedrate <= 0) {
			feedrate = std::max(stepperX.maximumFeedrate, stepperY.maximumFeedrate);
			LOG_WARNING("Warning: Using default feedrate: %f mm/min", feedrate);
		}

		// Determine which axis it is (X or Y)
		bool isXAxis = (config.ports[0] == stepperX.ports[0]);

		// The initial position in millimeters for this axis
		double startPosMm = isXAxis ? arc.startX : arc.startY;

		// Break the arc into segments (increase the number of segments for smoothness)
		std::vector<LinearSegment> segments = approximateArcWithLines(
			arc.centerX, arc.centerY, arc.radius,
			arc.startAngle, arc.endAngle, arc.clockwise,
			50, 0.01); // 50 segments, error 0.01 mm

		if (segments.empty()) {
			return points;
		}

		// Calculate the total length of the path
		double totalPathLength = 0.0;
		for (const auto& segment : segments) {
			totalPathLength += segment.length;
		}

		// Total time to pass the arc (seconds)
		double totalTime = totalPathLength / (feedrate / 60.0);

		if (totalTime <= 0) {
			return points;
		}

		// Convert maximum speed from mm/min to rpm
		double maxSpeedRevPerSec = (config.maximumFeedrate * config.gearRatio) /
			(config.rotationDistance * 60.0);

		if (maxSpeedRevPerSec <= 0) {
			LOG_ERROR("MaxSpeedRevolutionsPerSecond is zero or negative");
			return points;
		}

		// Initial point with zero velocity
		SpeedProfilePoint startPoint;
		startPoint.distance = 0.0;
		startPoint.speed = 0;
		startPoint.tolerance = 0.1;
		points.push_back(startPoint);

		// Accumulated distance in revolutions
		double accumulatedRevolutions = 0.0;

		// Skip the first point (it's already added)
		for (size_t i = 0; i < segments.size(); i++) {
			const auto& segment = segments[i];

			// Determine the coordinate for the current axis
			double axisPos = isXAxis ? segment.endX : segment.endY;

			// Offset from starting point (mm)
			double linearDisplacement = axisPos - startPosMm;

			// Convert to stepper motor revolutions (ABSOLUTE value, always positive)
			double revolutions = (std::abs(linearDisplacement) * config.gearRatio) / config.rotationDistance;

			// Direction of movement (offset sign)
			double direction = (linearDisplacement >= 0) ? 1.0 : -1.0;

			// Time to pass this segment
			double segmentTime = segment.length / (feedrate / 60.0);

			if (segmentTime <= 0) {
				continue; // Skip zero segments
			}

			// Speed ​​for this segment (mm/sec)
			double segmentSpeedMmPerSec = (axisPos - (isXAxis ? segment.startX : segment.startY)) / segmentTime;

			// Convert to rpm
			double segmentSpeedRevPerSec = (std::abs(segmentSpeedMmPerSec) * config.gearRatio) / config.rotationDistance;

			// Percentage of maximum speed taking into account direction
			double speedPercent = (segmentSpeedRevPerSec / maxSpeedRevPerSec) * 100.0 * direction;

			// Taking into account the motor direction from the configuration
			if (!config.direction) {
				speedPercent = -speedPercent;
			}

			// Speed ​​limit
			double minSpeedPercent = (config.minimumFeedrate / config.maximumFeedrate) * 100.0;

			if (speedPercent > 0) {
				speedPercent = std::max(speedPercent, minSpeedPercent);
				speedPercent = std::min(speedPercent, 100.0);
			}
			else {
				speedPercent = std::min(speedPercent, -minSpeedPercent);
				speedPercent = std::max(speedPercent, -100.0);
			}

			// Absolute distance in revolutions (never negative!)
			accumulatedRevolutions = revolutions;

			SpeedProfilePoint point;
			point.distance = accumulatedRevolutions; // ABSOLUTE distance from the start
			point.speed = static_cast<signed char>(std::round(speedPercent));
			point.tolerance = 0.1;

			points.push_back(point);
		}

		// Add a final point with zero velocity
		if (!points.empty()) {
			SpeedProfilePoint stopPoint;
			stopPoint.distance = points.back().distance;
			stopPoint.speed = 0;
			stopPoint.tolerance = 1.0;
			points.push_back(stopPoint);
		}

		// Debugging
		if (isXAxis) {
			LOG_ARC("Generated %d points for X axis, total revolutions: %d", points.size(), points.back().distance);
		}
		else {
			LOG_ARC("Generated %d points for Y axis, total revolutions: %d", points.size(), points.back().distance);
		}

		return points;
	}

	bool validateArc(const ArcParameters& arc) {
		if (arc.radius < 0.1) {
			LOG_ERROR("Invalid arc radius: %f", arc.radius);
			return false;
		}

		// Check if the end point lies on the circle
		double distanceToEnd = std::sqrt(
			(arc.endX - arc.centerX) * (arc.endX - arc.centerX) +
			(arc.endY - arc.centerY) * (arc.endY - arc.centerY)
		);

		if (std::abs(distanceToEnd - arc.radius) > 0.1) {
			LOG_ARC("End point is not on arc circle. Radius: %f, Distance to end: %f", arc.radius, distanceToEnd);
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

	// Improved executeArcMovement method with linear approximation
	void executeArcMovement(const ArcParameters& arc, double feedrate) {
		if (!currentPrinter || !currentPrinter->vtable) {
			return;
		}

		if (!validateArc(arc)) {
			return;
		}

		// Automatic detection of the number of segments
		double arcLength = arc.radius * std::abs(arc.endAngle - arc.startAngle);
		int segments = static_cast<int>(std::ceil(arcLength / 1.0)); // 1 mm per segment
		segments = std::max(10, std::min(segments, 200));

		// Generate points using linear approximation
		std::vector<SpeedProfilePoint> pointsX = generateArcPointsLinearApproximation(
			stepperX, arc, feedrate);
		std::vector<SpeedProfilePoint> pointsY = generateArcPointsLinearApproximation(
			stepperY, arc, feedrate);

		if (pointsX.size() != pointsY.size() || pointsX.size() < 3) {
			LOG_ERROR("Failed to generate arc points with linear approximation");
			return;
		}

		// Debugging
		debugArcPoints(pointsX, "X (Linear)");
		debugArcPoints(pointsY, "Y (Linear)");

		// Create profiles for all ports
		std::vector<SpeedProfile> allProfiles;

		// For the X axis
		for (uint8_t port : stepperX.ports) {
			SpeedProfile profile;
			profile.port = port;
			profile.count = static_cast<int>(pointsX.size());
			profile.timeoutMs = static_cast<int>((arcLength / (feedrate / 60.0)) * 1000) + 1000;

			profile.points = new SpeedProfilePoint[pointsX.size()];
			std::copy(pointsX.begin(), pointsX.end(), profile.points);

			allProfiles.push_back(profile);
		}

		// For Y axis
		for (uint8_t port : stepperY.ports) {
			SpeedProfile profile;
			profile.port = port;
			profile.count = static_cast<int>(pointsY.size());
			profile.timeoutMs = static_cast<int>((arcLength / (feedrate / 60.0)) * 1000) + 1000;

			profile.points = new SpeedProfilePoint[pointsY.size()];
			std::copy(pointsY.begin(), pointsY.end(), profile.points);

			allProfiles.push_back(profile);
		}

		// We execute all profiles
		bool success = true;
		for (const auto& profile : allProfiles) {
			if (!currentPrinter->vtable->printer_printer_execute_speed_profile(currentPrinter, &profile)) {
				success = false;
				break;
			}
		}

		// Cleanup
		for (auto& profile : allProfiles) {
			delete[] profile.points;
		}

		if (success) {
			// Update the current position
			currentX = arc.endX;
			currentY = arc.endY;
			LOG_ARC("Arc movement completed with linear approximation");
		}
	}

	void stopMotorsAfterProfile(const StepperConfig& config) {
		if (!currentPrinter || !currentPrinter->vtable) {
			return;
		}

		for (uint8_t port : config.ports) {
			currentPrinter->vtable->printer_set_motor_speed(currentPrinter, port, 0);
			LOG_DEBUG("Stopped motor on port %c", port);
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
			LOG_ARC("%s Point %d: distance = %f, speed = %d", axisName.c_str(), i, points[i].distance, speed);
		}

		LOG_ARC("%s Summary: total distance = %f, est time = %f seconds", axisName.c_str(), totalDistance, totalTime);
	}

	void debugArcCalculation(const ArcParameters& arc, double feedrate) {
		LOG_DEBUG("=== DEBUG ARC CALCULATION ===");
		LOG_DEBUG("Start: (%f , %f)", arc.startX, arc.startY);
		LOG_DEBUG("End: (%f, %f)", arc.endX, arc.endY);
		LOG_DEBUG("Center (%f, %f)", arc.centerX, arc.centerY);
		LOG_DEBUG("Radius: %f", arc.radius);
		LOG_DEBUG("Start angle: %f degrees", arc.startAngle * 180.0 / PI);
		LOG_DEBUG("End angle: %f degrees", arc.endAngle * 180.0 / PI);
		LOG_DEBUG("Total angle: %f degress", (arc.endAngle - arc.startAngle) * 180.0 / PI);
		LOG_DEBUG("Feedrate: %f mm/min", feedrate);

		// Calculating the approximate speed
		double arcLength = arc.radius * std::abs(arc.endAngle - arc.startAngle);
		LOG_DEBUG("Arc length: %f mm", arcLength);
		LOG_DEBUG("Estimated time: %f seconds", arcLength / (feedrate / 60.0));
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

			logBuffer = std::make_unique<LogEntry[]>(MAX_LOG_ENTRIES);		
			enabledCategories.store(LOG_CATEGORY_RELEASE, std::memory_order_relaxed);

			stepperX = StepperConfig();
			stepperY = StepperConfig();
			stepperZ = StepperConfig();

			LOG_INFO("Interpreter initialized successfully");
		}
		catch (const std::exception& ex) {
			LOG_ERROR("Error in interpreter constructor: %s", ex.what());
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

		LOG_INFO("Interpreter destroyed");
		gActiveInterpreters--;
	}

	void setLogCategories(uint32_t categories) {
		enabledCategories.store(categories, std::memory_order_relaxed);
		addLogInternal(LOG_CATEGORY_INFO, "Log categories updated: 0x%08X", categories);
	}

	uint32_t getLogCategories() const {
		return enabledCategories.load(std::memory_order_relaxed);
	}

	int getLogCount() {
		size_t writeIndex = logWriteIndex.load(std::memory_order_acquire);
		size_t readIndex = logReadIndex.load(std::memory_order_acquire);

		if (writeIndex >= readIndex) {
			size_t count = writeIndex - readIndex;
			return static_cast<int>(std::min(count, MAX_LOG_ENTRIES));
		}
		else {
			size_t count = (writeIndex + MAX_LOG_ENTRIES) - readIndex;
			return static_cast<int>(std::min(count, MAX_LOG_ENTRIES));
		}
	}

	const char* getLogEntry(int index, LogCategory* outCategory = nullptr) {
		size_t readIndex = logReadIndex.load(std::memory_order_acquire);
		size_t writeIndex = logWriteIndex.load(std::memory_order_acquire);

		size_t available;
		if (writeIndex >= readIndex) {
			available = writeIndex - readIndex;
		}
		else {
			available = (writeIndex + MAX_LOG_ENTRIES) - readIndex;
		}

		available = std::min(available, MAX_LOG_ENTRIES);

		if (index < 0 || static_cast<size_t>(index) >= available) {
			return "";
		}

		size_t bufferIndex = (readIndex + index) % MAX_LOG_ENTRIES;

		if (outCategory) {
			*outCategory = logBuffer[bufferIndex].category;
		}

		return logBuffer[bufferIndex].message;
	}

	int getFilterLogCount(uint32_t categoryMask) {
		size_t writeIndex = logWriteIndex.load(std::memory_order_acquire);
		size_t readIndex = logReadIndex.load(std::memory_order_relaxed);

		int count = 0;

		std::lock_guard<std::mutex> lock(logBufferMutex);

		for (size_t i = readIndex; i < writeIndex; i++) {
			size_t index = i % MAX_LOG_ENTRIES;
			if (logBuffer[index].category & categoryMask) {
				count++;
			}
		}

		return count;
	}

	void clearLog() {
		logWriteIndex.store(0, std::memory_order_release);
		logReadIndex.store(0, std::memory_order_relaxed);

		addLogInternal(LOG_CATEGORY_INFO, "Log buffer cleared");
	}

	const char* getLastErrorMessage() {
		return lastError.empty() ? "" : lastError.c_str();
	}

	// Execute G-code from file
	bool executeFile(const char* filename, IPrinter* printer) {
		std::lock_guard<std::mutex> lock(mutex);
		LOG_EXECUTION("=== ExecuteFile called ===");
		LOG_EXECUTION("Current status: %f", static_cast<int>(status));
		LOG_EXECUTION("Printer valid: %f", std::string(printer && printer->vtable ? "YES" : "NO"));

		if (!filename) {
			LOG_ERROR("Filename: NULL");
			return false;
		}

		if (strlen(filename) == 0) {
			LOG_ERROR("Filename: EMPTY STRING");
			return false;
		}

		if (threadRunning) {
			LOG_WARNING("Interpreter is already executing");
			return false;
		}

		// Clean up previous thread
		if (executionThread && executionThread->joinable()) executionThread->detach();

		LOG_EXECUTION("Filename: %f", filename);
		LOG_EXECUTION("Filename length: %f", strlen(filename));

		if (status == RUNNING) {
			LOG_WARNING("Interpreter ia already running");
			return false;
		}

		if (!printer || !printer->vtable) {
			LOG_ERROR("Invalid printer instance");
			return false;
		}

		// Check if file exists
		std::ifstream testFile(filename);
		if (!testFile.is_open()) {
			LOG_ERROR("File does not exist or cannot be opened: %f", filename);
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

		LOG_EXECUTION("Starting execution thread...");
		LOG_DEBUG("Reset coordinates to X = 0; Y = 0; Z = 0");

		std::string filenameCopy = filename;
		executionThread = std::make_unique<std::thread>([this, filenameCopy]() {
				LOG_DEBUG("Execution thread started");
				LOG_EXECUTION("Thread filename: %f", filenameCopy);
				runFile(filenameCopy);
				LOG_EXECUTION("Execution thread finished");
				threadRunning = false;
			});

		LOG_DEBUG("Execution started: %f", filename);
		return true;
	}

	void pause() // Pause execution
	{
		if (status == RUNNING) {
			pauseRequested = true;
			status = PAUSED;
			LOG_INFO("Execution paused");
		}
		else {
			LOG_WARNING("Pause request ignored - interpreter not running");
		}
	}

	void resume() { // Resume execution
		if (status == PAUSED) {
			pauseRequested = false;
			status = RUNNING;
			LOG_INFO("Execution resumed");
		}
		else {
			LOG_WARNING("Resume request ignored - interpreter not paused");
		}
	}

	void stop()	{ // Stop execution
		LOG_DEBUG("Stop requested");
		stopRequested = true;
		if (status == RUNNING || status == PAUSED || status == CHECKING_CODE) {
			status = IDLE;
			LOG_INFO("Execution stopped by user request");
		}

		if (executionThread->joinable()) {
			executionThread->join();
			LOG_INFO("Execution thread joined");
		}
	}

	Status getStatus() { return status; }

	double getProgress() { return progress; }

	const char* getLastError() {
		std::lock_guard<std::mutex> lock(logMutex);
		return cacheString(lastError);
	}

	double getSpeed() { return speed; }

	bool testFunction(IPrinter* printer) {
		currentPrinter = printer;
		LOG_DEBUG("Test function started");

		if (!currentPrinter || !currentPrinter->vtable)	{
			LOG_ERROR("Printer is not available for test");
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

		LOG_DEBUG("Multi profile test completed");
		return true;
	}

	bool readConfigFile(const std::string& filename) {
		LOG_CONFIG("Reading interpreter config from: %s", filename.c_str());

		try {
			std::ifstream file(filename);
			if (!file.is_open()) {
				LOG_ERROR("Cannot open file: %s", filename.c_str());
				return false;
			}

			std::string line;
			Section currentSection = Section::UNKNOWN;
			int lineNumber = 0;
			int processedSections = 0;

			while (std::getline(file, line)) {
				lineNumber++;
				if (stopRequested) {
					LOG_INFO("Config reading interrupted by stop request");
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
						LOG_CONFIG("Found interpreter section: %s", sectionName.c_str());
					}
					else {
						// Ignore non-interpreter sections
						LOG_CONFIG("Ignoring non-interpreter section: %s", sectionName.c_str());
					}
					continue;
				}

				// Process only lines in interpreter sections
				if (currentSection == Section::UNKNOWN) continue;

				// Process key=value pairs
				size_t delimiterPosition = line.find('=');
				if (delimiterPosition == std::string::npos) {
					LOG_CONFIG("Invalid config line in interpreter section: %s", line.c_str());
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
						LOG_WARNING("Unknown interpreter config key: %s", key.c_str());
					}
					else {
						setConfigValue(currentSection, configKey, value);

						// Check status after setting value
						if (status == ERROR) {
							LOG_ERROR("Error setting config value for key: %s", key.c_str());
							file.close();
							return false;
						}
					}
				}
			}

			file.close();

			// Validate required settings
			if (!validateConfig()) {
				LOG_ERROR("Configuration validation failed");
			}

			if (status != ERROR) {
				LOG_CONFIG("Interpreter config loaded - processed %d sections", processedSections);
				return true;
			}

			return false;
		}
		catch (const std::exception& ex) {
			LOG_ERROR("Error with read: %s config file: %s", filename.c_str(), ex.what());
			lastError = ex.what();
			status = ERROR;
		}
	}	

	bool executeLine(const std::string& line, IPrinter* printer) {
		LOG_EXECUTION("ExecuteLine started: %s", line.c_str());

		if (status == RUNNING) {
			LOG_ERROR("Interpreter is already running");
			return false;
		}

		if (!printer || !printer->vtable) {
			LOG_ERROR("Invalid printer instance");
			return false;
		}
		
		currentPrinter = printer;

		if (!currentPrinter || !currentPrinter->vtable) {
			LOG_ERROR("Printer is not available for execution");
			status = ERROR;
			return false;
		}

		if (threadRunning) {
			LOG_ERROR("Interpreter is already executing");
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
				LOG_INFO("Execution aborted due to errors");
				threadRunning = false;
				return false;
			}

			// Second pass: execution
			status = RUNNING;
			processLine(line, 1, false);

			if (!stopRequested)	{
				LOG_EXECUTION("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else {
				LOG_EXECUTION("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex) {
			LOG_ERROR("Runtime error: %s", ex.what());
			lastError = ex.what();
			status = ERROR;
		}

		threadRunning = false;
		LOG_EXECUTION("Line executed successfully!");
		return true;
	}

private:

	bool validateConfig() { // Validate configuration
		if (stepperX.ports.empty())	{
			LOG_ERROR("Stepper X has no ports configured");
			return false;
		}
		if (stepperX.rotationDistance <= 0) {
			LOG_ERROR("Stepper X rotation distance not set");
			return false;
		}

		if (stepperY.ports.empty()) {
			LOG_ERROR("Stepper Y has no ports configured");
			return false;
		}
		if (stepperY.rotationDistance <= 0) {
			LOG_ERROR("Stepper Y rotation distance not set");
			return false;
		}

		if (stepperZ.ports.empty()) {
			LOG_ERROR("Stepper Z has no ports configured");
			return false;
		}
		if (stepperZ.rotationDistance <= 0) {
			LOG_ERROR("Stepper Z rotation distance not set");
			return false;
		}

		// Validate speed ranges
		if (stepperX.minimumFeedrate >= stepperX.maximumFeedrate) {
			LOG_ERROR("Stepper X feedrate range invalid");
			return false;
		}
		if (stepperY.minimumFeedrate >= stepperY.maximumFeedrate) {
			LOG_ERROR("Stepper Y feedrate range invalid");
			return false;
		}
		if (stepperZ.minimumFeedrate >= stepperZ.maximumFeedrate) {
			LOG_ERROR("Stepper Z feedrate range invalid");
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
			LOG_ERROR("CRITICAL ERROR in RunFile: %s", ex.what());
			status = ERROR;
			threadRunning = false;
		}
		catch (...)	{
			LOG_ERROR("CRITICAL ERROR: Unknown exception in RunFile");
			status = ERROR;
			threadRunning = false;
		}
	}

	void runFileInternal(const std::string& filename) {
		LOG_INFO("run file started: %s", filename.c_str());

		if (!currentPrinter || !currentPrinter->vtable)	{
			LOG_ERROR("Printer is not available for execution");
			status = ERROR;
			return;
		}

		try	{
			// First pass: syntax checking
			status = CHECKING_CODE;
			std::ifstream file(filename);
			if (!file.is_open()) {
				LOG_ERROR("Cannot open file: %s", filename.c_str());
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
				LOG_INFO("Execution aborted due to errors");
				status = ERROR;
				threadRunning = false;
				return;
			}

			// Second pass: execution
			status = RUNNING;
			std::ifstream file2(filename);
			if (!file2.is_open()) {
				LOG_ERROR("Cannot open file: %s", filename.c_str());
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
				LOG_INFO("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else {
				LOG_INFO("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex) {
			LOG_ERROR("Runtime error: %s", ex.what());
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
			LOG_EXECUTION("Empty command in line: %s", line.c_str());
			return;
		}

		if (isTryingInterpret) {
			try {
				LOG_EXECUTION("Syntax checking line: %d : %s", linesCount, cleanLine.c_str());
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
						LOG_ERROR("Unknown G-code: %d at line %d", gCode, linesCount);
						break;
					}
				}
				else if (command[0] == 'M' || command[0] == 'm') {
					int mCode = std::stoi(command.substr(1));
					switch (mCode) {
					case 30:
						break;
					default:
						LOG_ERROR("Unknown M-code: %d at line %d", mCode, linesCount);
						break;
					}
				}
				else if (command[0] == 'F' || command[0] == 'f') {
					try {
						double newSpeed = std::stoi(command.substr(1));
						if (newSpeed < 0) {
							LOG_ERROR("Negative feedrate not allowed: %s", command.c_str());
						}
					}
					catch (const std::exception& ex) {
						LOG_ERROR("Invalid feedrate value: %s", command.c_str());
					}
				}
				else {
					LOG_ERROR("Unknown processing command '%s' at line %d", command.c_str(), linesCount);
				}
			}
			catch (const std::exception& ex) {
				LOG_ERROR("Execution in processLine: %s at line %d", ex.what(), linesCount);
			}
		}
		else {
			try {
				LOG_EXECUTION("Executing line %d : %s", linesCount, cleanLine.c_str());
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
				LOG_ERROR("Exception during execution: %s at line %d", ex.what(), linesCount);
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
						LOG_ERROR("Invalid axis: %c", axis);
						return;
					}
				}
				catch (...) {
					LOG_ERROR("Invalid number format: %s", token.c_str());
					return;
				}
			}

			// Minimum requirements: X,Y and (R or I,J)
			if (!hasX || !hasY) {
				LOG_ERROR("Arc requires X and Y coordinates");
				return;
			}

			if (!hasR && (!hasI || !hasJ)) {
				LOG_ERROR("Arc requires either R or I,J parameters");
				return;
			}

			LOG_ARC("Arc syntax OK");
		}
		else {
			// Execution
			LOG_EXECUTION("Executing %s", std::string(clockwise ? "G2" : "G3").c_str());

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
				LOG_ERROR("Arc radius not specified");
				return;
			}

			try {
				ArcParameters arc = calculateArcParameters(x, y, i, j, r, clockwise);

				if (arc.radius <= 0) {
					LOG_ERROR("Invalid arc parameters");
					return;
				}

				executeArcMovement(arc, speed);
			}
			catch (const std::exception& ex) {
				LOG_ERROR("Arc error: %s", ex.what());
			}
		}
	}

	void processMovement(std::istringstream& string, int lineCount, bool isTryingInterpret) {
		std::string token;
		char axis;
		double value;		

		if (isTryingInterpret) {
			if (!currentPrinter || !currentPrinter->vtable) {
				LOG_ERROR("Printer is not available for movement");
				return;
			}

			if (stepperX.ports.empty() || stepperY.ports.empty() || stepperZ.ports.empty())	{
				LOG_ERROR("Motor ports are not configured");
				return;
			}

			LOG_EXECUTION("Checking movement command syntax");
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
					LOG_ERROR("unknown axis: %c at line %d", axis, lineCount);
					break;
				}
			}
		}
		else {
			LOG_EXECUTION("Execute movement command");

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

			LOG_EXECUTION("Execute movement command - X: %f Y: %f Z: %f", xMovement, yMovement, zMovement);

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

						LOG_EXECUTION("X axis - Port: %c, Speed: %f, Revolutions: %f", port, calculatedSpeed, revolutionsX);
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

						LOG_EXECUTION("Y axis - Port: %c Speed: %f Revolutions: %f", port, calculatedSpeed, revolutionsY);
					}
				}

				// Send synchronized commands for X and Y axis
				if (!xyCommands.empty()) {
					MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
					std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
					currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, xyCommands.size());
					delete[] finalCommands;

					LOG_EXECUTION("XY movement synchronized. Max time: %d", maxTime);
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

			LOG_EXECUTION("Movement completed. New position: X = %f Y = %f Z = %f", currentX, currentY, currentZ);
		}
	}

	void processHoming() {
		LOG_EXECUTION("Homing command started");

		double xMovement = -currentX;
		double yMovement = -currentY;
		double zMovement = -currentZ;

		LOG_EXECUTION("Execution movement command - X: %f, Y: %f, Z: %f", xMovement, yMovement, zMovement);

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

					LOG_EXECUTION("X axis - Port: %c Speed: %f Revolutions: %f", port, calculatedSpeed, revolutionsX);
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

					LOG_EXECUTION("Y axis - Port: %c Speed: %f Revolutions: %f", port, calculatedSpeed, revolutionsY);
				}
			}

			// Send synchronized commands for X and Y axis
			if (!xyCommands.empty()) {
				MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
				std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
				currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, xyCommands.size());
				delete[] finalCommands;

				LOG_EXECUTION("XY movement synchronized. Max time: %f", maxTime);
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

		LOG_EXECUTION("Movement completed. New position: X = %f Y = %f Z = %f", currentX, currentY, currentZ);

		LOG_EXECUTION("Homing completed");
	}

	void stopAllMotors() {
		if (!currentPrinter || !currentPrinter->vtable) {
			LOG_ERROR("Printer is not available for movement");
			return;
		}

		if (stepperX.ports.empty() || stepperY.ports.empty() || stepperZ.ports.empty()) {
			LOG_ERROR("Motor ports are not configured");
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

		LOG_INFO("All motors have already stopped");
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
		if (!handle || !printer || !filename) {
			return false;
		}

		return static_cast<Interpreter*>(handle)->executeFile(filename, printer);
	}

	GCODE_API bool ExecuteLine(InterpreterHandle handle, const char* line, IPrinter* printer) {

		if (!handle || !printer) {
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

	GCODE_API const char* GetError(InterpreterHandle handle, int index) {
		if (!handle) return nullptr;

		return static_cast<Interpreter*>(handle)->getLastError();
	}

	GCODE_API int GetLogCount(InterpreterHandle handle)	{
		if (!handle) return 0;

		return static_cast<Interpreter*>(handle)->getLogCount();
	}

	GCODE_API const char* GetLogEntry(InterpreterHandle handle, int index) {
		if (!handle) return nullptr;

		return static_cast<Interpreter*>(handle)->getLogEntry(index);
	}

	GCODE_API void ClearLog(InterpreterHandle handle) {
		if (handle)	{
			static_cast<Interpreter*>(handle)->clearLog();
		}
	}

	GCODE_API void SetLogCategories(InterpreterHandle handle, unsigned int categories) {
		if (!handle) return;

		static_cast<Interpreter*>(handle)->setLogCategories(categories);
	}

	GCODE_API unsigned int GetLogCategories(InterpreterHandle handle) {
		if (!handle) return 0;

		return static_cast<Interpreter*>(handle)->getLogCategories();
	}

	GCODE_API int GetFilterLogCount(InterpreterHandle handle, unsigned int categoryMask) {
		if (!handle) return 0;

		return static_cast<Interpreter*>(handle)->getFilterLogCount(categoryMask);
	}

	GCODE_API bool ReadConfig(InterpreterHandle handle, const char* filename) {
		if (!handle || !filename) return false;

		return static_cast<Interpreter*>(handle)->readConfigFile(filename);
	}
}
