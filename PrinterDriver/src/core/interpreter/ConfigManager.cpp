#include "ConfigManager.h"
#include "../../logging/LogManager.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

bool ConfigManager::load(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		LOG_ERROR("Cannot open config file: %s", filename.c_str());
	}
}

bool ConfigManager::parseSection(const std::string& section, const std::string& key, const std::string& value) {
	return false;
}
