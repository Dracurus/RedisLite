#include "../headers/TCPServer.h"
#include "../headers/CommandParser.h"
#include "../headers/ResponseFormatter.h"

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <thread>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

TCPServer::TCPServer(KeyValueStore& store, AOFManager* aof) :
	kvStore(store), aofManager(aof), running(false) , port(0), serverSocket(INVALID_SOCKET) {}

TCPServer::~TCPServer() {
	stop();
}

bool TCPServer::start(int port) {
	this->port = port;
	running = true;

	WSADATA wsaData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (result != 0) {
		std::cerr << "WSAStartup failed: " << result << std::endl;
		return false;
	}

	serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serverSocket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return false;
	}

	int opt = 1;
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
		std::cerr << "setsockopt(SO_REUSEADDR) failed: " << WSAGetLastError() << std::endl;
	}
	sockaddr_in service{};
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = INADDR_ANY;
	service.sin_port = htons(static_cast<u_short>(port));

	if (bind(serverSocket, reinterpret_cast<sockaddr*>(&service), sizeof(service)) == SOCKET_ERROR) {
		std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return false;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
		closesocket(serverSocket);
		WSACleanup();
		return false;
	}

	if (aofManager && aofManager->loadFromFile(kvStore)) {
		std::cout << "[AOF] Loaded data from AOF." << std::endl;
	}
	else {
		std::cout << "[AOF] No AOF data loaded (file missing or empty)." << std::endl;
	}

	acceptThread = std::thread(&TCPServer::acceptClients, this);
	std::cout << "RedisLite listening on port " << port << std::endl;
	return true;
}

void TCPServer::acceptClients() {
	while (running.load()) {
		sockaddr_in clientAddr;
		int addrlen = sizeof(clientAddr);
		SOCKET clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrlen);

		if (clientSocket == INVALID_SOCKET) {
			int err = WSAGetLastError();
			if (!running.load()) {
				break;
			}
			std::cerr << "accept() failed: " << err << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		workers.emplace_back(&TCPServer::handleClient, this, clientSocket);
	}
}

void TCPServer::cleanupThreads() {
	for (auto& t : workers) {
		if (t.joinable()) {
			t.join();
		}
	}
	workers.clear();
}

void TCPServer::stop() {
	if (!running.load()) return;
	running = false;

	if (serverSocket != INVALID_SOCKET) {
		closesocket(serverSocket);
		serverSocket = INVALID_SOCKET;
	}

	if (acceptThread.joinable()) {
		acceptThread.join();
	}
	cleanupThreads();
	WSACleanup();
	std::cout << "RedisLite server stopped." << std::endl;
}

void TCPServer::handleClient(SOCKET clientSocket) {
	sockaddr_in addr;
	int len = sizeof(addr);
	getpeername(clientSocket, reinterpret_cast<sockaddr*>(&addr), &len);

	char clientIp[INET_ADDRSTRLEN];
	if (InetNtopA(AF_INET, &addr.sin_addr, clientIp, INET_ADDRSTRLEN)) {
		std::cout << "Client connected: " << clientIp << ":" << ntohs(addr.sin_port) << std::endl;
	}
	else {
		std::cerr << "Failed to convert client IP address." << std::endl;
	}

	std::string buffer;
	const int BUF_SIZE = 4096;
	std::vector<char> temp(BUF_SIZE);

	while (running.load()) {
		int bytesRead = recv(clientSocket, temp.data(), static_cast<int>(temp.size()), 0);

		if (bytesRead == 0) break;
		if (bytesRead == SOCKET_ERROR) {
			int err = WSAGetLastError();
			std::cerr << "recv() failed: " << err << std::endl;
			break;
		}

		buffer.append(temp.data(), bytesRead);

		while (true) {
			ParseResult result = CommandParser::parseCommand(buffer);

			if (result.status == ParseResult::Status::INCOMPLETE) break;

			if (result.status == ParseResult::Status::ERR) {
				std::string errMsg = ResponseFormatter::Error(result.errorMessage);
				send(clientSocket, errMsg.c_str(), static_cast<int>(errMsg.size()), 0);
				buffer.clear();
				continue;
			}

			const Command& cmd = result.command;
			std::string response;

			switch (cmd.type) {
				case CommandType::SET:
					kvStore.set(cmd.key, cmd.value, cmd.ttlSeconds);
					response = ResponseFormatter::SimpleString("OK");
					if (aofManager) aofManager->appendCommand(cmd);
					break;

				case CommandType::GET: {
					auto val = kvStore.get(cmd.key);
					if (val.has_value()) {
						response = ResponseFormatter::BulkString(val.value());
					}
					else {
						response = ResponseFormatter::NilBulkString();
					}
					break;
				}

				case CommandType::DEL: {
					bool deleted = kvStore.del(cmd.key);
					response = ResponseFormatter::Integer(deleted ? 1 : 0);
					if (aofManager) aofManager->appendCommand(cmd);
					break;
				}
				case CommandType::EXISTS: {
					bool exists = kvStore.exists(cmd.key);
					response = ResponseFormatter::Integer(exists ? 1 : 0);
					break;
				}

				default:
					response = ResponseFormatter::Error("Unknown command");
					break;
			}

			int sent = send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
			if (sent == SOCKET_ERROR) {
				int err = WSAGetLastError();
				std::cerr << "send() failed: " << err << std::endl;
				break;
			}

			buffer.erase(0, result.bytesConsumed);
		}
	}

	closesocket(clientSocket);
	std::cout << "Client disconnected: " << clientIp << ":" << ntohs(addr.sin_port) << std::endl;
}