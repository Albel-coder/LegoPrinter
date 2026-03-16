#include "PrinterFirmware.h"
#include "protocol/Constants.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <thread>

static std::vector<uint8_t> readBinaryFile(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {};
	}

	return {
		std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()
	};
}

PrinterFirmware::PrinterFirmware(std::unique_ptr<ITransport> transport)
	: transport(std::move(transport)) {
	bootloader = std::make_unique<BootloaderProtocol>(*(this->transport));
	printerProtocol = std::make_unique<PrinterProtocol>(*(this->transport));
	runtime = std::make_unique<RuntimeSession>(*(this->transport));
}

PrinterFirmware::~PrinterFirmware() {
	disconnectRuntime();
}

std::vector<HubDescriptor> PrinterFirmware::scanHubs(int timeoutSeconds) {
	std::vector<HubDescriptor> result;

	transport->startScan(timeoutSeconds);
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

		result.push_back(std::move(hub));
	}

	return result;
}

HubMode PrinterFirmware::detectMode(const std::string& address) {
	if (!transport->connect(address)) {
		return HubMode::Unknown;
	}

	const auto services = transport->getServices();
	for (const auto& service : services) {
		if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
			transport->disconnect();
			return HubMode::Bootloader;
		}
		if (service == protocol::PYBRICKS_SERVICE_UUID) {
			transport->disconnect();
			return HubMode::PybricksRuntime;
		}
		if (service == protocol::LWP3_BOOTLOADER_SERVICE_UUID) {
			transport->disconnect();
			return HubMode::LegoOfficial;
		}
	}

	transport->disconnect();
	return HubMode::Unknown;
}

bool PrinterFirmware::flashFirmware(const std::filesystem::path& firmwareBootloaderPath, const std::string& address, ProgressCallback progress, LogCallback log) {
	const auto mode = detectMode(address);
	if (mode != HubMode::Bootloader && mode != HubMode::LegoOfficial) {
		if (log) {
			log("Hub is not in bootloader mode.");
		}
		return false;
	}

	const auto firmwareBootloader = readBinaryFile(firmwareBootloaderPath);
	if (firmwareBootloader.empty()) {
		if (log) {
			log("Firmware bootloader is empty or unreadable.");
		}
		return false;
	}

	if (!transport->connect(address)) {
		if (log) {
			log("Failed to connect to hub for flashing.");
		}
		return false;
	}

	if (progress) {
		progress({5, "Flashing firmware"});
	}

	const bool ok = bootloader->flashFirmware(firmwareBootloader, [progress](int percent, const std::string& stage) {
		if (progress) {
			progress({percent, stage});
		}
	}, log
	);

	transport->disconnect();
	return ok;
}

bool PrinterFirmware::uploadProgram(const std::filesystem::path& scriptFile, const std::string& address, ProgressCallback progress, LogCallback log) {
	const auto mode = detectMode(address);
	if (mode != HubMode::PybricksRuntime && mode != HubMode::LegoOfficial) {
		if (log) {
			log("Hub is not in runtime mode");
		}
		return false;
	}

	const auto script = readBinaryFile(scriptFile);
	if (script.empty()) {
		if (log) {
			log("Script file is empty or unreadable");
		}
		return false;
	}

	if (!transport->connect(address)) {
		if (log) {
			log("Failed to connect to hub");
		}
		return false;
	}

	const bool ok = printerProtocol->uploadProgram(script, {}, log);

	transport->disconnect();
	return ok;
}

bool PrinterFirmware::connectRuntime(const std::string& address) {
	if (!runtime->connect(address)) {
		connectedAddress.clear();
		connectedMode = HubMode::Unknown;
		return false;
	}

	runtime->setCallback([this](const uint8_t* data, size_t length) {
		if (runtimeCallback) {
			runtimeCallback(data, length);
		}
	});

	connectedAddress = address;
	connectedMode = HubMode::PybricksRuntime;
	return true;
}

void PrinterFirmware::disconnectRuntime() {
	if (runtime) {
		runtime->disconnect();
	}

	connectedAddress.clear();
	connectedMode = HubMode::Unknown;
}

bool PrinterFirmware::startUserProgram() {
	return printerProtocol && printerProtocol->startUserProgram();
}

bool PrinterFirmware::stopUserProgram() {
	return printerProtocol && printerProtocol->startUserProgram();
}

bool PrinterFirmware::sendRuntimePacket(const uint8_t* data, size_t length, bool withResponse) {
	if (!runtime) {
		return false;
	}

	return runtime->send(data, length, withResponse);
}

void PrinterFirmware::setRuntimeCallback(RuntimeCallback callback) {
	runtimeCallback = std::move(callback);
}

bool PrinterFirmware::restoreOfficialFirmwareInstructions(LogCallback log) {
	if (log) {
		log("Open the LEGO CONTROL+ / Powered Up application and use the official recovery flow.");
	}

	return true;
}
