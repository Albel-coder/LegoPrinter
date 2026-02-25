#include "MotorManager.h"

MotorManager::MotorManager(ITransport& transport) : transport_(transport) {
}

MotorManager::~MotorManager() = default;

void MotorManager::handleNotification(const uint8_t* data, size_t length) {
	if (!data || length < 3) {
		return;
	}
	uint8_t messageType = data[2];

	if (messageType == 0x04 && length >= 8) {
		uint8_t port = data[3];
		int32_t raw = (int32_t(data[4]) | (int32_t(data[5]) << 8) |
			(int32_t(data[6]) << 16) | (int32_t(data[7]) << 24));
		
		double position = raw / 360.0;
		{
			std::lock_guard<std::mutex> lock(completionMutex_);
			states_[port].position.store(position);
		}
		LOG_ENCODER("Port 0x%02X position = %.3f revolutions", port, position);
	}
	else if (messageType == 0x82 && length >= 5) {
		uint8_t port = data[3];
		uint8_t feedback = data[4];

		if (feedback == 0x0A) {
			std::lock_guard<std::mutex> lock(completionMutex_);

			if (commandStatus[port].waiting && !commandStatus[port].completed)
			{
				commandStatus[port].completed = true;
				completionCv_.notify_all();
			}
		}
	}
}

void MotorManager::rotate(const MotorCommand* commands, int count) {
	if (!commands || count <= 0) {
		LOG_ERROR("RotateMotor: Invalid parameters");
		return;
	}

	LOG_INFO("RotateMotor called with %d commands", count);

	if (!transport_.isConnected()) {
		LOG_ERROR("Printer is not connected!");
		return;
	}

	{
		// Prepare command tracking
		std::lock_guard<std::mutex> lock(completionMutex_);
		for (int i = 0; i < count; ++i) {
			commandStatus[commands[i].port].completed = false;
			commandStatus[commands[i].port].waiting = true;
		}
	}

	for (int i = 0; i < count; ++i) {
		sendMotorCommand(commands[i]);
	}

	waitForCommandsCompletion(commands, count);
	LOG_INFO("RotateMotor completed");
}

void MotorManager::setSpeed(uint8_t port, int8_t speed) {
	if (!transport_.isConnected()) {
		return;
	}

	LOG_MOTOR("Setting motor speed: Port=0x%02X, Speed=%d", port, speed);

	// First command: Activate mode
	std::vector<uint8_t> setupCommand = {
		0x09,       // Package length
		0x00,       // Hub ID
		0x41,       // Port configuration command
		port,       // Motor port
		0x01,       // Mode: Power (1)
		0x00,       // Data Format
		0x00,       // Unit
		0x00,       // Range min
		0x00        // Range max
	};

	transport_.write(setupCommand.data(), setupCommand.size());

	// Second Team: motor control
	std::vector<uint8_t> motorCommand = {
		0x08,       // Package length
		0x00,       // Hub ID
		0x81,       // Output control command
		port,       // Motor port
		0x02,       // Subcommand: WriteDirectModeData
		0x01,       // Mode: Power (1)
		static_cast<uint8_t>(speed) // Speed
	};

	transport_.write(motorCommand.data(), motorCommand.size());
}

double MotorManager::getPosition(uint8_t port) const {
	std::lock_guard<std::mutex> lock(completionMutex_);
	auto it = states_.find(port);
	if (it != states_.end()) {
		return it->second.position.load();
	}
	
	return 0.0;
}

bool MotorManager::isMoving(uint8_t port) {
	std::lock_guard<std::mutex> lock(completionMutex_);
	auto it = states_.find(port);
	if (it != states_.end()) {
		return it->second.moving.load();
	}

	return false;
}

void MotorManager::resetPosition(uint8_t port) {
	states_[port].position.store(0.0);
}

void MotorManager::sendMotorCommand(const MotorCommand& command) {
	LOG_MOTOR("Motor command - Port: 0x%02X, Speed: %d, Revolutions: %.2f",
		command.port, command.speed, command.revolutions);

	// Convert revolutions to absolute degrees (1 revolution = 360 degrees)
	int32_t degrees = static_cast<int32_t>(std::round(command.revolutions * 360.0));
	LOG_MOTOR("Calculated degrees: %d", degrees);

	// Command 1: Activate the rotation mode by angle
	std::vector<uint8_t> setupCommand = {
		0x09, // Message length
		0x00, // Hub ID
		0x41, // Port configuration command
		command.port, // Motor port
		0x02, // Mode: speed (to rotate at a certain angle)
		0x00, // Data Format
		0x01, // Unit of measurement: degrees
		0x00, // Range
		0x00  // Range
	};
	transport_.write(setupCommand.data(), setupCommand.size());
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	// Command 2: Rotate by a given angle
	std::vector<uint8_t> payload = {
		0x0F,       // Message length (15 bytes)
		0x00,       // Message counter
		0x81,       // Output control command
		command.port, // Port or combo port
		0x11,
		0x0B,       // Sub-team
		// Rotation angle (4 bytes little-endian)
		static_cast<uint8_t>(degrees & 0xFF),
		static_cast<uint8_t>((degrees >> 8) & 0xFF),
		static_cast<uint8_t>((degrees >> 16) & 0xFF),
		static_cast<uint8_t>((degrees >> 24) & 0xFF),
		// Speed (1 byte)
		static_cast<uint8_t>(command.speed),
		// Maximum power (usually 100%)
		100,
		// Final state (0 = float/coast, 1 = brake/hold)
		0x01,       // Hold the position after completion
		// Use profile (0 = use acceleration profile)
		0x00
	};

	LOG_MOTOR("Sending motor command to port 0x%02X", command.port);
	transport_.write(payload.data(), payload.size());
}

void MotorManager::waitForCommandsCompletion(const MotorCommand* Commands, int count) {
	std::unique_lock<std::mutex> lock(completionMutex_);

	bool allCompleted = completionCv_.wait_for(lock, std::chrono::seconds(30),
		[this, Commands, count]() {
			for (int i = 0; i < count; i++) {
				if (!commandStatus[Commands[i].port].completed) return false;
			}
			return true;
		});

	if (!allCompleted) {
		// For timeout - end all
		for (int i = 0; i < count; i++) {
			commandStatus[Commands[i].port].waiting = false;
		}
	}

	// Delete waiting status
	for (int i = 0; i < count; i++) {
		commandStatus[Commands[i].port].waiting = false;
	}
}
