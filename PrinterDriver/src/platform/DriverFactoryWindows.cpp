#include "DriverFactory.h"
#include "../transport/TransportSimpleBLE.h"

std::unique_ptr<ITransport> createBleDriver() {
	return std::make_unique<TransportSimpleBLE>();
}