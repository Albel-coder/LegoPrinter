#include "DriverFactory.h"
#include "../logging/LogManager.h"

#if defined(_WIN32) || defined(_WIN64)
	#include "../transport/TransportSimpleBLE.h"
std::unique_ptr<ITransport> createBleDriver() {
	LogManager logger;
	return std::make_unique<TransportSimpleBLE>(logger);
	}
#endif