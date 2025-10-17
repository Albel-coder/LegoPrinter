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

	GCODE_API int GetErrorCount(InterpreterHandle Handle);
	GCODE_API const char* GetError(InterpreterHandle Handle, int Index);
	GCODE_API int GetErrorCode(InterpreterHandle Handle, int Index);
	GCODE_API void CleanErrors(InterpreterHandle Handle);
	GCODE_API const char* GetStatusString(InterpreterHandle Handle);
}