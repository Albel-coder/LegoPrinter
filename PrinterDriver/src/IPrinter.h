#pragma once

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct IPrinter IPrinter;

    // Motor operating modes
    typedef enum 
    {
        STOP = 0,
        CONST_SPEED = 1, // Constant speed mode
        POSITION = 2,    // Positioning mode
        PROFILE = 3      // Speed ​​profile mode
    } MotorMode;

    typedef struct
    {
        unsigned char Port;
        signed char Speed;
        double Revolutions;
    } MotorCommand;

    // Optimized motor command
    typedef struct 
    {
        MotorMode Mode;

        // For speed mode
        signed char Speed;

        // For positioning mode
        double TargetRevolutions;
        signed char MaxSpeed;

        // For profile mode
        struct Profile
        {
            signed char StartSpeed;
            signed char EndSpeed;
            double Acceleration; // revolutions / (second^2)
            double Distance;     // revolutions
        } Profile;

    } MotorCommandExe;

    // Optimized command flow structure
    typedef struct 
    {
        unsigned char Port;
        MotorCommandExe* Commands;
        int Count;
        unsigned int Timestamp;
    } CommandStream;

    // Encoder event types
    typedef enum 
    {
        ENCODER_POSITION_REACHED = 0,
        ENCODER_SEGMENT_COMPLETED = 1,
        ENCODER_MOVEMENT_FINISHED = 2
    } EncoderEventType;

    // Callback for encoder events
    typedef void (*EncoderCallback)(unsigned char port, EncoderEventType event, double position, void* user_data);

    // Event structure
    typedef struct 
    {
        unsigned char Port;
        EncoderEventType Type;
        double TargetPosition; // Target position in turnover
        double Tolerance;      // Tolerance
        EncoderCallback Callback;
        void* UserData;
    } EncoderEvent;

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

    typedef struct
    {
        unsigned char Port;
        signed char Speed;
        bool Immediate;
    } SpeedChangeCommand;

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

        // Encoder event system
        bool (*SubscribeToEncoderEvents)(IPrinter* Self, const EncoderEvent* events, int count);
        bool (*UnsubscribeFromEncoderEvents)(IPrinter* Self, unsigned char port);
        bool (*WaitForEncoderEvent)(IPrinter* Self, unsigned char port, EncoderEventType event_type,
            double target_position, double tolerance, int timeout_ms);

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
