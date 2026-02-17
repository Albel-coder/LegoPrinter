#include "LegoPrinterCore.h"
#include <cstdarg>
#include <map>
#include <windows.h>

PrinterDriver::PrinterDriver(std::unique_ptr<ITransport> transport) : transport_(std::move(transport)) {
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

    LOG_INFO("Connecting using transport: %s", transport_->getName());

	bool result = transport_->open();
    if (result) {
        LOG_INFO("Connection successful!");
    }
    else {
        LOG_INFO("Connection failed");
    }

    return result;
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

bool PrinterDriver::isConnected() const {
    return transport_ && transport_->isConnected();
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
