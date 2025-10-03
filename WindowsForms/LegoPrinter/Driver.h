#pragma once

#ifdef LEGOPRINTERCORE_EXPORTS
#define PRINTER_DRIVER_API __declspec(dllexport)
#else
#define PRINTER_DRIVER_API __declspec(dllimport)
#endif

#include <string>

PRINTER_DRIVER_API bool IsNotDisconnect;
PRINTER_DRIVER_API bool IsTest;
PRINTER_DRIVER_API bool IsInterpreter;

PRINTER_DRIVER_API void SendCommand();

PRINTER_DRIVER_API void RotateMotor();

PRINTER_DRIVER_API void Connect();

PRINTER_DRIVER_API void Test();

PRINTER_DRIVER_API void Interpreter();