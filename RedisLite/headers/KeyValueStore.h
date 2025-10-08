#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include "KVPair.h"

class KeyValueStore {
private:
	std::unordered_map<std::string, KVPair> store;
	std::mutex mtx;

public:
	KeyValueStore() = default;
	~KeyValueStore() = default;

	void set(const std::string& key, const std::string& value, std::optional<int> ttlSeconds = std::nullopt);
	std::optional<std::string> get(const std::string& key);
	bool del(const std::string& key);
	bool exists(const std::string& key);
};