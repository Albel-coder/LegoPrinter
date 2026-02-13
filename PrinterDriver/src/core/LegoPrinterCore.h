#pragma once

#include "../transport/ITransport.h"
#include "../logging/LogManager.h"
#include <memory>
#include <atomic>

class PrinterDriver {
private:
	std::unique_ptr<ITransport> transport_;
	
public:
	explicit PrinterDriver(std::unique_ptr<ITransport> transport);
	~PrinterDriver();

	bool connect();
	bool disconnect();
	bool isConnected() const;
	//void rotateMotor(const MotorCommand* commands, int count);
	//void setMotorSpeed(uint8_t port, int8_t speed);

	int getLogCount();
	void clearLog();
	const char* getLogEntry(int index);

	ITransport* getTransport() { return transport_.get(); }

	LogManager logger;
};