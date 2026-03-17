#include "PrinterFirmware.h"
#include "protocol/Constants.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <thread>
#include <chrono>

static std::vector<uint8_t> readBinaryFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {};
	}

	return {
		std::istreambuf_iterator<char>(file), 
		std::istreambuf_iterator<char>()
	};
}

PrinterFirmware::PrinterFirmware(std::unique_ptr<ITransport> transport)
	: transport(std::move(transport)) {
	bootloader = std::make_unique<BootloaderProtocol>(*(this->transport));
	printerProtocol = std::make_unique<PrinterProtocol>(*(this->transport));
	runtime = std::make_unique<RuntimeSession>(*(this->transport));

	LOG_INFO("PrinterFirmware created");
}

PrinterFirmware::~PrinterFirmware() {
	LOG_INFO("PrinterFirmware destroyed");
	disconnectRuntime();
}

std::vector<HubDescriptor> PrinterFirmware::scanHubs(int timeoutSeconds) {
	LOG_INFO("Scanning hubs (%d seconds)...", timeoutSeconds);
	
	std::vector<HubDescriptor> result;

	if (!transport->startScan(timeoutSeconds)) {
		LOG_ERROR("scanHubs: startScan failed");
		return result;
	}

	std::this_thread::sleep_for(std::chrono::seconds(timeoutSeconds + 1));
	transport->stopScan();

	const auto devices = transport->getScanResults();
	result.reserve(devices.size());

	for (const auto& device : devices) {
		HubDescriptor hub;
		hub.address = device.address;
		hub.name = device.name;
		hub.rssi = device.rssi;

		for (const auto& uuid : device.serviceUuids) {
			if (uuid == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
				hub.mode = HubMode::Bootloader;
				break;
			}
			if (uuid == protocol::PYBRICKS_SERVICE_UUID) {
				hub.mode = HubMode::PybricksRuntime;
				break;
			}
			if (uuid == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
				hub.mode = HubMode::LegoOfficial;
				break;
			}
		}

		if (hub.mode == HubMode::Unknown) {
			std::string upper = hub.name;
			std::transform(upper.begin(), upper.end(), upper.begin(),[](unsigned char character) {
				return static_cast<char>(std::toupper(character));
			});

			if (upper.find("BOOTLOADER") != std::string::npos) {
				hub.mode = HubMode::Bootloader;
			}
			else if (upper.find("PYBRICKS") != std::string::npos) {
				hub.mode = HubMode::PybricksRuntime;
			}
			else if (upper.find("LEGO") != std::string::npos ||
				upper.find("HUB") != std::string::npos ||
				upper.find("CONTROL") != std::string::npos) {
				hub.mode = HubMode::LegoOfficial;
			}
		}

		LOG_BLUETOOTH("Found hub: %s [%s] rssi=%d mode=%d",
			hub.name.c_str(), hub.address.c_str(), hub.rssi, static_cast<int>(hub.mode));

		result.push_back(std::move(hub));
	}

	LOG_INFO("Scan complete. Found %zu hubs", result.size());
	return result;
}

HubMode PrinterFirmware::detectMode(const std::string& address) {
	LOG_INFO("Detecting hub mode: %s", address.c_str());
	
	if (!transport->connect(address)) {
		LOG_WARNING("detectMode: connect failed");
		return HubMode::Unknown;
	}

	HubMode mode = HubMode::Unknown;
	const auto services = transport->getServices();

	for (const auto& service : services) {
		if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
			transport->disconnect();
			mode = HubMode::Bootloader;
			break;
		}
		if (service == protocol::PYBRICKS_SERVICE_UUID) {
			transport->disconnect();
			mode = HubMode::PybricksRuntime;
			break;
		}
		if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
			transport->disconnect();
			mode = HubMode::LegoOfficial;
			break;
		}
	}

	transport->disconnect();

	LOG_INFO("detectMode result: %d", static_cast<int>(mode));
	return mode;
}

bool PrinterFirmware::flashFirmware(const std::filesystem::path& firmwarePath, const std::string& address) {
	LOG_INFO("Flashing firmware: %s -> %s", firmwarePath.string().c_str(), address.c_str());

	const auto mode = detectMode(address);

	if (mode != HubMode::Bootloader && mode != HubMode::LegoOfficial) {
		LOG_ERROR("Hub not in bootloader or official mode");
		return false;
	}

	const auto firmware = readBinaryFile(firmwarePath);

	if (firmware.empty()) {
		LOG_ERROR("Firmware file is empty: %s", firmwarePath.string().c_str());
		return false;
	}

	if (!transport->connect(address)) {
		LOG_ERROR("Failed to connect for flashing");
		return false;
	}

	LOG_INFO("Starting firmware flash (%zu bytes)", firmware.size());

	const bool result = bootloader->flashFirmware(firmware);

	transport->disconnect();

	if (result) {
		LOG_INFO("Firmware flash complete");
	}
	else {
		LOG_ERROR("Firmware flash failed");
	}

	return result;
}

bool PrinterFirmware::uploadProgram(const std::filesystem::path& scriptFile, const std::string& address) {
	LOG_INFO("uploadProgram: %s -> %s", scriptFile.string().c_str(), address.c_str());
	
	const auto mode = detectMode(address);
	if (mode != HubMode::PybricksRuntime && mode != HubMode::LegoOfficial) {
		LOG_ERROR("Hub is not in runtime mode");
		return false;
	}

	const auto script = readBinaryFile(scriptFile);
	if (script.empty()) {
		LOG_ERROR("Script file is empty or unreadable");
		return false;
	}

	if (!transport->connect(address)) {
		LOG_ERROR("Failed to connect to hub");
		return false;
	}

	LOG_INFO("Starting program upload (%zu bytes)", script.size());

	const bool result = printerProtocol->uploadProgram(script);

	transport->disconnect();

	if (result) {
		LOG_INFO("Program upload complete");
	}
	else {
		LOG_ERROR("Program upload failed");
	}

	return result;
}

bool PrinterFirmware::connectRuntime(const std::string& address) {
	if (!runtime->connect(address)) {
		connectedAddress.clear();
		connectedMode = HubMode::Unknown;
		LOG_ERROR("Runtime connect failed");
		return false;
	}

	runtime->setCallback([this](const uint8_t* data, size_t length) {
		LOG_COMMAND("Runtime RX: %zu bytes", length);
		if (runtimeCallback) {
			runtimeCallback(data, length);
		}
	});

	connectedAddress = address;
	connectedMode = HubMode::PybricksRuntime;

	LOG_INFO("Runtime connected");
	return true;
}

void PrinterFirmware::disconnectRuntime() {
	if (runtime) {
		runtime->disconnect();
	}

	connectedAddress.clear();
	connectedMode = HubMode::Unknown;

	LOG_INFO("Runtime disconnect");
}

bool PrinterFirmware::startUserProgram() {
	LOG_COMMAND("startUserProgram()");
	return printerProtocol && printerProtocol->startUserProgram();
}

bool PrinterFirmware::stopUserProgram() {
	LOG_COMMAND("stopUserProgram");
	return printerProtocol && printerProtocol->startUserProgram();
}

bool PrinterFirmware::sendRuntimePacket(const uint8_t* data, size_t length, bool withResponse) {
	if (!runtime) {
		LOG_ERROR("sendRuntimePacket: runtime is null");
		return false;
	}

	LOG_COMMAND("sendRuntimePacket: %zu bytes", length);
	return runtime->send(data, length, withResponse);
}

void PrinterFirmware::setRuntimeCallback(RuntimeCallback callback) {
	runtimeCallback = std::move(callback);
}

bool PrinterFirmware::restoreOfficialFirmwareInstructions() {
	LOG_WARNING("Open the LEGO CONTROL+ / Powered Up application and use the official recovery flow");
	return true;
}
