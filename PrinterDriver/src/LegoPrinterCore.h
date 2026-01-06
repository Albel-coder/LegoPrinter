#pragma once

// Automatic compiler detection
#if defined(_MSC_VER)
	// MSVC compiler
#ifdef LEGOPRINTERCORE_EXPORTS
#define PRINTER_DRIVER_API __declspec(dllexport)
#else
#define PRINTER_DRIVER_API __declspec(dllimport)
#endif
#elif defined(GNUC) || defined(clang)
	// GCC or Clang compiler (Android, Linux, macOS)
#define PRINTER_DRIVER_API attribute((visibility("default")))
#else
	// Other compilers
#define PRINTER_DRIVER_API
#pragma warning Unknown dynamic link import/export semantics.
#endif

#include "IPrinter.h"

#include <cstdint>

// C - style for maximum compatibility with C# and java UI
extern "C" 
{
	// Connection controls
	PRINTER_DRIVER_API IPrinter* CreatePrinter();
	PRINTER_DRIVER_API void DestroyPrinter(IPrinter* printer);
	
	PRINTER_DRIVER_API bool PrinterConnect(IPrinter* printer);
	PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* printer);
	PRINTER_DRIVER_API bool IsConnected(IPrinter* printer);
	PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* printer, MotorCommand* commands, int count);
	PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* printer, const unsigned char* command, int length);
	PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* printer, unsigned char port, signed char speed);

	PRINTER_DRIVER_API int GetLogCount(IPrinter* printer);
	PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* printer, int index);
	PRINTER_DRIVER_API void ClearLog(IPrinter* printer);
	PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* printer);
	PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* printer);

	PRINTER_DRIVER_API void PrinterSetLogCategories(IPrinter* printer, unsigned int categories);
	PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(IPrinter* printer);

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* printer, const SpeedProfile* profile);
	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(IPrinter* printer, const SpeedProfile* profiles, int count);

	PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* printer, int count);
	PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* printer, unsigned char port);

	// Test functions
	PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* printer, const char* testName);

	PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(IPrinter* printer);
	PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(IPrinter* printer);
	PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(IPrinter* printer, int maxAgeSeconds);
}
