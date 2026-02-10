#include "LegoDriverAPI.h"
#include "IPrinterInternal.h"
#include "PrinterImplementation.h"
#include "PrinterFactory.h"
#include <new>

IPrinter::IPrinter(PrinterImplementation* printerImplementation, const IPrinterVTable* virtualTable)
	: implementation(printerImplementation), vtable(virtualTable)
{
}

IPrinter::~IPrinter() {
	implementation = nullptr;
	vtable = nullptr;
}

IPrinter::IPrinter(IPrinter&& other) noexcept 
	: implementation(other.implementation), vtable(other.vtable) {
	other.implementation = nullptr;
	other.vtable = nullptr;
}

IPrinter& IPrinter::operator=(IPrinter&& other) noexcept {
	if (this != &other) {
		implementation = other.implementation;
		vtable = other.vtable;

		other.implementation = nullptr;
		other.vtable = nullptr;
	}

	return *this;
}

static bool printer_connect(IPrinter* printer) {
	auto* internal = static_cast<IPrinter*>(printer);
	return internal && internal->implementation && internal->implementation->connect();
}

static bool printer_disconnect(IPrinter* printer) {
	auto* internal = static_cast<IPrinter*>(printer);
	return internal && internal->implementation && internal->implementation->disconnect();
}

static const IPrinterVTable PRINTER_VTABLE = {
	printer_connect,
	printer_disconnect,

};

extern "C" {
	PRINTER_DRIVER_API IPrinter* CreatePrinter() {
		try {
			auto transport = PrinterFactory::CreateTransport();
			if (!transport) return nullptr;

			auto implementation = new PrinterImplementation(std::move(transport));

			return new IPrinter(implementation, &PRINTER_VTABLE);
		}
		catch (...) {
			return nullptr;
		}
	}

	PRINTER_DRIVER_API void DestroyPrinter(IPrinter* printer) {
		if (!printer) return;

		auto* internal = static_cast<IPrinter*>(printer);

		if (internal->implementation) {
			delete internal->implementation;
			internal->implementation = nullptr;
		}

		delete internal;
	}

	PRINTER_DRIVER_API bool PrinterConnect(IPrinter* printer) {
		if (!printer) return false;
		auto* internal = static_cast<IPrinter*>(printer);
		return internal->implementation && internal->implementation->connect();
	}

	PRINTER_DRIVER_API bool PrinterDisconnect(IPrinter* printer) {
		return false;
	}

	PRINTER_DRIVER_API bool IsConnected(IPrinter* printer) {
		return false;
	}

	PRINTER_DRIVER_API void PrinterRotateMotor(IPrinter* printer, MotorCommand* commands, int count) {
		
	}

	PRINTER_DRIVER_API void PrinterSendCommand(IPrinter* printer, const unsigned char* command, int length)	{

	}

	PRINTER_DRIVER_API void PrinterSetMotorSpeed(IPrinter* printer, unsigned char port, signed char speed) {

	}

	PRINTER_DRIVER_API int GetLogCount(IPrinter* printer) {
		return 0;
	}

	PRINTER_DRIVER_API const char* GetLogEntry(IPrinter* printer, int index) {
		return "";
	}

	PRINTER_DRIVER_API void ClearLog(IPrinter* printer) {

	}

	PRINTER_DRIVER_API const char* GetLastErrorMessage(IPrinter* printer)	{
		return "";
	}

	PRINTER_DRIVER_API void PrinterConnectionInfo(IPrinter* printer) {

	}

	PRINTER_DRIVER_API void PrinterSetLogCategories(IPrinter* printer, unsigned int categories)	{

	}

	PRINTER_DRIVER_API unsigned int PrinterGetLogCategories(IPrinter* printer) {
		return 0;
	}

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfile(IPrinter* printer, const SpeedProfile* profile) {
		return false;
	}

	PRINTER_DRIVER_API bool PrinterExecuteSpeedProfiles(IPrinter* printer, const SpeedProfile* profiles, int count)	{
		return false;
	}

	PRINTER_DRIVER_API bool PrinterIsMotorMoving(IPrinter* printer, int count) {
		return false;
	}

	PRINTER_DRIVER_API double PrinterGetMotorPosition(IPrinter* printer, unsigned char port) {
		return 0.0;
	}

	PRINTER_DRIVER_API bool RunPrinterTest(IPrinter* printer, const char* testName)	{
		return false;
	}

	PRINTER_DRIVER_API bool PrinterRequestBatteryLevel(IPrinter* printer) {
		return false;
	}

	PRINTER_DRIVER_API unsigned char PrinterGetBatteryLevel(IPrinter* printer) {
		return 0;
	}

	PRINTER_DRIVER_API bool PrinterIsBatteryLevelFresh(IPrinter* printer, int maxAgeSeconds) {
		return false;
	}


}