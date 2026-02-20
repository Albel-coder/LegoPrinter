#include "LegoDriverAPI.h"
#include "../core/LegoPrinterCore.h"
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
		return false;
	}

	PRINTER_DRIVER_API void PrinterRotateMotor(DriverHandle printer, MotorCommand* commands, int count)	{

	}

	PRINTER_DRIVER_API void PrinterSendCommand(DriverHandle printer, const unsigned char* command, int length) {

	}

	PRINTER_DRIVER_API void PrinterSetMotorSpeed(DriverHandle printer, unsigned char port, signed char speed) {

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
		return "";
	}

	PRINTER_DRIVER_API void PrinterConnectionInfo(DriverHandle printer) {

	}

	PRINTER_DRIVER_API void PrinterSetLogCategories(DriverHandle printer, unsigned int categories) {

	}

	PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(DriverHandle printer) {
		return 0;
	}

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(DriverHandle printer, const SpeedProfile* profile) {
		return false;
	}

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(DriverHandle printer, const SpeedProfile* profiles, int count) {
		return false;
	}

	PRINTER_DRIVER_API bool PrinterIsMotorMoving(DriverHandle printer, int count) {
		return false;
	}

	PRINTER_DRIVER_API double PrinterGetMotorPosition(DriverHandle printer, unsigned char port)	{
		return 0.0;
	}

	PRINTER_DRIVER_API bool RunPrinterTest(DriverHandle printer, const char* testName) {
		return false;
	}

	PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(DriverHandle printer) {
		return false;
	}

	PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(DriverHandle printer) {
		return 0;
	}

	PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(DriverHandle printer, int maxAgeSeconds) {
		return false;
	}
}

