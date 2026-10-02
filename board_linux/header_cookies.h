#pragma once
#include <cctype>
#include <sstream>
#include <string>

inline std::string header_cookie_pairs(const std::string& headers) {
    std::istringstream input(headers);
    std::string result;
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t colon = line.find(':');
        if (colon != 10) continue;
        std::string name = line.substr(0, colon);
        for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (name != "set-cookie") continue;
        const size_t start = line.find_first_not_of(" \t", colon + 1);
        if (start == std::string::npos) continue;
        const size_t end = line.find(';', start);
        const std::string pair = line.substr(start, end == std::string::npos ? end : end - start);
        if (pair.find('=') == std::string::npos) continue;
        if (!result.empty()) result += "; ";
        result += pair;
    }
    return result;
}
