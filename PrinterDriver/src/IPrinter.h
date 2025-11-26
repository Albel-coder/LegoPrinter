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
        bool (*printer_disconnect)(IPrinter* self);
        bool (*printer_is_connected)(IPrinter* self);
        void (*printer_destroy)(IPrinter* self);

        void (*printer_rotate_motor)(IPrinter* self, const MotorCommand* commands, int count);

        void (*printer_set_motor_speed)(IPrinter* self, unsigned char port, signed char speed);

        // Raw command
        void (*printer_send_command)(IPrinter* self, const unsigned char* command, int length);

        bool (*printer_printer_execute_speed_profile)(IPrinter* self, const SpeedProfile* profile);

        // Monitoring
        bool (*printer_is_motor_moving)(IPrinter* self, unsigned char port);
        double (*printer_get_motor_position)(IPrinter* self, unsigned char port);

        // Logging
        int (*printer_get_log_count)(IPrinter* self);
        const char* (*printer_get_log_entry)(IPrinter* self, int index);
        void (*printer_clear_log)(IPrinter* self);
        const char* (*printer_get_last_error)(IPrinter* self);
        void (*printer_printer_connection_info)(IPrinter* self);

        bool (*printer_test_encoder_functionality)(IPrinter* self);

    } IPrinterVirtualTable;

    struct IPrinter 
    {
        IPrinterVirtualTable* vtable;
    };    

#ifdef __cplusplus
}
#endif
