#include "Interpreter.h"

Interpreter::Interpreter(PrinterDriver& driver) : driver(driver) {
	stepperX.rotationDistance = 43.982297150;
	stepperX.gearRatio = 24;
	stepperX.direction = true;
	stepperX.ports = { 0x01 };
	stepperX.minimumFeedrate = 55;
	stepperX.maximumFeedrate = 90;

	stepperY.rotationDistance = 43.982297150;
	stepperY.gearRatio = 24;
	stepperY.direction = true;
	stepperY.ports = { 0x02 };
	stepperY.minimumFeedrate = 55;
	stepperY.maximumFeedrate = 90;

	stepperZ.rotationDistance = 43.982297150;
	stepperZ.gearRatio = 1;
	stepperZ.direction = true;
	stepperZ.ports = { 0x03 };
	stepperZ.minimumFeedrate = 55;
	stepperZ.maximumFeedrate = 90;

	absolutePositioning = false;

	LOG_INFO("Interpreter created");
}

Interpreter::~Interpreter() = default;

bool Interpreter::executeGCode(const std::string& filename) {
	std::lock_guard<std::mutex> lock(mutex);
	if (status != Status::IDLE) {
		LOG_WARNING("Interpreter is busy");
		return false;
	}
	if (!driver.getTransport()) {
		LOG_ERROR("Printer is invalid");
		return false;
	}
	
	status = Status::RUNNING;
	stopRequested = false;
	pauseRequested = false;
	progress = 0.0;
	currentX = currentY = currentZ = 0.0;
	absolutePositioning = true;
	speed = 0.0;

	executionThread = std::make_unique<std::thread>([this, filename]() {
		LOG_DEBUG("Execution thread started");
		LOG_DEBUG("Thread filename: %s", filename.c_str());
		runFile(filename);
		LOG_DEBUG("Execution thread finished");
		threadRunning = false;
		});

	LOG_DEBUG("Execution started: %f", filename);
	return true;
}

bool Interpreter::executeLine(const std::string& line) {
	LOG_DEBUG("ExecuteLine started: %s", line.c_str());

	if (status == Status::RUNNING) {
		LOG_ERROR("Interpreter is already running");
		return false;
	}

	if (!driver.getTransport()) {
		LOG_ERROR("Invalid printer instance");
		return false;
	}

	if (threadRunning) {
		LOG_ERROR("Interpreter is already executing");
		return false;
	}

	// Clean up previous thread
	if (executionThread && executionThread->joinable()) {
		executionThread->detach();;
	}

	try {
		// Try interpret single line to find errors
		status = Status::CHECKING;
		bool hasErrors = false;
		processLine(line, 1, true);

		if (status == Status::ERROR) {
			LOG_INFO("Execution aborted due to errors");
			threadRunning = false;
			return false;
		}

		// Second pass: execution
		status = Status::RUNNING;
		processLine(line, 1, false);

		if (!stopRequested) {
			LOG_DEBUG("Execution completed successfully");
			status = Status::COMPLETED;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			status = Status::IDLE;
		}
		else {
			LOG_DEBUG("Execution stopped by user");
			status = Status::IDLE;
		}
	}
	catch (const std::exception& ex) {
		LOG_ERROR("Runtime error: %s", ex.what());
		status = Status::ERROR;
	}

	threadRunning = false;
	LOG_DEBUG("Line executed successfully!");
	return true;
}

void Interpreter::pauseExecution() {

}

void Interpreter::resumeExecution() {

}

Status Interpreter::getStatus() {
	return Status();
}

double Interpreter::getProgress() {
	return 0.0;
}

const char* Interpreter::getLastError() {
	return nullptr;
}

bool Interpreter::readConfig(const std::string filename) {
	return false;
}

void Interpreter::runFile(const std::string& filename) {
	try {
		runFileInternal(filename);
	}
	catch (const std::exception& ex) {
		LOG_ERROR("CRITICAL ERROR in RunFile: %s", ex.what());
		status = Status::ERROR;
		threadRunning = false;
	}
	catch (...) {
		LOG_ERROR("CRITICAL ERROR: Unknown exception in RunFile");
		status = Status::ERROR;
		threadRunning = false;
	}
}

void Interpreter::runFileInternal(const std::string& filename) {
	LOG_INFO("run file started: %s", filename.c_str());

	if (!driver.getTransport()) {
		LOG_ERROR("Printer is not available for execution");
		status = Status::ERROR;
		return;
	}

	try {
		// First pass: syntax checking
		status = Status::CHECKING;
		std::ifstream file(filename);
		if (!file.is_open()) {
			LOG_ERROR("Cannot open file: %s", filename.c_str());
			lastError = "Cannot open file: " + filename;
			status = Status::ERROR;
			threadRunning = false;
			return;
		}

		std::string line = "";
		size_t linesCount = 0;
		bool hasErrors = false;

		// Try interpret lines to find errors
		while (std::getline(file, line)) {
			waitIfPaused();
			if (stopRequested) break;

			while (pauseRequested && !stopRequested) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

			if (stopRequested) break;

			processLine(line, linesCount, true);
			linesCount++;

			if (status == Status::ERROR) {
				hasErrors = true;
				break;
			}
		}

		file.close();

		if (hasErrors) {
			LOG_INFO("Execution aborted due to errors");
			status = Status::ERROR;
			threadRunning = false;
			return;
		}

		// Second pass: execution
		status = Status::RUNNING;
		std::ifstream file2(filename);
		if (!file2.is_open()) {
			LOG_ERROR("Cannot open file: %s", filename.c_str());
			lastError = "Cannot open file: " + filename;
			status = Status::ERROR;
			threadRunning = false;
			return;
		}

		linesCount = 0;
		size_t totalLines = 0;

		std::ifstream countFile(filename);
		totalLines = std::count(std::istreambuf_iterator<char>(countFile),
			std::istreambuf_iterator<char>(), '\n');

		countFile.close();

		while (std::getline(file2, line)) {
			waitIfPaused();
			if (stopRequested) break;

			while (pauseRequested && !stopRequested) {
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}

			if (stopRequested) break;

			processLine(line, linesCount, false);
			linesCount++;

			if (totalLines > 0) {
				progress = static_cast<double>(linesCount) / totalLines * 100.0;
			}
		}

		file2.close();

		if (!stopRequested) {
			LOG_INFO("Execution completed successfully");
			status = Status::COMPLETED;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
			status = Status::IDLE;
		}
		else {
			LOG_INFO("Execution stopped by user");
			status = Status::IDLE;
		}
	}
	catch (const std::exception& ex) {
		LOG_ERROR("Runtime error: %s", ex.what());
		lastError = ex.what();
		status = Status::ERROR;
	}

	threadRunning = false;
}

void Interpreter::processLine(const std::string& line, int linesCount, bool isTryingInterpret) {
	// Clean line from comments and whitespace
	std::string cleanLine = line.substr(0, line.find(';'));
	cleanLine.erase(0, cleanLine.find_first_not_of(" \t"));
	cleanLine.erase(cleanLine.find_last_not_of(" \t") + 1);

	if (cleanLine.empty()) return;

	std::istringstream commandStream(cleanLine);
	std::string command;
	commandStream >> command;

	if (command.empty()) {
		LOG_DEBUG("Empty command in line: %s", line.c_str());
		return;
	}

	if (isTryingInterpret) {
		try {
			LOG_DEBUG("Syntax checking line: %d : %s", linesCount, cleanLine.c_str());
			if (command[0] == 'G' || command[0] == 'g') {
				int gCode = std::stoi(command.substr(1));

				switch (gCode) {
				case 0:
				case 1:
					processMovement(commandStream, linesCount, true);
					break;
				case 2:
					processArc(commandStream, linesCount, true, true);
					break;
				case 3:
					processArc(commandStream, linesCount, true, false);
					break;
				case 4:
				case 28:
				case 90:
				case 91:
					break;
				default:
					LOG_ERROR("Unknown G-code: %d at line %d", gCode, linesCount);
					break;
				}
			}
			else if (command[0] == 'M' || command[0] == 'm') {
				int mCode = std::stoi(command.substr(1));
				switch (mCode) {
				case 30:
					break;
				default:
					LOG_ERROR("Unknown M-code: %d at line %d", mCode, linesCount);
					break;
				}
			}
			else if (command[0] == 'F' || command[0] == 'f') {
				try {
					double newSpeed = std::stoi(command.substr(1));
					if (newSpeed < 0) {
						LOG_ERROR("Negative feedrate not allowed: %s", command.c_str());
					}
				}
				catch (const std::exception& ex) {
					LOG_ERROR("Invalid feedrate value: %s", command.c_str());
				}
			}
			else {
				LOG_ERROR("Unknown processing command '%s' at line %d", command.c_str(), linesCount);
			}
		}
		catch (const std::exception& ex) {
			LOG_ERROR("Execution in processLine: %s at line %d", ex.what(), linesCount);
		}
	}
	else {
		try {
			LOG_DEBUG("Executing line %d : %s", linesCount, cleanLine.c_str());
			if (command[0] == 'G' || command[0] == 'g') {
				int gCode = std::stoi(command.substr(1));

				switch (gCode) {
				case 0:
				case 1:
					processMovement(commandStream, linesCount, false);
					break;
				case 2:
					processArc(commandStream, linesCount, false, true);
					break;
				case 3:
					processArc(commandStream, linesCount, false, false);
					break;
				case 4:
					//stopAllMotors();
					break;
				case 28:
					//processHoming();
					break;
				case 90:
					absolutePositioning = true;
					break;
				case 91:
					absolutePositioning = false;
					break;
				default:
					break;
				}
			}
			else if (command[0] == 'M' || command[0] == 'm') {
				int mCode = std::stoi(command.substr(1));
				switch (mCode) {
				case 30:
					stopRequested = true;
					break;
				default:
					break;
				}
			}
			else if (command[0] == 'F' || command[0] == 'f') {
				speed = std::stoi(command.substr(1));
			}
		}
		catch (const std::exception& ex) {
			LOG_ERROR("Exception during execution: %s at line %d", ex.what(), linesCount);
		}
	}
}

void Interpreter::waitIfPaused() {
	while (pauseRequested && !stopRequested) {
		std::this_thread::sleep_for(std::chrono::microseconds(50));
	}
}

void Interpreter::processMovement(std::istringstream& string, int lineCount, bool isTryingInterpret) {
	std::string token;
	char axis;
	double value;

	if (isTryingInterpret) {
		if (!driver.getTransport()) {
			LOG_ERROR("Printer is not available for movement");
			return;
		}

		if (stepperX.ports.empty() || stepperY.ports.empty() || stepperZ.ports.empty()) {
			LOG_ERROR("Motor ports are not configured");
			return;
		}

		LOG_DEBUG("Checking movement command syntax");
		while (string >> token) {
			axis = token[0];
			value = std::stof(token.substr(1));

			switch (axis) {
			case 'X':
			case 'x':
			case 'Y':
			case 'y':
			case 'Z':
			case 'z':
				break;
			default:
				LOG_ERROR("unknown axis: %c at line %d", axis, lineCount);
				break;
			}
		}
	}
	else {
		LOG_DEBUG("Execute movement command");

		// Initialize target coordinates
		double targetX = absolutePositioning ? currentX : 0.0;
		double targetY = absolutePositioning ? currentY : 0.0;
		double targetZ = absolutePositioning ? currentZ : 0.0;

		// Parse movement commands
		while (string >> token) {
			axis = token[0];
			value = std::stof(token.substr(1));

			switch (axis) {
			case 'X':
			case 'x':
				if (absolutePositioning) targetX = value;
				else targetX += value;
				break;
			case 'Y':
			case 'y':
				if (absolutePositioning) targetY = value;
				else targetY += value;
				break;
			case 'Z':
			case 'z':
				if (absolutePositioning) targetZ = value;
				else targetZ += value;
				break;
			default:
				break;
			}
		}

		double xMovement = targetX - currentX;
		double yMovement = targetY - currentY;
		double zMovement = targetZ - currentZ;

		LOG_DEBUG("Execute movement command - X: %f Y: %f Z: %f", xMovement, yMovement, zMovement);

		// Process X and Y axis movement
		if (std::abs(xMovement) > 0 || std::abs(yMovement) > 0) {
			// Initialized final command for XY movement as a vector
			std::vector<MotorCommand> xyCommands;

			// Calculate movement times for each axis
			double timeX = 0.0;
			double timeY = 0.0;

			// ========== X Axis ==========
			if (std::abs(xMovement) > 0) {
				double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

				// Calculate base time for X movement
				double baseSpeedX = speed;
				if (xMovement < 0) baseSpeedX = -baseSpeedX;
				if (!stepperX.direction) baseSpeedX = -baseSpeedX;

				// Apply speed limits
				if (baseSpeedX > 0) {
					baseSpeedX = std::min(baseSpeedX, stepperX.maximumFeedrate);
					baseSpeedX = std::max(baseSpeedX, stepperX.minimumFeedrate);
				}
				else {
					baseSpeedX = std::max(baseSpeedX, -stepperX.maximumFeedrate);
					baseSpeedX = std::min(baseSpeedX, -stepperX.minimumFeedrate);
				}

				timeX = revolutionsX / std::abs(baseSpeedX);
			}

			// ========== Y Axis ==========
			if (std::abs(yMovement) > 0) {
				double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

				// Calculates base time for Y movement
				double baseSpeedY = speed;
				if (yMovement < 0) baseSpeedY = -baseSpeedY;
				if (!stepperY.direction) baseSpeedY = -baseSpeedY;

				// Apply speed limits
				if (baseSpeedY > 0) {
					baseSpeedY = std::min(baseSpeedY, stepperY.maximumFeedrate);
					baseSpeedY = std::max(baseSpeedY, stepperY.minimumFeedrate);
				}
				else {
					baseSpeedY = std::max(baseSpeedY, -stepperY.maximumFeedrate);
					baseSpeedY = std::min(baseSpeedY, -stepperY.minimumFeedrate);
				}

				timeY = revolutionsY / std::abs(baseSpeedY);
			}

			// Determine the maximum time needed
			double maxTime = std::max(timeX, timeY);
			if (maxTime == 0.0) { // Avoid division by zero
				maxTime = 1.0;
			}

			// ============ X Axis with synchronized speed =============
			if (std::abs(xMovement) > 0) {
				double revolutionsX = (std::abs(xMovement) * stepperX.gearRatio) / stepperX.rotationDistance;

				// Calculate speed to match the maximum time
				double synchronizedSpeedX = revolutionsX / maxTime;

				for (uint8_t port : stepperX.ports) {
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = synchronizedSpeedX;
					if (xMovement < 0) calculatedSpeed = -calculatedSpeed;
					if (!stepperX.direction) calculatedSpeed = -calculatedSpeed;

					// Apply speed limits to synchronized speed
					if (calculatedSpeed > 0) {
						calculatedSpeed = std::min(calculatedSpeed, stepperX.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperX.minimumFeedrate);
					}
					else {
						calculatedSpeed = std::max(calculatedSpeed, -stepperX.maximumFeedrate);
						calculatedSpeed = std::min(calculatedSpeed, -stepperX.minimumFeedrate);
					}

					command.speed = static_cast<signed char>(calculatedSpeed);
					command.revolutions = revolutionsX;

					xyCommands.push_back(command);

					LOG_DEBUG("X axis - Port: %c, Speed: %f, Revolutions: %f", port, calculatedSpeed, revolutionsX);
				}
			}

			// ============= Y Axis with synchronized speed ================
			if (std::abs(yMovement) > 0) {
				double revolutionsY = (std::abs(yMovement) * stepperY.gearRatio) / stepperY.rotationDistance;

				// Calculate speed to match the maximum time
				double synchronizedSpeedY = revolutionsY / maxTime;

				for (uint8_t port : stepperY.ports) {
					MotorCommand command;
					command.port = port;

					double calculatedSpeed = synchronizedSpeedY;
					if (yMovement < 0) calculatedSpeed = -calculatedSpeed;
					if (stepperY.direction) calculatedSpeed = -calculatedSpeed;

					// Apply speed limits to synchronized speed
					if (calculatedSpeed > 0) {
						calculatedSpeed = std::min(calculatedSpeed, stepperY.maximumFeedrate);
						calculatedSpeed = std::max(calculatedSpeed, stepperY.minimumFeedrate);
					}
					else {
						calculatedSpeed = std::max(calculatedSpeed, -stepperY.maximumFeedrate);
						calculatedSpeed = std::min(calculatedSpeed, -stepperY.minimumFeedrate);
					}

					command.speed = static_cast<signed char>(calculatedSpeed);
					command.revolutions = revolutionsY;

					xyCommands.push_back(command);

					LOG_DEBUG("Y axis - Port: %c Speed: %f Revolutions: %f", port, calculatedSpeed, revolutionsY);
				}
			}

			// Send synchronized commands for X and Y axis
			if (!xyCommands.empty()) {
				MotorCommand* finalCommands = new MotorCommand[xyCommands.size()];
				std::copy(xyCommands.begin(), xyCommands.end(), finalCommands);
				driver.rotateMotor(finalCommands, xyCommands.size());
				delete[] finalCommands;

				LOG_DEBUG("XY movement synchronized. Max time: %d", maxTime);
			}
		}

		// =================== Z Axis ===================
		if (std::abs(zMovement) > 0) {
			std::vector<MotorCommand> zCommands;

			for (uint8_t port : stepperZ.ports) {
				MotorCommand command;
				command.port = port;

				double calculatedSpeed = speed;
				if (zMovement < 0) calculatedSpeed = -calculatedSpeed;
				if (!stepperZ.direction) calculatedSpeed = -calculatedSpeed;

				if (calculatedSpeed > 0) {
					calculatedSpeed = std::min(calculatedSpeed, stepperZ.maximumFeedrate);
					calculatedSpeed = std::max(calculatedSpeed, stepperZ.minimumFeedrate);
				}
				else {
					calculatedSpeed = std::max(calculatedSpeed, -stepperZ.maximumFeedrate);
					calculatedSpeed = std::min(calculatedSpeed, -stepperZ.minimumFeedrate);
				}

				command.speed = static_cast<signed char>(calculatedSpeed);
				command.revolutions = (std::abs(zMovement) * stepperZ.gearRatio) / stepperZ.rotationDistance;

				zCommands.push_back(command);
			}

			MotorCommand* finalCommands = new MotorCommand[zCommands.size()];
			std::copy(zCommands.begin(), zCommands.end(), finalCommands);
			driver.rotateMotor(finalCommands, zCommands.size());
			delete[] finalCommands;
		}

		currentX = targetX;
		currentY = targetY;
		currentZ = targetZ;

		LOG_DEBUG("Movement completed. New position: X = %f Y = %f Z = %f", currentX, currentY, currentZ);
	}
}

void Interpreter::processArc(std::istringstream& stream, int lineCount, bool isTryingInterpret, bool clockwise) {

}
