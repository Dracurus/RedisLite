#pragma once
#include <string>
#include "../headers/Command.h"

struct ParseResult {
	enum Status {OK, INCOMPLETE, ERR};
	Status status = Status::INCOMPLETE;
	Command command;
	size_t bytesConsumed;
	std::string errorMessage;
};

class CommandParser {
public:
	static ParseResult parseCommand(const std::string& input);
};