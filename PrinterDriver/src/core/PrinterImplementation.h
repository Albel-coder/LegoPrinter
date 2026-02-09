#pragma once

#include "IPrinter.h"
#include "../transport/ITransport.h"

#include <memory>
#include <atomic>

class PrinterImplementation {
public:
    IPrinter interface;

    /**
     * @brief Constructor with transport dependency injection
     * @param transport Smart transport pointer
     */
    explicit PrinterImplementation(TransportPtr transport);
    ~PrinterImplementation();

    static PrinterImplementation* from(IPrinter* self) {
        return reinterpret_cast<PrinterImplementation*>(self);
    }

    // === Public methods (via C interface) ===
    bool connect();
    bool disconnect();
    bool isConnected() const;

    // TODO: Implement these methods later
    
    //void rotateMotor(const MotorCommand* commands, int count);
    //void setMotorSpeed(uint8_t port, int8_t speed);
    //bool executeSpeedProfile(const SpeedProfile* profile);
    //double getMotorPosition(uint8_t port) const;
    //bool isMotorMoving(uint8_t port) const;

    const char* getLogEntry(int index);
    void clearLog();
    int getLogCount();

    /**
    * @brief Gets the underlying transport (for internal use)
    */
    ITransport* getTransport() {
        return transport_.get();
    }

private:

    TransportPtr transport_;
};
