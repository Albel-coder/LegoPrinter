#pragma once
#include "../interfaces/IMotorManager.h"
#include "../../../transport/TransportSimpleBLE.h"
#include <map>
#include <mutex>
#include <atomic>

class MotorManager : public IMotorManager {
public:
	explicit MotorManager(ITransport& transportPointer);
	~MotorManager();

	void handleNotification(const uint8_t* data, size_t length) override;
	void rotate(const MotorCommand* commands, int count) override;
	void setSpeed(uint8_t port, int8_t speed) override;
	double getPosition(uint8_t port) const override;
	bool isMoving(uint8_t port) override;
	void resetPosition(uint8_t port) override;

private:
	struct MotorState {
		std::atomic<double> position{ 0.0 };
		std::atomic<bool> moving{ false };
		std::atomic<int8_t> currentSpeed{ 0 };
	};

	ITransport& transport;
	std::map<uint8_t, MotorState> states;

	mutable std::mutex completionMutex;
	std::condition_variable completionCv;

	struct CommandExecution {
		bool completed = false;
	};

	std::map<uint8_t, CommandExecution> commandStatus;

	void sendMotorCommand(const MotorCommand& command);

	void waitForCommandsCompletion(const MotorCommand* Commands, int count);
};