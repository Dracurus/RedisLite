#pragma once
#include <fstream>
#include <string>
#include <mutex>
#include "../headers/KeyValueStore.h"
#include "../headers/Command.h"

class AOFManager {
private:
	std::string filePath;
	std::ofstream aofFile;
	std::mutex mtx;

public:
	explicit AOFManager(const std::string& path = "./appendonly.aof");
	~AOFManager();

	bool appendCommand(const Command& cmd);
	bool loadFromFile(KeyValueStore& store);
	void close();
};