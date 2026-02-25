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

	executionThread_ = std::make_unique<std::thread>([this, filename]() {
		LOG_DEBUG("Execution thread started");
		LOG_DEBUG("Thread filename: %s", filename.c_str());
		runFileInternal(filename);
		LOG_DEBUG("Execution thread finished");
		threadRunning_ = false;
		});

	LOG_DEBUG("Execution started: %f", filename);
	return true;
}

bool Interpreter::executeLine(const std::string& line) {
	LOG_DEBUG("ExecuteLine started: %s", line.c_str());

	if (status_ == Status::RUNNING) {
		LOG_ERROR("Interpreter is already running");
		return false;
	}

	if (!driver_.isConnected()) {
		LOG_ERROR("Invalid printer instance");
		return false;
	}

	if (threadRunning_) {
		LOG_ERROR("Interpreter is already executing");
		return false;
	}

	// Clean up previous thread
	if (executionThread_ && executionThread_->joinable()) {
		executionThread_->detach();;
	}

	try {
		// Try interpret single line to find errors
		status_ = Status::CHECKING;
		bool hasErrors = false;
		processLine(line, 1, true);

		if (status_ == Status::ERROR) {
			LOG_INFO("Execution aborted due to errors");
			threadRunning_ = false;
			return false;
		}

		// Second pass: execution
		status_ = Status::RUNNING;
		processLine(line, 1, false);

		if (!stopRequested_) {
			LOG_DEBUG("Execution completed successfully");
			status_ = Status::COMPLETED;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			status_ = Status::IDLE;
		}
		else {
			LOG_DEBUG("Execution stopped by user");
			status_ = Status::IDLE;
		}
	}
	catch (const std::exception& ex) {
		LOG_ERROR("Runtime error: %s", ex.what());
		status_ = Status::ERROR;
	}

	threadRunning_ = false;
	LOG_DEBUG("Line executed successfully!");
	return true;
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

void Interpreter::runFileInternal(const std::string& filename) {

}

void Interpreter::processLine(const std::string& line, int lineNumber, bool dryRun) {

}
