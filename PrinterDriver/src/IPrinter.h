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
        struct 
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
        uint32_t Timestamp;
    } CommandStream;

    // Encoder event types
    typedef enum 
    {
        ENCODER_POSITION_REACHED = 0,
        ENCODER_SEGMENT_COMPLETED = 1,
        ENCODER_MOVEMENT_FINISHED = 2
    } EncoderEventType;

    // Callback for encoder events
    typedef void (*EncoderCallback)(uint8_t port, EncoderEventType event, double position, void* user_data);

    // Event structure
    typedef struct 
    {
        uint8_t Port;
        EncoderEventType Type;
        double TargetPosition; // Target position in turnover
        double Tolerance;      // Tolerance
        EncoderCallback Callback;
        void* UserData;
    } EncoderEvent;

    // Virtual table - ONLY low-level methods
    typedef struct 
    {
        // Basic Operations
        bool (*Connect)(IPrinter* Self);
        bool (*Disconnect)(IPrinter* Self);
        bool (*IsConnected)(IPrinter* Self);
        void (*Destroy)(IPrinter* Self);

        // Continuous motor control
        void (*StartCommandStream)(IPrinter* Self, const CommandStream* Stream);
        void (*UpdateCommandStream)(IPrinter* Self, const CommandStream* Stream);
        void (*StopCommandStream)(IPrinter* Self);

        // Direct control (for backward compatibility)
        void (*RotateMotor)(IPrinter* Self, const MotorCommandExe* Commands, int Count);

        // Raw teams
        void (*SendRawCommand)(IPrinter* Self, const unsigned char* Command, int Length);

        // Encoder event system
        bool (*SubscribeToEncoderEvents)(IPrinter* Self, const EncoderEvent* events, int count);
        bool (*UnsubscribeFromEncoderEvents)(IPrinter* Self, uint8_t port);
        bool (*WaitForEncoderEvent)(IPrinter* Self, uint8_t port, EncoderEventType event_type,
            double target_position, double tolerance, int timeout_ms);

        // Monitoring
        bool (*IsMotorMoving)(IPrinter* Self, unsigned char Port);
        double (*GetMotorPosition)(IPrinter* Self, unsigned char Port);

        // Logging
        int (*GetLogCount)(IPrinter* Self);
        const char* (*GetLogEntry)(IPrinter* Self, int Index);
        void (*ClearLog)(IPrinter* Self);
        const char* (*GetLastError)(IPrinter* Self);

    } IPrinterVirtualTable;

    struct IPrinter 
    {
        IPrinterVirtualTable* VirtualTable;
    };

#ifdef __cplusplus
}
#endif
