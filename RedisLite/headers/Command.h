#pragma once
#include <string>
#include <optional>
enum class CommandType {
	SET,
	GET,
	DEL,
	EXISTS,
	UNKNOWN
};

struct Command {
	CommandType type = CommandType::UNKNOWN;
	std::string key;
	std::string value; // Only for SET
	std::optional<int> ttlSeconds; // Only for SET with TTL
};