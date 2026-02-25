#pragma once
#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include "../driver/PrinterDriver.h"

enum class Status {
	IDLE,
	CHECKING,
	RUNNING,
	PAUSED,
	COMPLETED,
	ERROR
};

class Interpreter {
public:
	explicit Interpreter(PrinterDriver& driver);
	~Interpreter();

	bool executeGCode(const std::string& filename);	
	bool executeLine(const std::string& line);

	void pauseExecution();	
	void resumeExecution();
	
	Status getStatus();	
	double getProgress();	
	const char* getLastError();
	
	bool readConfig(const std::string filename);

private:
	PrinterDriver& driver_;

	std::atomic<Status> status_{ Status::IDLE };
	std::atomic<bool> stopRequested_{ false };
	std::atomic<bool> pauseRequested_{ false };
	std::atomic<double> progress_{ 0.0 };
	std::string lastError_;

	double currentX_ = 0.0;
	double currentY_ = 0.0;
	double currentZ_ = 0.0;
	bool absolutePositing_ = true;
	double feedrate_ = 0.0;

	std::unique_ptr<std::thread> executionThread_;
	mutable std::mutex mutex_;
	std::atomic<bool> threadRunning_;

	void runFile(const std::string& filename);
	void runFileInternal(const std::string& filename);
	void processLine(const std::string& line, int lineNumber, bool dryRun);
	void waitPaused();
};