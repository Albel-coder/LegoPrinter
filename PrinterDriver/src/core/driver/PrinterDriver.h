#pragma once

#include "interfaces/ITransport.h"
#include "../api/LegoDriverAPI.h"
#include "implementation/MotorManager.h"
#include <memory>
#include <atomic>

class PrinterDriver {
private:
	std::unique_ptr<ITransport> transport_;

	std::unique_ptr<MotorManager> motorManager_;
	
public:
	explicit PrinterDriver(std::unique_ptr<ITransport> transport);
	~PrinterDriver();

	bool connect();
	bool disconnect();
	bool isConnected();
	void rotateMotor(const MotorCommand* commands, int count);
	void sendCommand(const unsigned char* command, int length);
	void setMotorSpeed(uint8_t port, int8_t speed);

	int getLogCount();
	void clearLog();
	const char* getLogEntry(int index);
	
	const char* getLastErrorMessage();
	void printerConnectionInfo();
	
	void printerSetLogCategories(unsigned int categories);
	unsigned int printerGetLogCategories();	
	
	bool printerExecuteSpeedProfile(const SpeedProfile* profile);
	bool printerExecuteSpeedProfiles(const SpeedProfile* profiles, int count);
	
	bool printerIsMotorMoving(int count);
	double printerGetMotorPosition(unsigned char port);
	
	bool runPrinterTest(const char* testName);
	
	bool printerRequestBatteryLevel();
    unsigned char printerGetBatteryLevel();
    bool printerIsBatteryLevelFresh(int maxAgeSeconds);

	ITransport* getTransport() { return transport_.get(); }
};