#include <iostream>
#include <thread>
#include <chrono>
#include "headers/KeyValueStore.h"
#include "headers/TCPServer.h"
#include "headers/AOFManager.h"
#include "headers/Command.h"

int main() {
    KeyValueStore kvStore;
    
	std::string aofPath = "appendonly.aof";
	AOFManager aofManager(aofPath);

    if (aofManager.loadFromFile(kvStore)) {
		std::cout << "[AOF] Sucessfully loaded data from AOF." << std::endl;
    }
    else {
		std::cout << "[AOF] No AOF data to load or error occurred." << std::endl;
    }

	TCPServer server(kvStore, &aofManager);
    if (!server.start(6379)) {
        std::cerr << "Failed to start server." << std::endl;
		return -1;
    }

	std::cout << "[Server] RedisLite server listening on port 6379..." << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
	}
    return 0;
}
