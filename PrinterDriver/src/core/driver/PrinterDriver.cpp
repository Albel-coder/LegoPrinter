#include "PrinterDriver.h"
#include "../logging/LogManager.h"
#include <cstdarg>
#include <map>
#include <thread>

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transport) 
    : transport_(std::move(transport)),
    motorManager_(std::make_unique<MotorManager>(*transport_)) {

    gLog.setLogCategories(LOG_CATEGORY_ALL);
    LOG_INFO("PrinterImplementation created");
}

PrinterDriver::~PrinterDriver() {
    LOG_INFO("PrinterImplementation destroyed");
}

bool PrinterDriver::connect() {
    if (!transport_) {
        LOG_ERROR("Transport not initialized");
        return false;
    }

    LOG_INFO("Starting connection...");
    bool connected = transport_->open();

    const auto timeout = std::chrono::seconds(30);
    auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < timeout) {
        if (transport_->isConnected()) {
            LOG_INFO("Connection established");

            transport_->setDataCallback([this](const uint8_t* data, size_t length) {
                motorManager_->handleNotification(data, length);
            });

            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    LOG_ERROR("Connection timeout");
    return false;
}

bool PrinterDriver::disconnect() {
    if (!transport_) {
        LOG_ERROR("Transport not initialized");
        return false;
    }

    LOG_INFO("Disconnecting using transport: %s", transport_->getName());

    transport_->close();
    return true;
}

bool PrinterDriver::isConnected() {
    return transport_->isConnected();
}

void PrinterDriver::rotateMotor(const MotorCommand* commands, int count) {
    motorManager_->rotate(commands, count);
}

void PrinterDriver::sendCommand(const unsigned char* command, int length) {
    transport_->write(command, length);
}

void PrinterDriver::setMotorSpeed(uint8_t port, int8_t speed) {
    motorManager_->setSpeed(port, speed);
}

int PrinterDriver::getLogCount() {
    return gLog.getLogCount();
}

void PrinterDriver::clearLog() {
    gLog.clearLog();
}

const char* PrinterDriver::getLogEntry(int index) {
    return gLog.getLogEntry(index);
}

const char* PrinterDriver::getLastErrorMessage() {
    return "";
}

void PrinterDriver::printerConnectionInfo() {

}

void PrinterDriver::printerSetLogCategories(unsigned int categories) {

}

unsigned int PrinterDriver::printerGetLogCategories() {
    return 0;
}

bool PrinterDriver::printerExecuteSpeedProfile(const SpeedProfile* profile) {
    return false;
}

bool PrinterDriver::printerExecuteSpeedProfiles(const SpeedProfile* profiles, int count) {
    return false;
}

bool PrinterDriver::printerIsMotorMoving(int count) {
    return false;
}

double PrinterDriver::printerGetMotorPosition(unsigned char port) {
    return motorManager_->getPosition(port);
}

bool PrinterDriver::runPrinterTest(const char* testName) {
    return false;
}

bool PrinterDriver::printerRequestBatteryLevel() {
    return false;
}

unsigned char PrinterDriver::printerGetBatteryLevel() {
    return 0;
}

bool PrinterDriver::printerIsBatteryLevelFresh(int maxAgeSeconds) {
    return false;
}
