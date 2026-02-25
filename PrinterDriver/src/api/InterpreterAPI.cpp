#include "InterpreterAPI.h"
#include "../core/interpreter/Interpreter.h"
#include "../core/driver/PrinterDriver.h"

extern "C" {
	
	InterpreterHandle CreateInterpreter(DriverHandle printer) {
		if (!printer) return nullptr;
		auto* driver = static_cast<PrinterDriver*>(printer);
		
		return new Interpreter(*driver);
	}
	
	void DestroyInterpreter(InterpreterHandle handle) {
		delete static_cast<Interpreter*>(handle);
	}
	
	bool ExecuteGCode(InterpreterHandle handle, const char* filename) {
		return false;
	}
	
	bool ExecuteLine(InterpreterHandle handle, const char* line) {
		return false;
	}
	
	void PauseExecution(InterpreterHandle handle) {
		
	}
	
	void ResumeExecution(InterpreterHandle handle) {
		
	}
	
	int GetStatus(InterpreterHandle handle) {
		return 0;
	}
	
	double GetProgress(InterpreterHandle handle) {
		return 0.0;
	}
	
	const char* GetLastInterpreterError(InterpreterHandle handle) {
		return "";
	}
	
	bool ReadConfig(InterpreterHandle handle, const char* filename) {
		return false;
	}
}