#include <memory>
#include "../transport/ITransport.h"
#include "../core/PrinterImplementation.h"
#include "../core/IPrinter.h"

/**
* @brief Factory for creating an IPrinter from a pre-existing transport
*
* Core doesn't know HOW the transport works.
* It only accepts it.
*/
class PrinterFactory {
public:
    /**
    * @brief Create a printer with the given transport
    *
    * @param transport The prepared transport (BLE, mock, replay, etc.)
    * @return IPrinter C interface
    */
    static IPrinter* Create(TransportPtr transport);
};
