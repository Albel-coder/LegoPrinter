#include "LegoDriverAPI.h"
#include "../core/driver/PrinterDriver.h"
#include "../transport/TransportSimpleBLE.h"

static void copyString(char* destination, int capacity, const std::string& string) {
	if (!destination || capacity <= 0) {
		return;
	}

	std::strncpy(destination, string.c_str(), static_cast<size_t>(capacity) - 1);
	destination[capacity - 1] = '\0';
}

static PrinterDeviceInfoC toCDevice(const DeviceInfo& device) {
	PrinterDeviceInfoC result{};
	copyString(result.address, static_cast<int>(sizeof(result.address)), device.address);
	copyString(result.name, static_cast<int>(sizeof(result.name)), device.name);
	result.rssi = device.rssi;
	result.isLegoHub = (device.manufacturerData.find(0x0397) != device.manufacturerData.end()) ? 1 : 0;
	
	return result;
}

extern "C" {

	PRINTER_DRIVER_API DriverHandle CreatePrinter()	{
		auto transport = std::make_unique<TransportSimpleBLE>();
		auto* driver = new PrinterDriver(std::move(transport));
		return driver;
	}

	PRINTER_DRIVER_API void DestroyPrinter(DriverHandle printer) {
		delete static_cast<PrinterDriver*>(printer);
	}

	PRINTER_DRIVER_API int PrinterScan(DriverHandle printer, int timeoutSeconds, int legoOnly, PrinterDeviceInfoC* outDevices, int maxDevices) {
		if (!printer || !outDevices || maxDevices <= 0) {
			return 0;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		const auto devices = driver->scan(timeoutSeconds, legoOnly != 0);

		const int count = std::min(static_cast<int>(devices.size()), maxDevices);
		for (int i = 0; i < count; ++i) {
			outDevices[i] = toCDevice(devices[i]);
		}

		return count;
	}

	PRINTER_DRIVER_API bool PrinterConnectAuto(DriverHandle printer, int timeoutMs, bool legoOnly)	{
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->connectAuto(timeoutMs, legoOnly);
	}

	PRINTER_DRIVER_API bool PrinterConnect(DriverHandle printer, const char* address) {
		if (!printer || !address) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->connect(address);
	}

	PRINTER_DRIVER_API bool PrinterReconnectLast(DriverHandle printer) {
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->reconnectLast();
	}

	PRINTER_DRIVER_API bool PrinterDisconnect(DriverHandle printer) {
		if (!printer) {
			return false;
		}
		
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->disconnect();
	}

	PRINTER_DRIVER_API bool IsConnected(DriverHandle printer) {
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->isConnected();
	}

	PRINTER_DRIVER_API int PrinterGetConnectedAddress(DriverHandle printer, char* outAddress, int capacity)	{
		if (!printer || !outAddress || capacity <= 0) {
			return 0;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		copyString(outAddress, capacity, driver->getConnectedAddress());
		return 1;
	}

	PRINTER_DRIVER_API int PrinterGetRecentHubCount(DriverHandle printer) {
		if (!printer) {
			return 0;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return static_cast<int>(driver->getRecentHubs().size());
	}

	PRINTER_DRIVER_API int PrinterGetRecentHub(DriverHandle printer, int index, PrinterDeviceInfoC* outHub) {
		if (!printer || !outHub || index <= 0) {
			return 0;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		const auto hubs = driver->getRecentHubs();
		if (index >= static_cast<int>(hubs.size())) {
			return 0;
		}
		*outHub = toCDevice(hubs[index]);
		return 1;
	}

	PRINTER_DRIVER_API int PrinterDetectHubMode(DriverHandle printer, const char* address) {
		if (!printer || !address) {
			return 0;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return static_cast<int>(driver->detectHubMode(address));
	}

	PRINTER_DRIVER_API bool PrinterProbeRuntime(DriverHandle printer, const char* address, int timeoutMs) {
		if (!printer || !address) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->probeRuntime(address, timeoutMs);
	}

	PRINTER_DRIVER_API bool PrinterFlashFirmware(DriverHandle printer, const char* firmwareBootloaderPath, const char* address) {
		if (!printer || !firmwareBootloaderPath || !address) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->flashFirmware(firmwareBootloaderPath, address);
		return true;
	}

	PRINTER_DRIVER_API bool PrinterUploadProgram(DriverHandle printer, const char* scriptPath, const char* address) {
		if (!printer || !scriptPath || !address) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->uploadProgram(scriptPath, address);
	}

	PRINTER_DRIVER_API bool PrinterStartUserProgram(DriverHandle printer) {
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->startUserProgram();
	}

	PRINTER_DRIVER_API bool PrinterStopUserProgram(DriverHandle printer) {
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->stopUserProgram();
	}

	PRINTER_DRIVER_API bool PrinterConnectRuntime(DriverHandle printer, const char* address) {
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->connectRuntime(address ? address : "");
	}

	PRINTER_DRIVER_API bool PrinterDisconnectRuntime(DriverHandle printer) {
		if (!printer) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->disconnectRuntime();
		return true;
	}

	PRINTER_DRIVER_API bool PrinterSendRuntime(DriverHandle printer, const unsigned char* data, int length) {
		if (!printer || !data || length <= 0) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->sendRuntime(data, static_cast<size_t>(length));
	}

	PRINTER_DRIVER_API bool PrinterSendMotorCommands(DriverHandle printer, const MotorCommand* commands, int count) {
		if (!printer || !commands || count <= 0) {
			return false;
		}

		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->sendMotorCommands(reinterpret_cast<const MotorCommand*>(commands), count);
	}

	PRINTER_DRIVER_API void PrinterRotateMotor(DriverHandle printer, MotorCommand* commands, int count)	{
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->rotateMotor(commands, count);
	}

	PRINTER_DRIVER_API void PrinterSetMotorSpeed(DriverHandle printer, unsigned char port, signed char speed) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->setMotorSpeed(port, speed);
	}

	PRINTER_DRIVER_API int GetLogCount(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->getLogCount();
	}

	PRINTER_DRIVER_API const char* GetLogEntry(DriverHandle printer, int index) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->getLogEntry(index);
	}

	PRINTER_DRIVER_API void ClearLog(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->clearLog();
	}

	PRINTER_DRIVER_API const char* GetLastErrorMessage(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->getLastErrorMessage();
	}

	PRINTER_DRIVER_API void PrinterConnectionInfo(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->printerConnectionInfo();
	}

	PRINTER_DRIVER_API void PrinterSetLogCategories(DriverHandle printer, unsigned int categories) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->printerSetLogCategories(categories);
	}

	PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerGetLogCategories();
	}

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(DriverHandle printer, const SpeedProfile* profile) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerExecuteSpeedProfile(profile);
	}

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(DriverHandle printer, const SpeedProfile* profiles, int count) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerExecuteSpeedProfiles(profiles, count);
	}

	PRINTER_DRIVER_API bool PrinterIsMotorMoving(DriverHandle printer, int count) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerIsMotorMoving(count);
	}

	PRINTER_DRIVER_API double PrinterGetMotorPosition(DriverHandle printer, unsigned char port)	{
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerGetMotorPosition(port);
	}

	PRINTER_DRIVER_API bool RunPrinterTest(DriverHandle printer, const char* testName) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->runPrinterTest(testName);
	}

	PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerRequestBatteryLevel();
	}

	PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerGetBatteryLevel();
	}

	PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(DriverHandle printer, int maxAgeSeconds) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->printerIsBatteryLevelFresh(maxAgeSeconds);
	}
}
