#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct IPrinter IPrinter;

    typedef struct
    {
        unsigned char Port;
        signed char Speed;
        double Revolutions;
    } MotorCommand;

    typedef struct
    {
        double Distance; // Target position in revolutions
        signed char Speed; // Speed for current position (-100 to 100)
        double Tolerance; // Tolerance for position
    } SpeedProfilePoint;

    typedef struct
    {
        unsigned char Port;
        SpeedProfilePoint* Points;
        int Count;
        int TimeoutMs; // Timeout for all profile
    } SpeedProfile;

    // Virtual table - ONLY low-level methods
    typedef struct 
    {
        // Basic Operations
        bool (*Connect)(IPrinter* Self);
        bool (*Disconnect)(IPrinter* Self);
        bool (*IsConnected)(IPrinter* Self);
        void (*Destroy)(IPrinter* Self);

        void (*RotateMotor)(IPrinter* Self, const MotorCommand* Commands, int Count);

        void (*SetMotorSpeed)(IPrinter* Self, unsigned char Port, signed char Speed);

        // Raw command
        void (*SendCommand)(IPrinter* Self, const unsigned char* Command, int Length);

        bool (*PrinterExecuteSpeedProfile)(IPrinter* Self, const SpeedProfile* Profile);

        // Monitoring
        bool (*IsMotorMoving)(IPrinter* Self, unsigned char Port);
        double (*GetMotorPosition)(IPrinter* Self, unsigned char Port);

        // Logging
        int (*GetLogCount)(IPrinter* Self);
        const char* (*GetLogEntry)(IPrinter* Self, int Index);
        void (*ClearLog)(IPrinter* Self);
        const char* (*GetLastError)(IPrinter* Self);
        void (*PrintConnectionInfo)(IPrinter* Self);

        bool (*TestEncoderFunctionality)(IPrinter* Self);

    } IPrinterVirtualTable;

    struct IPrinter 
    {
        IPrinterVirtualTable* VirtualTable;
    };    

#ifdef __cplusplus
}
#endif
