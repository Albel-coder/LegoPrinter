#define GCODEINTERPRETER_EXPORTS

#include "Interpreter.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdarg>
#include <sstream>
#include <fstream>

enum GCodeError
{
	IDENTIFIER_NOT_DEFINED = 0,
	VALUE_NOT_DEFINED = 1,
	OUT_OF_RANGE = 2,
	NO_ERROR = 3
};

enum Status
{
	IDLE = 0,
	CHECKING_CODE = 1,
	RUNNING = 2,
	PAUSED = 3,
	COMPLETED = 4,
	ERROR = 5
};

enum class ConfigKey
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

enum class Section
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
	std::atomic<bool> StopRequested;
	std::atomic<bool> PauseRequested;
	std::atomic<double> Progress;
	std::string LastError;

	struct GcodeErrorRecord
	{
		GCodeError ErrorCode;
		std::string ErrorMessage;
		std::string LineContent;
		int LineCount;
	};

	std::vector<GcodeErrorRecord> GcodeErrors;

	double CurrentX;
	double CurrentY;
	double CurrentZ;
	bool AbsolutePositioning;
	double Speed;

	std::thread ExecutionThread;
	std::mutex Mutex;
	std::mutex LogMutex;

	struct StepperConfig
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
	}

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
		return (it != KeyMap.end()) ? it->second : ConfigKey::UNKNOWN;
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
			return;
		}

		switch (Key)
		{
		case ConfigKey::ROTATE_DISTANCE:
			try
			{
				Config->RotationDistance = std::stod(ParseValue(Value));
			}
			catch (const std::exception& ex)
			{
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::GEAR_RATIO:
			try
			{
				Config->GearRatio = std::stod(ParseValue(Value));
			}
			catch (const std::exception& ex)
			{
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::DIRECTION:
			try
			{
				Config->Direction = std::stod(ParseValue(Value));
			}
			catch (const std::exception& ex)
			{
				LastError = ex.what();
				status = ERROR;
			}
			break;
			
		case ConfigKey::PORTS:
			try
			{
				std::vector<uint8_t> Ports;
				Ports.push_back(static_cast<uint8_t>(std::stoi(Value)));
				Config->Ports = Ports;
			}
			catch (const std::exception& ex)
			{
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MINIMUM_FEEDRATE:
			try
			{
				Config->MinimumFeedrate = std::stod(ParseValue(Value));
			}
			catch (const std::exception& ex)
			{
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::MAXIMUM_FEEDRATE:
			try
			{
				Config->MaximumFeedrate = std::stod(ParseValue(Value));
			}
			catch (const std::exception& ex)
			{
				LastError = ex.what();
				status = ERROR;
			}
			break;

		case ConfigKey::UNKNOWN:
			LastError = "Unknown config key";
			status = ERROR;
			break;
		}
	}

public:	
	Interpreter()
	{
		status = IDLE;
		StopRequested = false;
		Progress = 0.0;
		CurrentX = 0.0;
		CurrentY = 0.0;
		CurrentZ = 0.0;
		AbsolutePositioning = true;
		Speed = 0.0;
		CurrentPrinter = nullptr;
	}

	~Interpreter()
	{

	}

	bool ExecuteFile(const char* Filename, IPrinter* Printer)
	{
		std::lock_guard<std::mutex> Lock(Mutex);

		if (status == RUNNING)
		{
			LastError = "Already running";
			return false;
		}

		if (!Printer || !Printer->VirtualTable)
		{
			LastError = "Invalid running";
			return false;
		}

		CurrentPrinter = Printer;
		status = RUNNING;
		StopRequested = false;
		PauseRequested = false;
		Progress = 0.0;

		
	}

	void Pause()
	{
		if (status == RUNNING)
		{
			PauseRequested = true;
			status = PAUSED;
		}
	}

	void Resume()
	{
		if (status == PAUSED)
		{
			PauseRequested = false;
			status = RUNNING;
		}
	}

	void Stop()
	{
		StopRequested = true;
		if (ExecutionThread.joinable())
		{
			ExecutionThread.join();
		}
		
		status = IDLE;
	}

	Status GetStatus()
	{
		return status;
	}

	double GetProgress()
	{
		return Progress;
	}

	const char* GetLastError()
	{
		return LastError.c_str();
	}

	double GetSpeedMultiplier()
	{
		return Speed;
	}

	bool TestFunction(IPrinter* Printer)
	{
		CurrentPrinter = Printer;

		MotorCommand Command[2];
		Command[0].Port = 0x00;
		Command[0].Speed = 30;
		Command[0].Revolutions = 2.5;

		Command[1].Port = 0x01;
		Command[1].Speed = 30;
		Command[1].Revolutions = 2.5;

		CurrentPrinter->VirtualTable->RotateMotor(Printer, Command, 2);

		std::string Line = "";

		return true;
	}

	bool ReadConfigFile(const std::string& Filename)
	{
		std::ifstream File(Filename);

		try
		{
			if (!File.is_open())
			{
				LastError = "Cannot open file: " + Filename;
				status = ERROR;
				return false;
			}

			std::string Line;
			Section CurrentSection = Section::UNKNOWN;

			while (std::getline(File, Line))
			{
				size_t CommentPos = Line.find('#');
				if (CommentPos != std::string::npos)
				{
					Line = Line.substr(0, CommentPos);
				}

				Line.erase(0, Line.find_first_not_of(" \t"));
				Line.erase(Line.find_last_not_of(" \t") + 1);

				if (Line.empty())
				{
					continue;
				}

				if (Line.front() == '[' && Line.back() == ']')
				{
					std::string SectionName = Line.substr(1, Line.length() - 2);
					CurrentSection = StringToSection(SectionName);
					continue;
				}

				size_t DelimiterPosition = Line.find('=');
				if (DelimiterPosition != std::string::npos && CurrentSection != Section::UNKNOWN)
				{
					std::string Key = Line.substr(0, DelimiterPosition);
					std::string Value = Line.substr(DelimiterPosition + 1);

					Key.erase(0, Key.find_first_not_of(" \t"));
					Key.erase(Key.find_last_not_of(" \t") + 1);
					Value.erase(0, Value.find_first_not_of(" \t"));
					Value.erase(Value.find_last_not_of(" \t") + 1);

					ConfigKey configKey = StringToKey(Key);
					SetConfigValue(CurrentSection, configKey, Value);
				}
			}
			
			File.close();
			return true;
		}
		catch (const std::exception& ex)
		{
			LastError = ex.what();
			status = ERROR;
		}
	}

	int GetErrorCount()
	{
		return GcodeErrors.size();
	}

	const char* GetError(int Index)
	{
		if (Index < 0 || Index > GcodeErrors.size())
		{
			return "";
		}

		std::string ErrorString;
		ErrorString = "Line " + std::to_string(GcodeErrors[Index].LineCount) +
			": [" + std::to_string(GcodeErrors[Index].ErrorCode) + "] " +
			GcodeErrors[Index].ErrorMessage +
			" - " + GcodeErrors[Index].LineContent;

		return ErrorString.c_str();
	}

	GCodeError GetErrorCode(int Index)
	{
		if (Index < 0 || Index >= GcodeErrors.size())
		{
			return NO_ERROR;	
		}

		return GcodeErrors[Index].ErrorCode;
	}

	void ClearErrors()
	{
		GcodeErrors.clear();
	}

	void AddError(GCodeError ErrorCode, const std::string Message,
		const std::string& Line = "", int LineCount = -1)
	{
		GcodeErrorRecord Error;
		Error.ErrorCode = ErrorCode;
		Error.ErrorMessage = Message;
		Error.LineContent = Line;
		Error.LineCount = LineCount;
		GcodeErrors.push_back(Error);

		LastError = Message;
		status = ERROR;
	}

private:

	void RunFile(const std::string& Filename)
	{
		std::ifstream File(Filename);

		try
		{
			if (!File.is_open())
			{
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
			}
		}
		catch (const std::exception& ex)
		{
			LastError = ex.what();
			status = ERROR;
		}
	}

	void ProcessLine(const std::string& Line, int LinesCount, bool IsTryingInterpret)
	{
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
					AddError(VALUE_NOT_DEFINED,
						"Unsupported G-code command: " + Command,
						CleanLine, LinesCount);
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
					AddError(VALUE_NOT_DEFINED,
						"Unsupported M-code command: " + Command,
						CleanLine, LinesCount);
					break;
				}
			}
			else if (Command[0] == 'F')
			{
			}
			else
			{
				AddError(IDENTIFIER_NOT_DEFINED,
					"Unknown command identifier: " + Command,
					CleanLine, LinesCount);
			}
		}
		else
		{
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
					AddError(VALUE_NOT_DEFINED,
						"Unsupported G-code command: " + Command,
						CleanLine, LinesCount);
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
					AddError(VALUE_NOT_DEFINED,
						"Unsupported M-code command: " + Command,
						CleanLine, LinesCount);
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
			else
			{
				AddError(IDENTIFIER_NOT_DEFINED,
					"Unknown command identifier: " + Command,
					CleanLine, LinesCount);
			}
		}
	}

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
					AddError(IDENTIFIER_NOT_DEFINED,
						"Identifier is not defined: " + Axis,
						std::to_string(Axis), LineCount);
					break;
				}
			}
		}
		else
		{
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
					AddError(IDENTIFIER_NOT_DEFINED,
						"Identifier is not defined: " + Axis,
						std::to_string(Axis), LineCount);
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

			if (std::abs(XMovement) > 0.0001 || std::abs(YMovement) > 0.0001)
			{
				MotorCommand Commands[2];

				if (std::abs(XMovement) > 0.0001)
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

				if (std::abs(YMovement) > 0.0001)
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

				if (std::abs(ZMovement) > 0.0001)
				{
					MotorCommand Command[1];
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

					if (CurrentPrinter && CurrentPrinter->VirtualTable)
					{
						CurrentPrinter->VirtualTable->RotateMotor(CurrentPrinter, Command, 1);
					}
				}
			}
		}
	}

	void ProcessHoming()
	{
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
	}
};

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

GCODE_API int GetErrorCode(InterpreterHandle Handle, int Index)
{
	if (!Handle)
	{
		return 0;
	}

	return static_cast<int>(static_cast<Interpreter*>(Handle)->GetErrorCode(Index));
}

GCODE_API void CleanErrors(InterpreterHandle Handle)
{
	if (Handle)
	{
		static_cast<Interpreter*>(Handle)->ClearErrors();
	}
}

GCODE_API const char* GetStatusString(InterpreterHandle Handle)
{
	if (!Handle)
	{
		return "INVALID_HANDLE";
	}

	Status status = static_cast<Interpreter*>(Handle)->GetStatus();

	switch (status)
	{
	case IDLE:
		return "IDLE";
	case CHECKING_CODE:
		return "CHECKING_CODE";
	case RUNNING:
		return "RUNNING";
	case PAUSED:
		return "PAUSED";
	case COMPLETED:
		return "COMPLETED";
	case ERROR:
		return "ERROR";
	default:
		return "UNKNOWN";
	}
}
