#include "../headers/AOFManager.h"	
#include "../headers/ResponseFormatter.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include "../headers/CommandParser.h"

AOFManager::AOFManager(const std::string& path) : filePath(path) {
	try {
		std::filesystem::path p(filePath);

		if (!p.parent_path().empty() && !std::filesystem::exists(p.parent_path())) {
			std::filesystem::create_directories(p.parent_path());
		}

		aofFile.open(filePath, std::ios::out | std::ios::app);
		if (!aofFile.is_open()) {
			std::cerr << "[AOF] Failed to open AOF file: " << filePath << std::endl;
		}
		else {
			std::cout << "[AOF] AOF file active at: " << filePath << std::endl;
		}
	}
	catch (const std::exception& e) {
		std::cerr << "[AOF] Error initalizing AOF: " << e.what() << std::endl;
	}
}

AOFManager::~AOFManager() {
	close();
}

void AOFManager::close() {
	if (aofFile.is_open()) {
		aofFile.flush();
		aofFile.close();
		std::cout << "[AOF] AOF file closed." << std::endl;
	}
}

bool AOFManager::appendCommand(const Command& cmd) {
	std::lock_guard<std::mutex> lock(mtx);

	if (!aofFile.is_open()) {
		std::cerr << "[AOF] File not open for writing!" << std::endl;
		return false;
	}
	std::string serialized;

	switch (cmd.type) {
	case CommandType::SET: {
		if (cmd.ttlSeconds && *cmd.ttlSeconds > 0) {
			int ttl = *cmd.ttlSeconds;
			std::string ttlStr = std::to_string(ttl);
			serialized = "*5\r\n$3\r\nSET\r\n$" +
				std::to_string(cmd.key.size()) + "\r\n" + cmd.key + "\r\n$" +
				std::to_string(cmd.value.size()) + "\r\n" + cmd.value +
				"\r\n$2\r\nEX\r\n$" + std::to_string(ttlStr.size()) +
				"\r\n" + ttlStr + "\r\n";
		}
		else {
			serialized = "*3\r\n$3\r\nSET\r\n$" +
				std::to_string(cmd.key.size()) + "\r\n" + cmd.key + "\r\n$" +
				std::to_string(cmd.value.size()) + "\r\n" + cmd.value + "\r\n";
		}

		break;
	}

	case CommandType::DEL: {
		serialized = "*2\r\n$3\r\nDEL\r\n$" +
			std::to_string(cmd.key.size()) + "\r\n" + cmd.key + "\r\n";
		break;
	}


	default:
		return false;
	}

	aofFile << serialized;
	aofFile.flush();
	std::cout << "[AOF] Logged command: " << serialized;
	return true;
}

bool AOFManager::loadFromFile(KeyValueStore& kvStore) {
	std::ifstream inFile(filePath, std::ios::in);
	if (!inFile.is_open()) {
		std::cerr << "[AOF] Failed to open AOF file for reading: " << filePath << std::endl;
		return false;
	}

	std::string buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
	inFile.close();

	size_t totalBytesProcessed = 0;

	while (totalBytesProcessed < buffer.size()) {
		std::string remaining = buffer.substr(totalBytesProcessed);
		ParseResult result = CommandParser::parseCommand(remaining);

		if (result.status == ParseResult::Status::INCOMPLETE) {
			std::cerr << "[AOF] Incomplete command in AOF file. Stopping replay." << std::endl;
			break;
		}
		else if (result.status == ParseResult::Status::ERR) {
			std::cerr << "[AOF] Malformed command in AOF: " << result.errorMessage << ". Stopping replay." << std::endl;
			break;
		}

		const Command& cmd = result.command;

		switch (cmd.type) {
		case CommandType::SET:
			kvStore.set(cmd.key, cmd.value, cmd.ttlSeconds);
			break;

		case CommandType::DEL:
			kvStore.del(cmd.key);
			break;

		default:
			std::cerr << "[AOF] Skipping unsupported command in AOF: " << static_cast<int>(cmd.type) << std::endl;
			break;
		}
		totalBytesProcessed += result.bytesConsumed;
	}
	std::cout << "[AOF] Replay completed. Processed " << totalBytesProcessed << " bytes from AOF." << std::endl;
	return true;
}