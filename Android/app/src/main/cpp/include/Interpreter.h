#include "LegoPrinterCore.h"

class Interpreter {
private:
	PrinterDriver& driver_;

public:
	explicit Interpreter(PrinterDriver& driver) : driver_(driver) {

	}
	
	void destroyInterpreter() {
		
	}

    bool executeGCode(const char* filename) {
		return false;
	}
	
    bool executeLine(const char* line) {
		return false;
	}

    void pauseExecution() {
		
	}
	
    void resumeExecution() {
		
	}
	
    int getStatus() {
		return 0;
	}
	
    double getProgress() {
		return 0.0;
	}
	
    const char* getLastInterpreterError() {
		return "";
	}
	
    bool readConfig(const char* filename) {
		return false;
	}
};