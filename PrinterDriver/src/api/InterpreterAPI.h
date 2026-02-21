#pragma once
#include "LegoDriverAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef void* InterpreterHandle;

    PRINTER_DRIVER_API InterpreterHandle CreateInterpreter(DriverHandle printer);
    PRINTER_DRIVER_API void DestroyInterpreter(InterpreterHandle handle);

    PRINTER_DRIVER_API bool ExecuteGCode(InterpreterHandle handle, const char* filename);
    PRINTER_DRIVER_API bool ExecuteLine(InterpreterHandle handle, const char* line);

    PRINTER_DRIVER_API void PauseExecution(InterpreterHandle handle);
    PRINTER_DRIVER_API void ResumeExecution(InterpreterHandle handle);
    PRINTER_DRIVER_API int GetStatus(InterpreterHandle handle);
    PRINTER_DRIVER_API double GetProgress(InterpreterHandle handle);
    PRINTER_DRIVER_API const char* GetLastInterpreterError(InterpreterHandle handle);
	
    PRINTER_DRIVER_API bool ReadConfig(InterpreterHandle handle, const char* filename);

#ifdef __cplusplus
}
#endif