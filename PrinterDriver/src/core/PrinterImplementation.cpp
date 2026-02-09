#include "PrinterImplementation.h"
#include "../logging/Logger.h"
#include <cstdarg>
#include <map>
#include <windows.h>

PrinterImplementation::PrinterImplementation(TransportPtr transport) : transport_(std::move(transport)) {
#ifdef _WIN32
    OutputDebugStringA("CreatePrinterENTER\n");
#endif
}

PrinterImplementation::~PrinterImplementation() {
    LOG_INFO("PrinterImplementation destroyed");
}

bool PrinterImplementation::connect() {
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

bool PrinterImplementation::disconnect() {
    if (!transport_) {
        LOG_ERROR("Transport not initialized");
        return false;
    }

    LOG_INFO("Disconnecting using transport: %s", transport_->getName());

    transport_->close();
    return true;
}

bool PrinterImplementation::isConnected() const {
    return transport_ && transport_->isConnected();
}

int PrinterImplementation::getLogCount() {
	return Logger::instance().getLogCount();
}

void PrinterImplementation::clearLog() {
	Logger::instance().clearLog();
}

const char* PrinterImplementation::getLogEntry(int index) {
	return Logger::instance().getLogEntry(index);
}
