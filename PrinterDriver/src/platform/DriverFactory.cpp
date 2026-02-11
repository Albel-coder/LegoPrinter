#include "DriverFactory.h"

#if defined(_WIN32) || defined(_WIN64)
	#include "../transport/TransportSimpleBLE.h"
std::unique_ptr<ITransport> createBleDriver() {
	return std::make_unique<TransportSimpleBLE>();
	}
#endif