#include "PrinterImplementation.h"
#include "../logging/Logger.h"
#include <cstdarg>
#include <map>

// Main context and virtual table
namespace
{
    std::mutex contextsMutex;
    std::map<PrinterImplementation*, std::unique_ptr<PrinterImplementation>> contexts;

    // Virtual table functions - a bridge between C++ and C-INTERFACE
    bool printer_connect(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        return implementation->connect();
    }

    bool printer_disconnect(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        return implementation->disconnect();
    }

    bool printer_is_connected(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->isConnected();
        return false;
    }

    void printer_destroy(IPrinter* self) {
        if (!self) return;

        try {
            PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);

            //Implementation->safeShutdown();

            std::lock_guard<std::mutex> lock(contextsMutex);

            if (contexts.find(implementation) != contexts.end()) {
                contexts.erase(implementation);
            }
        }
        catch (...) {
            // Ignore all errors
        }
    }

    void printer_set_motor_speed(IPrinter* self, unsigned char port, signed char speed) {
        if (!self || !self->vtable) return;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //implementation->setMotorSpeed(port, speed);
    }

    void printer_rotate_motor(IPrinter* self, const MotorCommand* commands, int count) {
        if (!self || !self->vtable || !commands || count < -1) return;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //implementation->rotateMotor(commands, count);
    }

    bool printer_printer_execute_speed_profile(IPrinter* self, const SpeedProfile* profile) {
        if (!self || !self->vtable || !profile) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->executeSpeedProfile(profile);
    }

    void printer_send_command(IPrinter* self, const unsigned char* command, int length) {
        if (!self || !self->vtable || !command || length < -1) return;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //implementation->sendCommand(command, length);
    }

    bool printer_is_motor_moving(IPrinter* self, unsigned char port) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->isMotorMoving(port);
        return false;
    }

    double printer_get_motor_position(IPrinter* self, unsigned char port) {
        if (!self || !self->vtable) return 0.0;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->getMotorPosition(port);
        return 0.0;
    }

    int printer_get_log_count(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        return implementation->getLogCount();
    }

    const char* printer_get_log_entry(IPrinter* self, int index) {
        if (!self || !self->vtable) return "";

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        return implementation->getLogEntry(index);
    }

    void printer_printer_connection_info(IPrinter* self) {
        if (!self || !self->vtable) return;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->printConnectionInfo();
    }

    void printer_clear_log(IPrinter* self) {
        if (!self || !self->vtable) return;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        return implementation->clearLog();
    }

    const char* printer_get_last_error(IPrinter* self) {
        if (!self || !self->vtable) return "";

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->getLastErrorMessage();
        return "";
    }

    bool printer_test_encoder_functionality(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->testEncoderFunctionality(self);
        return false;
    }

    bool printer_execute_speed_profiles(IPrinter* self, const SpeedProfile* profiles, int count) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->executeSpeedProfiles(profiles, count);
        return false;
    }

    bool printer_request_battery_level(IPrinter* self) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->requestBatteryLevel();
        return false;
    }

    unsigned char printer_get_battery_level(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->getBatteryLevel();
        return ' ';
    }

    bool printer_is_battery_fresh(IPrinter* self, int maxAgeSeconds) {
        if (!self || !self->vtable) return false;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->isBatteryLevelFresh(maxAgeSeconds);
        return false;
    }

    void printer_set_log_categories(IPrinter* self, uint32_t categories) {
        if (!self || !self->vtable) return;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->setLogCategories(categories);
    }

    unsigned int printer_get_log_categories(IPrinter* self) {
        if (!self || !self->vtable) return 0;

        PrinterImplementation* implementation = reinterpret_cast<PrinterImplementation*>(self);
        //return implementation->getLogCategories();
        return 0;
    }
}

// Virtual Method Table - C-INTERFACE
static IPrinterVirtualTable PrinterVTable = {
    printer_connect,
    printer_disconnect,
    printer_is_connected,
    printer_destroy,
    printer_rotate_motor,
    printer_set_motor_speed,
    printer_send_command,
    printer_printer_execute_speed_profile,
    printer_is_motor_moving,
    printer_get_motor_position,
    printer_get_log_count,
    printer_get_log_entry,
    printer_clear_log,
    printer_get_last_error,
    printer_printer_connection_info,
    printer_set_log_categories,
    printer_get_log_categories,
    printer_test_encoder_functionality,
    printer_execute_speed_profiles,
    printer_request_battery_level,
    printer_get_battery_level,
    printer_is_battery_fresh
};

PrinterImplementation::PrinterImplementation(TransportPtr transport) : transport_(std::move(transport)) {
	interface.vtable = &PrinterVTable;

	LOG_INFO("Create driver implementation");
}

PrinterImplementation::~PrinterImplementation() = default;

bool PrinterImplementation::disconnect() {
	if (!transport_) {
		LOG_ERROR("Transport not initialized");
		return false;
	}

	LOG_INFO("Disconnecting using transport: %s", transport_->getName());

	transport_->close();
	return true;
}

bool PrinterImplementation::connect() {
	if (!transport_) {
		LOG_ERROR("Transport not initialized");
		return false;
	}

	LOG_INFO("Connecting using transport: %s", transport_->getName());

	return transport_->open();
}

int PrinterImplementation::getLogCount() {
	return Logger::instance().getLogCount();
}

void PrinterImplementation::clearLog() {
	return Logger::instance().clearLog();
}

const char* PrinterImplementation::getLogEntry(int index) {
	return Logger::instance().getLogEntry(index);
}
