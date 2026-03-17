#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

bool aici_read_file_text(const std::string& path, std::string& out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    ifs.seekg(0, std::ios::end);
    std::string tmp;
    tmp.resize(static_cast<size_t>(ifs.tellg()));
    ifs.seekg(0);
    ifs.read(&tmp[0], static_cast<std::streamsize>(tmp.size()));
    out.swap(tmp);
    return true;
}

std::vector<std::string> aici_split_lines(const std::string& s) {
    std::vector<std::string> lines;
    size_t start = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') { lines.emplace_back(s.substr(start, i - start)); start = i + 1; }
    }
    if (start <= s.size()) lines.emplace_back(s.substr(start));
    return lines;
}

std::string aici_to_lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return (char)std::tolower(c);}); return s; }

bool aici_ends_with(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

std::string aici_escape_json(const std::string& s) {
    std::string out; out.reserve(s.size()+16);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break; case '\\': out += "\\\\"; break; case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break; case '\n': out += "\\n"; break; case '\r': out += "\\r"; break; case '\t': out += "\\t"; break;
            default: if (c < 0x20) { char buf[7]; std::snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; } else out += (char)c;
        }
    }
    return out;
}

static bool aici_wildcard_match_impl(const char* p, const char* t) {
    const char* star = nullptr; const char* match = nullptr;
    while (*t) {
        if (*p == '?' || std::tolower(*p) == std::tolower(*t)) { ++p; ++t; continue; }
        if (*p == '*') { star = p++; match = t; continue; }
        if (star) { p = star + 1; t = ++match; continue; }
        return false;
    }
    while (*p == '*') ++p;
    return *p == '\0';
}

bool aici_wildcard_match(const std::string& pattern, const std::string& text) { return aici_wildcard_match_impl(pattern.c_str(), text.c_str()); }
