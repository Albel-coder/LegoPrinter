#pragma once

#include <memory>
#include "../transport/ITransport.h"

class PrinterImplementation;

/**
 * @brief Platform factory for creating transports
 *
 * Core logic owns PrinterImplementation.
 * Factory only creates transport.
 */
class PrinterFactory {
public:
    static TransportPtr CreateTransport();
};
