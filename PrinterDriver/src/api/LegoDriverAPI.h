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

#pragma pack(push, 1)
typedef struct
{
    unsigned char port;
    signed char speed;
    double revolutions;
} MotorCommand;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    double distance; // Target position in revolutions
    signed char speed; // Speed for current position (-100 to 100)
    double tolerance; // Tolerance for position
} SpeedProfilePoint;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    unsigned char port;
    SpeedProfilePoint* points;
    int count;
    int timeoutMs; // Timeout for all profile
} SpeedProfile;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct PrinterDeviceInfoC{
    char address[64];
    char name[64];
    int rssi;
    int isLegoHub;
} PrinterDeviceInfoC;
#pragma pack(pop)

// C-style for maximum compatibility with C# and Java UI
#ifdef __cplusplus
extern "C" {
#endif

    typedef void* DriverHandle;

    // Connection controls
    PRINTER_DRIVER_API DriverHandle CreatePrinter();
    PRINTER_DRIVER_API void DestroyPrinter(DriverHandle printer);

    PRINTER_DRIVER_API int PrinterScan(DriverHandle printer, int timeoutSeconds, int legoOnly, PrinterDeviceInfoC* outDevices, int maxDevices);

    PRINTER_DRIVER_API bool PrinterConnectAuto(DriverHandle printer, int timeoutMs, bool legoOnly);
    PRINTER_DRIVER_API bool PrinterConnect(DriverHandle printer, const char* address);
    PRINTER_DRIVER_API bool PrinterReconnectLast(DriverHandle printer);
    PRINTER_DRIVER_API bool PrinterDisconnect(DriverHandle printer);
    PRINTER_DRIVER_API bool IsConnected(DriverHandle printer);
    PRINTER_DRIVER_API int PrinterGetConnectedAddress(DriverHandle printer, char* outAddress, int capacity);

    PRINTER_DRIVER_API int PrinterGetRecentHubCount(DriverHandle printer);
    PRINTER_DRIVER_API int PrinterGetRecentHub(DriverHandle printer, int index, PrinterDeviceInfoC* outHub);

    PRINTER_DRIVER_API int PrinterDetectHubMode(DriverHandle printer, const char* address);
    PRINTER_DRIVER_API bool PrinterProbeRuntime(DriverHandle printer, const char* address, int timeoutMs);

    PRINTER_DRIVER_API bool PrinterFlashFirmware(DriverHandle printer, const char* firmwareBootloaderPath, const char* address);
    PRINTER_DRIVER_API bool PrinterUploadProgram(DriverHandle printer, const char* scriptPath, const char* address);

    PRINTER_DRIVER_API bool PrinterStartUserProgram(DriverHandle printer);
    PRINTER_DRIVER_API bool PrinterStopUserProgram(DriverHandle printer);

    PRINTER_DRIVER_API bool PrinterRuntimeRotateMotor(DriverHandle printer, unsigned char port, int speed, int angle, bool hold);

    PRINTER_DRIVER_API bool PrinterSendMotorCommands(DriverHandle printer, const MotorCommand* commands, int count);

    PRINTER_DRIVER_API void PrinterRotateMotor(DriverHandle printer, MotorCommand* commands, int count);
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

    PRINTER_DRIVER_API bool RunPrinterTest(DriverHandle printer, const char* testName);

    PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(DriverHandle printer);
    PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(DriverHandle printer);
    PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(DriverHandle printer, int maxAgeSeconds);

#ifdef __cplusplus
}
#endif
