#pragma once

#include <string>
#include <vector>

bool aici_read_file_text(const std::string& path, std::string& out);
std::vector<std::string> aici_split_lines(const std::string& s);
std::string aici_to_lower(std::string s);
bool aici_ends_with(const std::string& s, const std::string& suffix);
std::string aici_escape_json(const std::string& s);
bool aici_wildcard_match(const std::string& pattern, const std::string& text);
