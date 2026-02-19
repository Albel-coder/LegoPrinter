#include "DriverFactory.h"

std::unique_ptr<ITransport> createBleDriver() {
	return nullptr;
}