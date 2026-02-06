#pragma once

// Automatic platform and compiler detection
#if defined(_WIN32) || defined(_WIN64)
#define LEGOPRINTER_WINDOWS 1
#define LEGOPRINTER_ANDROID 0
#elif defined(__ANDROID__)
#define LEGOPRINTER_WINDOWS 0
#define LEGOPRINTER_ANDROID 1
#else
#define LEGOPRINTER_WINDOWS 0
#define LEGOPRINTER_ANDROID 0
#endif

// Export/Import settings
#if LEGOPRINTER_WINDOWS
#ifdef LEGOPRINTERCORE_EXPORTS
#define PRINTER_DRIVER_API __declspec(dllexport)
#else
#define PRINTER_DRIVER_API __declspec(dllimport)
#endif
#elif LEGOPRINTER_ANDROID
    // For Android with GCC/Clang
#if defined(__GNUC__) || defined(__clang__)
#define PRINTER_DRIVER_API __attribute__((visibility("default")))
#else
#define PRINTER_DRIVER_API
#endif
#else
    // For other platforms (Linux/macOS)
#define PRINTER_DRIVER_API
#endif

#include "../core/IPrinter.h"
#include <cstdint>

// C-style for maximum compatibility with C# and Java UI
#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
