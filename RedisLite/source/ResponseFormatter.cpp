#include "../headers/ResponseFormatter.h"

std::string ResponseFormatter::SimpleString(const std::string& msg) {
	return "+" + msg + "\r\n";
}

std::string ResponseFormatter::BulkString(const std::string& msg) {
	return "$" + std::to_string(msg.size()) + "\r\n" + msg + "\r\n";
}

std::string ResponseFormatter::NilBulkString() {
	return "$-1\r\n";
}

std::string ResponseFormatter::Integer(int value) {
	return ":" + std::to_string(value) + "\r\n";
}

std::string ResponseFormatter::Error(const std::string& msg) {
	return "-ERR " + msg + "\r\n";
}