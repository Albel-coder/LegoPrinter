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

// Global mutex for thread-safe string access
std::mutex stringCacheMutex;
std::map<int, std::string> stringCache;
int nextStringId = 0;
std::mutex gInterpreterMutex;
std::atomic<int> gActiveInterpreters(0);

// Functions for caching strings
const char* cacheString(const std::string& string)
{
	std::lock_guard<std::mutex> lock(stringCacheMutex);
	int id = nextStringId;
	stringCache[id] = string;
	return stringCache[id].c_str();
}

// Function for clearing cache (optional)
void clearStringCache()
{
	std::lock_guard<std::mutex> lock(stringCacheMutex);
	stringCache.clear();
	nextStringId = 0;
}

enum GcodeError // Interpreter error enumeration
{
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

enum Status // Interpreter execution status
{
	IDLE = 0,
	CHECKING_CODE = 1,
	RUNNING = 2,
	PAUSED = 3,
	COMPLETED = 4,
	ERROR = 5
};

enum class ConfigKey // Configuration keys
{
	ROTATE_DISTANCE = 0,
	DISTANCE_TO_PRINT_POSITION = 1,
	GEAR_RATIO = 2,
	DIRECTION = 3,
	PORTS = 4,
	MINIMUM_FEEDRATE = 5,
	MAXIMUM_FEEDRATE = 6,
	UNKNOWN = 7
};

enum class Section // Configuration sections
{
	STEPPER_X = 0,
	STEPPER_Y = 1,
	STEPPER_Z = 2,
	UNKNOWN = 3
};

class Interpreter
{
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

	struct StepperConfig // Stepper motor configuration
	{
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

	void addLogEntry(const std::string& entry)
	{
		std::unique_lock<std::mutex> lock(logMutex, std::try_to_lock);
		if (lock.owns_lock())
		{
			// Size limit without recursion
			if (executionLog.size() >= 1000)
			{
				executionLog.erase(executionLog.begin(), executionLog.begin() + 100);

				static bool cleanupMessageAdded = false;
				if (!cleanupMessageAdded)
				{
					executionLog.push_back("Log buffer limit reached - old entries are being removed");
					cleanupMessageAdded = true;
				}	
			}

			executionLog.push_back(entry);
		}
	}

	// Add G-code error information
	void addGCodeErrorInfo(const std::string& code, GcodeError errorType = VALUE_NOT_DEFINED)
	{
		std::unique_lock<std::mutex> lock(logMutex, std::try_to_lock);

		if (gCodeErrors.size() > 999)
		{
			gCodeErrors.erase(gCodeErrors.begin());
		}

		std::string ErrorInfo;
		currentError = errorType;

		switch (errorType)
		{
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
		if (lock.owns_lock())
		{
			if (executionLog.size() > 999)
			{
				executionLog.erase(executionLog.begin());
			}
			executionLog.push_back("[ERROR] " + ErrorInfo);
		}
	}

	void setError(GcodeError error, const std::string& message)
	{
		addGCodeErrorInfo(message, error);
	}

	Section stringToSection(const std::string& section)
	{
		if (section == "stepper_x")
		{
			return Section::STEPPER_X;
		}
		if (section == "stepper_y")
		{
			return Section::STEPPER_Y;
		}
		if (section == "stepper_z")
		{
			return Section::STEPPER_Z;
		}

		addLogEntry("Unknown section in configuration: " + section);
		return Section::UNKNOWN;
	}

	// Convert std::string to configuration key
	ConfigKey stringToKey(const std::string& key)
	{
		static const std::unordered_map<std::string, ConfigKey> keyMap =
		{
			{ "rotation_distance", ConfigKey::ROTATE_DISTANCE },
			{ "distance_to_print_position", ConfigKey::DISTANCE_TO_PRINT_POSITION },
			{ "gear_ratio", ConfigKey::GEAR_RATIO },
			{ "direction", ConfigKey::DIRECTION },
			{ "ports", ConfigKey::PORTS },
			{ "minimum_feedrate", ConfigKey::MINIMUM_FEEDRATE },
			{ "maximum_feedrate", ConfigKey::MAXIMUM_FEEDRATE }
		};

		auto it = keyMap.find(key);
		if (it != keyMap.end())
		{
			return it->second;
		}

		addLogEntry("Unknown configuration key: " + key);
		return ConfigKey::UNKNOWN;
	}

	double evaluateExpression(const std::string& expression)
	{
		std::string Processed = expression;
	}

	std::string parseValue(const std::string& value)
	{
		if (value.find('{') == std::string::npos || value.find('}') == std::string::npos)
		{
			return value;
		}

		// TODO: Implement expression parsing if needed

		return value;
	}

	void setConfigValue(Section section, ConfigKey key, const std::string& value)
	{
		StepperConfig* config = nullptr;

		switch (section)
		{
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

		switch (key)
		{
		case ConfigKey::ROTATE_DISTANCE:
			try
			{
				config->rotationDistance = std::stod(parseValue(value));
				addLogEntry("Set rotation_distance: " + std::to_string(config->rotationDistance));
			}
			catch (const std::exception& ex)
			{				
				addGCodeErrorInfo("Invalid rotation distance value: " + std::to_string(config->rotationDistance), CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::GEAR_RATIO:
			try
			{
				config->gearRatio = std::stod(parseValue(value));
				addLogEntry("Set gear_ratio: " + std::to_string(config->gearRatio));
			}
			catch (const std::exception& ex)
			{
				addGCodeErrorInfo("Invalid gear ratio value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::DIRECTION:
			try
			{
				std::string directionString = parseValue(value);
				std::transform(directionString.begin(), directionString.end(), directionString.begin(), ::tolower);

				if (directionString == "clockwise" || directionString == "cw")
				{
					config->direction = true;
					addLogEntry("Set direction: clockwise (true)");
				}
				else if (directionString == "counterclockwise" || directionString == "ccw")
				{
					config->direction = false;
					addLogEntry("Set direction: counterclockwise (false)");
				}
				else
				{
					// Try to convert as number for backward compatibility
					config->direction = std::stoi(directionString) != 0;
					addLogEntry("Set direction: " + std::to_string(config->direction));
				}
			}
			catch (const std::exception& ex)
			{
				addGCodeErrorInfo("Invalid direction value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;
			
		case ConfigKey::PORTS:
			try
			{
				std::vector<uint8_t> ports;
				addLogEntry("Processing ports configuration: " + value);

				std::string processedValue = value;
				processedValue.erase(std::remove(processedValue.begin(), processedValue.end(), ' '), processedValue.end());
				processedValue.erase(std::remove(processedValue.begin(), processedValue.end(), ','), processedValue.end());
				processedValue.erase(std::remove(processedValue.begin(), processedValue.end(), ';'), processedValue.end());

				for (char character : processedValue)
				{
					uint8_t portValue = 0xFF;
					switch(character)
					{
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
					for (auto existingPort : ports)
					{
						if (existingPort == portValue)
						{
							isDuplicate = true;
							break;
						}
					}

					if (!isDuplicate)
					{
						ports.push_back(portValue);
						addLogEntry("Added port " + std::string(1, character));
					}
					else
					{
						addLogEntry("Duplicate port detected: " + std::string(1, character));
					}
				}				

				config->ports = ports;
				addLogEntry("Ports configuration completed. Total ports: " + std::to_string(config->ports.size()));

				if (config->ports.empty())
				{
					addLogEntry("WARNING: No valid ports configured!");
				}
				else
				{
					std::string portsList = "Configured ports: ";
					for (auto port : config->ports)
					{
						portsList += std::to_string(port) + " ";
					}

					addLogEntry(portsList);
				}
			}
			catch (const std::exception& ex)
			{
				addGCodeErrorInfo("Invalid ports configuration: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MINIMUM_FEEDRATE:
			try
			{
				config->minimumFeedrate = std::stod(parseValue(value));
				addLogEntry("Set miminum_feedrate: " + value);
			}
			catch (const std::exception& ex)
			{
				addGCodeErrorInfo("Invalid minimum feedrate value: " + value, CONFIG_ERROR);
				lastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MAXIMUM_FEEDRATE:
			try
			{
				config->maximumFeedrate = std::stod(parseValue(value));
				addLogEntry("Set maximum_feedrate: " + value);
			}
			catch (const std::exception& ex)
			{
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
	struct MovementCalculation
	{
		double revolutions;
		double baseSpeed;
		double time;
	};

	MovementCalculation calculateAxisMovement(const StepperConfig& config, double movement, double defaultSpeed)
	{
		MovementCalculation result;
		result.revolutions = (std::abs(movement) * config.gearRatio) / config.rotationDistance;

		result.baseSpeed = defaultSpeed;
		if (movement < 0)
		{
			result.baseSpeed = -result.baseSpeed;
		}
		if (!config.direction)
		{
			result.baseSpeed = -result.baseSpeed;
		}

		// Apply speed limits
		if (result.baseSpeed > 0)
		{
			result.baseSpeed = std::min(result.baseSpeed, config.maximumFeedrate);
			result.baseSpeed = std::max(result.baseSpeed, config.minimumFeedrate);
		}
		else
		{
			result.baseSpeed = std::max(result.baseSpeed, -config.maximumFeedrate);
			result.baseSpeed = std::min(result.baseSpeed, -config.minimumFeedrate);
		}

		result.time = (result.revolutions > 0 && std::abs(result.baseSpeed) > 0)
			? result.revolutions / std::abs(result.baseSpeed)
			: 0.0;

		return result;
	}

	std::vector<MotorCommand> generateMotorCommands(const StepperConfig& config, double synchronizedSpeed, double revolutions)
	{
		std::vector<MotorCommand> commands;
		for (uint8_t port : config.ports)
		{
			MotorCommand command;
			command.port = port;
			command.speed = static_cast<signed char>(synchronizedSpeed);
			command.revolutions = revolutions;
			commands.push_back(command);
		}

		return commands;
	}

public:	
	Interpreter() // Constructor
	{
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
		addLogEntry("Interpreter initialized successfully");
	}

	~Interpreter() // Destructor
	{
		std::lock_guard<std::mutex> lock(gInterpreterMutex);
		stopRequested = true; // Set stop flag
		threadRunning = false;

		if (executionThread && executionThread->joinable())
		{
			// Wait for thread to finish with timeout
			for (int i = 0; i < 50; i++) // Increased timeout to 5 seconds
			{
				if (!threadRunning)
				{
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			if (executionThread->joinable())
			{
				// Use detach instead of join to avoid blocking
				executionThread->detach();
			}
		}

		addLogEntry("Interpreter destroyed");
		gActiveInterpreters--;
	}

	// Execute G-code from file
	bool executeFile(const char* filename, IPrinter* printer)
	{
		std::lock_guard<std::mutex> lock(mutex);
		addLogEntry("=== ExecuteFile called ===");
		addLogEntry("Current status: " + std::to_string(static_cast<int>(status)));
		addLogEntry("Printer valid: " + std::string(printer && printer->vtable ? "YES" : "NO"));

		if (!filename)
		{
			addLogEntry("Filename: NULL");
			return false;
		}

		if (strlen(filename) == 0)
		{
			addLogEntry("Filename: EMPTY STRING");
			return false;
		}

		if (threadRunning)
		{
			addGCodeErrorInfo("Interpreter is already executing", PRINTER_ERROR);
			return false;
		}

		// Clean up previous thread
		if (executionThread && executionThread->joinable())
		{
			executionThread->detach();
		}

		addLogEntry("Filename: " + std::string(filename));
		addLogEntry("Filename length: " + std::to_string(strlen(filename)));

		if (status == RUNNING)
		{
			addGCodeErrorInfo("Interpreter ia already running", PRINTER_ERROR);
			return false;
		}

		if (!printer || !printer->vtable)
		{
			addGCodeErrorInfo("Invalid printer instance", PRINTER_ERROR);
			return false;
		}

		// Check if file exists
		std::ifstream testFile(filename);
		if (!testFile.is_open())
		{
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
		executionThread = std::make_unique<std::thread>([this, filenameCopy]()
			{
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
		if (status == RUNNING)
		{
			pauseRequested = true;
			status = PAUSED;
			addLogEntry("Execution paused");
		}
		else
		{
			addLogEntry("Pause request ignored - interpreter not running");
		}
	}

	void resume() // Resume execution
	{
		if (status == PAUSED)
		{
			pauseRequested = false;
			status = RUNNING;
			addLogEntry("Execution resumed");
		}
		else
		{
			addLogEntry("Resume request ignored - interpreter not paused");
		}
	}

	void stop() // Stop execution
	{
		addLogEntry("Stop requested");
		stopRequested = true;
		if (status == RUNNING || status == PAUSED || status == CHECKING_CODE)
		{
			status = IDLE;
			addLogEntry("Execution stopped by user request");
		}

		if (executionThread->joinable())
		{
			executionThread->join();
			addLogEntry("Execution thread joined");
		}
	}

	Status getStatus() // Get current status
	{
		return status;
	}

	double getProgress() // Get execution progress
	{
		return progress;
	}

	const char* getLastError() // Get last error message
	{
		std::lock_guard<std::mutex> lock(logMutex);
		return cacheString(lastError);
	}

	int getLastErrorCode() // Get last error code
	{
		return static_cast<int>(currentError.load());
	}

	double getSpeed() // Get current speed
	{
		return speed;
	}

	int getErrorCount() // Get error count
	{
		std::lock_guard<std::mutex> lock(logMutex);
		return static_cast<int>(gCodeErrors.size());
	}

	const char* GetError(int index) // Get error by index
	{
		std::lock_guard<std::mutex> lock(logMutex);
		if (index >= 0 && index < gCodeErrors.size())
		{
			return cacheString(gCodeErrors[index]);
		}

		return cacheString("");
	}

	int getLogCount() // Get log entry count
	{
		std::lock_guard<std::mutex> lock(logMutex);
		return static_cast<int>(executionLog.size());
	}

	const char* GetLogEntry(int index) // Get log entry by index
	{
		std::lock_guard<std::mutex> lock(logMutex);
		if (index >= 0 && index < executionLog.size())
		{
			return cacheString(executionLog[index]);
		}

		addLogEntry("Invalid log index requested: " + std::to_string(index));
		return cacheString("");
	}

	void clearErrors() // Clear all errors
	{
		std::lock_guard<std::mutex> lock(logMutex);
		gCodeErrors.clear();
		lastError.clear();
		currentError = NO_ERROR;
		addLogEntry("All errors cleared");
	}

	void clearLog() // Clear log
	{
		std::lock_guard<std::mutex> lock(logMutex);
		executionLog.clear();
		addLogEntry("Log cleared");
	}

	bool testFunction(IPrinter* printer) // Test function
	{
		currentPrinter = printer;
		addLogEntry("Test function started");

		if (!currentPrinter || !currentPrinter->vtable)
		{
			addGCodeErrorInfo("Printer is not available for test", PRINTER_ERROR);
			return false;
		}

		std::vector<MotorCommand> commands = {
			{0x02, 50, 1.0},
			{0x03, 50, 1.0}
		};

		addLogEntry("Sending test commands to printer");
		currentPrinter->vtable->printer_rotate_motor(printer, commands.data(), commands.size());

		// Reverse direction
		for (auto& command : commands)
		{
			command.speed = -50;
		}
		currentPrinter->vtable->printer_rotate_motor(printer, commands.data(), commands.size());

		addLogEntry("Test function completed successfully");
		return true;
	}

	bool readConfigFile(const std::string& filename) // Read configuration file
	{
		addLogEntry("Reading interpreter config from: " + filename);

		try
		{
			std::ifstream file(filename);
			if (!file.is_open())
			{
				addGCodeErrorInfo("Cannot open file: " + filename, FILE_ERROR);
				return false;
			}

			std::string line;
			Section currentSection = Section::UNKNOWN;
			int lineNumber = 0;
			int processedSections = 0;

			while (std::getline(file, line))
			{
				lineNumber++;
				if (stopRequested)
				{
					addLogEntry("Config reading interrupted by stop request");
					break;
				}

				// Remove comments and trim
				size_t commentPosition = line.find('#');
				if (commentPosition != std::string::npos)
				{
					line = line.substr(0, commentPosition);
				}

				// Trim whitespace
				line.erase(0, line.find_first_not_of(" \t"));
				line.erase(line.find_last_not_of(" \t") + 1);

				if (line.empty())
				{
					continue;
				}

				// Process sections [section]
				if (line.front() == '[' && line.back() == ']')
				{
					std::string sectionName = line.substr(1, line.length() - 2);
					currentSection = stringToSection(sectionName);

					if (currentSection != Section::UNKNOWN)
					{
						processedSections++;
						addLogEntry("Found interpreter section: " + sectionName);
					}
					else
					{
						// Ignore non-interpreter sections
						addLogEntry("Ignoring non-interpreter section: " + sectionName);
					}
					continue;
				}

				// Process only lines in interpreter sections
				if (currentSection == Section::UNKNOWN)
				{
					continue;
				}

				// Process key=value pairs
				size_t delimiterPosition = line.find('=');
				if (delimiterPosition == std::string::npos)
				{
					addLogEntry("Invalid config line in interpreter section: " + line);
					continue;
				}
				else
				{
					std::string key = line.substr(0, delimiterPosition);
					std::string value = line.substr(delimiterPosition + 1);

					// Trim whitespace around key and value
					key.erase(0, key.find_first_not_of(" \t"));
					key.erase(key.find_last_not_of(" \t") + 1);
					value.erase(0, value.find_first_not_of(" \t"));
					value.erase(value.find_last_not_of(" \t") + 1);

					ConfigKey configKey = stringToKey(key);
					if (configKey == ConfigKey::UNKNOWN)
					{
						addLogEntry("Unknown interpreter config key: " + key);
					}
					else
					{
						setConfigValue(currentSection, configKey, value);

						// Check status after setting value
						if (status == ERROR)
						{
							addLogEntry("Error setting config value for key: " + key);
							file.close();
							return false;
						}
					}
				}
			}
			
			file.close();

			// Validate required settings
			if (!validateConfig())
			{
				addGCodeErrorInfo("Configuration validation failed", CONFIG_ERROR);
			}

			if (status != ERROR)
			{
				addLogEntry("Interpreter config loaded - processed " +
				std::to_string(processedSections) + " sections");
				return true;
			}

			return false;
		}
		catch (const std::exception& ex)
		{
			addLogEntry("Error with read: " + filename + " config file: " + ex.what());
			lastError = ex.what();
			status = ERROR;
		}
	}	

	bool executeLine(const std::string& line, IPrinter* printer)
	{
		addLogEntry("ExecuteLine started: " + line);

		if (status == RUNNING)
		{
			addGCodeErrorInfo("Interpreter ia already running", PRINTER_ERROR);
			return false;
		}

		if (!printer || !printer->vtable)
		{
			addGCodeErrorInfo("Invalid printer instance", PRINTER_ERROR);
			return false;
		}
		
		currentPrinter = printer;

		if (!currentPrinter || !currentPrinter->vtable)
		{
			addGCodeErrorInfo("Printer is not available for execution", PRINTER_ERROR);
			status = ERROR;
			return false;
		}

		if (threadRunning)
		{
			addGCodeErrorInfo("Interpreter is already executing", PRINTER_ERROR);
			return false;
		}

		// Clean up previous thread
		if (executionThread && executionThread->joinable())
		{
			executionThread->detach();
		}

		try
		{
			// Try interpret single line to find errors
			status = CHECKING_CODE;
			bool hasErrors = false;
			processLine(line, 1, true);

			if (status == ERROR)
			{
				addLogEntry("Execution aborted due to errors");
				threadRunning = false;
				return false;
			}

			// Second pass: execution
			status = RUNNING;
			processLine(line, 1, false);

			if (!stopRequested)
			{
				addLogEntry("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else
			{
				addLogEntry("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex)
		{
			addGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			lastError = ex.what();
			status = ERROR;
		}

		threadRunning = false;
		addLogEntry("Line executed successfully!");
		return true;
	}

private:

	bool validateConfig() // Validate configuration
	{
		if (stepperX.ports.empty())
		{
			addGCodeErrorInfo("Stepper X has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (stepperX.rotationDistance <= 0)
		{
			addGCodeErrorInfo("Stepper X rotation distance not set", CONFIG_ERROR);
			return false;
		}

		if (stepperY.ports.empty())
		{
			addGCodeErrorInfo("Stepper Y has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (stepperY.rotationDistance <= 0)
		{
			addGCodeErrorInfo("Stepper Y rotation distance not set", CONFIG_ERROR);
			return false;
		}

		if (stepperZ.ports.empty())
		{
			addGCodeErrorInfo("Stepper Z has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (stepperZ.rotationDistance <= 0)
		{
			addGCodeErrorInfo("Stepper Z rotation distance not set", CONFIG_ERROR);
			return false;
		}

		// Validate speed ranges
		if (stepperX.minimumFeedrate >= stepperX.maximumFeedrate)
		{
			addGCodeErrorInfo("Stepper X feedrate range invalid", CONFIG_ERROR);
			return false;
		}
		if (stepperY.minimumFeedrate >= stepperY.maximumFeedrate)
		{
			addGCodeErrorInfo("Stepper Y feedrate range invalid", CONFIG_ERROR);
			return false;
		}
		if (stepperZ.minimumFeedrate >= stepperZ.maximumFeedrate)
		{
			addGCodeErrorInfo("Stepper Z feedrate range invalid", CONFIG_ERROR);
			return false;
		}

		return true;
	}

	// Execute G-code file
	void runFile(const std::string& filename)
	{
		try
		{
			RunFileInternal(filename);
		}
		catch (const std::exception& ex)
		{
			addLogEntry("CRITICAL ERROR in RunFile: " + std::string(ex.what()));
			status = ERROR;
			threadRunning = false;
		}
		catch (...)
		{
			addLogEntry("CRITICAL ERROR: Unknown exception in RunFile");
			status = ERROR;
			threadRunning = false;
		}
	}

	void RunFileInternal(const std::string& filename)
	{
		addLogEntry("Runfile started: " + filename);

		if (!currentPrinter || !currentPrinter->vtable)
		{
			addGCodeErrorInfo("Printer is not available for execution", PRINTER_ERROR);
			status = ERROR;
			return;
		}

		try
		{
			// First pass: syntax checing
			status = CHECKING_CODE;
			std::ifstream file(filename);
			if (!file.is_open())
			{
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
			while (std::getline(file, line))
			{
				waitIfPaused();
				if (stopRequested)
				{
					break;
				}

				while (pauseRequested && !stopRequested)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}

				if (stopRequested)
				{
					break;
				}

				processLine(line, linesCount, true);
				linesCount++;

				if (status == ERROR)
				{
					hasErrors = true;
					break;
				}
			}

			file.close();

			if (hasErrors)
			{
				addLogEntry("Execution aborted due to errors");
				status = ERROR;
				threadRunning = false;
				return;
			}

			// Second pass: execution
			status = RUNNING;
			std::ifstream file2(filename);
			if (!file2.is_open())
			{
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

			while (std::getline(file2, line))
			{
				waitIfPaused();
				if (stopRequested)
				{
					break;
				}

				while (pauseRequested && !stopRequested)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}

				if (stopRequested)
				{
					break;
				}

				processLine(line, linesCount, false);
				linesCount++;

				if (totalLines > 0)
				{
					progress = static_cast<double>(linesCount) / totalLines * 100.0;
				}
			}

			file2.close();

			if (!stopRequested)
			{
				addLogEntry("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else
			{
				addLogEntry("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex)
		{
			addGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			lastError = ex.what();
			status = ERROR;
		}		

		threadRunning = false;
	}

	void waitIfPaused()
	{
		while (pauseRequested && !stopRequested)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(50));
		}
	}

	// Process G-code line
	void processLine(const std::string& line, int linesCount, bool isTryingInterpret)
	{
		// Clean line from comments and whitespace
		std::string cleanLine = line.substr(0, line.find(';'));
		cleanLine.erase(0, cleanLine.find_first_not_of(" \t"));
		cleanLine.erase(cleanLine.find_last_not_of(" \t") + 1);

		if (cleanLine.empty())
		{
			return;
		}

		std::istringstream string(cleanLine);
		std::string command;
		string >> command;

		if (isTryingInterpret)
		{
			addLogEntry("Syntax checking line: " + std::to_string(linesCount) + " : " + cleanLine);
			if (command[0] == 'G')
			{
				int gCode = std::stoi(command.substr(1));

				switch (gCode)
				{
				case 0:
				case 1:
					processMovement(string, linesCount, true);
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
					addGCodeErrorInfo("Unknown G-code: " + std::to_string(gCode) + 
						" at line " + std::to_string(linesCount), VALUE_NOT_DEFINED);
					break;
				}
			}
			else if (command[0] == 'M')
			{
				int mCode = std::stoi(command.substr(1));
				switch (mCode)
				{
				case 30:
					break;
				default:
					addGCodeErrorInfo("Unknown M-code: " + std::to_string(mCode) +
					" at line " + std::to_string(linesCount));
					break;
				}
			}
			else if (command[0] == 'F')
			{
				try
				{
					double newSpeed = std::stoi(command.substr(1));
					if (newSpeed < 0)
					{
						addGCodeErrorInfo("Negative feedrate not allowed: " + command, VALUE_NOT_DEFINED);
					}
				}
				catch (const std::exception& ex)
				{
					addGCodeErrorInfo("Invalid feedrate value: " + command, VALUE_NOT_DEFINED);
				}
			}
			else
			{
				addGCodeErrorInfo("Unknown processing command '" + command + "' " +
				" at line " + std::to_string(linesCount));
			}
		}
		else
		{
			addLogEntry("Executing line " + std::to_string(linesCount) + " : " + cleanLine);
			if (command[0] == 'G')
			{
				int gCode = std::stoi(command.substr(1));

				switch (gCode)
				{
				case 0:
				case 1:
					processMovement(string, linesCount, false);
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
			else if (command[0] == 'M')
			{
				int mCode = std::stoi(command.substr(1));
				switch (mCode)
				{
				case 30:
					stopRequested = true;
					break;
				default:
					break;
				}
			}
			else if (command[0] == 'F')
			{
				speed = std::stoi(command.substr(1));
			}
		}
	}
	
	//Process movement commands
	void processMovement(std::istringstream& string, int lineCount, bool isTryingInterpret)
	{
		std::string token;
		char axis;
		double value;		

		if (isTryingInterpret)
		{
			if (!currentPrinter || !currentPrinter->vtable)
			{
				addGCodeErrorInfo("Printer is not available for movement", PRINTER_ERROR);
				return;
			}

			if (stepperX.ports.empty() || stepperY.ports.empty() || stepperZ.ports.empty())
			{
				addGCodeErrorInfo("Motor ports are not configured", CONFIG_ERROR);
				return;
			}

			addLogEntry("Checking movement command syntax");
			while (string >> token)
			{
				axis = token[0];
				value = std::stof(token.substr(1));

				switch (axis)
				{
				case 'X':
				case 'Y':
				case 'Z':
					break;
				default:
					addGCodeErrorInfo("Unknown axis: " + std::string(1, axis) + 
					" at line " + std::to_string(lineCount));
					break;
				}
			}
		}
		else
		{
			addLogEntry("Execute movement command");

			// Initialize target coordinates
			double targetX = absolutePositioning ? currentX : 0.0;
			double targetY = absolutePositioning ? currentY : 0.0;
			double targetZ = absolutePositioning ? currentZ : 0.0;

			// Parse movement commands
			while (string >> token)
			{
				axis = token[0];
				value = std::stof(token.substr(1));

				switch (axis)
				{
				case 'X':
					if (absolutePositioning)
					{
						targetX = value;
					}
					else
					{
						targetX += value;
					}
					break;
				case 'Y':
					if (absolutePositioning)
					{
						targetY = value;
					}
					else
					{
						targetY += value;
					}
					break;
				case 'Z':
					if (absolutePositioning)
					{
						targetZ = value;
					}
					else
					{
						targetZ += value;
					}
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
			if (std::abs(xMovement) > 0 || std::abs(yMovement) > 0)
			{
				// Initialized final command for XY movement as a vector
				std::vector<MotorCommand> xyCommands;

				// Calculate movement times for each axis
				double timeX = 0.0;
				double timeY = 0.0;

				// ========== X Axis ==========
				if (std::abs(xMovement) > 0)
				{
					double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

					// Calculate base time for X movement
					double baseSpeedX = speed;
					if (xMovement < 0)
					{
						baseSpeedX = -baseSpeedX;
					}
					if (!stepperX.direction)
					{
						baseSpeedX = -baseSpeedX;
					}

					// Apply speed limits
					if (baseSpeedX > 0)
					{
						baseSpeedX = std::min(baseSpeedX, stepperX.maximumFeedrate);
						baseSpeedX = std::max(baseSpeedX, stepperX.minimumFeedrate);
					}
					else
					{
						baseSpeedX = std::max(baseSpeedX, -stepperX.maximumFeedrate);
						baseSpeedX = std::min(baseSpeedX, -stepperX.minimumFeedrate);
					}

					timeX = revolutionsX / std::abs(baseSpeedX);
				}

				// ========== Y Axis ==========
				if (std::abs(yMovement) > 0)
				{
					double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

					// Calculates base time for Y movement
					double baseSpeedY = speed;
					if (yMovement < 0)
					{
						baseSpeedY = -baseSpeedY;
					}
					if (!stepperY.direction)
					{
						baseSpeedY = -baseSpeedY;
					}

					// Apply speed limits
					if (baseSpeedY > 0)
					{
						baseSpeedY = std::min(baseSpeedY, stepperY.maximumFeedrate);
						baseSpeedY = std::max(baseSpeedY, stepperY.minimumFeedrate);
					}
					else
					{
						baseSpeedY = std::max(baseSpeedY, -stepperY.maximumFeedrate);
						baseSpeedY = std::min(baseSpeedY, -stepperY.minimumFeedrate);
					}

					timeY = revolutionsY / std::abs(baseSpeedY);
				}

				// Determine the maximum time needed
				double maxTime = std::max(timeX, timeY);
				if (maxTime == 0.0) // Avoid division by zero
				{
					maxTime = 1.0;
				}

				// ============ X Axis with synchronized speed =============
				if (std::abs(xMovement) > 0)
				{
					double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

					// Calculate speed to match the maximum time
					double synchronizedSpeedX = revolutionsX / maxTime;

					for (uint8_t port : stepperX.ports)
					{
						MotorCommand command;
						command.port = port;

						double calculatedSpeed = synchronizedSpeedX;
						if (xMovement < 0)
						{
							calculatedSpeed = -calculatedSpeed;
						}
						if (!stepperX.direction)
						{
							calculatedSpeed = -calculatedSpeed;
						}

						// Apply speed limits to synchronized speed
						if (calculatedSpeed > 0)
						{
							calculatedSpeed = std::min(calculatedSpeed, stepperX.maximumFeedrate);
							calculatedSpeed = std::max(calculatedSpeed, stepperX.minimumFeedrate);
						}
						else
						{
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
				if (std::abs(yMovement) > 0)
				{
					double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

					// Calculate speed to match the maximum time
					double synchronizedSpeedY = revolutionsY / maxTime;

					for (uint8_t port : stepperY.ports)
					{
						MotorCommand command;
						command.port = port;

						double calculatedSpeed = synchronizedSpeedY;
						if (yMovement < 0)
						{
							calculatedSpeed = -calculatedSpeed;
						}
						if (stepperY.direction)
						{
							calculatedSpeed = -calculatedSpeed;
						}

						// Apply speed limits to synchronized speed
						if (calculatedSpeed > 0)
						{
							calculatedSpeed = std::min(calculatedSpeed, stepperY.maximumFeedrate);
							calculatedSpeed = std::max(calculatedSpeed, stepperY.minimumFeedrate);
						}
						else
						{
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
				if (!xyCommands.empty())
				{
					MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
					std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
					currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, xyCommands.size());
					delete[] finalCommands;

					addLogEntry("XY movement synchronized. Max time: " + std::to_string(maxTime));
				}
			}

			// =================== Z Axis ===================
			if (std::abs(zMovement) > 0)
			{
				std::vector<MotorCommand> zCommands;
				
				for (uint8_t port : stepperZ.ports)
				{
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = speed;
					if (zMovement < 0)
					{
						calculatedSpeed = -calculatedSpeed;
					}
					if (!stepperZ.direction)
					{
						calculatedSpeed = -calculatedSpeed;
					}

					if (calculatedSpeed > 0)
					{
						calculatedSpeed = std::min(calculatedSpeed, stepperZ.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperZ.minimumFeedrate);
					}
					else
					{
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

	// Process homing command
	void processHoming()
	{
		addLogEntry("Homing command started");

		double xMovement = -currentX;
		double yMovement = -currentY;
		double zMovement = -currentZ;

		addLogEntry("Execute movement command - X:" + std::to_string(xMovement) +
			" Y: " + std::to_string(yMovement) + " Z:" + std::to_string(zMovement));

		// Process X and Y axis movement
		if (std::abs(xMovement) > 0 || std::abs(yMovement) > 0)
		{
			// Initialized final command for XY movement as a vector
			std::vector<MotorCommand> xyCommands;

			// Calculate movement times for each axis
			double timeX = 0.0;
			double timeY = 0.0;

			// ========== X Axis ==========
			if (std::abs(xMovement) > 0)
			{
				double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

				// Calculate base time for X movement
				double baseSpeedX = speed;
				if (xMovement < 0)
				{
					baseSpeedX = -baseSpeedX;
				}
				if (!stepperX.direction)
				{
					baseSpeedX = -baseSpeedX;
				}

				// Apply speed limits
				if (baseSpeedX > 0)
				{
					baseSpeedX = std::min(baseSpeedX, stepperX.maximumFeedrate);
					baseSpeedX = std::max(baseSpeedX, stepperX.minimumFeedrate);
				}
				else
				{
					baseSpeedX = std::max(baseSpeedX, -stepperX.maximumFeedrate);
					baseSpeedX = std::min(baseSpeedX, -stepperX.minimumFeedrate);
				}

				timeX = revolutionsX / std::abs(baseSpeedX);
			}

			// ========== Y Axis ==========
			if (std::abs(yMovement) > 0)
			{
				double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

				// Calculates base time for Y movement
				double baseSpeedY = speed;
				if (yMovement < 0)
				{
					baseSpeedY = -baseSpeedY;
				}
				if (!stepperY.direction)
				{
					baseSpeedY = -baseSpeedY;
				}

				// Apply speed limits
				if (baseSpeedY > 0)
				{
					baseSpeedY = std::min(baseSpeedY, stepperY.maximumFeedrate);
					baseSpeedY = std::max(baseSpeedY, stepperY.minimumFeedrate);
				}
				else
				{
					baseSpeedY = std::max(baseSpeedY, -stepperY.maximumFeedrate);
					baseSpeedY = std::min(baseSpeedY, -stepperY.minimumFeedrate);
				}

				timeY = revolutionsY / std::abs(baseSpeedY);
			}

			// Determine the maximum time needed
			double maxTime = std::max(timeX, timeY);
			if (maxTime == 0.0) // Avoid division by zero
			{
				maxTime = 1.0;
			}


			// ============ X Axis with synchronized speed =============
			if (std::abs(xMovement) > 0)
			{
				double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

				// Calculate speed to match the maximum time
				double synchronizedSpeedX = revolutionsX / maxTime;

				for (uint8_t port : stepperX.ports)
				{
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = synchronizedSpeedX;
					if (xMovement < 0)
					{
						calculatedSpeed = -calculatedSpeed;
					}
					if (!stepperX.direction)
					{
						calculatedSpeed = -calculatedSpeed;
					}

					// Apply speed limits to synchronized speed
					if (calculatedSpeed > 0)
					{
						calculatedSpeed = std::min(calculatedSpeed, stepperX.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperX.minimumFeedrate);
					}
					else
					{
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
			if (std::abs(yMovement) > 0)
			{
				double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

				// Calculate speed to match the maximum time
				double synchronizedSpeedY = revolutionsY / maxTime;

				for (uint8_t port : stepperY.ports)
				{
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = synchronizedSpeedY;
					if (yMovement < 0)
					{
						calculatedSpeed = -calculatedSpeed;
					}
					if (stepperY.direction)
					{
						calculatedSpeed = -calculatedSpeed;
					}

					// Apply speed limits to synchronized speed
					if (calculatedSpeed > 0)
					{
						calculatedSpeed = std::min(calculatedSpeed, stepperY.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperY.minimumFeedrate);
					}
					else
					{
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
			if (!xyCommands.empty())
			{
				MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
				std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
				currentPrinter->vtable->printer_rotate_motor(currentPrinter, finalCommands, xyCommands.size());
				delete[] finalCommands;

				addLogEntry("XY movement synchronized. Max time: " + std::to_string(maxTime));
			}
		}

		// =================== Z Axis ===================
		if (std::abs(zMovement) > 0)
		{
			std::vector<MotorCommand> zCommands;

			for (uint8_t port : stepperZ.ports)
			{
				MotorCommand command;
				command.port = port;

				double calculatedSpeed = speed;
				if (zMovement < 0)
				{
					calculatedSpeed = -calculatedSpeed;
				}
				if (!stepperZ.direction)
				{
					calculatedSpeed = -calculatedSpeed;
				}

				if (calculatedSpeed > 0)
				{
					calculatedSpeed = std::min(calculatedSpeed, stepperZ.maximumFeedrate);
					calculatedSpeed = std::max(calculatedSpeed, stepperZ.minimumFeedrate);
				}
				else
				{
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
};

// C API exports
extern "C"
{
	GCODE_API InterpreterHandle CreateInterpreter()
	{
		return new Interpreter();
	}

	GCODE_API void DestroyInterpreter(InterpreterHandle handle)
	{
		delete static_cast<Interpreter*>(handle);
	}

	GCODE_API bool TestCode(InterpreterHandle handle, IPrinter* printer)
	{
		if (!handle || !printer)
		{
			return false;
		}

		return static_cast<Interpreter*>(handle)->testFunction(printer);
	}

	GCODE_API bool ExecuteGcode(InterpreterHandle handle, const char* filename, IPrinter* printer)
	{
		// Добавляем детальное логирование
		printf("C++: ExecuteGcode called\n");
		printf("C++: Handle: %p\n", handle);
		printf("C++: Printer: %p\n", printer);
		if (printer) 
		{
			printf("C++: Printer->VirtualTable: %p\n", printer->vtable);
		}
		printf("C++: Filename: %s\n", filename ? filename : "NULL");

		if (!handle || !printer || !filename)
		{
			printf("C++: ERROR - Invalid parameters\n");
			return false;
		}

		// БЕЗОПАСНАЯ проверка файла с использованием fopen_s
		FILE* testFile = nullptr;
		errno_t err = fopen_s(&testFile, filename, "r");
		if (err != 0 || !testFile)
		{
			printf("C++: ERROR - Cannot open file: %s, error code: %d\n", filename, err);
			return false;
		}
		fclose(testFile);

		printf("C++: File exists, calling ExecuteFile\n");
		return static_cast<Interpreter*>(handle)->executeFile(filename, printer);
	}

	GCODE_API bool ExecuteLine(InterpreterHandle handle, const char* line, IPrinter* printer)
	{
		if (printer)
		{
			printf("C++: Printer->VirtualTable: %p\n", printer->vtable);
		}

		if (!handle || !printer)
		{
			printf("C++: ERROR - Invalid parameters\n");
			return false;
		}

		return static_cast<Interpreter*>(handle)->executeLine(line, printer);
	}

	GCODE_API void PauseExecution(InterpreterHandle handle)
	{
		if (handle)
		{
			static_cast<Interpreter*>(handle)->pause();
		}
	}

	GCODE_API void ResumeExecution(InterpreterHandle handle)
	{
		if (handle)
		{
			static_cast<Interpreter*>(handle)->resume();
		}
	}

	GCODE_API int GetStatus(InterpreterHandle handle)
	{
		if (!handle)
		{
			return -1;
		}

		return static_cast<Interpreter*>(handle)->getStatus();
	}

	GCODE_API double GetProgress(InterpreterHandle handle)
	{
		if (!handle)
		{
			return 0.0;
		}

		return static_cast<Interpreter*>(handle)->getProgress();
	}

	GCODE_API const char* GetLastInterpreterError(InterpreterHandle handle)
	{
		if (!handle)
		{
			return nullptr;
		}

		return static_cast<Interpreter*>(handle)->getLastError();
	}

	GCODE_API int GetErrorCount(InterpreterHandle handle)
	{
		if (!handle)
		{
			return 0;
		}

		return static_cast<Interpreter*>(handle)->getErrorCount();
	}

	GCODE_API const char* GetError(InterpreterHandle handle, int index)
	{
		if (!handle)
		{
			return nullptr;
		}

		return static_cast<Interpreter*>(handle)->GetError(index);
	}

	GCODE_API int GetLogCount(InterpreterHandle handle)
	{
		if (!handle)
		{
			return 0;
		}

		return static_cast<Interpreter*>(handle)->getLogCount();
	}

	GCODE_API const char* GetLogEntry(InterpreterHandle handle, int index)
	{
		if (!handle)
		{
			return nullptr;
		}

		return static_cast<Interpreter*>(handle)->GetLogEntry(index);
	}

	GCODE_API void ClearErrors(InterpreterHandle handle)
	{
		if (handle)
		{
			static_cast<Interpreter*>(handle)->clearErrors();
		}
	}

	GCODE_API void ClearLog(InterpreterHandle handle)
	{
		if (handle)
		{
			static_cast<Interpreter*>(handle)->clearLog();
		}
	}

	GCODE_API bool ReadConfig(InterpreterHandle handle, const char* filename)
	{
		if (!handle || !filename)
		{
			return false;
		}

		return static_cast<Interpreter*>(handle)->readConfigFile(filename);
	}
}
