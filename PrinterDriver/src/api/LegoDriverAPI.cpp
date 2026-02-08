#include "LegoDriverAPI.h"
#include "../core/PrinterImplementation.h"
#include "../platform/PrinterFactory.h"
#include <mutex>
#include <map>

extern IPrinterVirtualTable PrinterVTable;

// Tested function - remove after deep testing

bool testSpeedProfileAdvanced(IPrinter* Printer) {
    SpeedProfilePoint points[] = {
        {3.0, 20, 0.0005},
        {3.0, 30, 0.0005},
        {0.0, 0, 1.0}
    };

    SpeedProfile profile;
    profile.port = 0x00;
    profile.points = points;
    profile.count = 3;
    profile.timeoutMs = 30000;

    bool result = PrinterExecuteSpeedProfile(Printer, &profile);
    return result;
}

// C-INTERFACE functions are exported to DLL
extern "C"
{
    PRINTER_DRIVER_API IPrinter* CreatePrinter() {
        auto transport = PrinterFactory::CreateTransport();

        if (!transport) return nullptr;

        auto implementation = std::make_unique<PrinterImplementation>(std::move(transport));
        implementation->interface.vtable = &PrinterVTable;

        auto* handle = implementation.get();
        contexts[handle] = std::move(implementation);

        return &handle->interface;
    }

    PRINTER_DRIVER_API void DestroyPrinter(IPrinter* printer)
    {
        if (!printer) return;

        try {
            if (printer->vtable && printer->vtable->printer_destroy) {
                printer->vtable->printer_destroy(printer);
            }
        }
        catch (...) {
            // Ignore all errors
        }
    }

    PRINTER_DRIVER_API bool PrinterConnect(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_connect) return false;

        return printer->vtable->printer_connect(printer);
    }

    PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_disconnect) return false;

        return printer->vtable->printer_disconnect(printer);
    }

    PRINTER_DRIVER_API bool IsConnected(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_connected) return false;

        return printer->vtable->printer_is_connected(printer);
    }

    PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* printer, MotorCommand* commands, int count) {
        if (!printer || !printer->vtable || !printer->vtable->printer_rotate_motor) return;

        return printer->vtable->printer_rotate_motor(printer, commands, count);
    }

    PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* printer, const unsigned char* command, int length) {
        if (!printer || !printer->vtable || !printer->vtable->printer_send_command) return;

        return printer->vtable->printer_send_command(printer, command, length);
    }

    PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* printer, unsigned char port, signed char speed) {
        if (!printer || !printer->vtable || !printer->vtable->printer_set_motor_speed) return;

        return printer->vtable->printer_set_motor_speed(printer, port, speed);
    }

    PRINTER_DRIVER_API int GetLogCount(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_log_count) return 0;

        return printer->vtable->printer_get_log_count(printer);
    }

    PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* printer, int index) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_log_entry) return "";

        return printer->vtable->printer_get_log_entry(printer, index);
    }

    PRINTER_DRIVER_API void ClearLog(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_clear_log) return;

        return printer->vtable->printer_clear_log(printer);
    }

    PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_last_error) return "";

        return printer->vtable->printer_get_last_error(printer);
    }

    PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* printer, int count) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_motor_moving) return false;

        return printer->vtable->printer_is_motor_moving(printer, count);
    }

    PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* printer, unsigned char port) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_motor_position) return 0.0;

        return printer->vtable->printer_get_motor_position(printer, port);
    }

    // Test function
    PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* printer, const char* testName) {
        if (!printer || !testName) return false;

        std::string name(testName);

        if (name == "SpeedProfileAdvanced") {
            return testSpeedProfileAdvanced(printer);
        }
        else {
            return false;
        }
    }

    PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* printer) {
        if (printer) printer->vtable->printer_printer_connection_info(printer);
    }

    PRINTER_DRIVER_API void PrinterSetLogCategories(IPrinter* printer, unsigned int categories)
    {
        if (printer && categories > 0) printer->vtable->printer_set_log_categories(printer, categories);
    }

    PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(IPrinter* printer)
    {
        if (!printer || !printer->vtable) return 0;

        return printer->vtable->printer_get_log_categories(printer);
    }

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* printer, const SpeedProfile* profile) {
        if (!printer || !printer->vtable || !printer->vtable->printer_printer_execute_speed_profile) return false;

        return printer->vtable->printer_printer_execute_speed_profile(printer, profile);
    }

    PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(IPrinter* printer, const SpeedProfile* profiles, int count) {
        if (!printer || !printer->vtable) return false;

        if (printer->vtable->printer_execute_speed_profiles) {
            return printer->vtable->printer_execute_speed_profiles(printer, profiles, count);
        }
        else {
            return false;
        }
    }

    PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_request_battery_level) return false;

        return printer->vtable->printer_request_battery_level(printer);
    }

    PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(IPrinter* printer) {
        if (!printer || !printer->vtable || !printer->vtable->printer_get_battery_level) return 0;

        return printer->vtable->printer_get_battery_level(printer);
    }

    PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(IPrinter* printer, int maxAgeSeconds) {
        if (!printer || !printer->vtable || !printer->vtable->printer_is_battery_fresh) return false;

        return printer->vtable->printer_is_battery_fresh(printer, maxAgeSeconds);
    }
}