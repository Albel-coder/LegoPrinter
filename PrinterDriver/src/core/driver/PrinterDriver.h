#pragma once

#include "../core/driver/PrinterFirmware.h"
#include "interfaces/ITransport.h"
#include "../api/LegoDriverAPI.h"
#include "implementation/MotorManager.h"

#include <memory>
#include <atomic>

class PrinterDriver {
private:
	std::unique_ptr<ITransport> transport;
	std::unique_ptr<MotorManager> motorManager;	
	std::unique_ptr<PrinterFirmware> printerFirmware;
	
public:
	explicit PrinterDriver(std::unique_ptr<ITransport> transport);
	~PrinterDriver();

	std::vector<HubDescriptor> scanHubs(int timeoutSeconds = 10);

	bool flashFirmware(const std::filesystem::path& firmwareBootloaderPath, const std::string& address,
		PrinterFirmware::ProgressCallback progress = nullptr, PrinterFirmware::LogCallback log = nullptr);

	bool uploadProgram(const std::filesystem::path& scriptFile, const std::string& address,
		PrinterFirmware::ProgressCallback progress = nullptr, PrinterFirmware::LogCallback log = nullptr);

	bool connect(const std::string& address);
	bool disconnect();
	bool isConnected();

	bool sendCommand(const uint8_t* data, size_t length);

	void rotateMotor(const MotorCommand* commands, int count);
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

	ITransport* getTransport() { return transport.get(); }
};