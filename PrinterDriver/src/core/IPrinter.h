#pragma once

// Platform-independent part of the API
typedef struct IPrinter IPrinter;  // Opaque handle - declaration only

// Data structures (no methods, no vtable)
typedef struct {
    unsigned char port;
    signed char speed;
    double revolutions;
} MotorCommand;

typedef struct {
    double distance;
    signed char speed;
    double tolerance;
} SpeedProfilePoint;

typedef struct {
    unsigned char port;
    SpeedProfilePoint* points;
    int count;
    int timeoutMs;
} SpeedProfile;

// C API functions
#ifdef __cplusplus
extern "C" {
#endif

    IPrinter* CreatePrinter();
    void DestroyPrinter(IPrinter* printer);
    bool PrinterConnect(IPrinter* printer);
    bool PrinterDisconnect(IPrinter* printer);
    bool IsConnected(IPrinter* printer);
    void PrinterRotateMotor(IPrinter* printer, MotorCommand* commands, int count);
    // ... 

#ifdef __cplusplus
}
#endif
