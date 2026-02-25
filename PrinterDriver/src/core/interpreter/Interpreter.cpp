#include "Interpreter.h"

Interpreter::Interpreter(PrinterDriver& driver) : driver_(driver) {
}

Interpreter::~Interpreter() = default;

bool Interpreter::executeGCode(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex_);
	if (status_ != Status::IDLE) {
		LOG_WARNING("Interpreter is busy");
		return false;
	}
	if (!driver_.isConnected()) {
		LOG_ERROR("Printer is invalid or not connected");
		return false;
	}
	
	status_ = Status::RUNNING;
	stopRequested_ = false;
	pauseRequested_ = false;
	progress_ = 0.0;
	currentX_ = currentY_ = currentZ_ = 0.0;
	absolutePositing_ = true;
	feedrate_ = 0.0;

	workerThread_ = std::make_unique<std::thread>(&Interpreter::runFileInternal, this, filename);
}

bool Interpreter::executeLine(const std::string& line)
{
	return false;
}

void Interpreter::pauseExecution()
{
}

void Interpreter::resumeExecution()
{
}

Status Interpreter::getStatus()
{
	return Status();
}

double Interpreter::getProgress()
{
	return 0.0;
}

const char* Interpreter::getLastError()
{
	return nullptr;
}

bool Interpreter::readConfig(const std::string filename)
{
	return false;
}
