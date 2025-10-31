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
std::mutex StringCacheMutex;
std::map<int, std::string> StringCache;
int NextStringId = 0;
std::mutex GInterpreterMutex;
std::atomic<int> GActiveInterpreters(0);

// Functions for caching strings
const char* CacheString(const std::string& String)
{
	std::lock_guard<std::mutex> Lock(StringCacheMutex);
	int Id = NextStringId;
	StringCache[Id] = String;
	return StringCache[Id].c_str();
}

// Function for clearing cache (optional)
void ClearStringCache()
{
	std::lock_guard<std::mutex> Lock(StringCacheMutex);
	StringCache.clear();
	NextStringId = 0;
}

enum GnodeError // Interpreter error enumeration
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
public:	

private:
	IPrinter* CurrentPrinter;

	std::atomic<Status> status;
	std::atomic<GnodeError> CurrentError;
	std::atomic<bool> StopRequested;
	std::atomic<bool> PauseRequested;
	std::atomic<double> Progress;
	std::string LastError;
	std::vector<std::string> GcodeErrors;
	std::vector<std::string> ExecutionLog;
	std::atomic<bool> ThreadRunning;

	// Current coordinates
	double CurrentX;
	double CurrentY;
	double CurrentZ;
	bool AbsolutePositioning;
	double Speed;

	std::unique_ptr<std::thread> ExecutionThread;
	std::mutex Mutex;
	std::mutex LogMutex;

	struct StepperConfig // Stepper motor configuration
	{
		double RotationDistance = 0.0;
		double GearRatio;
		bool Direction;
		std::vector<uint8_t> Ports;
		double MinimumFeedrate = 0.0;
		double MaximumFeedrate = 0.0;
	};

	StepperConfig StepperX;
	StepperConfig StepperY;
	StepperConfig StepperZ;

	double ZDistanceToPrint = 0.0;

	void AddLogEntry(const std::string& Entry)
	{
		std::unique_lock<std::mutex> Lock(LogMutex, std::try_to_lock);
		if (Lock.owns_lock())
		{
			// Size limit without recursion
			if (ExecutionLog.size() > 999)
			{
				ExecutionLog.erase(ExecutionLog.begin());
			}

			ExecutionLog.push_back(Entry);

			// Add a cleaning message only if it is not the message itself
			if (Entry.find("cleared") == std::string::npos && ExecutionLog.size() == 1000)
			{
				ExecutionLog.push_back("Log buffer limit reached - old entries are being removed");
			}
		}
	}

	// Add G-code error information
	void AddGCodeErrorInfo(const std::string& Code, GnodeError ErrorType = VALUE_NOT_DEFINED)
	{
		std::unique_lock<std::mutex> Lock(LogMutex, std::try_to_lock);

		if (GcodeErrors.size() > 999)
		{
			GcodeErrors.erase(GcodeErrors.begin());
		}

		std::string ErrorInfo;
		CurrentError = ErrorType;

		switch (ErrorType)
		{
		case IDENTIFIER_NOT_DEFINED:
			ErrorInfo = "Identifier '" + Code + "' is not defined";
			break;
		case VALUE_NOT_DEFINED:
			ErrorInfo = "Value '" + Code + "' is not defined or invalid";
			break;
		case OUT_OF_RANGE:
			ErrorInfo = "Value '" + Code + "' is out of range";
			break;
		case FILE_ERROR:
			ErrorInfo = "File error: " + Code;
			break;
		case CONFIG_ERROR:
			ErrorInfo = "Configuration error: " + Code;
			break;
		case PRINTER_ERROR:
			ErrorInfo = "Printer error: " + Code;
			break;
		case SYNTAX_ERROR:
			ErrorInfo = "Syntax error: " + Code;
			break;
		case MOVEMENT_ERROR:
			ErrorInfo = "Movement error: " + Code;
			break;
		case NO_ERROR:
			break;
		default:
			ErrorInfo = "Unknown error: " + Code;
			break;
		}

		GcodeErrors.push_back(ErrorInfo);
		AddLogEntry("[ERROR] " + ErrorInfo);
		LastError = ErrorInfo;
		status = ERROR;

		// Add to the main log without recursion
		if (Lock.owns_lock())
		{
			if (ExecutionLog.size() > 999)
			{
				ExecutionLog.erase(ExecutionLog.begin());
			}
			ExecutionLog.push_back("[ERROR] " + ErrorInfo);
		}
	}

	void SetError(GnodeError Error, const std::string& Message)
	{
		AddGCodeErrorInfo(Message, Error);
	}

	Section StringToSection(const std::string& Section)
	{
		if (Section == "stepper_x")
		{
			return Section::STEPPER_X;
		}
		if (Section == "stepper_y")
		{
			return Section::STEPPER_Y;
		}
		if (Section == "stepper_z")
		{
			return Section::STEPPER_Z;
		}

		AddLogEntry("Unknown section in configuration: " + Section);
		return Section::UNKNOWN;
	}

	// Convert std::string to configuration key
	ConfigKey StringToKey(const std::string& Key)
	{
		static const std::unordered_map<std::string, ConfigKey> KeyMap =
		{
			{ "rotation_distance", ConfigKey::ROTATE_DISTANCE },
			{ "distance_to_print_position", ConfigKey::DISTANCE_TO_PRINT_POSITION },
			{ "gear_ratio", ConfigKey::GEAR_RATIO },
			{ "direction", ConfigKey::DIRECTION },
			{ "ports", ConfigKey::PORTS },
			{ "minimum_feedrate", ConfigKey::MINIMUM_FEEDRATE },
			{ "maximum_feedrate", ConfigKey::MAXIMUM_FEEDRATE }
		};

		auto it = KeyMap.find(Key);
		if (it != KeyMap.end())
		{
			return it->second;
		}

		AddLogEntry("Unknown configuration key: " + Key);
		return ConfigKey::UNKNOWN;
	}

	double EvaluateExpression(const std::string& Expression)
	{
		std::string Processed = Expression;
	}

	std::string ParseValue(const std::string& Value)
	{
		if (Value.find('{') == std::string::npos || Value.find('}') == std::string::npos)
		{
			return Value;
		}
	}

	void SetConfigValue(Section section, ConfigKey Key, const std::string& Value)
	{
		StepperConfig* Config = nullptr;

		switch (section)
		{
		case Section::STEPPER_X:
			Config = &StepperX;
			break;
		case Section::STEPPER_Y:
			Config = &StepperY;
			break;
		case Section::STEPPER_Z:
			Config = &StepperZ;
			break;
		default:
			AddGCodeErrorInfo("Unknown section in configuration", CONFIG_ERROR);
			return;
		}

		switch (Key)
		{
		case ConfigKey::ROTATE_DISTANCE:
			try
			{
				Config->RotationDistance = std::stod(ParseValue(Value));
				AddLogEntry("Set rotation_distance: " + std::to_string(Config->RotationDistance));
			}
			catch (const std::exception& ex)
			{				
				AddGCodeErrorInfo("Invalid rotation distance value: " + std::to_string(Config->RotationDistance), CONFIG_ERROR);
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::GEAR_RATIO:
			try
			{
				Config->GearRatio = std::stod(ParseValue(Value));
				AddLogEntry("Set gear_ratio: " + std::to_string(Config->GearRatio));
			}
			catch (const std::exception& ex)
			{
				AddGCodeErrorInfo("Invalid gear ratio value: " + Value, CONFIG_ERROR);
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::DIRECTION:
			try
			{
				std::string DirectionString = ParseValue(Value);
				std::transform(DirectionString.begin(), DirectionString.end(), DirectionString.begin(), ::tolower);

				if (DirectionString == "clockwise" || DirectionString == "cw")
				{
					Config->Direction = true;
					AddLogEntry("Set direction: clockwise (true)");
				}
				else if (DirectionString == "counterclockwise" || DirectionString == "ccw")
				{
					Config->Direction = false;
					AddLogEntry("Set direction: counterclockwise (false)");
				}
				else
				{
					// Try to convert as number for backward compatibility
					Config->Direction = std::stoi(DirectionString) != 0;
					AddLogEntry("Set direction: " + std::to_string(Config->Direction));
				}
			}
			catch (const std::exception& ex)
			{
				AddGCodeErrorInfo("Invalid direction value: " + Value, CONFIG_ERROR);
				LastError = ex.what();
				status = ERROR;
			}
			break;
			
		case ConfigKey::PORTS:
			try
			{
				std::vector<uint8_t> Ports;
				AddLogEntry("Processing ports configuration: " + Value);

				std::string ProcessedValue = Value;
				ProcessedValue.erase(std::remove(ProcessedValue.begin(), ProcessedValue.end(), ' '), ProcessedValue.end());
				ProcessedValue.erase(std::remove(ProcessedValue.begin(), ProcessedValue.end(), ','), ProcessedValue.end());
				ProcessedValue.erase(std::remove(ProcessedValue.begin(), ProcessedValue.end(), ';'), ProcessedValue.end());

				for (char Character : ProcessedValue)
				{
					uint8_t PortValue = 0xFF;
					switch(Character)
					{
					case 'A':
					case 'a':
						PortValue = 0x00;
						break;
					case 'B':
					case 'b':
						PortValue = 0x01;
						break;
					case 'C':
					case 'c':
						PortValue = 0x02;
						break;
					case 'D':
					case 'd':
						PortValue = 0x03;
						break;
					default:
						AddLogEntry("Warning: unknown port character '" + std::string(1, Character) + "'");
						break;
					}

					bool IsDuplicate = false;
					for (auto ExistingPort : Ports)
					{
						if (ExistingPort == PortValue)
						{
							IsDuplicate = true;
							break;
						}
					}

					if (!IsDuplicate)
					{
						Ports.push_back(PortValue);
						AddLogEntry("Added port " + std::string(1, Character));
					}
					else
					{
						AddLogEntry("Duplicate port detected: " + std::string(1, Character));
					}
				}				

				Config->Ports = Ports;
				AddLogEntry("Ports configuration completed. Total ports: " + std::to_string(Config->Ports.size()));

				if (Config->Ports.empty())
				{
					AddLogEntry("WARNING: No valid ports configured!");
				}
				else
				{
					std::string PortsList = "Configured ports: ";
					for (auto Port : Config->Ports)
					{
						PortsList += std::to_string(Port) + " ";
					}

					AddLogEntry(PortsList);
				}
			}
			catch (const std::exception& ex)
			{
				AddGCodeErrorInfo("Invalid ports configuration: " + Value, CONFIG_ERROR);
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MINIMUM_FEEDRATE:
			try
			{
				Config->MinimumFeedrate = std::stod(ParseValue(Value));
				AddLogEntry("Set miminum_feedrate: " + Value);
			}
			catch (const std::exception& ex)
			{
				AddGCodeErrorInfo("Invalid minimum feedrate value: " + Value, CONFIG_ERROR);
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MAXIMUM_FEEDRATE:
			try
			{
				Config->MaximumFeedrate = std::stod(ParseValue(Value));
				AddLogEntry("Set maximum_feedrate: " + Value);
			}
			catch (const std::exception& ex)
			{
				AddGCodeErrorInfo("Invalid maximum feedrate value: " + Value, CONFIG_ERROR);
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::UNKNOWN:
			AddGCodeErrorInfo("Unknown configuration key", CONFIG_ERROR);
			break;
		}
	}

public:	
	Interpreter() // Constructor
	{
		std::lock_guard<std::mutex> Lock(GInterpreterMutex);
		GActiveInterpreters++;
		ThreadRunning = false;
		status = IDLE;
		CurrentError = NO_ERROR;
		StopRequested = false;
		Progress = 0.0;
		CurrentX = 0.0;
		CurrentY = 0.0;
		CurrentZ = 0.0;
		AbsolutePositioning = true;
		Speed = 0.0;
		CurrentPrinter = nullptr;
		ExecutionThread = nullptr;
		AddLogEntry("Interpreter initialized successfully");
	}

	~Interpreter() // Destructor
	{
		std::lock_guard<std::mutex> Lock(GInterpreterMutex);
		StopRequested = true; // Set stop flag
		ThreadRunning = false;

		if (ExecutionThread && ExecutionThread->joinable())
		{
			for (int i = 0; i < 20; i++)
			{
				if (!ThreadRunning)
				{
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}

			if (ExecutionThread->joinable())
			{
				ExecutionThread->join();
				AddLogEntry("Execution thread joined");
			}
		}

		AddLogEntry("Interpreter destroyed");
		GActiveInterpreters--;
	}

	// Execute G-code from file
	bool ExecuteFile(const char* Filename, IPrinter* Printer)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		AddLogEntry("=== ExecuteFile called ===");
		AddLogEntry("Current status: " + std::to_string(static_cast<int>(status)));
		AddLogEntry("Printer valid: " + std::string(Printer && Printer->VirtualTable ? "YES" : "NO"));

		if (!Filename)
		{
			AddLogEntry("Filename: NULL");
			return false;
		}

		if (strlen(Filename) == 0)
		{
			AddLogEntry("Filename: EMPTY STRING");
			return false;
		}

		if (ThreadRunning)
		{
			AddGCodeErrorInfo("Interpreter is already executing", PRINTER_ERROR);
			return false;
		}

		if (ExecutionThread && ExecutionThread->joinable())
		{
			ExecutionThread->detach();
			ExecutionThread.reset();
		}

		AddLogEntry("Filename: " + std::string(Filename));
		AddLogEntry("Filename length: " + std::to_string(strlen(Filename)));

		if (status == RUNNING)
		{
			AddGCodeErrorInfo("Interpreter ia already running", PRINTER_ERROR);
			return false;
		}

		if (!Printer || !Printer->VirtualTable)
		{
			AddGCodeErrorInfo("Invalid printer instance", PRINTER_ERROR);
			return false;
		}

		std::ifstream TestFile(Filename);
		if (!TestFile.is_open())
		{
			AddGCodeErrorInfo("File does not exist or cannot be opened: " + std::string(Filename), FILE_ERROR);
		}
		TestFile.close();

		CurrentPrinter = Printer;
		status = RUNNING;
		StopRequested = false;
		PauseRequested = false;
		ThreadRunning = true;
		Progress = 0.0;

		CurrentX = 0.0;
		CurrentY = 0.0;
		CurrentZ = 0.0;
		AbsolutePositioning = true;
		Speed = 0.0;

		AddLogEntry("Starting execution thread...");
		AddLogEntry("Reset coordinates to X = 0; Y = 0; Z = 0");

		std::string FilenameCopy = Filename;
		ExecutionThread = std::make_unique<std::thread>([this, FilenameCopy]()
			{
				AddLogEntry("Execution thread started");
				AddLogEntry("Thread filename: " + FilenameCopy);
				RunFile(FilenameCopy);
				AddLogEntry("Execution thread finished");
				ThreadRunning = false;
			});

		AddLogEntry("Execution started: " + std::string(Filename));
		return true;
	}

	void Pause() // Pause execution
	{
		if (status == RUNNING)
		{
			PauseRequested = true;
			status = PAUSED;
			AddLogEntry("Execution paused");
		}
		else
		{
			AddLogEntry("Pause request ignored - interpreter not running");
		}
	}

	void Resume() // Resume execution
	{
		if (status == PAUSED)
		{
			PauseRequested = false;
			status = RUNNING;
			AddLogEntry("Execution resumed");
		}
		else
		{
			AddLogEntry("Resume request ignored - interpreter not paused");
		}
	}

	void Stop() // Stop execution
	{
		AddLogEntry("Stop requested");
		StopRequested = true;
		if (status == RUNNING || status == PAUSED || status == CHECKING_CODE)
		{
			status = IDLE;
			AddLogEntry("Execution stopped by user request");
		}

		if (ExecutionThread->joinable())
		{
			ExecutionThread->join();
			AddLogEntry("Execution thread joined");
		}
	}

	Status GetStatus() // Get current status
	{
		return status;
	}

	double GetProgress() // Get execution progress
	{
		return Progress;
	}

	const char* GetLastError() // Get last error message
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		return CacheString(LastError);
	}

	int GetLastErrorCode() // Get last error code
	{
		return static_cast<int>(CurrentError.load());
	}

	double GetSpeed() // Get current speed
	{
		return Speed;
	}

	int GetErrorCount() // Get error count
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		return static_cast<int>(GcodeErrors.size());
	}

	const char* GetError(int Index) // Get error by index
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		if (Index >= 0 && Index < GcodeErrors.size())
		{
			return CacheString(GcodeErrors[Index]);
		}

		return CacheString("");
	}

	int GetLogCount() // Get log entry count
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		return static_cast<int>(ExecutionLog.size());
	}

	const char* GetLogEntry(int Index) // Get log entry by index
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		if (Index >= 0 && Index < ExecutionLog.size())
		{
			return CacheString(ExecutionLog[Index]);
		}

		AddLogEntry("Invalid log index requested: " + std::to_string(Index));
		return CacheString("");
	}

	void ClearErrors() // Clear all errors
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		GcodeErrors.clear();
		LastError.clear();
		CurrentError = NO_ERROR;
		AddLogEntry("All errors cleared");
	}

	void ClearLog() // Clear log
	{
		std::lock_guard<std::mutex> Lock(LogMutex);
		ExecutionLog.clear();
		AddLogEntry("Log cleared");
	}

	bool TestFunction(IPrinter* Printer) // Test function
	{
		CurrentPrinter = Printer;
		AddLogEntry("Test function started");

		if (!CurrentPrinter || !CurrentPrinter->VirtualTable)
		{
			AddGCodeErrorInfo("Printer is not available for test", PRINTER_ERROR);
			return false;
		}

		MotorCommand Command[2];
		Command[0].Port = 0x02;
		Command[0].Speed = 50;
		Command[0].Revolutions = 1;

		Command[1].Port = 0x03;
		Command[1].Speed = 50;
		Command[1].Revolutions = 1;

		CurrentPrinter->VirtualTable->RotateMotor(Printer, Command, 2);

		Command[0].Port = 0x02;
		Command[0].Speed = -50;
		Command[0].Revolutions = 1;

		Command[1].Port = 0x03;
		Command[1].Speed = -50;
		Command[1].Revolutions = 1;

		AddLogEntry("Sending test commands to printer");
		CurrentPrinter->VirtualTable->RotateMotor(Printer, Command, 2);

		std::string Line = "";

		AddLogEntry("Test function completed successfully");
		return true;
	}

	bool ReadConfigFile(const std::string& Filename) // Read configuration file
	{
		AddLogEntry("Reading interpreter config from: " + Filename);

		try
		{
			std::ifstream File(Filename);
			if (!File.is_open())
			{
				AddGCodeErrorInfo("Cannot open file: " + Filename, FILE_ERROR);
				return false;
			}

			std::string Line;
			Section CurrentSection = Section::UNKNOWN;
			int LineNumber = 0;
			int ProcessedSections = 0;

			while (std::getline(File, Line))
			{
				LineNumber++;
				if (StopRequested)
				{
					AddLogEntry("Config reading interrupted by stop request");
					break;
				}

				// Remove comments
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string::npos)
				{
					Line = Line.substr(0, CommentPos);
				}

				// Trim whitespace
				Line.erase(0, Line.find_first_not_of(" \t"));
				Line.erase(Line.find_last_not_of(" \t") + 1);

				if (Line.empty())
				{
					continue;
				}

				// Process sections [section]
				if (Line.front() == '[' && Line.back() == ']')
				{
					std::string SectionName = Line.substr(1, Line.length() - 2);
					CurrentSection = StringToSection(SectionName);

					if (CurrentSection != Section::UNKNOWN)
					{
						ProcessedSections++;
						AddLogEntry("Found interpreter section: " + SectionName);
					}
					else
					{
						// Ignore non-interpreter sections
						AddLogEntry("Ignoring non-interpreter section: " + SectionName);
					}
					continue;
				}

				// Process only lines in interpreter sections
				if (CurrentSection == Section::UNKNOWN)
				{
					continue;
				}

				// Process key=value pairs
				size_t DelimiterPosition = Line.find('=');
				if (DelimiterPosition == std::string::npos)
				{
					AddLogEntry("Invalid config line in interpreter section: " + Line);
					continue;
				}
				else
				{
					std::string Key = Line.substr(0, DelimiterPosition);
					std::string Value = Line.substr(DelimiterPosition + 1);

					// Trim whitespace around key and value
					Key.erase(0, Key.find_first_not_of(" \t"));
					Key.erase(Key.find_last_not_of(" \t") + 1);
					Value.erase(0, Value.find_first_not_of(" \t"));
					Value.erase(Value.find_last_not_of(" \t") + 1);

					ConfigKey configKey = StringToKey(Key);
					if (configKey == ConfigKey::UNKNOWN)
					{
						AddLogEntry("Unknown interpreter config key: " + Key);
					}
					else
					{
						SetConfigValue(CurrentSection, configKey, Value);

						// Check status after setting value
						if (status == ERROR)
						{
							AddLogEntry("Error setting config value for key: " + Key);
							File.close();
							return false;
						}
					}
				}
			}
			
			File.close();

			// Validate required settings
			if (!ValidateConfig())
			{
				AddGCodeErrorInfo("Configuration validation failed", CONFIG_ERROR);
			}

			if (status != ERROR)
			{
				AddLogEntry("Interpreter config loaded - processed " +
				std::to_string(ProcessedSections) + " sections");
				return true;
			}

			return false;
		}
		catch (const std::exception& ex)
		{
			AddLogEntry("Error with read: " + Filename + " config file: " + ex.what());
			LastError = ex.what();
			status = ERROR;
		}
	}	

	bool ExecuteLine(const std::string& Line, IPrinter* Printer)
	{
		AddLogEntry("ExecuteLine started: " + Line);

		if (status == RUNNING)
		{
			AddGCodeErrorInfo("Interpreter ia already running", PRINTER_ERROR);
			return false;
		}

		if (!Printer || !Printer->VirtualTable)
		{
			AddGCodeErrorInfo("Invalid printer instance", PRINTER_ERROR);
			return false;
		}
		
		CurrentPrinter = Printer;

		if (!CurrentPrinter || !CurrentPrinter->VirtualTable)
		{
			AddGCodeErrorInfo("Printer is not available for execution", PRINTER_ERROR);
			status = ERROR;
			return false;
		}

		if (ThreadRunning)
		{
			AddGCodeErrorInfo("Interpreter is already executing", PRINTER_ERROR);
			return false;
		}

		if (ExecutionThread && ExecutionThread->joinable())
		{
			ExecutionThread->detach();
			ExecutionThread.reset();
		}

		try
		{
			// Try interpret single line to find errors
			status = CHECKING_CODE;
			bool HasErrors = false;
			ProcessLine(Line, 1, true);

			if (status == ERROR)
			{
				AddLogEntry("Execution aborted due to errors");
				ThreadRunning = false;
				return false;
			}

			// Second pass: execution
			status = RUNNING;
			ProcessLine(Line, 1, false);

			if (!StopRequested)
			{
				AddLogEntry("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else
			{
				AddLogEntry("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex)
		{
			AddGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			LastError = ex.what();
			status = ERROR;
		}

		ThreadRunning = false;
		AddLogEntry("Line executed successfully!");
		return true;
	}

private:

	bool ValidateConfig() // Validate configuration
	{
		if (StepperX.Ports.empty())
		{
			AddGCodeErrorInfo("Stepper X has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (StepperX.RotationDistance <= 0)
		{
			AddGCodeErrorInfo("Stepper X rotation distance not set", CONFIG_ERROR);
			return false;
		}

		if (StepperY.Ports.empty())
		{
			AddGCodeErrorInfo("Stepper Y has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (StepperY.RotationDistance <= 0)
		{
			AddGCodeErrorInfo("Stepper Y rotation distance not set", CONFIG_ERROR);
			return false;
		}

		if (StepperZ.Ports.empty())
		{
			AddGCodeErrorInfo("Stepper Z has no ports configured", CONFIG_ERROR);
			return false;
		}
		if (StepperZ.RotationDistance <= 0)
		{
			AddGCodeErrorInfo("Stepper Z rotation distance not set", CONFIG_ERROR);
			return false;
		}

		// Validate speed ranges
		if (StepperX.MinimumFeedrate >= StepperX.MaximumFeedrate)
		{
			AddGCodeErrorInfo("Stepper X feedrate range invalid", CONFIG_ERROR);
			return false;
		}
		if (StepperY.MinimumFeedrate >= StepperY.MaximumFeedrate)
		{
			AddGCodeErrorInfo("Stepper Y feedrate range invalid", CONFIG_ERROR);
			return false;
		}
		if (StepperZ.MinimumFeedrate >= StepperZ.MaximumFeedrate)
		{
			AddGCodeErrorInfo("Stepper Z feedrate range invalid", CONFIG_ERROR);
			return false;
		}

		return true;
	}

	// Execute G-code file
	void RunFile(const std::string& Filename)
	{
		try
		{
			RunFileInternal(Filename);
		}
		catch (const std::exception& ex)
		{
			AddLogEntry("CRITICAL ERROR in RunFile: " + std::string(ex.what()));
			status = ERROR;
			ThreadRunning = false;
		}
		catch (...)
		{
			AddLogEntry("CRITICAL ERROR: Unknown exception in RunFile");
			status = ERROR;
			ThreadRunning = false;
		}
	}

	void RunFileInternal(const std::string& Filename)
	{
		AddLogEntry("Runfile started: " + Filename);

		if (!CurrentPrinter || !CurrentPrinter->VirtualTable)
		{
			AddGCodeErrorInfo("Printer is not available for execution", PRINTER_ERROR);
			status = ERROR;
			return;
		}

		try
		{
			status = CHECKING_CODE;
			std::ifstream File(Filename);
			if (!File.is_open())
			{
				AddGCodeErrorInfo("Cannot open file: " + Filename, FILE_ERROR);
				LastError = "Cannot open file: " + Filename;
				status = ERROR;
				ThreadRunning = false;
				return;
			}

			std::string Line = "";
			size_t LinesCount = 0;
			bool HasErrors = false;

			// Try interpret lines to find errors
			while (std::getline(File, Line))
			{
				if (StopRequested)
				{
					break;
				}

				while (PauseRequested && !StopRequested)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}

				if (StopRequested)
				{
					break;
				}

				ProcessLine(Line, LinesCount, true);
				LinesCount++;

				if (status == ERROR)
				{
					HasErrors = true;
					break;
				}
			}

			File.close();

			if (HasErrors)
			{
				AddLogEntry("Execution aborted due to errors");
				status = ERROR;
				ThreadRunning = false;
				return;
			}

			// Second pass: execution
			status = RUNNING;
			std::ifstream File2(Filename);
			if (!File2.is_open())
			{
				AddGCodeErrorInfo("Cannot open file: " + Filename, FILE_ERROR);
				LastError = "Cannot open file: " + Filename;
				status = ERROR;
				ThreadRunning = false;
				return;
			}

			LinesCount = 0;
			size_t TotalLines = 0;

			std::ifstream CountFile(Filename);
			TotalLines = std::count(std::istreambuf_iterator<char>(CountFile),
				std::istreambuf_iterator<char>(), '\n');

			CountFile.close();

			while (std::getline(File2, Line))
			{
				if (StopRequested)
				{
					break;
				}

				while (PauseRequested && !StopRequested)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(5));
				}

				if (StopRequested)
				{
					break;
				}

				ProcessLine(Line, LinesCount, false);
				LinesCount++;

				if (TotalLines > 0)
				{
					Progress = static_cast<double>(LinesCount) / TotalLines * 100.0;
				}
			}

			File2.close();

			if (!StopRequested)
			{
				AddLogEntry("Execution completed successfully");
				status = COMPLETED;
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				status = IDLE;
			}
			else
			{
				AddLogEntry("Execution stopped by user");
				status = IDLE;
			}
		}
		catch (const std::exception& ex)
		{
			AddGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			LastError = ex.what();
			status = ERROR;
		}		

		ThreadRunning = false;
	}

	// Process G-code line
	void ProcessLine(const std::string& Line, int LinesCount, bool IsTryingInterpret)
	{
		// Clean line from comments and whitespace
		std::string CleanLine = Line.substr(0, Line.find(';'));
		CleanLine.erase(0, CleanLine.find_first_not_of(" \t"));
		CleanLine.erase(CleanLine.find_last_not_of(" \t") + 1);

		if (CleanLine.empty())
		{
			return;
		}

		std::istringstream String(CleanLine);
		std::string Command;
		String >> Command;

		if (IsTryingInterpret)
		{
			AddLogEntry("Syntax checking line: " + std::to_string(LinesCount) + " : " + CleanLine);
			if (Command[0] == 'G')
			{
				int Gcode = std::stoi(Command.substr(1));

				switch (Gcode)
				{
				case 0:
				case 1:
					ProcessMovement(String, LinesCount, true);
					break;
				case 28:
					ProcessHoming();
					break;
				case 90:
					AbsolutePositioning = true;
					break;
				case 91:
					AbsolutePositioning = false;
					break;
				default:
					AddGCodeErrorInfo("Unknown G-code: " + std::to_string(Gcode) + 
						" at line " + std::to_string(LinesCount), VALUE_NOT_DEFINED);
					break;
				}
			}
			else if (Command[0] == 'M')
			{
				int Mcode = std::stoi(Command.substr(1));
				switch (Mcode)
				{
				case 30:
					break;
				default:
					AddGCodeErrorInfo("Unknown M-code: " + std::to_string(Mcode) +
					" at line " + std::to_string(LinesCount));
					break;
				}
			}
			else if (Command[0] == 'F')
			{
				try
				{
					double NewSpeed = std::stoi(Command.substr(1));
					if (NewSpeed < 0)
					{
						AddGCodeErrorInfo("Negative feedrate not allowed: " + Command, VALUE_NOT_DEFINED);
					}
				}
				catch (const std::exception& ex)
				{
					AddGCodeErrorInfo("Invalid feedrate value: " + Command, VALUE_NOT_DEFINED);
				}
			}
			else
			{
				AddGCodeErrorInfo("Unknown processing command '" + Command + "' " +
				" at line " + std::to_string(LinesCount));
			}
		}
		else
		{
			AddLogEntry("Executing line " + std::to_string(LinesCount) + " : " + CleanLine);
			if (Command[0] == 'G')
			{
				int Gcode = std::stoi(Command.substr(1));

				switch (Gcode)
				{
				case 0:
				case 1:
					ProcessMovement(String, LinesCount, false);
					break;
				case 28:
					ProcessHoming();
					break;
				case 90:
					AbsolutePositioning = true;
					break;
				case 91:
					AbsolutePositioning = false;
					break;
				default:
					break;
				}
			}
			else if (Command[0] == 'M')
			{
				int Mcode = std::stoi(Command.substr(1));
				switch (Mcode)
				{
				case 30:
					StopRequested = true;
					break;
				default:
					break;
				}
			}
			else if (Command[0] == 'F')
			{
				Speed = std::stoi(Command.substr(1));
			}
		}
	}

	//Process movement commands
	void ProcessMovement(std::istringstream& String, int LineCount, bool IsTryingInterpret)
	{
		std::string Token;

		char Axis;
		double Value;		

		if (IsTryingInterpret)
		{
			if (!CurrentPrinter || !CurrentPrinter->VirtualTable)
			{
				AddGCodeErrorInfo("Printer is not available for movement", PRINTER_ERROR);
				return;
			}

			if (StepperX.Ports.empty() || StepperY.Ports.empty() || StepperZ.Ports.empty())
			{
				AddGCodeErrorInfo("Motor ports are not configured", CONFIG_ERROR);
				return;
			}

			AddLogEntry("Checking movement command syntax");
			while (String >> Token)
			{
				Axis = Token[0];
				Value = std::stof(Token.substr(1));

				switch (Axis)
				{
				case 'X':
				case 'Y':
				case 'Z':
					break;
				default:
					AddGCodeErrorInfo("Unknown axis: " + std::string(1, Axis) + 
					" at line " + std::to_string(LineCount));
					break;
				}
			}
		}
		else
		{
			AddLogEntry("Execute movement command");

			// Initialize target coordinates
			double TargetX = AbsolutePositioning ? CurrentX : 0.0;
			double TargetY = AbsolutePositioning ? CurrentY : 0.0;
			double TargetZ = AbsolutePositioning ? CurrentZ : 0.0;

			// Parse movement commands
			while (String >> Token)
			{
				Axis = Token[0];
				Value = std::stof(Token.substr(1));

				switch (Axis)
				{
				case 'X':
					if (AbsolutePositioning)
					{
						TargetX = Value;
					}
					else
					{
						TargetX += Value;
					}
					break;
				case 'Y':
					if (AbsolutePositioning)
					{
						TargetY = Value;
					}
					else
					{
						TargetY += Value;
					}
					break;
				case 'Z':
					if (AbsolutePositioning)
					{
						TargetZ = Value;
					}
					else
					{
						TargetZ += Value;
					}
					break;
				default:
					break;
				}
			}

			double XMovement = TargetX - CurrentX;
			double YMovement = TargetY - CurrentY;
			double ZMovement = TargetZ - CurrentZ;

			AddLogEntry("Execute movement command - X:" + std::to_string(XMovement) +
			 " Y: " + std::to_string(YMovement) + " Z:" + std::to_string(ZMovement));

			// Process X and Y axis movement
			if (std::abs(XMovement) > 0 || std::abs(YMovement) > 0)
			{
				// Initialized final command for XY movement as a vector
				std::vector<MotorCommand> XYCommands;

				// Calculate movement times for each axis
				double TimeX = 0.0;
				double TimeY = 0.0;

				// ========== X Axis ==========
				if (std::abs(XMovement) > 0)
				{
					double RevolutionsX = (std::abs(XMovement) * StepperX.GearRatio) / StepperX.RotationDistance;

					// Calculate base time for X movement
					double BaseSpeedX = Speed;
					if (XMovement < 0)
					{
						BaseSpeedX = -BaseSpeedX;
					}
					if (!StepperX.Direction)
					{
						BaseSpeedX = -BaseSpeedX;
					}

					// Apply speed limits
					if (BaseSpeedX > 0)
					{
						BaseSpeedX = std::min(BaseSpeedX, StepperX.MaximumFeedrate);
						BaseSpeedX = std::max(BaseSpeedX, StepperX.MinimumFeedrate);
					}
					else
					{
						BaseSpeedX = std::max(BaseSpeedX, -StepperX.MaximumFeedrate);
						BaseSpeedX = std::min(BaseSpeedX, -StepperX.MinimumFeedrate);
					}

					TimeX = RevolutionsX / std::abs(BaseSpeedX);
				}

				// ========== Y Axis ==========
				if (std::abs(YMovement) > 0)
				{
					double RevolutionsY = (std::abs(YMovement) * StepperY.GearRatio) / StepperY.RotationDistance;

					// Calculates base time for Y movement
					double BaseSpeedY = Speed;
					if (YMovement < 0)
					{
						BaseSpeedY = -BaseSpeedY;
					}
					if (!StepperY.Direction)
					{
						BaseSpeedY = -BaseSpeedY;
					}

					// Apply speed limits
					if (BaseSpeedY > 0)
					{
						BaseSpeedY = std::min(BaseSpeedY, StepperY.MaximumFeedrate);
						BaseSpeedY = std::max(BaseSpeedY, StepperY.MinimumFeedrate);
					}
					else
					{
						BaseSpeedY = std::max(BaseSpeedY, -StepperY.MaximumFeedrate);
						BaseSpeedY = std::min(BaseSpeedY, -StepperY.MinimumFeedrate);
					}

					TimeY = RevolutionsY / std::abs(BaseSpeedY);
				}

				// Determine the maximum time needed
				double MaxTime = std::max(TimeX, TimeY);
				if (MaxTime == 0.0) // Avoid division by zero
				{
					MaxTime = 1.0;
				}


				// ============ X Axis with synchronized speed =============
				if (std::abs(XMovement) > 0)
				{
					double RevolutionsX = (std::abs(XMovement) * StepperX.GearRatio) / StepperX.RotationDistance;

					// Calculate speed to match the maximum time
					double SynchronizedSpeedX = RevolutionsX / MaxTime;

					for (uint8_t Port : StepperX.Ports)
					{
						MotorCommand Command;
						Command.Port = Port;

						double CalculatedSpeed = SynchronizedSpeedX;
						if (XMovement < 0)
						{
							CalculatedSpeed = -CalculatedSpeed;
						}
						if (!StepperX.Direction)
						{
							CalculatedSpeed = -CalculatedSpeed;
						}

						// Apply speed limits to synchronized speed
						if (CalculatedSpeed > 0)
						{
							CalculatedSpeed = std::min(CalculatedSpeed, StepperX.MaximumFeedrate);
							CalculatedSpeed = std::max(CalculatedSpeed, StepperX.MinimumFeedrate);
						}
						else
						{
							CalculatedSpeed = std::max(CalculatedSpeed, -StepperX.MaximumFeedrate);
							CalculatedSpeed = std::min(CalculatedSpeed, -StepperX.MinimumFeedrate);
						}

						Command.Speed = static_cast<signed char>(CalculatedSpeed);
						Command.Revolutions = RevolutionsX;

						XYCommands.push_back(Command);

						AddLogEntry("X axis - Port: " + std::to_string(Port) +
							" Speed: " + std::to_string(CalculatedSpeed) +
							" Revolutions: " + std::to_string(RevolutionsX));
					}
				}

				// ============= Y Axis with synchronized speed ================
				if (std::abs(YMovement) > 0)
				{
					double RevolutionsY = (std::abs(YMovement) * StepperY.GearRatio) / StepperY.RotationDistance;

					// Calculate speed to match the maximum time
					double SynchronizedSpeedY = RevolutionsY / MaxTime;

					for (uint8_t Port : StepperY.Ports)
					{
						MotorCommand Command;
						Command.Port = Port;

						double CalculatedSpeed = SynchronizedSpeedY;
						if (YMovement < 0)
						{
							CalculatedSpeed = -CalculatedSpeed;
						}
						if (StepperY.Direction)
						{
							CalculatedSpeed = -CalculatedSpeed;
						}

						// Apply speed limits to synchronized speed
						if (CalculatedSpeed > 0)
						{
							CalculatedSpeed = std::min(CalculatedSpeed, StepperY.MaximumFeedrate);
							CalculatedSpeed = std::max(CalculatedSpeed, StepperY.MinimumFeedrate);
						}
						else
						{
							CalculatedSpeed = std::max(CalculatedSpeed, -StepperY.MaximumFeedrate);
							CalculatedSpeed = std::min(CalculatedSpeed, -StepperY.MinimumFeedrate);
						}

						Command.Speed = static_cast<signed char>(CalculatedSpeed);
						Command.Revolutions = RevolutionsY;

						XYCommands.push_back(Command);

						AddLogEntry("Y axis - Port: " + std::to_string(Port) +
						" Speed: " + std::to_string(CalculatedSpeed) +
						" Revolutions " + std::to_string(RevolutionsY));
					}
				}

				// Send synchronized commands for X and Y axis
				if (!XYCommands.empty())
				{
					MotorCommand* FinalCommands = new MotorCommand[XYCommands.size()];
					std::copy(XYCommands.begin(), XYCommands.end(), FinalCommands);
					CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, FinalCommands, XYCommands.size());
					delete[] FinalCommands;

					AddLogEntry("XY movement synchronized. Max time: " + std::to_string(MaxTime));
				}
			}

			// =================== Z Axis ===================
			if (std::abs(ZMovement) > 0)
			{
				std::vector<MotorCommand> ZCommands;
				
				for (uint8_t Port : StepperZ.Ports)
				{
					MotorCommand Command;
					Command.Port = Port;

					double CalculatedSpeed = Speed;
					if (ZMovement < 0)
					{
						CalculatedSpeed = -CalculatedSpeed;
					}
					if (!StepperZ.Direction)
					{
						CalculatedSpeed = -CalculatedSpeed;
					}

					if (CalculatedSpeed > 0)
					{
						CalculatedSpeed = std::min(CalculatedSpeed, StepperZ.MaximumFeedrate);
						CalculatedSpeed = std::max(CalculatedSpeed, StepperZ.MinimumFeedrate);
					}
					else
					{
						CalculatedSpeed = std::max(CalculatedSpeed, -StepperZ.MaximumFeedrate);
						CalculatedSpeed = std::min(CalculatedSpeed, -StepperZ.MinimumFeedrate);
					}

					Command.Speed = static_cast<signed char>(CalculatedSpeed);
					Command.Revolutions = (std::abs(ZMovement) * StepperZ.GearRatio) / StepperZ.RotationDistance;

					ZCommands.push_back(Command);
				}

				MotorCommand* FinalCommands = new MotorCommand[ZCommands.size()];
				std::copy(ZCommands.begin(), ZCommands.end(), FinalCommands);
				CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, FinalCommands, ZCommands.size());
				delete[] FinalCommands;
			}

			CurrentX = TargetX;
			CurrentY = TargetY;
			CurrentZ = TargetZ;

			AddLogEntry("Movement completed. New position: X=" + std::to_string(CurrentX) +
			" Y=" + std::to_string(CurrentY) + " Z=" + std::to_string(CurrentZ));
		}
	}

	// Process homing command
	void ProcessHoming()
	{
		AddLogEntry("Homing command started");

		double XMovement = -CurrentX;
		double YMovement = -CurrentY;
		double ZMovement = -CurrentZ;

		AddLogEntry("Execute movement command - X:" + std::to_string(XMovement) +
			" Y: " + std::to_string(YMovement) + " Z:" + std::to_string(ZMovement));

		// Process X and Y axis movement
		if (std::abs(XMovement) > 0 || std::abs(YMovement) > 0)
		{
			// Initialized final command for XY movement as a vector
			std::vector<MotorCommand> XYCommands;

			// Calculate movement times for each axis
			double TimeX = 0.0;
			double TimeY = 0.0;

			// ========== X Axis ==========
			if (std::abs(XMovement) > 0)
			{
				double RevolutionsX = (std::abs(XMovement) * StepperX.GearRatio) / StepperX.RotationDistance;

				// Calculate base time for X movement
				double BaseSpeedX = Speed;
				if (XMovement < 0)
				{
					BaseSpeedX = -BaseSpeedX;
				}
				if (!StepperX.Direction)
				{
					BaseSpeedX = -BaseSpeedX;
				}

				// Apply speed limits
				if (BaseSpeedX > 0)
				{
					BaseSpeedX = std::min(BaseSpeedX, StepperX.MaximumFeedrate);
					BaseSpeedX = std::max(BaseSpeedX, StepperX.MinimumFeedrate);
				}
				else
				{
					BaseSpeedX = std::max(BaseSpeedX, -StepperX.MaximumFeedrate);
					BaseSpeedX = std::min(BaseSpeedX, -StepperX.MinimumFeedrate);
				}

				TimeX = RevolutionsX / std::abs(BaseSpeedX);
			}

			// ========== Y Axis ==========
			if (std::abs(YMovement) > 0)
			{
				double RevolutionsY = (std::abs(YMovement) * StepperY.GearRatio) / StepperY.RotationDistance;

				// Calculates base time for Y movement
				double BaseSpeedY = Speed;
				if (YMovement < 0)
				{
					BaseSpeedY = -BaseSpeedY;
				}
				if (!StepperY.Direction)
				{
					BaseSpeedY = -BaseSpeedY;
				}

				// Apply speed limits
				if (BaseSpeedY > 0)
				{
					BaseSpeedY = std::min(BaseSpeedY, StepperY.MaximumFeedrate);
					BaseSpeedY = std::max(BaseSpeedY, StepperY.MinimumFeedrate);
				}
				else
				{
					BaseSpeedY = std::max(BaseSpeedY, -StepperY.MaximumFeedrate);
					BaseSpeedY = std::min(BaseSpeedY, -StepperY.MinimumFeedrate);
				}

				TimeY = RevolutionsY / std::abs(BaseSpeedY);
			}

			// Determine the maximum time needed
			double MaxTime = std::max(TimeX, TimeY);
			if (MaxTime == 0.0) // Avoid division by zero
			{
				MaxTime = 1.0;
			}


			// ============ X Axis with synchronized speed =============
			if (std::abs(XMovement) > 0)
			{
				double RevolutionsX = (std::abs(XMovement) * StepperX.GearRatio) / StepperX.RotationDistance;

				// Calculate speed to match the maximum time
				double SynchronizedSpeedX = RevolutionsX / MaxTime;

				for (uint8_t Port : StepperX.Ports)
				{
					MotorCommand Command;
					Command.Port = Port;

					double CalculatedSpeed = SynchronizedSpeedX;
					if (XMovement < 0)
					{
						CalculatedSpeed = -CalculatedSpeed;
					}
					if (!StepperX.Direction)
					{
						CalculatedSpeed = -CalculatedSpeed;
					}

					// Apply speed limits to synchronized speed
					if (CalculatedSpeed > 0)
					{
						CalculatedSpeed = std::min(CalculatedSpeed, StepperX.MaximumFeedrate);
						CalculatedSpeed = std::max(CalculatedSpeed, StepperX.MinimumFeedrate);
					}
					else
					{
						CalculatedSpeed = std::max(CalculatedSpeed, -StepperX.MaximumFeedrate);
						CalculatedSpeed = std::min(CalculatedSpeed, -StepperX.MinimumFeedrate);
					}

					Command.Speed = static_cast<signed char>(CalculatedSpeed);
					Command.Revolutions = RevolutionsX;

					XYCommands.push_back(Command);

					AddLogEntry("X axis - Port: " + std::to_string(Port) +
						" Speed: " + std::to_string(CalculatedSpeed) +
						" Revolutions: " + std::to_string(RevolutionsX));
				}
			}

			// ============= Y Axis with synchronized speed ================
			if (std::abs(YMovement) > 0)
			{
				double RevolutionsY = (std::abs(YMovement) * StepperY.GearRatio) / StepperY.RotationDistance;

				// Calculate speed to match the maximum time
				double SynchronizedSpeedY = RevolutionsY / MaxTime;

				for (uint8_t Port : StepperY.Ports)
				{
					MotorCommand Command;
					Command.Port = Port;

					double CalculatedSpeed = SynchronizedSpeedY;
					if (YMovement < 0)
					{
						CalculatedSpeed = -CalculatedSpeed;
					}
					if (StepperY.Direction)
					{
						CalculatedSpeed = -CalculatedSpeed;
					}

					// Apply speed limits to synchronized speed
					if (CalculatedSpeed > 0)
					{
						CalculatedSpeed = std::min(CalculatedSpeed, StepperY.MaximumFeedrate);
						CalculatedSpeed = std::max(CalculatedSpeed, StepperY.MinimumFeedrate);
					}
					else
					{
						CalculatedSpeed = std::max(CalculatedSpeed, -StepperY.MaximumFeedrate);
						CalculatedSpeed = std::min(CalculatedSpeed, -StepperY.MinimumFeedrate);
					}

					Command.Speed = static_cast<signed char>(CalculatedSpeed);
					Command.Revolutions = RevolutionsY;

					XYCommands.push_back(Command);

					AddLogEntry("Y axis - Port: " + std::to_string(Port) +
						" Speed: " + std::to_string(CalculatedSpeed) +
						" Revolutions " + std::to_string(RevolutionsY));
				}
			}

			// Send synchronized commands for X and Y axis
			if (!XYCommands.empty())
			{
				MotorCommand* FinalCommands = new MotorCommand[XYCommands.size()];
				std::copy(XYCommands.begin(), XYCommands.end(), FinalCommands);
				CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, FinalCommands, XYCommands.size());
				delete[] FinalCommands;

				AddLogEntry("XY movement synchronized. Max time: " + std::to_string(MaxTime));
			}
		}

		// =================== Z Axis ===================
		if (std::abs(ZMovement) > 0)
		{
			std::vector<MotorCommand> ZCommands;

			for (uint8_t Port : StepperZ.Ports)
			{
				MotorCommand Command;
				Command.Port = Port;

				double CalculatedSpeed = Speed;
				if (ZMovement < 0)
				{
					CalculatedSpeed = -CalculatedSpeed;
				}
				if (!StepperZ.Direction)
				{
					CalculatedSpeed = -CalculatedSpeed;
				}

				if (CalculatedSpeed > 0)
				{
					CalculatedSpeed = std::min(CalculatedSpeed, StepperZ.MaximumFeedrate);
					CalculatedSpeed = std::max(CalculatedSpeed, StepperZ.MinimumFeedrate);
				}
				else
				{
					CalculatedSpeed = std::max(CalculatedSpeed, -StepperZ.MaximumFeedrate);
					CalculatedSpeed = std::min(CalculatedSpeed, -StepperZ.MinimumFeedrate);
				}

				Command.Speed = static_cast<signed char>(CalculatedSpeed);
				Command.Revolutions = (std::abs(ZMovement) * StepperZ.GearRatio) / StepperZ.RotationDistance;

				ZCommands.push_back(Command);
			}

			MotorCommand* FinalCommands = new MotorCommand[ZCommands.size()];
			std::copy(ZCommands.begin(), ZCommands.end(), FinalCommands);
			CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, FinalCommands, ZCommands.size());
			delete[] FinalCommands;
		}

		CurrentX = 0;
		CurrentY = 0;
		CurrentZ = 0;

		AddLogEntry("Movement completed. New position: X=" + std::to_string(CurrentX) +
			" Y=" + std::to_string(CurrentY) + " Z=" + std::to_string(CurrentZ));

		AddLogEntry("Homing completed");
	}
};

// C API exports
extern "C"
{
	GCODE_API InterpreterHandle CreateInterpreter()
	{
		return new Interpreter();
	}

	GCODE_API void DestroyInterpreter(InterpreterHandle Handle)
	{
		delete static_cast<Interpreter*>(Handle);
	}

	GCODE_API bool TestCode(InterpreterHandle Handle, IPrinter* Printer)
	{
		if (!Handle || !Printer)
		{
			return false;
		}

		return static_cast<Interpreter*>(Handle)->TestFunction(Printer);
	}

	GCODE_API bool ExecuteGcode(InterpreterHandle Handle, const char* Filename, IPrinter* Printer)
	{
		// Добавляем детальное логирование
		printf("C++: ExecuteGcode called\n");
		printf("C++: Handle: %p\n", Handle);
		printf("C++: Printer: %p\n", Printer);
		if (Printer) 
		{
			printf("C++: Printer->VirtualTable: %p\n", Printer->VirtualTable);
		}
		printf("C++: Filename: %s\n", Filename ? Filename : "NULL");

		if (!Handle || !Printer || !Filename)
		{
			printf("C++: ERROR - Invalid parameters\n");
			return false;
		}

		// БЕЗОПАСНАЯ проверка файла с использованием fopen_s
		FILE* testFile = nullptr;
		errno_t err = fopen_s(&testFile, Filename, "r");
		if (err != 0 || !testFile)
		{
			printf("C++: ERROR - Cannot open file: %s, error code: %d\n", Filename, err);
			return false;
		}
		fclose(testFile);

		printf("C++: File exists, calling ExecuteFile\n");
		return static_cast<Interpreter*>(Handle)->ExecuteFile(Filename, Printer);
	}

	GCODE_API bool ExecuteLine(InterpreterHandle Handle, const char* Line, IPrinter* Printer)
	{
		if (Printer)
		{
			printf("C++: Printer->VirtualTable: %p\n", Printer->VirtualTable);
		}

		if (!Handle || !Printer)
		{
			printf("C++: ERROR - Invalid parameters\n");
			return false;
		}

		return static_cast<Interpreter*>(Handle)->ExecuteLine(Line, Printer);
	}

	GCODE_API void PauseExecution(InterpreterHandle Handle)
	{
		if (Handle)
		{
			static_cast<Interpreter*>(Handle)->Pause();
		}
	}

	GCODE_API void ResumeExecution(InterpreterHandle Handle)
	{
		if (Handle)
		{
			static_cast<Interpreter*>(Handle)->Resume();
		}
	}

	GCODE_API int GetStatus(InterpreterHandle Handle)
	{
		if (!Handle)
		{
			return -1;
		}

		return static_cast<Interpreter*>(Handle)->GetStatus();
	}

	GCODE_API double GetProgress(InterpreterHandle Handle)
	{
		if (!Handle)
		{
			return 0.0;
		}

		return static_cast<Interpreter*>(Handle)->GetProgress();
	}

	GCODE_API const char* GetLastInterpreterError(InterpreterHandle Handle)
	{
		if (!Handle)
		{
			return "";
		}

		return static_cast<Interpreter*>(Handle)->GetLastError();
	}

	GCODE_API int GetErrorCount(InterpreterHandle Handle)
	{
		if (!Handle)
		{
			return 0;
		}

		return static_cast<Interpreter*>(Handle)->GetErrorCount();
	}

	GCODE_API const char* GetError(InterpreterHandle Handle, int Index)
	{
		if (!Handle)
		{
			return "";
		}

		return static_cast<Interpreter*>(Handle)->GetError(Index);
	}

	GCODE_API int GetLogCount(InterpreterHandle Handle)
	{
		if (!Handle)
		{
			return 0;
		}

		return static_cast<Interpreter*>(Handle)->GetLogCount();
	}

	GCODE_API const char* GetLogEntry(InterpreterHandle Handle, int Index)
	{
		if (!Handle)
		{
			return "";
		}

		return static_cast<Interpreter*>(Handle)->GetLogEntry(Index);
	}

	GCODE_API void ClearErrors(InterpreterHandle Handle)
	{
		if (Handle)
		{
			static_cast<Interpreter*>(Handle)->ClearErrors();
		}
	}

	GCODE_API void ClearLog(InterpreterHandle Handle)
	{
		if (Handle)
		{
			static_cast<Interpreter*>(Handle)->ClearLog();
		}
	}

	GCODE_API bool ReadConfing(InterpreterHandle Handle, const char* Filename)
	{
		if (!Handle || !Filename)
		{
			return false;
		}

		return static_cast<Interpreter*>(Handle)->ReadConfigFile(Filename);
	}
}
