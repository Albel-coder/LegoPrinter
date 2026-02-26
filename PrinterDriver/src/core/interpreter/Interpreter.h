#pragma once
#include <string>
#include <atomic>
#include <memory>
#include <thread>
#include <fstream>
#include "../driver/PrinterDriver.h"

struct StepperConfig {
	double rotationDistance = 0.0;
	double gearRatio = 1.0;
	bool direction = true;
	std::vector<uint8_t> ports;
	double minimumFeedrate = 0.0;
	double maximumFeedrate = 0.0;
};

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
	PrinterDriver& driver;

	std::atomic<Status> status{ Status::IDLE };
	std::atomic<bool> stopRequested{ false };
	std::atomic<bool> pauseRequested{ false };
	std::atomic<double> progress{ 0.0 };
	std::string lastError;

	double currentX = 0.0;
	double currentY = 0.0;
	double currentZ = 0.0;
	bool absolutePositioning = true;
	double speed = 0.0;

	StepperConfig stepperX;
	StepperConfig stepperY;
	StepperConfig stepperZ;

	std::unique_ptr<std::thread> executionThread;
	mutable std::mutex mutex;
	std::atomic<bool> threadRunning;

	void runFile(const std::string& filename);
	void runFileInternal(const std::string& filename);
	void processLine(const std::string& line, int linesCount, bool isTryingInterpret);
	void waitIfPaused();

	void processMovement(std::istringstream& string, int lineCount, bool isTryingInterpret);

	void processArc(std::istringstream& stream, int lineCount, bool isTryingInterpret, bool clockwise);
};