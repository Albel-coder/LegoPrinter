#pragma once

#include "interfaces/ITransport.h"
#include "../logging/LogManager.h"
#include "../core/driver/protocol/BootloaderProtocol.h"
#include "../core/driver/protocol/PrinterProtocol.h"
#include "../core/driver/RuntimeSession.h"
#include "../api/LegoDriverAPI.h"
#include "implementation/MotorManager.h"

#include <filesystem>
#include <memory>
#include <atomic>
#include <string>
#include <vector>

enum class HubMode {
	Unknown = 0,
	LegoOfficial = 1,
	Bootloader = 2,
	PybricksRuntime = 3,
};

class PrinterDriver {
private:
	std::unique_ptr<ITransport> transport;
	std::unique_ptr<MotorManager> motorManager;	
	std::unique_ptr<BootloaderProtocol> bootloaderProtocol;
	std::unique_ptr<RuntimeSession> runtime;
	std::unique_ptr<PrinterProtocol> printerProtocol;

	std::vector<DeviceInfo> scanResults;
	std::vector<DeviceInfo> recentHubs;

	std::string connectedAddress;
	std::string lastKnownAddress;
	
public:
	explicit PrinterDriver(std::unique_ptr<ITransport> transportPointer);
	~PrinterDriver();

	// scanning / connection
	std::vector<DeviceInfo> scan(int timeoutSeconds = 2, bool legoOnly = true);
	bool connectAuto(int timeoutMs = 5000, bool legoOnly = true);
	bool connect(const std::string& address);
	bool reconnectLast();
	bool disconnect();
	bool isConnected();
	std::string getConnectedAddress() const;

	std::vector<DeviceInfo> getRecentHubs() const;
	std::vector<DeviceInfo> getLastScanResults() const;

	HubMode detectHubMode(const std::string& address);
	bool probeRuntime(const std::string& address, int timeoutMs = 1500);

	// firmware / program
	bool flashFirmware(const std::string& firmwareBootloaderPath, const std::string& address = "");
	bool uploadProgram(const std::string& scriptFile, const std::string& address = "");

	bool startUserProgram();
	bool stopUserProgram();

	bool runtimeRotateMotor(uint8_t port, int32_t speed, int32_t angle, bool hold);
	bool runtimePing();

	void testFunction() const {
		LOG_INFO("testFunction called on %p", this);
	}

	// runtime / raw commands
	void disconnectRuntime();
	bool connectRuntime(const std::string& address);
	bool sendRuntime(const uint8_t* data, size_t length);
	bool sendMotorCommands(const MotorCommand* commands, int count);

	void rotateMotor(const MotorCommand* commands, int count);
	void setMotorSpeed(uint8_t port, int8_t speed);

	// logging
	int getLogCount();
	void clearLog();
	const char* getLogEntry(int index);
	void setLogCategories(uint32_t mask) noexcept;
	uint32_t getLogCategories() const noexcept;
	
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

private:
	static bool isLegoHub(const DeviceInfo& device);

	std::vector<DeviceInfo> filterAndSortHubs(const std::vector<DeviceInfo>& input, bool legoOnly) const;
	void rememberHub(const DeviceInfo& device);
	std::string resolveAddress(const std::string& address) const;
};