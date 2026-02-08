#include "PrinterFactory.h"
#include "../core/PrinterImplementation.h"

IPrinter* PrinterFactory::Create(TransportPtr transport) {
    if (!transport) {
        return nullptr;
    }

    try {
        auto* implementation = new PrinterImplementation(std::move(transport));

        return &implementation->interface;
    }
    catch (...) {
        return nullptr;
    }
}
