#pragma once
#include "../headers/KeyValueStore.h"
#include "../headers/AOFManager.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <WinSock2.h>
#include <WS2tcpip.h>

class TCPServer {
private:
	SOCKET serverSocket;
	int port;
	KeyValueStore& kvStore;
	AOFManager* aofManager;
	std::atomic<bool> running;
	std::thread acceptThread;
	std::vector<std::thread> workers;

	void acceptClients();
	void handleClient(SOCKET client_fd);
	void cleanupThreads();

public:
	explicit TCPServer(KeyValueStore& store, AOFManager* aof = nullptr);
	~TCPServer();

	bool start(int port);
	void stop();
};