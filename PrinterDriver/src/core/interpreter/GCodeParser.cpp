#include "GCodeParser.h"
#include <cctype>
#include <sstream>

std::optional<GCodeCommand> GCodeParser::parse(const std::string& line) const {
	// Удаляем комментарии (после ;)
	size_t commentPosition = line.find(';');
	std::string clean = (commentPosition != std::string::npos) ? line.substr(0, commentPosition) : line;

	// Удаляем лишние пробелы
	size_t start = clean.find_first_not_of(" \t\r\n");
	if (start == std::string::npos) {
		return std::nullopt;
	}
	size_t end = clean.find_last_not_of(" \t\r\n");
	clean = clean.substr(start, end - start + 1);

	if (clean.empty()) {
		return std::nullopt;
	}

	std::istringstream stringStream(clean);
	std::string token;
	GCodeCommand command;

	// Первый токен - команда (G1, M30, F500, ...)
	stringStream >> token;
	command.command = token;

	while (stringStream >> token) {
		if (token.size() < 2) {
			continue;
		}
		char letter = std::toupper(token[0]);
		try {
			double value = std::stod(token.substr(1));
			command.parameters[letter] = value;
		}
		catch (...) {
			// ignore invalid numbers
		}
	}

	return command;
}
