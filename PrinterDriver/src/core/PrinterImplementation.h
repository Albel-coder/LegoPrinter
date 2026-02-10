#pragma once

#include "IPrinterInternal.h"
#include "../transport/ITransport.h"
#include <memory>
#include <atomic>

class PrinterImplementation {
private:
	std::unique_ptr<ITransport> transport_;

public:
	explicit PrinterImplementation(std::unique_ptr<ITransport> transport);
	~PrinterImplementation();

	bool connect();
	bool disconnect();
	bool isConnected() const;
	//void rotateMotor(const MotorCommand* commands, int count);
	//void setMotorSpeed(uint8_t port, int8_t speed);

	int getLogCount();
	void clearLog();
	const char* getLogEntry(int index);


	ITransport* getTransport() { return transport_.get(); }
};