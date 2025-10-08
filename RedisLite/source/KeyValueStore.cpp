#include "../headers/KeyValueStore.h"

void KeyValueStore::set(const std::string& key, const std::string& value, std::optional<int> ttlSeconds)
{
	std::lock_guard<std::mutex> lock(mtx);
	KVPair kvp;
	kvp.value = value;
	if (ttlSeconds.has_value()) {
		kvp.expireAt = std::chrono::steady_clock::now() + std::chrono::seconds(ttlSeconds.value());
	}
	store[key] = kvp;
}

std::optional<std::string> KeyValueStore::get(const std::string& key) 
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = store.find(key);
	if (it == store.end()) {
		return std::nullopt;
	}

	if (it->second.isExpired()) {
		store.erase(it);
		return std::nullopt;
	}

	return it->second.value;
}

bool KeyValueStore::del(const std::string& key)
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = store.find(key);
	if (it == store.end()) {
		return false;
	}
	if (it->second.isExpired()) {
		store.erase(it);
		return false;
	}
	store.erase(it);
	return true;
}

bool KeyValueStore::exists(const std::string& key) 
{
	std::lock_guard<std::mutex> lock(mtx);
	auto it = store.find(key);
	if (it == store.end()) 
	{
		return false;
	}

	if (it->second.isExpired()) 
	{
		store.erase(it);
		return false;
	}
	return true;
}