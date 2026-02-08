#include "PrinterFactory.h"
#include "../transport/TransportSimpleBLE.h"

TransportPtr PrinterFactory::CreateTransport() {
#ifdef _WIN32
	return std::make_unique<TransportSimpleBLE>();
#else
	return nullptr;
#endif
}
