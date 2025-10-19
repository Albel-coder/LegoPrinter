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

	// Current coordinates
	double CurrentX;
	double CurrentY;
	double CurrentZ;
	bool AbsolutePositioning;
	double Speed;

	std::thread ExecutionThread;
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
		if (ExecutionLog.size() > 1000)
		{
			ExecutionLog.erase(ExecutionLog.begin());
			AddLogEntry("Log buffer cleared - reached maximum size");
		}

		ExecutionLog.push_back(Entry);
	}

	// Add G-code error information
	void AddGCodeErrorInfo(const std::string& Code, GnodeError ErrorType = VALUE_NOT_DEFINED)
	{
		std::unique_lock<std::mutex> Lock(LogMutex, std::try_to_lock);

		if (GcodeErrors.size() > 1000)
		{
			GcodeErrors.erase(GcodeErrors.begin());
			AddLogEntry("Error buffer cleared - reached maximum size");
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
				
				for (char Character : Value)
				{
					switch(Character)
					{
					case 'A':
					case 'a':
						if (IsPortNotDuplicated(Ports, 0x00))
						{
							Ports.push_back(0x00);
							AddLogEntry("Added port A");
						}
						break;
					case 'B':
					case 'b':
						if (IsPortNotDuplicated(Ports, 0x01))
						{
							Ports.push_back(0x01);
							AddLogEntry("Added port B");
						}
						break;
					case 'C':
					case 'c':
						if (IsPortNotDuplicated(Ports, 0x02))
						{
							Ports.push_back(0x02);
							AddLogEntry("Added port C");
						}
						break;
					case 'D':
					case 'd':
						if (IsPortNotDuplicated(Ports, 0x03))
						{
							Ports.push_back(0x03);
							AddLogEntry("Added port D");
						}
						break;
					case ' ':
					case ',':
					case ';':
						// Ignore all separators
						break;
					default:
						AddLogEntry("Warning: unknown port character '" + std::string(1, Character) + "'");
						break;
					}
				}

				Config->Ports = Ports;
				AddLogEntry("Ports configuration completed. Total ports: " + std::to_string(Ports.size()));
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

	// Check for port duplication
	inline bool IsPortNotDuplicated(std::vector<uint8_t> Ports, uint8_t Port)
	{
		for (uint8_t i = 0; i < Ports.size(); i++)
		{
			if (Ports[i] == Port)
			{
				AddLogEntry("Duplicate port detected: " + std::to_string(Port));
				return false;
			}
		}

		return true;
	}

public:	
	Interpreter() // Constructor
	{
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
		AddLogEntry("Interpreter initialized successfully");
	}

	~Interpreter() // Destructor
	{
		StopRequested = true; // Set stop flag

		if (ExecutionThread.joinable())
		{
			ExecutionThread.detach();
			AddLogEntry("Execution thread detach");
		}

		AddLogEntry("Execution thread destroyed");
	}

	// Execute G-code from file
	bool ExecuteFile(const char* Filename, IPrinter* Printer)
	{
		std::lock_guard<std::mutex> Lock(Mutex);
		AddLogEntry("ExecuteFile called with filename: " + std::string(Filename));

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
		status = RUNNING;
		StopRequested = false;
		PauseRequested = false;
		Progress = 0.0;

		ExecutionThread = std::thread([this, Filename]()
			{
				RunFile(Filename);
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

		if (ExecutionThread.joinable())
		{
			ExecutionThread.join();
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
		Command[0].Port = 0x00;
		Command[0].Speed = 30;
		Command[0].Revolutions = 2.5;

		Command[1].Port = 0x01;
		Command[1].Speed = 30;
		Command[1].Revolutions = 2.5;

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
		AddLogEntry("Runfile started: " + Filename);
		std::ifstream File(Filename);

		try
		{
			if (!File.is_open())
			{
				AddGCodeErrorInfo("Cannot open file: " + Filename, FILE_ERROR);
				LastError = "Cannot open file: " + Filename;
				status = ERROR;
				return;
			}

			std::string Line = "";
			size_t LinesCount = 0;

			// Try interpret lines to find errors
			status = CHECKING_CODE;
			while (!File.eof())
			{
				std::getline(File, Line);

				if (StopRequested)
				{
					break;
				}

				while (PauseRequested && !StopRequested)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}

				if (StopRequested)
				{
					break;
				}

				ProcessLine(Line, LinesCount, true);
				LinesCount++;
				Line.clear();
			}

			File.close();

			if (status != ERROR)
			{
				// Second pass: execution
				status = RUNNING;
				while (!File.eof())
				{
					std::getline(File, Line);

					if (StopRequested)
					{
						break;
					}

					while (PauseRequested && !StopRequested)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
					}

					if (StopRequested)
					{
						break;
					}

					ProcessLine(Line, LinesCount, false);
					LinesCount++;
					Line.clear();
				}

				File.close();
				AddLogEntry("Execution completed successfully");
				status = COMPLETED;
			}
			else
			{
				AddLogEntry("Execution aborted due to syntax errors");
			}
		}
		catch (const std::exception& ex)
		{
			AddGCodeErrorInfo("Runtime error: " + std::string(ex.what()), MOVEMENT_ERROR);
			LastError = ex.what();
			status = ERROR;
		}
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
				AddLogEntry("Feedrate command detection: " + Command);
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
				int Fcode = std::stoi(Command.substr(1));
				if (Fcode > 100)
				{
					Speed = 100;
				}
				else if (Fcode < 0)
				{
					Speed = 1;
				}
				else
				{
					Speed = Fcode;
				}
			}
		}
	}

	//Process movement commands
	void ProcessMovement(std::istringstream& String, int LineCount, bool IsTryingInterpret)
	{
		double X = CurrentX;
		double Y = CurrentY;
		double Z = CurrentZ;
		std::string Token;

		char Axis;
		double Value;
		if (IsTryingInterpret)
		{
			AddLogEntry("Checking movement command syntax");
			while (String >> Token)
			{
				Axis = Token[0];
				Value = std::stof(Token.substr(1));

				switch (Axis)
				{
				case 'X':
					X = Value;
					break;
				case 'Y':
					Y = Value;
					break;
				case 'Z':
					Z = Value;
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
			while (String >> Token)
			{
				Axis = Token[0];
				Value = std::stof(Token.substr(1));

				switch (Axis)
				{
				case 'X':
					X = Value;
					break;
				case 'Y':
					Y = Value;
					break;
				case 'Z':
					Z = Value;
					break;
				default:
					break;
				}
			}

			double XMovement = 0.0;
			double YMovement = 0.0;
			double ZMovement = 0.0;
			if (AbsolutePositioning)
			{
				XMovement = X - CurrentX;
				YMovement = Y - CurrentY;
				ZMovement = Z - CurrentZ;
			}
			else
			{
				XMovement = X;
				YMovement = Y;
				ZMovement = Z;
			}

			// Process X and Y axis movement
			if (std::abs(XMovement) > 0 || std::abs(YMovement) > 0)
			{
				// Initialized final command for XY movement as a vector
				std::vector<MotorCommand> XYCommands;

				if (std::abs(XMovement) > 0)
				{
					// If a single motor is used to move the axis
					if (StepperX.Ports.size() == 1)
					{
						MotorCommand Command;
						Command.Port = StepperX.Ports[0];
						if (Speed > StepperX.MaximumFeedrate)
						{
							Command.Speed = StepperX.MaximumFeedrate;
						}
						else if (Speed < StepperX.MinimumFeedrate)
						{
							Command.Speed = StepperX.MaximumFeedrate;
						}
						else
						{
							Command.Speed = Speed;
						}
						Command.Revolutions = std::abs(XMovement) / (StepperX.RotationDistance * StepperX.GearRatio);

						if (!StepperX.Direction)
						{
							Speed *= -1;
						}

						XYCommands.push_back(Command);
					}
					else // If multiple motors are used to move the axis
					{
						std::vector<MotorCommand> Command(StepperX.Ports.size());
						// Commands on different ports of the same axis are the same
						// Therefore, we initialize only one port, and copy the rest from other ports.
						Command[0].Port = StepperX.Ports[0];
						if (Speed > StepperX.MaximumFeedrate)
						{
							Command[0].Speed = StepperX.MaximumFeedrate;
						}
						else if (Speed < StepperX.MinimumFeedrate)
						{
							Command[0].Speed = StepperX.MaximumFeedrate;
						}
						else
						{
							Command[0].Speed = Speed;
						}

						// Converting distance traveled into engine speed
						Command[0].Revolutions = std::abs(XMovement) / (StepperX.RotationDistance * StepperX.GearRatio);

						// If we turn in the other direction
						if (!StepperX.Direction)
						{
							Speed *= -1;
						}

						// Copy commands to all ports
						for (uint8_t i = 1; i < StepperX.Ports.size(); i++)
						{
							Command[0].Port = StepperX.Ports[i];
							Command.push_back(Command[0]);
						}

						for (uint8_t i = 0; i < Command.size(); i++)
						{
							XYCommands.push_back(Command[i]);
						}
					}
				}

				if (std::abs(YMovement) > 0)
				{
					// If a single motor is used to move the axis
					if (StepperY.Ports.size() == 1)
					{
						MotorCommand Command;
						Command.Port = StepperY.Ports[0];
						if (Speed > StepperY.MaximumFeedrate)
						{
							Command.Speed = StepperY.MaximumFeedrate;
						}
						else if (Speed < StepperY.MinimumFeedrate)
						{
							Command.Speed = StepperY.MaximumFeedrate;
						}
						else
						{
							Command.Speed = Speed;
						}
						Command.Revolutions = std::abs(XMovement) / (StepperY.RotationDistance * StepperY.GearRatio);

						if (!StepperY.Direction)
						{
							Speed *= -1;
						}

						XYCommands.push_back(Command);
					}
					else // If multiple motors are used to move the axis
					{
						std::vector<MotorCommand> Command(StepperY.Ports.size());

						Command[0].Port = StepperY.Ports[0];
						if (Speed > StepperY.MaximumFeedrate)
						{
							Command[0].Speed = StepperY.MaximumFeedrate;
						}
						else if (Speed < StepperY.MinimumFeedrate)
						{
							Command[0].Speed = StepperY.MaximumFeedrate;
						}
						else
						{
							Command[0].Speed = Speed;
						}
						Command[0].Revolutions = std::abs(XMovement) / (StepperY.RotationDistance * StepperY.GearRatio);

						if (!StepperY.Direction)
						{
							Speed *= -1;
						}

						// Copy all commands to the remaining ports
						for (uint8_t i = 1; i < StepperY.Ports.size(); i++)
						{
							Command[0].Port = StepperY.Ports[i];
							Command.push_back(Command[0]);
						}

						for (uint8_t i = 0; i < Command.size(); i++)
						{
							XYCommands.push_back(Command[i]);
						}
					}
				}

				// We will convert it into the required format for a C-style interface.
				uint8_t FinalCommandsSize = XYCommands.size();
				MotorCommand* FinalCommands = new MotorCommand[FinalCommandsSize];

				for (uint8_t i = 0; i < FinalCommandsSize; i++)
				{
					FinalCommands[i] = XYCommands[i];
				}

				if (CurrentPrinter && CurrentPrinter->VirtualTable)
				{
					CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, FinalCommands, FinalCommandsSize);
				}

				delete[] FinalCommands;
				FinalCommands = nullptr;
			}

			if (std::abs(ZMovement) > 0)
			{
				std::vector<MotorCommand> ZCommands;
				// If a single motor is used to move the axis
				if (StepperZ.Ports.size() == 1)
				{
					MotorCommand Command;
					Command.Port = StepperZ.Ports[0];
					if (Speed > StepperZ.MaximumFeedrate)
					{
						Command.Speed = StepperZ.MaximumFeedrate;
					}
					else if (Speed < StepperZ.MinimumFeedrate)
					{
						Command.Speed = StepperZ.MaximumFeedrate;
					}
					else
					{
						Command.Speed = Speed;
					}
					Command.Revolutions = std::abs(XMovement) / (StepperZ.RotationDistance * StepperZ.GearRatio);

					if (!StepperZ.Direction)
					{
						Speed *= -1;
					}

					ZCommands.push_back(Command);
				}
				else // If multiple motors are used to move the axis
				{
					std::vector<MotorCommand> Command(StepperZ.Ports.size());

					Command[0].Port = StepperZ.Ports[0];
					if (Speed > StepperZ.MaximumFeedrate)
					{
						Command[0].Speed = StepperZ.MaximumFeedrate;
					}
					else if (Speed < StepperZ.MinimumFeedrate)
					{
						Command[0].Speed = StepperZ.MaximumFeedrate;
					}
					else
					{
						Command[0].Speed = Speed;
					}
					Command[0].Revolutions = std::abs(XMovement) / (StepperZ.RotationDistance * StepperZ.GearRatio);

					if (!StepperZ.Direction)
					{
						Speed *= -1;
					}

					// Copy the commands to the remaining ports
					for (uint8_t i = 1; i < StepperZ.Ports.size(); i++)
					{
						Command[0].Port = StepperZ.Ports[i];
						Command.push_back(Command[0]);
					}

					for (uint8_t i = 0; i < Command.size(); i++)
					{
						ZCommands.push_back(Command[i]);
					}
				}

				// We will convert it into the required format for a C-style interface.
				uint8_t FinalCommandsSize = ZCommands.size();
				MotorCommand* FinalCommands = new MotorCommand[FinalCommandsSize];

				for (uint8_t i = 0; i < FinalCommandsSize; i++)
				{
					FinalCommands[i] = ZCommands[i];
				}

				if (CurrentPrinter && CurrentPrinter->VirtualTable)
				{
					CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, FinalCommands, FinalCommandsSize);
				}

				delete[] FinalCommands;
				FinalCommands = nullptr;
			}
		}
	}

	// Process homing command
	void ProcessHoming()
	{
		AddLogEntry("Homing command started");
		if (std::abs(CurrentZ) > 0.0001)
		{
			MotorCommand Command[1];
			Command[0].Port = StepperZ.Ports[0];
			Command[0].Speed = -50;
			Command[0].Revolutions = std::abs(CurrentZ) / (StepperZ.RotationDistance * StepperZ.GearRatio);

			if (!StepperZ.Direction)
			{
				Speed *= -1;
			}

			if (CurrentPrinter && CurrentPrinter->VirtualTable)
			{
				CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, Command, 1);
			}
		}
		MotorCommand Commands[2];

		if (std::abs(CurrentX) > 0.0001)
		{
			MotorCommand Command;
			Command.Port = StepperX.Ports[0];
			Command.Speed = -50;
			Command.Revolutions = std::abs(CurrentX) / (StepperX.RotationDistance * StepperX.GearRatio);

			if (!StepperX.Direction)
			{
				Speed *= -1;
			}
			Commands[0] = Command;
		}
		else
		{
			MotorCommand Command;
			Command.Port = StepperX.Ports[0];
			Command.Speed = 0;
			Command.Revolutions = 0;
			Commands[0] = Command;
		}
		if (std::abs(CurrentY) > 0.0001)
		{
			MotorCommand Command;
			Command.Port = StepperY.Ports[0];
			Command.Speed = -50;
			Command.Revolutions = std::abs(CurrentY) / (StepperY.RotationDistance * StepperY.GearRatio);

			if (!StepperY.Direction)
			{
				Speed *= -1;
			}
			
			Commands[1] = Command;
		}
		else
		{
			MotorCommand Command;
			Command.Port = StepperY.Ports[0];
			Command.Speed = 0;
			Command.Revolutions = 0;

			Commands[1] = Command;
		}

		if (CurrentPrinter && CurrentPrinter->VirtualTable)
		{
			CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, Commands, 2);
		}

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

	GCODE_API bool ExecuteGcode(InterpreterHandle Handle, IPrinter* Printer, const char* Filename)
	{
		if (!Handle || !Printer)
		{
			return false;
		}

		return static_cast<Interpreter*>(Handle)->ExecuteFile(Filename, Printer);
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
