#pragma once

#include "core/driver/interfaces/ITransport.h"
#include "protocol/BootloaderProtocol.h"
#include "protocol/PrinterProtocol.h"
#include "RuntimeSession.h"
#include "../logging/LogManager.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

enum class HubMode {
	Unknown,
	LegoOfficial,
	Bootloader,
	PybricksRuntime
};

struct HubDescriptor {
	std::string address;
	std::string name;
	int16_t rssi = 0;
	HubMode mode = HubMode::Unknown;
};

struct ProgressInfo {
	int percent = 0;
	std::string stage;
};

class PrinterFirmware {
public:
	using RuntimeCallback = std::function<void(const uint8_t* data, size_t length)>;

	explicit PrinterFirmware(std::unique_ptr<ITransport> transport);
	~PrinterFirmware();

	std::vector<HubDescriptor> scanHubs(int timeoutSeconds = 10);
	HubMode detectMode(const std::string& address);

	bool flashFirmware(const std::filesystem::path& firmwareBootloaderPath, const std::string& address);

	bool uploadProgram(const std::filesystem::path& scriptFile, const std::string& address);

	bool connectRuntime(const std::string& address);
	void disconnectRuntime();

	bool startUserProgram();
	bool stopUserProgram();
	bool sendRuntimePacket(const uint8_t* data, size_t length, bool withResponse = false);

	void setRuntimeCallback(RuntimeCallback callback);

	bool restoreOfficialFirmwareInstructions();

private:
	std::unique_ptr<ITransport> transport;
	std::unique_ptr<BootloaderProtocol> bootloader;
	std::unique_ptr<PrinterProtocol> printerProtocol;
	std::unique_ptr<RuntimeSession> runtime;

	RuntimeCallback runtimeCallback;
	std::string connectedAddress;
	HubMode connectedMode = HubMode::Unknown;
};