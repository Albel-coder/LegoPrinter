#include "LegoPrinterCore.h"

class Interpreter {
private:
	PrinterDriver& driver_;

public:
	explicit Interpreter(PrinterDriver& driver) : driver_(driver) {

	}
};