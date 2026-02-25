#include "LegoDriverAPI.h"
#include "../core/driver/PrinterDriver.h"
#include "../transport/TransportSimpleBLE.h"

extern "C" {
	PRINTER_DRIVER_API DriverHandle CreatePrinter()
	{
		auto transport = std::make_unique<TransportSimpleBLE>();
		auto* driver = new PrinterDriver(std::move(transport));
		return driver;
	}

	PRINTER_DRIVER_API void DestroyPrinter(DriverHandle printer) {
		delete static_cast<PrinterDriver*>(printer);
	}

	PRINTER_DRIVER_API bool PrinterConnect(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->connect();
	}

	PRINTER_DRIVER_API bool PrinterDisconnect(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->disconnect();
	}

	PRINTER_DRIVER_API bool IsConnected(DriverHandle printer) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		return driver->isConnected();
	}

	PRINTER_DRIVER_API void PrinterRotateMotor(DriverHandle printer, MotorCommand* commands, int count)	{
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->rotateMotor(commands, count);
	}

	PRINTER_DRIVER_API void PrinterSendCommand(DriverHandle printer, const unsigned char* command, int length) {
		auto* driver = static_cast<PrinterDriver*>(printer);
		driver->sendCommand(command, length);
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
