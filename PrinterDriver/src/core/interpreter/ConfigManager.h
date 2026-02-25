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

	const StepperConfig& getX() const { return stepperX_; }
	const StepperConfig& getY() const { return stepperY_; }
	const StepperConfig& getZ() const { return stepperZ_; }

	double getZDistanceToPrint() { return zDistanceToPrint_; }

private:
	StepperConfig stepperX_, stepperY_, stepperZ_;
	double zDistanceToPrint_ = 0.0;

	bool parseSection(const std::string& section, const std::string& key, const std::string& value);
};