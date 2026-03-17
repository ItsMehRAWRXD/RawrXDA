#include "CodeAnalysisUtils.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

// JsonValue implementation
std::string JsonValue::toString() const {
    switch (type) {
        case Null: return "null";
        case Bool: return boolVal ? "true" : "false";
        case Number: {
            std::stringstream ss;
            ss << numberVal;
            return ss.str();
        }
        case String: return "\"" + stringVal + "\"";
        case Array: {
            std::string result = "[";
            for (size_t i = 0; i < arrayVal.size(); ++i) {
                result += arrayVal[i].toString();
                if (i < arrayVal.size() - 1) result += ",";
            }
            result += "]";
            return result;
        }
        case Object: {
            std::string result = "{";
            bool first = true;
            for (const auto& [key, val] : objectVal) {
                if (!first) result += ",";
                result += "\"" + key + "\":" + val.toString();
                first = false;
            }
            result += "}";
            return result;
        }
    }
    return "null";
}

// JsonObject implementation
JsonObject& JsonObject::set(const std::string& key, const JsonValue& value) {
    m_data[key] = value;
    return *this;
}
JsonObject& JsonObject::set(const std::string& key, bool value) {
    m_data[key] = JsonValue(value);
    return *this;
}
JsonObject& JsonObject::set(const std::string& key, double value) {
    m_data[key] = JsonValue(value);
    return *this;
}
JsonObject& JsonObject::set(const std::string& key, const std::string& value) {
    m_data[key] = JsonValue(value);
    return *this;
}
JsonObject& JsonObject::set(const std::string& key, int value) {
    m_data[key] = JsonValue(static_cast<double>(value));
    return *this;
}

JsonValue JsonObject::build() const {
    JsonValue result;
    result.type = JsonValue::Object;
    result.objectVal = m_data;
    return result;
}

std::string JsonObject::toString() const {
    return build().toString();
}

// FileUtils implementation
std::string FileUtils::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool FileUtils::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << content;
    return true;
}

bool FileUtils::fileExists(const std::string& path) {
    return fs::exists(path) && fs::is_regular_file(path);
}

std::vector<std::string> FileUtils::listFiles(const std::string& path) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    } catch (...) {}
    return files;
}

std::vector<std::string> FileUtils::listFilesRecursive(const std::string& path) {
    std::vector<std::string> files;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                files.push_back(entry.path().string());
            }
        }
    } catch (...) {}
    return files;
}

std::string FileUtils::getFileExtension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) return "";
    return path.substr(pos + 1);
}

// StringUtils implementation
std::vector<std::string> StringUtils::split(const std::string& str, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    return result;
}

std::vector<std::string> StringUtils::split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        result.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    result.push_back(str.substr(start));
    return result;
}

std::string StringUtils::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::string StringUtils::toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string StringUtils::toUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

bool StringUtils::contains(const std::string& str, const std::string& substring) {
    return str.find(substring) != std::string::npos;
}

bool StringUtils::startsWith(const std::string& str, const std::string& prefix) {
    return str.compare(0, prefix.length(), prefix) == 0;
}

bool StringUtils::endsWith(const std::string& str, const std::string& suffix) {
    if (str.length() < suffix.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

std::string StringUtils::replace(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::string::npos) {
        result.replace(pos, from.length(), to);
        pos += to.length();
    }
    return result;
}

// CodeAnalysisUtils implementation
int CodeAnalysisUtils::countLines(const std::string& code) {
    if (code.empty()) return 0;
    return std::count(code.begin(), code.end(), '\n') + 1;
}

int CodeAnalysisUtils::countNonEmptyLines(const std::string& code) {
    int count = 0;
    std::stringstream ss(code);
    std::string line;
    while (std::getline(ss, line)) {
        if (!StringUtils::trim(line).empty()) {
            count++;
        }
    }
    return count;
}

int CodeAnalysisUtils::countCommentLines(const std::string& code) {
    int count = 0;
    std::stringstream ss(code);
    std::string line;
    while (std::getline(ss, line)) {
        std::string trimmed = StringUtils::trim(line);
        if (StringUtils::startsWith(trimmed, "//") || StringUtils::startsWith(trimmed, "#") ||
            StringUtils::startsWith(trimmed, "/*") || StringUtils::startsWith(trimmed, "*")) {
            count++;
        }
    }
    return count;
}

int CodeAnalysisUtils::countFunctions(const std::string& code, const std::string& language) {
    int count = 0;
    try {
        if (language == "cpp" || language == "c" || language == "java" || language == "csharp") {
            std::regex funcPattern(R"(\b\w+\s+\w+\s*\([^)]*\)\s*\{)");
            count = std::distance(std::sregex_iterator(code.begin(), code.end(), funcPattern),
                                  std::sregex_iterator());
        } else if (language == "python") {
            std::regex funcPattern(R"(^\s*def\s+\w+\s*\()");
            count = std::distance(std::sregex_iterator(code.begin(), code.end(), funcPattern),
                                  std::sregex_iterator());
        } else if (language == "javascript" || language == "typescript") {
            std::regex funcPattern(R"(\b(function|const|let|var)\s+\w+\s*=?\s*\([^)]*\)\s*\{)");
            count = std::distance(std::sregex_iterator(code.begin(), code.end(), funcPattern),
                                  std::sregex_iterator());
        }
    } catch (...) {}
    return count;
}

int CodeAnalysisUtils::countClasses(const std::string& code, const std::string& language) {
    int count = 0;
    try {
        if (language == "cpp" || language == "java" || language == "csharp") {
            std::regex classPattern(R"(\b(class|struct)\s+\w+\b)");
            count = std::distance(std::sregex_iterator(code.begin(), code.end(), classPattern),
                                  std::sregex_iterator());
        } else if (language == "python") {
            std::regex classPattern(R"(^\s*class\s+\w+)");
            count = std::distance(std::sregex_iterator(code.begin(), code.end(), classPattern),
                                  std::sregex_iterator());
        }
    } catch (...) {}
    return count;
}

bool CodeAnalysisUtils::matchesPattern(const std::string& code, const std::string& pattern) {
    try {
        std::regex re(pattern);
        return std::regex_search(code, re);
    } catch (...) {
        return false;
    }
}

std::vector<std::string> CodeAnalysisUtils::findMatches(const std::string& code, const std::string& pattern) {
    std::vector<std::string> matches;
    try {
        std::regex re(pattern);
        std::sregex_iterator iter(code.begin(), code.end(), re);
        std::sregex_iterator end;
        for (; iter != end; ++iter) {
            matches.push_back(iter->str());
        }
    } catch (...) {}
    return matches;
}

double CodeAnalysisUtils::calculateComplexity(const std::string& code) {
    // Simple heuristic: count decision points
    int decisions = std::count(code.begin(), code.end(), '?') +
                   std::count(code.begin(), code.end(), ':');
    int loops = 0;
    try {
        std::regex loopPattern(R"(\b(for|while|do)\b)");
        loops = std::distance(std::sregex_iterator(code.begin(), code.end(), loopPattern),
                             std::sregex_iterator());
    } catch (...) {}
    
    return 1.0 + (decisions * 0.5) + (loops * 0.3);
}

int CodeAnalysisUtils::countCyclomaticComplexity(const std::string& code) {
    int cc = 1;
    try {
        std::regex ifPattern(R"(\bif\b)");
        std::regex elsePattern(R"(\belse\b)");
        std::regex switchPattern(R"(\bswitch\b)");
        std::regex casePattern(R"(\bcase\b)");
        std::regex forPattern(R"(\bfor\b)");
        std::regex whilePattern(R"(\bwhile\b)");
        std::regex catchPattern(R"(\bcatch\b)");
        
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), ifPattern),
                           std::sregex_iterator());
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), elsePattern),
                           std::sregex_iterator());
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), switchPattern),
                           std::sregex_iterator());
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), casePattern),
                           std::sregex_iterator());
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), forPattern),
                           std::sregex_iterator());
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), whilePattern),
                           std::sregex_iterator());
        cc += std::distance(std::sregex_iterator(code.begin(), code.end(), catchPattern),
                           std::sregex_iterator());
    } catch (...) {}
    return cc;
}

std::string CodeAnalysisUtils::detectLanguage(const std::string& code) {
    // Simple heuristic based on common patterns
    if (StringUtils::contains(code, "def ") && StringUtils::contains(code, "import ")) {
        return "python";
    } else if (StringUtils::contains(code, "#include") || StringUtils::contains(code, "std::")) {
        return "cpp";
    } else if (StringUtils::contains(code, "public class") || StringUtils::contains(code, "package ")) {
        return "java";
    } else if (StringUtils::contains(code, "function") || StringUtils::contains(code, "const ")) {
        return "javascript";
    } else if (StringUtils::contains(code, "async") || StringUtils::contains(code, "await")) {
        return "typescript";
    }
    return "unknown";
}
