#pragma once

#ifdef GCODEINTERPRETER_EXPORTS
#define GCODE_API __declspec(dllexport)
#else
#define GCODE_API __declspec(dllimport)
#endif

#include "IPrinter.h"

typedef void* InterpreterHandle;

extern "C"
{
	GCODE_API InterpreterHandle CreateInterpreter();
	GCODE_API void DestroyInterpreter(InterpreterHandle Handle);

	GCODE_API bool TestCode(InterpreterHandle Handle, IPrinter* Printer);
	GCODE_API bool ExecuteGcode(InterpreterHandle Handle, IPrinter* Printer, const char* Filename);

	GCODE_API void PauseExecution(InterpreterHandle Handle);
	GCODE_API void ResumeExecution(InterpreterHandle Handle);

	GCODE_API int GetStatus(InterpreterHandle Handle);
	GCODE_API double GetProgress(InterpreterHandle Handle);
	GCODE_API const char* GetLastInterpreterError(InterpreterHandle Handle);
	GCODE_API int GetErrorCount(InterpreterHandle Handle);
	GCODE_API const char* GetError(InterpreterHandle Handle, int Index);
	GCODE_API int GetLogCount(InterpreterHandle Handle);
	GCODE_API const char* GetLogEntry(InterpreterHandle Handle, int Index);
	GCODE_API void ClearErrors(InterpreterHandle Handle);
	GCODE_API void ClearLog(InterpreterHandle Handle);
	GCODE_API bool ReadConfing(InterpreterHandle Handle, const char* Filename);
}