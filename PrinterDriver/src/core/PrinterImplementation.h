#pragma once

#include "IPrinter.h"
#include "../transport/ITransport.h"
//#include "ProtocolParser.h"
#include <memory>
#include <atomic>

class PrinterImplementation {
public:
    IPrinter interface;

    /**
     * @brief Конструктор с внедрением зависимости транспорта
     * @param transport Умный указатель на транспорт
     */
    PrinterImplementation(TransportPtr transport);
    ~PrinterImplementation();

    // === Публичные методы (через C-интерфейс) ===
    bool connect();
    bool disconnect();
    bool isConnected() const;

    void rotateMotor(const MotorCommand* commands, int count);
    void setMotorSpeed(uint8_t port, int8_t speed);
    bool executeSpeedProfile(const SpeedProfile* profile);
    double getMotorPosition(uint8_t port) const;
    bool isMotorMoving(uint8_t port) const;

    // Логирование
    int getLogCount() const;
    const char* getLogEntry(int index) const;
    void clearLog();
    const char* getLastErrorMessage() const;

private:
    // Транспорт
    TransportPtr transport_;


};
