#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct IPrinter IPrinter;

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

    // Virtual table - ONLY low-level methods
    typedef struct 
    {
        // Basic Operations
        bool (*printer_connect)(IPrinter* self);
        bool (*disconnect)(IPrinter* self);
        bool (*isConnected)(IPrinter* self);
        void (*Destroy)(IPrinter* self);

        void (*rotateMotor)(IPrinter* self, const MotorCommand* commands, int count);

        void (*setMotorSpeed)(IPrinter* self, unsigned char port, signed char speed);

        // Raw command
        void (*sendCommand)(IPrinter* self, const unsigned char* command, int length);

        bool (*PrinterExecuteSpeedProfile)(IPrinter* self, const SpeedProfile* profile);

        // Monitoring
        bool (*isMotorMoving)(IPrinter* self, unsigned char port);
        double (*getMotorPosition)(IPrinter* self, unsigned char port);

        // Logging
        int (*getLogCount)(IPrinter* self);
        const char* (*getLogEntry)(IPrinter* self, int index);
        void (*clearLog)(IPrinter* self);
        const char* (*getLastError)(IPrinter* self);
        void (*printConnectionInfo)(IPrinter* self);

        bool (*testEncoderFunctionality)(IPrinter* self);

    } IPrinterVirtualTable;

    struct IPrinter 
    {
        IPrinterVirtualTable* vtable;
    };    

#ifdef __cplusplus
}
#endif
