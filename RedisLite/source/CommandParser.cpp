// CommandParser.cpp
#include "../headers/CommandParser.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <vector>

// Helper: find CRLF starting from pos. returns npos if not found.
static size_t findCRLF(const std::string& buf, size_t pos) {
    return buf.find("\r\n", pos);
}

// Helper: parse a line (up to CRLF). On success line contains contents before CRLF and nextPos is position after CRLF.
// Returns true on success, false if CRLF not found (incomplete).
static bool parseLine(const std::string& buf, size_t pos, std::string& line, size_t& nextPos) {
    size_t crlf = findCRLF(buf, pos);
    if (crlf == std::string::npos) return false; // incomplete
    line = buf.substr(pos, crlf - pos);
    nextPos = crlf + 2; // skip CRLF
    return true;
}

// Helper: safe integer parsing
static bool parseInteger(const std::string& s, long long& out) {
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        long long v = std::stoll(s, &idx, 10);
        if (idx != s.size()) return false;
        out = v;
        return true;
    }
    catch (...) {
        return false;
    }
}

ParseResult CommandParser::parseCommand(const std::string& buffer) {
    ParseResult result;
    result.status = ParseResult::Status::INCOMPLETE;
    result.bytesConsumed = 0;
    result.errorMessage.clear();

    if (buffer.empty()) {
        return result; // incomplete
    }

    size_t pos = 0;

    // Expect array header: "*<N>\r\n"
    if (buffer[pos] != '*') {
        result.status = ParseResult::Status::ERR;
        result.errorMessage = "Expected '*'";
        return result;
    }

    // parse the "*<N>\r\n" line
    std::string arrayLine;
    size_t afterArrayLine = 0;
    if (!parseLine(buffer, pos + 1, arrayLine, afterArrayLine)) {
        // incomplete
        return result;
    }

    long long numElements = 0;
    if (!parseInteger(arrayLine, numElements)) {
        result.status = ParseResult::Status::ERR;
        result.errorMessage = "Invalid array length";
        return result;
    }
    if (numElements <= 0) {
        result.status = ParseResult::Status::ERR;
        result.errorMessage = "Array must contain at least one element";
        return result;
    }

    pos = afterArrayLine;

    std::vector<std::string> parts;
    parts.reserve(static_cast<size_t>(numElements));

    // Parse each bulk string: $<len>\r\n<data>\r\n
    for (long long i = 0; i < numElements; ++i) {
        // Need at least one char for '$'
        if (pos >= buffer.size()) {
            // incomplete
            result.status = ParseResult::Status::INCOMPLETE;
            return result;
        }

        if (buffer[pos] != '$') {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Expected '$'";
            return result;
        }

        // parse the "$<len>\r\n" line
        std::string lenLine;
        size_t afterLenLine = 0;
        if (!parseLine(buffer, pos + 1, lenLine, afterLenLine)) {
            // incomplete
            result.status = ParseResult::Status::INCOMPLETE;
            return result;
        }

        long long len = 0;
        if (!parseInteger(lenLine, len)) {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Invalid bulk length";
            return result;
        }

        // handle nil bulk string
        if (len == -1) {
            parts.emplace_back(std::string()); // represent nil as empty string for commands
            pos = afterLenLine;
            continue;
        }

        if (len < 0) {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Negative bulk length";
            return result;
        }

        // check we have data + CRLF available
        size_t dataStart = afterLenLine;
        size_t dataEnd = dataStart + static_cast<size_t>(len);
        if (buffer.size() < dataEnd + 2) {
            // incomplete
            result.status = ParseResult::Status::INCOMPLETE;
            return result;
        }

        // extract data
        std::string data = buffer.substr(dataStart, static_cast<size_t>(len));

        // validate trailing CRLF
        if (buffer[dataEnd] != '\r' || buffer[dataEnd + 1] != '\n') {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Missing CRLF after bulk data";
            return result;
        }

        parts.push_back(std::move(data));
        pos = dataEnd + 2; // advance past data and trailing CRLF
    }

    // We have parsed all parts successfully
    result.bytesConsumed = pos;

    // Interpret parts into Command
    Command cmd;
    cmd.type = CommandType::UNKNOWN;

    if (parts.empty()) {
        result.status = ParseResult::Status::ERR;
        result.errorMessage = "Empty command";
        return result;
    }

    std::string cmdName = parts[0];
    std::transform(cmdName.begin(), cmdName.end(), cmdName.begin(), [](unsigned char c) { return std::toupper(c); });

    if (cmdName == "SET") {
        // SET key value            -> parts.size() == 3
        // SET key value EX seconds -> parts.size() == 5
        if (parts.size() == 3) {
            cmd.type = CommandType::SET;
            cmd.key = parts[1];
            cmd.value = parts[2];
            cmd.ttlSeconds = std::nullopt;
        }
        else if (parts.size() == 5) {
            std::string opt = parts[3];
            std::transform(opt.begin(), opt.end(), opt.begin(), [](unsigned char c) { return std::toupper(c); });
            if (opt == "EX") {
                long long ttlVal = 0;
                if (!parseInteger(parts[4], ttlVal) || ttlVal < 0) {
                    result.status = ParseResult::Status::ERR;
                    result.errorMessage = "Invalid TTL value";
                    return result;
                }
                cmd.type = CommandType::SET;
                cmd.key = parts[1];
                cmd.value = parts[2];
                cmd.ttlSeconds = static_cast<int>(ttlVal);
            }
            else {
                result.status = ParseResult::Status::ERR;
                result.errorMessage = "Unknown SET option";
                return result;
            }
        }
        else {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Wrong number of arguments for SET";
            return result;
        }
    }
    else if (cmdName == "GET") {
        if (parts.size() == 2) {
            cmd.type = CommandType::GET;
            cmd.key = parts[1];
        }
        else {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Wrong number of arguments for GET";
            return result;
        }
    }
    else if (cmdName == "DEL") {
        if (parts.size() == 2) {
            cmd.type = CommandType::DEL;
            cmd.key = parts[1];
        }
        else {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Wrong number of arguments for DEL";
            return result;
        }
    }
    else if (cmdName == "EXISTS") {
        if (parts.size() == 2) {
            cmd.type = CommandType::EXISTS;
            cmd.key = parts[1];
        }
        else {
            result.status = ParseResult::Status::ERR;
            result.errorMessage = "Wrong number of arguments for EXISTS";
            return result;
        }
    }
    else {
        // Unknown command name — return OK but mark command UNKNOWN, or treat as error
        result.status = ParseResult::Status::ERR;
        result.errorMessage = "Unknown command";
        return result;
    }

    result.command = std::move(cmd);
    result.status = ParseResult::Status::OK;
    return result;
}
