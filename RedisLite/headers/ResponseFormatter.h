#pragma once
#include <string>

class ResponseFormatter {
public:
	static std::string SimpleString(const std::string& msg);
	static std::string BulkString(const std::string& msg);
	static std::string NilBulkString();
	static std::string Integer(int value);
	static std::string Error(const std::string& msg);
};