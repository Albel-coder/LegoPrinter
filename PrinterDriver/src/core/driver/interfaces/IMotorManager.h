#pragma once
#include "../api/LegoDriverAPI.h"
#include <cstdint>

class IMotorManager {
public:
	virtual ~IMotorManager() = default;

	virtual void handleNotification(const uint8_t* data, size_t length) = 0;

	virtual void rotate(const MotorCommand* commands, int count) = 0;

	virtual void setSpeed(uint8_t port, int8_t speed) = 0;

	virtual double getPosition(uint8_t port) const = 0;

	virtual bool isMoving(uint8_t port) = 0;

	virtual void resetPosition(uint8_t port) = 0;
};