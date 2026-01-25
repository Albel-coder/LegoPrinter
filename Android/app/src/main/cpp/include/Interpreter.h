// Automatic platform and compiler detection
#if defined(_WIN32) || defined(_WIN64)
    #define GCODE_WINDOWS 1
    #define GCODE_ANDROID 0
#elif defined(__ANDROID__)
    #define GCODE_WINDOWS 0
    #define GCODE_ANDROID 1
#else
    #define GCODE_WINDOWS 0
    #define GCODE_ANDROID 0
#endif

// Export/Import settings
#if GCODE_WINDOWS
    #ifdef GCODEINTERPRETER_EXPORTS
        #define GCODE_API __declspec(dllexport)
    #else
        #define GCODE_API __declspec(dllimport)
    #endif
#elif GCODE_ANDROID
    // For Android with GCC/Clang
    #if defined(__GNUC__) || defined(__clang__)
        #define GCODE_API __attribute__((visibility("default")))
    #else
        #define GCODE_API
    #endif
#endif

#include "IPrinter.h"

typedef void* InterpreterHandle;

#ifdef __cplusplus
extern "C" {
#endif
	GCODE_API InterpreterHandle CreateInterpreter();
	GCODE_API void DestroyInterpreter(InterpreterHandle handle);

	GCODE_API bool TestCode(InterpreterHandle handle, IPrinter* printer);
	GCODE_API bool ExecuteGcode(InterpreterHandle handle, const char* filename, IPrinter* printer);
	GCODE_API bool ExecuteLine(InterpreterHandle handle, const char* line, IPrinter* printer);

	GCODE_API void PauseExecution(InterpreterHandle handle);
	GCODE_API void ResumeExecution(InterpreterHandle handle);

	GCODE_API int GetStatus(InterpreterHandle handle);
	GCODE_API double GetProgress(InterpreterHandle handle);
	GCODE_API const char* GetLastInterpreterError(InterpreterHandle handle);
	GCODE_API const char* GetError(InterpreterHandle handle, int index);
	GCODE_API int GetLogCount(InterpreterHandle handle);
	GCODE_API const char* GetLogEntry(InterpreterHandle handle, int index);
	GCODE_API void ClearLog(InterpreterHandle handle);
	GCODE_API bool ReadConfig(InterpreterHandle handle, const char* filename);
	GCODE_API void SetLogCategories(InterpreterHandle handle, unsigned int categories);
	GCODE_API unsigned int GetLogCategories(InterpreterHandle handle);
	GCODE_API int GetFilterLogCount(InterpreterHandle handle, unsigned int categoryMask);
#ifdef __cplusplus
}
#endif