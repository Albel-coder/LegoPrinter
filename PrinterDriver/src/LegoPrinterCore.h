#pragma once

#ifdef LEGOPRINTERCORE_EXPORTS
#define PRINTER_DRIVER_API __declspec(dllexport)
#else
#define PRINTER_DRIVER_API __declspec(dllimport)
#endif

#include "IPrinter.h"

#include <cstdint>

// C - style for maximum compatibility with C# and java UI
extern "C" 
{
	// Connection controls
	PRINTER_DRIVER_API IPrinter* CreatePrinter();
	PRINTER_DRIVER_API void DestroyPrinter(IPrinter* Printer);
	
	PRINTER_DRIVER_API bool PrinterConnect(IPrinter* Printer);
	PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* Printer);
	PRINTER_DRIVER_API bool IsConnected(IPrinter* Printer);
	PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* Printer, MotorCommand* Commands, int Count);
	PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* Printer, const unsigned char* Command, int Length);
	PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* Printer, unsigned char Port, signed char Speed);

	PRINTER_DRIVER_API int GetLogCount(IPrinter* Printer);
	PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* Printer, int Index);
	PRINTER_DRIVER_API void ClearLog(IPrinter* Printer);
	PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* Printer);
	PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* Printer);

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* Printer, const SpeedProfile* Profile);

	PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* Printer, int Count);
	PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* Printer, unsigned char Port);

	// Test functions
	PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* Printer, const char* TestName);
}
