#include "LegoPrinterCore.h"
#include <cstdarg>
#include <map>
#include <windows.h>

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transport) : transport_(std::move(transport)) {
    logger.addLogInternal(LogCategory::LOG_CATEGORY_INFO, "PrinterImplementation created");
}

PrinterDriver::~PrinterDriver() {
    logger.addLogInternal(LogCategory::LOG_CATEGORY_INFO, "PrinterImplementation destroyed");
}

bool PrinterDriver::connect() {
	if (!transport_) {
        logger.addLogInternal(LogCategory::LOG_CATEGORY_ERROR, "Transport not initialized");
		return false;
	}

    logger.addLogInternal(LogCategory::LOG_CATEGORY_INFO, "Connecting using transport: %s", transport_->getName());

	bool result = transport_->open();
    if (result) {
        logger.addLogInternal(LogCategory::LOG_CATEGORY_INFO, "Connection successful!");
    }
    else {
        logger.addLogInternal(LogCategory::LOG_CATEGORY_INFO, "Connection failed");
    }

    return result;
}

bool PrinterDriver::disconnect() {
    if (!transport_) {
        logger.addLogInternal(LogCategory::LOG_CATEGORY_ERROR, "Transport not initialized");
        return false;
    }

    logger.addLogInternal(LogCategory::LOG_CATEGORY_INFO, "Disconnecting using transport: %s", transport_->getName());

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
