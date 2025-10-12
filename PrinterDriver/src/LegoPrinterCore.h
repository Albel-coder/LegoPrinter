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
}
