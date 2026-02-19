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

typedef struct
{
    unsigned char port;
    signed char speed;
    double revolutions;
} MotorCommand;

typedef struct
{
    double distance; // Target position in revolutions
    signed char speed; // Speed for current position (-100 to 100)
    double tolerance; // Tolerance for position
} SpeedProfilePoint;

typedef struct
{
    unsigned char port;
    SpeedProfilePoint* points;
    int count;
    int timeoutMs; // Timeout for all profile
} SpeedProfile;


// C-style for maximum compatibility with C# and Java UI
#ifdef __cplusplus
extern "C" {
#endif

    typedef void* DriverHandle;

    // Connection controls
    PRINTER_DRIVER_API DriverHandle CreatePrinter();
    PRINTER_DRIVER_API void DestroyPrinter(DriverHandle printer);

    PRINTER_DRIVER_API bool PrinterConnect(DriverHandle printer);
    PRINTER_DRIVER_API bool PrinterDisconnect(DriverHandle printer);
    PRINTER_DRIVER_API bool IsConnected(DriverHandle printer);
    PRINTER_DRIVER_API void PrinterRotateMotor(DriverHandle printer, MotorCommand* commands, int count);
    PRINTER_DRIVER_API void PrinterSendCommand(DriverHandle printer, const unsigned char* command, int length);
    PRINTER_DRIVER_API void PrinterSetMotorSpeed(DriverHandle printer, unsigned char port, signed char speed);

    PRINTER_DRIVER_API int GetLogCount(DriverHandle printer);
    PRINTER_DRIVER_API const char* GetLogEntry(DriverHandle printer, int index);
    PRINTER_DRIVER_API void ClearLog(DriverHandle printer);
    PRINTER_DRIVER_API const char* GetLastErrorMessage(DriverHandle printer);
    PRINTER_DRIVER_API void PrinterConnectionInfo(DriverHandle printer);

    PRINTER_DRIVER_API void PrinterSetLogCategories(DriverHandle printer, unsigned int categories);
    PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(DriverHandle printer);

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(DriverHandle printer, const SpeedProfile* profile);
    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(DriverHandle printer, const SpeedProfile* profiles, int count);

    PRINTER_DRIVER_API bool PrinterIsMotorMoving(DriverHandle printer, int count);
    PRINTER_DRIVER_API double PrinterGetMotorPosition(DriverHandle printer, unsigned char port);

    // Test functions
    PRINTER_DRIVER_API bool RunPrinterTest(DriverHandle printer, const char* testName);

    PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(DriverHandle printer);
    PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(DriverHandle printer);
    PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(DriverHandle printer, int maxAgeSeconds);

#ifdef __cplusplus
}
#endif
