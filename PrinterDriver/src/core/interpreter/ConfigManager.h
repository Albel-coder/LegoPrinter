#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct StepperConfig {
	double rotationDistance = 0.0;
	double gearRatio = 1.0;
	bool direction = true;
	std::vector<uint8_t> ports;
	double minFeedrate = 0.0;
	double maxFeedrate = 0.0;
};

class ConfigManager {
public:
	bool load(const std::string& filename);

	const StepperConfig& getX() const { return stepperX; }
	const StepperConfig& getY() const { return stepperY; }
	const StepperConfig& getZ() const { return stepperZ; }

	double getZDistanceToPrint() { return zDistanceToPrint; }

private:
	StepperConfig stepperX, stepperY, stepperZ;
	double zDistanceToPrint = 0.0;

	bool parseSection(const std::string& section, const std::string& key, const std::string& value);
};