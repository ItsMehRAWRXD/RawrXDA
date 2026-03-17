#ifndef CODE_ANALYSIS_UTILS_HPP
#define CODE_ANALYSIS_UTILS_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <regex>
#include <sstream>

// Simple JSON-like structure for data interchange
struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object };
    
    Type type = Null;
    bool boolVal = false;
    double numberVal = 0.0;
    std::string stringVal;
    std::vector<JsonValue> arrayVal;
    std::map<std::string, JsonValue> objectVal;
    
    // Constructors
    JsonValue() = default;
    JsonValue(bool b) : type(Bool), boolVal(b) {}
    JsonValue(double n) : type(Number), numberVal(n) {}
    JsonValue(const std::string& s) : type(String), stringVal(s) {}
    JsonValue(const char* s) : type(String), stringVal(s) {}
    
    // Helper methods
    std::string toString() const;
    bool isNull() const { return type == Null; }
    bool toBool() const { return boolVal; }
    double toNumber() const { return numberVal; }
};

// Simple JSON builder for safe object construction
class JsonObject {
public:
    JsonObject& set(const std::string& key, const JsonValue& value);
    JsonObject& set(const std::string& key, bool value);
    JsonObject& set(const std::string& key, double value);
    JsonObject& set(const std::string& key, const std::string& value);
    JsonObject& set(const std::string& key, int value);
    
    JsonValue build() const;
    std::string toString() const;
    
private:
    std::map<std::string, JsonValue> m_data;
};

// File I/O utilities
class FileUtils {
public:
    static std::string readFile(const std::string& path);
    static bool writeFile(const std::string& path, const std::string& content);
    static bool fileExists(const std::string& path);
    static std::vector<std::string> listFiles(const std::string& path);
    static std::vector<std::string> listFilesRecursive(const std::string& path);
    static std::string getFileExtension(const std::string& path);
};

// String utilities
class StringUtils {
public:
    static std::vector<std::string> split(const std::string& str, char delimiter);
    static std::vector<std::string> split(const std::string& str, const std::string& delimiter);
    static std::string trim(const std::string& str);
    static std::string toLower(const std::string& str);
    static std::string toUpper(const std::string& str);
    static bool contains(const std::string& str, const std::string& substring);
    static bool startsWith(const std::string& str, const std::string& prefix);
    static bool endsWith(const std::string& str, const std::string& suffix);
    static std::string replace(const std::string& str, const std::string& from, const std::string& to);
};

// Code analysis utilities
class CodeAnalysisUtils {
public:
    // Count lines, complexity, etc.
    static int countLines(const std::string& code);
    static int countNonEmptyLines(const std::string& code);
    static int countCommentLines(const std::string& code);
    static int countFunctions(const std::string& code, const std::string& language);
    static int countClasses(const std::string& code, const std::string& language);
    
    // Pattern matching
    static bool matchesPattern(const std::string& code, const std::string& pattern);
    static std::vector<std::string> findMatches(const std::string& code, const std::string& pattern);
    
    // Code complexity
    static double calculateComplexity(const std::string& code);
    static int countCyclomaticComplexity(const std::string& code);
    
    // Detect language
    static std::string detectLanguage(const std::string& code);
};

#endif // CODE_ANALYSIS_UTILS_HPP
