#pragma once
#include <string>
#include <optional>
#include <chrono>


struct KVPair {
	std::string value;
	std::optional<std::chrono::steady_clock::time_point> expireAt;

	bool isExpired() const {
		if (!expireAt.has_value()) {
			return false;
		}
		return std::chrono::steady_clock::now() >= expireAt.value();
	}
};