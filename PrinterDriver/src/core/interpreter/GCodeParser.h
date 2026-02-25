#pragma once
#include <string>
#include <optional>
#include <unordered_map>

struct GCodeCommand {
	std::string command;
	std::unordered_map<char, double> parameters;
};

class GCodeParser {
public:
	std::optional<GCodeCommand> parse(const std::string& line) const;
};