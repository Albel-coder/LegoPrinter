#include "LegoPrinterCore.h"
#include <cstdarg>
#include <map>
#include <windows.h>

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transport) : transport_(std::move(transport)) {
    logger.info("PrinterImplementation created");
}

PrinterDriver::~PrinterDriver() {
    logger.info("PrinterImplementation destroyed");
}

bool PrinterDriver::connect() {
	if (!transport_) {
        logger.error("Transport not initialized");
		return false;
	}

    logger.info("Connecting using transport: %s", transport_->getName());

	bool result = transport_->open();
    if (result) {
        logger.info("Connection successful!");
    }
    else {
        logger.info("Connection failed");
    }

    return result;
}

bool PrinterDriver::disconnect() {
    if (!transport_) {
        logger.error("Transport not initialized");
        return false;
    }

    logger.info("Disconnecting using transport: %s", transport_->getName());

    transport_->close();
    return true;
}

bool PrinterDriver::isConnected() const {
    return transport_ && transport_->isConnected();
}

int PrinterDriver::getLogCount() {
    return logger.getLogCount();
}

void PrinterDriver::clearLog() {
    logger.clearLog();
}

const char* PrinterDriver::getLogEntry(int index) {
    return logger.getLogEntry(index);
}
