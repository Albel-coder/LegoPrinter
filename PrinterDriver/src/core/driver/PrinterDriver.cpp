#include "PrinterDriver.h"
#include "../logging/LogManager.h"


#include <cstdarg>
#include <map>
#include <thread>

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transport) 
    : transport(std::move(transport)),
    motorManager(std::make_unique<MotorManager>(*transport)) {

    gLog.setLogCategories(LOG_CATEGORY_ALL);
    LOG_INFO("PrinterImplementation created");
}

PrinterDriver::~PrinterDriver() {
    LOG_INFO("PrinterImplementation destroyed");
}

bool PrinterDriver::flashFirmware(const std::filesystem::path& firmwareBootloaderPath, const std::string& address,
    PrinterFirmware::ProgressCallback progress = nullptr, PrinterFirmware::LogCallback log = nullptr) {

    const bool result = printerFirmware->flashFirmware(firmwareBootloaderPath, address, progress, log);

    if (!result) {
        LOG_ERROR("flashFirmware failed");
    }
    return result;
}

bool PrinterDriver::uploadProgram(const std::filesystem::path& scriptFile, const std::string& address,
    PrinterFirmware::ProgressCallback progress = nullptr, PrinterFirmware::LogCallback log = nullptr) {

    const bool result = printerFirmware->uploadProgram(scriptFile, address, progress, log);
    if (result) {
        LOG_ERROR("uploadProgram failed");
    }
    return result;
}

bool PrinterDriver::connect(const std::string& address) {
    const bool result = printerFirmware->connectRuntime(address);
    if (!result) {
        LOG_ERROR("connect failed");
    }
    return result;
}

bool PrinterDriver::disconnect() {
    printerFirmware->disconnectRuntime();
    return true;
}

bool PrinterDriver::sendCommand(const uint8_t* data, size_t length) {
    const bool result = printerFirmware->sendRuntimePacket(data, length, false);
    if (!result) {
        LOG_ERROR("sendCommand failed.");
    }
    return result;
}

bool PrinterDriver::isConnected() {
    return transport->isConnected();
}

void PrinterDriver::rotateMotor(const MotorCommand* commands, int count) {
    motorManager->rotate(commands, count);
}

void PrinterDriver::setMotorSpeed(uint8_t port, int8_t speed) {
    motorManager->setSpeed(port, speed);
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
    return motorManager->getPosition(port);
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
