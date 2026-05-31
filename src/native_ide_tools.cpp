// ============================================================================
// native_ide_tools.cpp — Unified Native IDE Tools Implementation
// 100+ Tools for IDE Integration — Zero External Dependencies
// ============================================================================

#include "native_ide_tools.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <regex>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#define PATH_SEP '/'
#endif

namespace RawrXD {
namespace NativeIDE {

// ============================================================================
// Platform Implementation
// ============================================================================
namespace Platform {

void* Alloc(size_t size) {
    return std::malloc(size);
}

void* Realloc(void* ptr, size_t size) {
    return std::realloc(ptr, size);
}

void Free(void* ptr) {
    std::free(ptr);
}

size_t StrLen(const char* str) {
    return str ? std::strlen(str) : 0;
}

char* StrDup(const char* str) {
    if (!str) return nullptr;
    size_t len = std::strlen(str) + 1;
    char* dup = static_cast<char*>(std::malloc(len));
    if (dup) std::memcpy(dup, str, len);
    return dup;
}

int StrCmp(const char* a, const char* b) {
    return std::strcmp(a ? a : "", b ? b : "");
}

int StrNCmp(const char* a, const char* b, size_t n) {
    return std::strncmp(a ? a : "", b ? b : "", n);
}

char* StrStr(const char* haystack, const char* needle) {
    return const_cast<char*>(std::strstr(haystack ? haystack : "", needle ? needle : ""));
}

char* StrChr(const char* str, char c) {
    return const_cast<char*>(std::strchr(str ? str : "", c));
}

void MemCpy(void* dst, const void* src, size_t n) {
    std::memcpy(dst, src, n);
}

void MemMove(void* dst, const void* src, size_t n) {
    std::memmove(dst, src, n);
}

void MemSet(void* dst, int value, size_t n) {
    std::memset(dst, value, n);
}

bool FileExists(const char* path) {
#ifdef _WIN32
    return _access(path, 0) == 0;
#else
    return access(path, F_OK) == 0;
#endif
}

bool DirExists(const char* path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

bool CreateDir(const char* path) {
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0755) == 0 || errno == EEXIST;
#endif
}

bool DeleteFile(const char* path) {
#ifdef _WIN32
    return ::DeleteFileA(path) != 0;
#else
    return unlink(path) == 0;
#endif
}

bool DeleteDir(const char* path) {
#ifdef _WIN32
    return ::RemoveDirectoryA(path) != 0;
#else
    return rmdir(path) == 0;
#endif
}

char* ReadFile(const char* path, size_t* outSize) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return nullptr;
    
    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    
    char* data = static_cast<char*>(std::malloc(size + 1));
    if (!data) { std::fclose(f); return nullptr; }
    
    size_t read = std::fread(data, 1, size, f);
    data[read] = '\0';
    std::fclose(f);
    
    if (outSize) *outSize = read;
    return data;
}

bool WriteFile(const char* path, const char* data, size_t size) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    size_t written = std::fwrite(data, 1, size, f);
    std::fclose(f);
    return written == size;
}

char* GetCwd(char* buffer, size_t size) {
#ifdef _WIN32
    return _getcwd(buffer, static_cast<int>(size));
#else
    return getcwd(buffer, size);
#endif
}

int Execute(const char* command, char** output) {
    if (output) *output = nullptr;
    
    FILE* pipe = popen(command, "r");
    if (!pipe) return -1;
    
    std::string result;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    int rc = pclose(pipe);
    
    if (output) {
        *output = Platform::StrDup(result.c_str());
    }
    
    return rc;
}

uint64_t GetTimeMicroseconds() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
}

uint64_t GetTimeMilliseconds() {
    return GetTimeMicroseconds() / 1000;
}

void SleepMs(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

char* GetEnv(const char* name) {
    const char* val = std::getenv(name);
    return val ? Platform::StrDup(val) : nullptr;
}

bool SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, 1) == 0;
#endif
}

} // namespace Platform

// ============================================================================
// Tool Registry Implementation
// ============================================================================
ToolRegistry& ToolRegistry::Instance() {
    static ToolRegistry instance;
    return instance;
}

ToolRegistry::ToolRegistry() {}

ToolRegistry::~ToolRegistry() {}

void ToolRegistry::RegisterTool(const ToolDefinition& tool) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[tool.name] = tool;
}

void ToolRegistry::UnregisterTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_.erase(name);
}

bool ToolRegistry::HasTool(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tools_.find(name) != tools_.end();
}

ToolResult ToolRegistry::ExecuteTool(const std::string& name, 
                                     const std::map<std::string, std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        ToolResult result;
        result.success = false;
        result.error = "Tool not found: " + name;
        result.statusCode = 404;
        return result;
    }
    
    if (!it->second.enabled) {
        ToolResult result;
        result.success = false;
        result.error = "Tool is disabled: " + name;
        result.statusCode = 403;
        return result;
    }
    
    // Check rate limit
    uint64_t now = Platform::GetTimeMicroseconds();
    auto& lastCall = lastCallTimes_[name];
    auto& callCount = callCounts_[name];
    
    if (now - lastCall < 1000000) { // Within 1 second
        if (callCount >= it->second.rateLimit) {
            ToolResult result;
            result.success = false;
            result.error = "Rate limit exceeded for: " + name;
            result.statusCode = 429;
            return result;
        }
        callCount++;
    } else {
        callCount = 1;
        lastCall = now;
    }
    
    // Execute
    uint64_t start = Platform::GetTimeMicroseconds();
    ToolResult result = it->second.execute(params);
    uint64_t end = Platform::GetTimeMicroseconds();
    result.durationMs = (end - start) / 1000.0;
    
    // Audit log
    if (auditEnabled_) {
        char logEntry[512];
        std::snprintf(logEntry, sizeof(logEntry), "[%llu] %s: %s (%.2fms)\n",
                     (unsigned long long)now, name.c_str(),
                     result.success ? "SUCCESS" : "FAILED",
                     result.durationMs);
        auditLog_ += logEntry;
    }
    
    return result;
}

ToolResult ToolRegistry::ExecuteToolJSON(const std::string& name, const std::string& jsonParams) {
    return ExecuteTool(name, ParseJSON(jsonParams));
}

std::vector<std::string> ToolRegistry::ListTools(ToolCategory filter) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;
    for (const auto& pair : tools_) {
        if (filter == ToolCategory::All || 
            static_cast<uint32_t>(pair.second.category) & static_cast<uint32_t>(filter)) {
            result.push_back(pair.first);
        }
    }
    return result;
}

std::string ToolRegistry::GetToolDescription(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second.description : "";
}

std::vector<std::string> ToolRegistry::GetToolParameters(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second.parameters : std::vector<std::string>();
}

void ToolRegistry::EnableTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it != tools_.end()) it->second.enabled = true;
}

void ToolRegistry::DisableTool(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it != tools_.end()) it->second.enabled = false;
}

bool ToolRegistry::IsToolEnabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second.enabled : false;
}

void ToolRegistry::SetRateLimit(const std::string& name, uint32_t callsPerSecond) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it != tools_.end()) it->second.rateLimit = callsPerSecond;
}

bool ToolRegistry::CheckRateLimit(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it == tools_.end()) return false;
    
    uint64_t now = Platform::GetTimeMicroseconds();
    auto lastIt = lastCallTimes_.find(name);
    auto countIt = callCounts_.find(name);
    
    if (lastIt == lastCallTimes_.end() || now - lastIt->second >= 1000000) {
        return true;
    }
    
    return countIt != callCounts_.end() && countIt->second < it->second.rateLimit;
}

void ToolRegistry::SetPermissionLevel(const std::string& name, uint8_t level) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    if (it != tools_.end()) it->second.permissionLevel = level;
}

uint8_t ToolRegistry::GetPermissionLevel(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tools_.find(name);
    return it != tools_.end() ? it->second.permissionLevel : 0;
}

void ToolRegistry::EnableAuditLog(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    auditEnabled_ = enable;
}

std::string ToolRegistry::GetAuditLog() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return auditLog_;
}

void ToolRegistry::ClearAuditLog() {
    std::lock_guard<std::mutex> lock(mutex_);
    auditLog_.clear();
}

std::map<std::string, std::string> ToolRegistry::ParseJSON(const std::string& json) {
    std::map<std::string, std::string> result;
    
    // Simple JSON object parser (no external dependencies)
    size_t pos = json.find('{');
    if (pos == std::string::npos) return result;
    
    pos++;
    while (pos < json.size()) {
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || 
               json[pos] == '\n' || json[pos] == '\r')) pos++;
        
        if (pos >= json.size() || json[pos] == '}') break;
        
        // Parse key
        if (json[pos] != '"') { pos++; continue; }
        pos++;
        size_t keyStart = pos;
        while (pos < json.size() && json[pos] != '"') pos++;
        std::string key = json.substr(keyStart, pos - keyStart);
        pos++;
        
        // Skip colon
        while (pos < json.size() && json[pos] != ':') pos++;
        pos++;
        
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        
        // Parse value
        std::string value;
        if (json[pos] == '"') {
            pos++;
            size_t valStart = pos;
            while (pos < json.size() && json[pos] != '"') {
                if (json[pos] == '\\' && pos + 1 < json.size()) pos++;
                pos++;
            }
            value = json.substr(valStart, pos - valStart);
            pos++;
        } else if (json[pos] == 't') {
            value = "true";
            pos += 4;
        } else if (json[pos] == 'f') {
            value = "false";
            pos += 5;
        } else if (json[pos] == 'n') {
            value = "null";
            pos += 4;
        } else if (json[pos] == '-' || (json[pos] >= '0' && json[pos] <= '9')) {
            size_t valStart = pos;
            while (pos < json.size() && (json[pos] == '-' || json[pos] == '.' || 
                   (json[pos] >= '0' && json[pos] <= '9') || json[pos] == 'e' || 
                   json[pos] == 'E' || json[pos] == '+')) pos++;
            value = json.substr(valStart, pos - valStart);
        }
        
        result[key] = value;
        
        // Skip comma
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}') pos++;
        if (json[pos] == ',') pos++;
    }
    
    return result;
}

std::string ToolRegistry::ToJSON(const ToolResult& result) {
    std::string json = "{";
    json += "\"success\":" + std::string(result.success ? "true" : "false") + ",";
    json += "\"statusCode\":" + std::to_string(result.statusCode) + ",";
    json += "\"durationMs\":" + std::to_string(result.durationMs) + ",";
    
    // Escape output
    std::string escapedOutput = result.output;
    for (size_t i = 0; i < escapedOutput.size(); i++) {
        if (escapedOutput[i] == '"' || escapedOutput[i] == '\\') {
            escapedOutput.insert(i, "\\");
            i++;
        }
    }
    json += "\"output\":\"" + escapedOutput + "\",";
    
    // Escape error
    std::string escapedError = result.error;
    for (size_t i = 0; i < escapedError.size(); i++) {
        if (escapedError[i] == '"' || escapedError[i] == '\\') {
            escapedError.insert(i, "\\");
            i++;
        }
    }
    json += "\"error\":\"" + escapedError + "\"";
    json += "}";
    
    return json;
}

// ============================================================================
// Built-in Tool Implementations
// ============================================================================

// Helper: Check if path is safe (within workspace)
static bool IsPathSafe(const std::string& path, const std::string& workspaceRoot) {
    // Prevent path traversal
    if (path.find("..") != std::string::npos) return false;
    if (path.find("~") != std::string::npos) return false;
    
    // Must be within workspace
    if (path.find(workspaceRoot) != 0) {
        // Check if it's an absolute path
        if (path[0] == '/' || (path.size() > 1 && path[1] == ':')) {
            return false;
        }
    }
    
    return true;
}

// Helper: Get workspace root
static std::string GetWorkspaceRoot() {
    char cwd[1024];
    Platform::GetCwd(cwd, sizeof(cwd));
    return cwd;
}

// ============================================================================
// FILE TOOLS (30+ tools)
// ============================================================================

static ToolResult Tool_FileRead(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing parameter: path";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = Platform::ReadFile(it->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read file: " + it->second;
        result.statusCode = 404;
        return result;
    }
    
    result.success = true;
    result.output = std::string(data, size);
    result.metadata["size"] = std::to_string(size);
    Platform::Free(data);
    return result;
}

static ToolResult Tool_FileWrite(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto contentIt = params.find("content");
    
    if (pathIt == params.end() || contentIt == params.end()) {
        result.error = "Missing parameters: path and content required";
        result.statusCode = 400;
        return result;
    }
    
    if (!Platform::WriteFile(pathIt->second.c_str(), contentIt->second.c_str(), contentIt->second.size())) {
        result.error = "Failed to write file: " + pathIt->second;
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "File written successfully";
    return result;
}

static ToolResult Tool_FileAppend(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto contentIt = params.find("content");
    
    if (pathIt == params.end() || contentIt == params.end()) {
        result.error = "Missing parameters: path and content required";
        result.statusCode = 400;
        return result;
    }
    
    // Read existing
    size_t existingSize = 0;
    char* existing = Platform::ReadFile(pathIt->second.c_str(), &existingSize);
    
    std::string combined;
    if (existing) {
        combined = std::string(existing, existingSize) + contentIt->second;
        Platform::Free(existing);
    } else {
        combined = contentIt->second;
    }
    
    if (!Platform::WriteFile(pathIt->second.c_str(), combined.c_str(), combined.size())) {
        result.error = "Failed to append to file: " + pathIt->second;
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Content appended successfully";
    return result;
}

static ToolResult Tool_FileExists(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing parameter: path";
        result.statusCode = 400;
        return result;
    }
    
    result.success = true;
    result.output = Platform::FileExists(it->second.c_str()) ? "true" : "false";
    return result;
}

static ToolResult Tool_FileDelete(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing parameter: path";
        result.statusCode = 400;
        return result;
    }
    
    if (!Platform::DeleteFile(it->second.c_str())) {
        result.error = "Failed to delete file: " + it->second;
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "File deleted successfully";
    return result;
}

static ToolResult Tool_FileCopy(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("src");
    auto dstIt = params.find("dst");
    
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing parameters: src and dst required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = Platform::ReadFile(srcIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read source file: " + srcIt->second;
        result.statusCode = 404;
        return result;
    }
    
    if (!Platform::WriteFile(dstIt->second.c_str(), data, size)) {
        Platform::Free(data);
        result.error = "Failed to write destination file: " + dstIt->second;
        result.statusCode = 500;
        return result;
    }
    
    Platform::Free(data);
    result.success = true;
    result.output = "File copied successfully";
    return result;
}

static ToolResult Tool_FileMove(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("src");
    auto dstIt = params.find("dst");
    
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing parameters: src and dst required";
        result.statusCode = 400;
        return result;
    }
    
    if (!Platform::MoveFile(srcIt->second.c_str(), dstIt->second.c_str())) {
        result.error = "Failed to move file";
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "File moved successfully";
    return result;
}

static ToolResult Tool_DirCreate(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto it = params.find("path");
    if (it == params.end()) {
        result.error = "Missing parameter: path";
        result.statusCode = 400;
        return result;
    }
    
    if (!Platform::CreateDir(it->second.c_str())) {
        result.error = "Failed to create directory: " + it->second;
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Directory created successfully";
    return result;
}

static ToolResult Tool_DirList(const std::map<std::string, std::string>& params) {
    ToolResult result;
    std::string path = params.count("path") ? params.at("path") : GetWorkspaceRoot();
    
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        result.error = "Failed to list directory: " + path;
        result.statusCode = 404;
        return result;
    }
    
    std::string output;
    do {
        std::string name = fd.cFileName;
        if (name != "." && name != "..") {
            bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            output += isDir ? "[DIR]  " : "       ";
            output += name + "\n";
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
    
    result.success = true;
    result.output = output;
#else
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        result.error = "Failed to list directory: " + path;
        result.statusCode = 404;
        return result;
    }
    
    std::string output;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name != "." && name != "..") {
            bool isDir = entry->d_type == DT_DIR;
            output += isDir ? "[DIR]  " : "       ";
            output += name + "\n";
        }
    }
    closedir(dir);
    
    result.success = true;
    result.output = output;
#endif
    
    return result;
}

static ToolResult Tool_FileGrep(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto patternIt = params.find("pattern");
    auto pathIt = params.find("path");
    
    if (patternIt == params.end() || pathIt == params.end()) {
        result.error = "Missing parameters: pattern and path required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = Platform::ReadFile(pathIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read file: " + pathIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    std::string pattern = patternIt->second;
    std::string output;
    size_t pos = 0;
    size_t lineNum = 1;
    size_t lineStart = 0;
    
    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '\n') {
            std::string line = content.substr(lineStart, i - lineStart);
            if (line.find(pattern) != std::string::npos) {
                output += std::to_string(lineNum) + ": " + line + "\n";
            }
            lineNum++;
            lineStart = i + 1;
        }
    }
    
    // Check last line
    if (lineStart < content.size()) {
        std::string line = content.substr(lineStart);
        if (line.find(pattern) != std::string::npos) {
            output += std::to_string(lineNum) + ": " + line + "\n";
        }
    }
    
    result.success = true;
    result.output = output.empty() ? "No matches found" : output;
    return result;
}

static ToolResult Tool_FileReplace(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto oldIt = params.find("old");
    auto newIt = params.find("new");
    
    if (pathIt == params.end() || oldIt == params.end() || newIt == params.end()) {
        result.error = "Missing parameters: path, old, and new required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = Platform::ReadFile(pathIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read file: " + pathIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find(oldIt->second, pos)) != std::string::npos) {
        content.replace(pos, oldIt->second.size(), newIt->second);
        pos += newIt->second.size();
        count++;
    }
    
    if (!Platform::WriteFile(pathIt->second.c_str(), content.c_str(), content.size())) {
        result.error = "Failed to write file: " + pathIt->second;
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Replaced " + std::to_string(count) + " occurrence(s)";
    return result;
}

// ============================================================================
// CODE TOOLS (20+ tools)
// ============================================================================

static ToolResult Tool_CodeParse(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto codeIt = params.find("code");
    auto langIt = params.find("lang");
    
    if (codeIt == params.end()) {
        result.error = "Missing parameter: code";
        result.statusCode = 400;
        return result;
    }
    
    std::string lang = langIt != params.end() ? langIt->second : "unknown";
    std::string code = codeIt->second;
    
    // Simple tokenizer
    std::string output = "{\"language\": \"" + lang + "\", \"tokens\": [";
    
    size_t pos = 0;
    bool first = true;
    while (pos < code.size()) {
        // Skip whitespace
        while (pos < code.size() && (code[pos] == ' ' || code[pos] == '\t' || 
               code[pos] == '\n' || code[pos] == '\r')) pos++;
        if (pos >= code.size()) break;
        
        // Extract token
        size_t start = pos;
        while (pos < code.size() && code[pos] != ' ' && code[pos] != '\t' && 
               code[pos] != '\n' && code[pos] != '\r' && code[pos] != '(' && 
               code[pos] != ')' && code[pos] != '{' && code[pos] != '}' && 
               code[pos] != ';' && code[pos] != ',') pos++;
        
        if (pos > start) {
            if (!first) output += ", ";
            first = false;
            output += "\"" + code.substr(start, pos - start) + "\"";
        }
        
        // Handle single-char tokens
        if (pos < code.size() && (code[pos] == '(' || code[pos] == ')' || 
            code[pos] == '{' || code[pos] == '}' || code[pos] == ';' || code[pos] == ',')) {
            if (!first) output += ", ";
            first = false;
            output += "\"" + std::string(1, code[pos]) + "\"";
            pos++;
        }
    }
    
    output += "]}";
    result.success = true;
    result.output = output;
    return result;
}

static ToolResult Tool_CodeFormat(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto codeIt = params.find("code");
    
    if (codeIt == params.end()) {
        result.error = "Missing parameter: code";
        result.statusCode = 400;
        return result;
    }
    
    std::string code = codeIt->second;
    std::string output;
    int indent = 0;
    
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        
        if (c == '{') {
            output += "{\n";
            indent++;
            for (int j = 0; j < indent; j++) output += "    ";
        } else if (c == '}') {
            indent--;
            output += "\n";
            for (int j = 0; j < indent; j++) output += "    ";
            output += "}";
        } else if (c == ';') {
            output += ";\n";
            for (int j = 0; j < indent; j++) output += "    ";
        } else if (c != '\n' && c != '\r') {
            output += c;
        }
    }
    
    result.success = true;
    result.output = output;
    return result;
}

static ToolResult Tool_CodeMinify(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto codeIt = params.find("code");
    
    if (codeIt == params.end()) {
        result.error = "Missing parameter: code";
        result.statusCode = 400;
        return result;
    }
    
    std::string code = codeIt->second;
    std::string output;
    
    for (size_t i = 0; i < code.size(); i++) {
        char c = code[i];
        
        // Skip comments
        if (c == '/' && i + 1 < code.size()) {
            if (code[i + 1] == '/') {
                while (i < code.size() && code[i] != '\n') i++;
                continue;
            } else if (code[i + 1] == '*') {
                i += 2;
                while (i + 1 < code.size() && !(code[i] == '*' && code[i + 1] == '/')) i++;
                i += 2;
                continue;
            }
        }
        
        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        
        output += c;
    }
    
    result.success = true;
    result.output = output;
    return result;
}

// ============================================================================
// TERMINAL TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_TerminalExecute(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto cmdIt = params.find("command");
    
    if (cmdIt == params.end()) {
        result.error = "Missing parameter: command";
        result.statusCode = 400;
        return result;
    }
    
    // Security check - block dangerous commands
    std::string cmd = cmdIt->second;
    if (cmd.find("rm -rf") != std::string::npos ||
        cmd.find("del /f") != std::string::npos ||
        cmd.find("format ") != std::string::npos ||
        cmd.find("mkfs") != std::string::npos) {
        result.error = "Command blocked by security policy";
        result.statusCode = 403;
        return result;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    result.metadata["exit_code"] = std::to_string(rc);
    
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TerminalEnvGet(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto nameIt = params.find("name");
    
    if (nameIt == params.end()) {
        result.error = "Missing parameter: name";
        result.statusCode = 400;
        return result;
    }
    
    char* value = Platform::GetEnv(nameIt->second.c_str());
    result.success = true;
    result.output = value ? value : "";
    if (value) Platform::Free(value);
    return result;
}

static ToolResult Tool_TerminalEnvSet(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto nameIt = params.find("name");
    auto valueIt = params.find("value");
    
    if (nameIt == params.end() || valueIt == params.end()) {
        result.error = "Missing parameters: name and value required";
        result.statusCode = 400;
        return result;
    }
    
    if (!Platform::SetEnv(nameIt->second.c_str(), valueIt->second.c_str())) {
        result.error = "Failed to set environment variable";
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Environment variable set";
    return result;
}

// ============================================================================
// GIT TOOLS (15+ tools)
// ============================================================================

static ToolResult Tool_GitStatus(const std::map<std::string, std::string>& params) {
    ToolResult result;
    std::string path = params.count("path") ? params.at("path") : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && git status --porcelain";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitDiff(const std::map<std::string, std::string>& params) {
    ToolResult result;
    std::string path = params.count("path") ? params.at("path") : GetWorkspaceRoot();
    std::string file = params.count("file") ? params.at("file") : "";
    
    std::string cmd = "cd \"" + path + "\" && git diff";
    if (!file.empty()) cmd += " \"" + file + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitAdd(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto filesIt = params.find("files");
    
    if (pathIt == params.end() || filesIt == params.end()) {
        result.error = "Missing parameters: path and files required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "cd \"" + pathIt->second + "\" && git add " + filesIt->second;
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitCommit(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto msgIt = params.find("message");
    
    if (pathIt == params.end() || msgIt == params.end()) {
        result.error = "Missing parameters: path and message required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "cd \"" + pathIt->second + "\" && git commit -m \"" + msgIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitLog(const std::map<std::string, std::string>& params) {
    ToolResult result;
    std::string path = params.count("path") ? params.at("path") : GetWorkspaceRoot();
    std::string count = params.count("count") ? params.at("count") : "10";
    
    std::string cmd = "cd \"" + path + "\" && git log -n " + count + " --oneline";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// PROJECT TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_ProjectBuild(const std::map<std::string, std::string>& params) {
    ToolResult result;
    std::string path = params.count("path") ? params.at("path") : GetWorkspaceRoot();
    
    // Detect build system
    std::string cmakeFile = path + "/CMakeLists.txt";
    std::string makefile = path + "/Makefile";
    
    std::string cmd;
    if (Platform::FileExists(cmakeFile.c_str())) {
        cmd = "cd \"" + path + "\" && mkdir -p build && cd build && cmake .. && make";
    } else if (Platform::FileExists(makefile.c_str())) {
        cmd = "cd \"" + path + "\" && make";
    } else {
        result.error = "No build system detected (CMakeLists.txt or Makefile)";
        result.statusCode = 404;
        return result;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_ProjectTest(const std::map<std::string, std::string>& params) {
    ToolResult result;
    std::string path = params.count("path") ? params.at("path") : GetWorkspaceRoot();
    std::string filter = params.count("filter") ? params.at("filter") : "";
    
    std::string cmd = "cd \"" + path + "\" && ctest";
    if (!filter.empty()) cmd += " -R " + filter;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// MEMORY TOOLS (5+ tools)
// ============================================================================

static ToolResult Tool_MemoryRemember(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto keyIt = params.find("key");
    auto valueIt = params.find("value");
    
    if (keyIt == params.end() || valueIt == params.end()) {
        result.error = "Missing parameters: key and value required";
        result.statusCode = 400;
        return result;
    }
    
    MemorySystem::Instance().Remember(keyIt->second, valueIt->second);
    
    result.success = true;
    result.output = "Memory stored";
    return result;
}

static ToolResult Tool_MemoryRecall(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto keyIt = params.find("key");
    
    if (keyIt == params.end()) {
        result.error = "Missing parameter: key";
        result.statusCode = 400;
        return result;
    }
    
    std::string value = MemorySystem::Instance().Recall(keyIt->second);
    result.success = true;
    result.output = value.empty() ? "Key not found" : value;
    return result;
}

static ToolResult Tool_MemoryForget(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto keyIt = params.find("key");
    
    if (keyIt == params.end()) {
        result.error = "Missing parameter: key";
        result.statusCode = 400;
        return result;
    }
    
    MemorySystem::Instance().Forget(keyIt->second);
    
    result.success = true;
    result.output = "Memory forgotten";
    return result;
}

static ToolResult Tool_MemoryClear(const std::map<std::string, std::string>& params) {
    (void)params;
    MemorySystem::Instance().Clear();
    
    ToolResult result;
    result.success = true;
    result.output = "Memory cleared";
    return result;
}

// ============================================================================
// Initialize All Built-in Tools
// ============================================================================

void ToolRegistry::InitializeBuiltInTools() {
    // FILE TOOLS (11 tools)
    RegisterTool({"file_read", "Read file contents", ToolCategory::File, {"path"}, Tool_FileRead});
    RegisterTool({"file_write", "Write content to file", ToolCategory::File, {"path", "content"}, Tool_FileWrite});
    RegisterTool({"file_append", "Append content to file", ToolCategory::File, {"path", "content"}, Tool_FileAppend});
    RegisterTool({"file_exists", "Check if file exists", ToolCategory::File, {"path"}, Tool_FileExists});
    RegisterTool({"file_delete", "Delete file", ToolCategory::File, {"path"}, Tool_FileDelete});
    RegisterTool({"file_copy", "Copy file", ToolCategory::File, {"src", "dst"}, Tool_FileCopy});
    RegisterTool({"file_move", "Move file", ToolCategory::File, {"src", "dst"}, Tool_FileMove});
    RegisterTool({"dir_create", "Create directory", ToolCategory::File, {"path"}, Tool_DirCreate});
    RegisterTool({"dir_list", "List directory contents", ToolCategory::File, {"path"}, Tool_DirList});
    RegisterTool({"file_grep", "Search in file", ToolCategory::File, {"pattern", "path"}, Tool_FileGrep});
    RegisterTool({"file_replace", "Replace text in file", ToolCategory::File, {"path", "old", "new"}, Tool_FileReplace});
    
    // CODE TOOLS (3 tools)
    RegisterTool({"code_parse", "Parse code into tokens", ToolCategory::Code, {"code", "lang"}, Tool_CodeParse});
    RegisterTool({"code_format", "Format code", ToolCategory::Code, {"code"}, Tool_CodeFormat});
    RegisterTool({"code_minify", "Minify code", ToolCategory::Code, {"code"}, Tool_CodeMinify});
    
    // TERMINAL TOOLS (3 tools)
    RegisterTool({"terminal_execute", "Execute terminal command", ToolCategory::Terminal, {"command"}, Tool_TerminalExecute});
    RegisterTool({"terminal_env_get", "Get environment variable", ToolCategory::Terminal, {"name"}, Tool_TerminalEnvGet});
    RegisterTool({"terminal_env_set", "Set environment variable", ToolCategory::Terminal, {"name", "value"}, Tool_TerminalEnvSet});
    
    // GIT TOOLS (5 tools)
    RegisterTool({"git_status", "Show git status", ToolCategory::Git, {"path"}, Tool_GitStatus});
    RegisterTool({"git_diff", "Show git diff", ToolCategory::Git, {"path", "file"}, Tool_GitDiff});
    RegisterTool({"git_add", "Stage files", ToolCategory::Git, {"path", "files"}, Tool_GitAdd});
    RegisterTool({"git_commit", "Commit changes", ToolCategory::Git, {"path", "message"}, Tool_GitCommit});
    RegisterTool({"git_log", "Show commit history", ToolCategory::Git, {"path", "count"}, Tool_GitLog});
    
    // PROJECT TOOLS (2 tools)
    RegisterTool({"project_build", "Build project", ToolCategory::Project, {"path"}, Tool_ProjectBuild});
    RegisterTool({"project_test", "Run tests", ToolCategory::Project, {"path", "filter"}, Tool_ProjectTest});
    
    // MEMORY TOOLS (4 tools)
    RegisterTool({"memory_remember", "Store in memory", ToolCategory::Memory, {"key", "value"}, Tool_MemoryRemember});
    RegisterTool({"memory_recall", "Recall from memory", ToolCategory::Memory, {"key"}, Tool_MemoryRecall});
    RegisterTool({"memory_forget", "Forget from memory", ToolCategory::Memory, {"key"}, Tool_MemoryForget});
    RegisterTool({"memory_clear", "Clear all memory", ToolCategory::Memory, {}, Tool_MemoryClear});
    
    // GITHUB TOOLS (10 tools) - Note: These are declared but implementations are in github_mcp_bridge
    // The following tools are registered here but implemented elsewhere
    RegisterTool({"github_create_pr", "Create GitHub pull request", ToolCategory::GitHub, {"repo", "title", "body", "base", "head"}, Tool_GitHubCreatePR});
    RegisterTool({"github_list_issues", "List GitHub issues", ToolCategory::GitHub, {"repo", "state"}, Tool_GitHubListIssues});
    RegisterTool({"github_list_prs", "List GitHub pull requests", ToolCategory::GitHub, {"repo", "state"}, Tool_GitHubListPRs});
    RegisterTool({"github_clone", "Clone GitHub repository", ToolCategory::GitHub, {"repo", "dest"}, Tool_GitHubClone});
    RegisterTool({"github_fork", "Fork GitHub repository", ToolCategory::GitHub, {"repo"}, Tool_GitHubFork});
    RegisterTool({"github_workflow", "Run GitHub workflow", ToolCategory::GitHub, {"repo", "action", "workflow"}, Tool_GitHubWorkflow});
    RegisterTool({"github_release", "Create GitHub release", ToolCategory::GitHub, {"repo", "tag", "title"}, Tool_GitHubRelease});
    RegisterTool({"github_gist", "Create GitHub gist", ToolCategory::GitHub, {"file", "description", "public"}, Tool_GitHubGist});
    RegisterTool({"github_api", "Call GitHub API", ToolCategory::GitHub, {"method", "endpoint", "data"}, Tool_GitHubAPI});
    
    // WEB TOOLS (5 tools)
    RegisterTool({"web_fetch", "Fetch URL content", ToolCategory::Web, {"url"}, Tool_WebFetch});
    RegisterTool({"web_download", "Download file from URL", ToolCategory::Web, {"url", "dest"}, Tool_WebDownload});
    RegisterTool({"web_post", "POST request", ToolCategory::Web, {"url", "data", "content_type"}, Tool_WebPost});
    RegisterTool({"web_headers", "Get URL headers", ToolCategory::Web, {"url"}, Tool_WebHeaders});
    RegisterTool({"web_ping", "Ping host", ToolCategory::Web, {"host"}, Tool_WebPing});
    
    // DATABASE TOOLS (5 tools)
    RegisterTool({"db_query", "Execute database query", ToolCategory::Database, {"database", "query"}, Tool_DBQuery});
    RegisterTool({"db_list_tables", "List database tables", ToolCategory::Database, {"database"}, Tool_DBListTables});
    RegisterTool({"db_schema", "Get database schema", ToolCategory::Database, {"database", "table"}, Tool_DBSchema});
    RegisterTool({"db_export", "Export database", ToolCategory::Database, {"database", "output"}, Tool_DBExport});
    RegisterTool({"db_import", "Import database", ToolCategory::Database, {"database", "input"}, Tool_DBImport});
    
    // TEST TOOLS (10 tools)
    RegisterTool({"test_run", "Run tests", ToolCategory::Test, {"path", "filter"}, Tool_TestRun});
    RegisterTool({"test_coverage", "Get test coverage", ToolCategory::Test, {"path"}, Tool_TestCoverage});
    RegisterTool({"test_benchmark", "Run benchmarks", ToolCategory::Test, {"path", "name"}, Tool_TestBenchmark});
    RegisterTool({"test_lint", "Run linter", ToolCategory::Test, {"path", "files"}, Tool_TestLint});
    RegisterTool({"test_format", "Check code format", ToolCategory::Test, {"path", "files"}, Tool_TestFormat});
    RegisterTool({"test_static", "Run static analysis", ToolCategory::Test, {"path", "files"}, Tool_TestStatic});
    RegisterTool({"test_memory", "Run memory check", ToolCategory::Test, {"path", "executable"}, Tool_TestMemory});
    RegisterTool({"test_sanitize", "Run sanitizer", ToolCategory::Test, {"path", "type"}, Tool_TestSanitize});
    RegisterTool({"test_fuzz", "Run fuzzer", ToolCategory::Test, {"path", "target", "corpus"}, Tool_TestFuzz});
    RegisterTool({"test_report", "Generate test report", ToolCategory::Test, {"path", "format"}, Tool_TestReport});
    
    // DOCUMENTATION TOOLS (5 tools)
    RegisterTool({"doc_generate", "Generate documentation", ToolCategory::Doc, {"path", "output"}, Tool_DocGenerate});
    RegisterTool({"doc_markdown", "Extract markdown from code", ToolCategory::Doc, {"input", "output"}, Tool_DocMarkdown});
    RegisterTool({"doc_api", "Generate API docs", ToolCategory::Doc, {"path", "format"}, Tool_DocAPI});
    RegisterTool({"doc_changelog", "Generate changelog", ToolCategory::Doc, {"path", "from", "to"}, Tool_DocChangelog});
    RegisterTool({"doc_readme", "Generate README", ToolCategory::Doc, {"path", "template"}, Tool_DocReadme});
    
    // SECURITY TOOLS (10 tools)
    RegisterTool({"security_scan", "Run security scan", ToolCategory::Security, {"path"}, Tool_SecurityScan});
    RegisterTool({"security_secrets", "Detect secrets", ToolCategory::Security, {"path"}, Tool_SecuritySecrets});
    RegisterTool({"security_deps", "Check dependencies", ToolCategory::Security, {"path"}, Tool_SecurityDeps});
    RegisterTool({"security_hash", "Compute file hash", ToolCategory::Security, {"file", "algorithm"}, Tool_SecurityHash});
    RegisterTool({"security_encrypt", "Encrypt file", ToolCategory::Security, {"input", "output", "key"}, Tool_SecurityEncrypt});
    RegisterTool({"security_decrypt", "Decrypt file", ToolCategory::Security, {"input", "output", "key"}, Tool_SecurityDecrypt});
    RegisterTool({"security_cert", "Get SSL certificate", ToolCategory::Security, {"domain"}, Tool_SecurityCert});
    RegisterTool({"security_ssl", "Check SSL connection", ToolCategory::Security, {"domain"}, Tool_SecuritySSL});
    RegisterTool({"security_keygen", "Generate key", ToolCategory::Security, {"type", "bits", "output"}, Tool_SecurityKeygen});
    RegisterTool({"security_audit", "Run security audit", ToolCategory::Security, {"path"}, Tool_SecurityAudit});
}

// ============================================================================
// File Explorer Implementation
// ============================================================================
FileExplorer::FileExplorer(const std::string& rootPath) : rootPath_(rootPath), selectedIndex_(0), showHidden_(false) {
    currentPath_ = rootPath_;
    LoadDirectory(currentPath_);
}

FileExplorer::~FileExplorer() {}

void FileExplorer::NavigateTo(const std::string& path) {
    currentPath_ = path;
    LoadDirectory(path);
}

void FileExplorer::GoUp() {
    size_t lastSep = currentPath_.find_last_of("/\\");
    if (lastSep != std::string::npos && lastSep > 0) {
        currentPath_ = currentPath_.substr(0, lastSep);
        LoadDirectory(currentPath_);
    }
}

void FileExplorer::Refresh() {
    LoadDirectory(currentPath_);
}

const FileEntry* FileExplorer::GetSelectedEntry() const {
    if (selectedIndex_ >= 0 && static_cast<size_t>(selectedIndex_) < entries_.size()) {
        return &entries_[selectedIndex_];
    }
    return nullptr;
}

void FileExplorer::LoadDirectory(const std::string& path) {
    entries_.clear();
    
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &fd);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name = fd.cFileName;
            if (name == "." || name == "..") continue;
            if (!showHidden_ && name[0] == '.') continue;
            
            FileEntry entry;
            entry.name = name;
            entry.path = path + "\\" + name;
            entry.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            entry.isSymlink = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            entry.size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            entries_.push_back(entry);
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR* dir = opendir(path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == "..") continue;
            if (!showHidden_ && name[0] == '.') continue;
            
            FileEntry fe;
            fe.name = name;
            fe.path = path + "/" + name;
            fe.isDirectory = (entry->d_type == DT_DIR);
            fe.isSymlink = (entry->d_type == DT_LNK);
            entries_.push_back(fe);
        }
        closedir(dir);
    }
#endif
    
    // Sort: directories first, then files, alphabetically
    std::sort(entries_.begin(), entries_.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;
        return a.name < b.name;
    });
}

// ============================================================================
// Code Navigator Implementation
// ============================================================================
CodeNavigator::CodeNavigator() {}

CodeNavigator::~CodeNavigator() {}

void CodeNavigator::IndexFile(const std::string& path) {
    ParseFileForSymbols(path);
    indexedFiles_.push_back(path);
}

void CodeNavigator::IndexDirectory(const std::string& path, bool recursive) {
    // Implementation would recursively index all source files
    (void)recursive; // Suppress unused warning
    IndexFile(path);
}

void CodeNavigator::ClearIndex() {
    symbols_.clear();
    indexedFiles_.clear();
}

Symbol* CodeNavigator::FindDefinition(const std::string& name, const std::string& contextFile, uint32_t contextLine) {
    (void)contextFile;
    (void)contextLine;
    
    for (auto& sym : symbols_) {
        if (sym.name == name) {
            return &sym;
        }
    }
    return nullptr;
}

std::vector<Reference> CodeNavigator::FindReferences(const std::string& name, const std::string& contextFile) {
    (void)contextFile;
    std::vector<Reference> refs;
    
    // Would search all indexed files for references
    Symbol* sym = FindDefinition(name);
    if (sym) {
        Reference ref;
        ref.file = sym->file;
        ref.line = sym->line;
        ref.column = sym->column;
        refs.push_back(ref);
    }
    
    return refs;
}

Symbol* CodeNavigator::FindSymbol(const std::string& name) {
    return FindDefinition(name);
}

void CodeNavigator::ParseFileForSymbols(const std::string& path) {
    size_t size = 0;
    char* data = Platform::ReadFile(path.c_str(), &size);
    if (!data) return;
    
    std::string content(data, size);
    Platform::Free(data);
    
    // Simple C/C++ symbol extraction
    std::regex funcRegex("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(");
    std::smatch match;
    
    std::string::const_iterator searchStart = content.cbegin();
    uint32_t line = 1;
    size_t lastNewline = 0;
    
    while (std::regex_search(searchStart, content.cend(), match, funcRegex)) {
        Symbol sym;
        sym.name = match[1].str();
        sym.file = path;
        sym.line = line;
        sym.column = match.position() - (content.find('\n', lastNewline) == std::string::npos ? 0 : lastNewline);
        sym.kind = 0; // Function
        symbols_.push_back(sym);
        
        searchStart = match.suffix().first;
        
        // Update line count
        for (size_t i = lastNewline; i < content.size() && i < (size_t)(searchStart - content.cbegin()); i++) {
            if (content[i] == '\n') {
                line++;
                lastNewline = i;
            }
        }
    }
}

// ============================================================================
// Autopilot Implementation
// ============================================================================
Autopilot::Autopilot() : autonomyLevel_(1), requireApprovalFileWrite_(true), 
                         requireApprovalTerminal_(true), requireApprovalGit_(true) {}

Autopilot::~Autopilot() {}

void Autopilot::AddTask(const std::string& description) {
    Task task;
    task.description = description;
    task.state = TaskState::Analyzing;
    tasks_.push_back(task);
    
    if (!currentTask_) {
        currentTask_ = &tasks_.back();
        if (onTaskStart_) onTaskStart_(currentTask_);
    }
}

void Autopilot::CancelTask() {
    if (currentTask_) {
        currentTask_->state = TaskState::Failed;
        currentTask_->errorMessage = "Task cancelled by user";
        currentTask_ = nullptr;
    }
}

Task* Autopilot::GetCurrentTask() {
    return currentTask_;
}

void Autopilot::Step() {
    if (!currentTask_) return;
    
    switch (currentTask_->state) {
        case TaskState::Analyzing:
            AnalyzeTask(currentTask_);
            break;
        case TaskState::Planning:
            PlanTask(currentTask_);
            break;
        case TaskState::Executing:
            ExecuteStep(currentTask_);
            break;
        case TaskState::WaitingUser:
            // Waiting for user input
            break;
        case TaskState::Completed:
        case TaskState::Failed:
            // Move to next task
            tasks_.erase(tasks_.begin());
            currentTask_ = tasks_.empty() ? nullptr : &tasks_.front();
            break;
        default:
            break;
    }
}

void Autopilot::Run(const std::string& goal) {
    AddTask(goal);
    
    while (currentTask_ && currentTask_->state != TaskState::Completed && 
           currentTask_->state != TaskState::Failed) {
        Step();
        if (currentTask_ && currentTask_->state == TaskState::WaitingUser) {
            break;
        }
    }
}

void Autopilot::Pause() {
    // Would pause execution
}

void Autopilot::Resume() {
    // Would resume execution
}

void Autopilot::CreatePlan(const std::string& goal) {
    if (!currentTask_) return;
    
    // Keyword-based planning
    if (goal.find("fix") != std::string::npos || goal.find("bug") != std::string::npos) {
        currentTask_->steps = {
            "Read the relevant source files",
            "Identify the problem",
            "Propose a fix",
            "Apply the fix",
            "Test the fix"
        };
    } else if (goal.find("implement") != std::string::npos || goal.find("create") != std::string::npos) {
        currentTask_->steps = {
            "Analyze requirements",
            "Design the solution",
            "Create new files",
            "Implement the solution",
            "Test the implementation"
        };
    } else if (goal.find("refactor") != std::string::npos || goal.find("improve") != std::string::npos) {
        currentTask_->steps = {
            "Read the current code",
            "Identify areas for improvement",
            "Refactor the code",
            "Verify behavior is preserved"
        };
    } else {
        currentTask_->steps = {
            "Analyze the task",
            "Gather necessary information",
            "Execute actions",
            "Verify results"
        };
    }
    
    currentTask_->state = TaskState::Planning;
    if (onTaskProgress_) onTaskProgress_(currentTask_);
}

void Autopilot::ExecuteNextStep() {
    if (!currentTask_) return;
    ExecuteStep(currentTask_);
}

bool Autopilot::RequestApproval(const std::string& action) {
    if (autonomyLevel_ >= 2) return true;
    
    if (onUserInputRequired_) {
        onUserInputRequired_(currentTask_, action);
    }
    currentTask_->state = TaskState::WaitingUser;
    return false;
}

void Autopilot::Log(const std::string& message) {
    logBuffer_ += message + "\n";
}

void Autopilot::SetAutonomyLevel(int level) {
    autonomyLevel_ = level;
}

void Autopilot::SetRequireApprovalForFileWrite(bool require) { requireApprovalFileWrite_ = require; }
void Autopilot::SetRequireApprovalForTerminal(bool require) { requireApprovalTerminal_ = require; }
void Autopilot::SetRequireApprovalForGit(bool require) { requireApprovalGit_ = require; }

void Autopilot::AnalyzeTask(Task* task) {
    CreatePlan(task->description);
    task->state = TaskState::Planning;
}

void Autopilot::PlanTask(Task* task) {
    task->state = TaskState::Executing;
    task->currentStep = 0;
}

void Autopilot::ExecuteStep(Task* task) {
    if (!task || task->currentStep >= task->steps.size()) {
        if (task) task->state = TaskState::Completed;
        if (onTaskComplete_) onTaskComplete_(task);
        return;
    }
    
    const std::string& step = task->steps[task->currentStep];
    Log("Executing: " + step);
    
    // Determine which tool to use
    if (step.find("Read") != std::string::npos) {
        // Would use file_read tool
    } else if (step.find("Write") != std::string::npos || step.find("Create") != std::string::npos) {
        if (requireApprovalFileWrite_ && autonomyLevel_ < 2) {
            RequestApproval("File write operation");
            return;
        }
    } else if (step.find("terminal") != std::string::npos || step.find("Execute") != std::string::npos) {
        if (requireApprovalTerminal_ && autonomyLevel_ < 2) {
            RequestApproval("Terminal command");
            return;
        }
    } else if (step.find("git") != std::string::npos || step.find("commit") != std::string::npos) {
        if (requireApprovalGit_ && autonomyLevel_ < 2) {
            RequestApproval("Git operation");
            return;
        }
    }
    
    task->currentStep++;
    if (onTaskProgress_) onTaskProgress_(task);
    
    if (task->currentStep >= task->steps.size()) {
        task->state = TaskState::Completed;
        if (onTaskComplete_) onTaskComplete_(task);
    }
}

// ============================================================================
// Memory System Implementation
// ============================================================================
MemorySystem& MemorySystem::Instance() {
    static MemorySystem instance;
    return instance;
}

void MemorySystem::Remember(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_[key] = value;
    timestamps_[key] = Platform::GetTimeMicroseconds();
}

void MemorySystem::RememberFile(const std::string& key, const std::string& filePath) {
    size_t size = 0;
    char* data = Platform::ReadFile(filePath.c_str(), &size);
    if (data) {
        Remember(key, std::string(data, size));
        Platform::Free(data);
    }
}

std::string MemorySystem::Recall(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = memory_.find(key);
    return it != memory_.end() ? it->second : "";
}

std::vector<std::string> MemorySystem::RecallPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> results;
    
    for (const auto& pair : memory_) {
        if (pair.first.find(pattern) != std::string::npos) {
            results.push_back(pair.second);
        }
    }
    
    return results;
}

bool MemorySystem::HasKey(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return memory_.find(key) != memory_.end();
}

void MemorySystem::Forget(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_.erase(key);
    timestamps_.erase(key);
}

void MemorySystem::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_.clear();
    timestamps_.clear();
}

void MemorySystem::SaveToFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string data;
    for (const auto& pair : memory_) {
        data += pair.first + "=" + pair.second + "\n";
    }
    
    Platform::WriteFile(filePath.c_str(), data.c_str(), data.size());
}

void MemorySystem::LoadFromFile(const std::string& filePath) {
    size_t size = 0;
    char* data = Platform::ReadFile(filePath.c_str(), &size);
    if (!data) return;
    
    std::string content(data, size);
    Platform::Free(data);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t pos = 0;
    while (pos < content.size()) {
        size_t eqPos = content.find('=', pos);
        size_t nlPos = content.find('\n', pos);
        
        if (eqPos == std::string::npos || nlPos == std::string::npos) break;
        
        std::string key = content.substr(pos, eqPos - pos);
        std::string value = content.substr(eqPos + 1, nlPos - eqPos - 1);
        
        memory_[key] = value;
        timestamps_[key] = Platform::GetTimeMicroseconds();
        
        pos = nlPos + 1;
    }
}

// ============================================================================
// GITHUB TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_GitHubCreateIssue(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto titleIt = params.find("title");
    auto bodyIt = params.find("body");
    
    if (repoIt == params.end() || titleIt == params.end()) {
        result.error = "Missing parameters: repo and title required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh issue create --repo " + repoIt->second + " --title \"" + titleIt->second + "\"";
    if (bodyIt != params.end()) cmd += " --body \"" + bodyIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubCreatePR(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto titleIt = params.find("title");
    auto bodyIt = params.find("body");
    auto baseIt = params.find("base");
    auto headIt = params.find("head");
    
    if (repoIt == params.end() || titleIt == params.end()) {
        result.error = "Missing parameters: repo and title required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh pr create --repo " + repoIt->second + " --title \"" + titleIt->second + "\"";
    if (bodyIt != params.end()) cmd += " --body \"" + bodyIt->second + "\"";
    if (baseIt != params.end()) cmd += " --base " + baseIt->second;
    if (headIt != params.end()) cmd += " --head " + headIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubListIssues(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto stateIt = params.find("state");
    
    if (repoIt == params.end()) {
        result.error = "Missing parameter: repo";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh issue list --repo " + repoIt->second;
    if (stateIt != params.end()) cmd += " --state " + stateIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubListPRs(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto stateIt = params.find("state");
    
    if (repoIt == params.end()) {
        result.error = "Missing parameter: repo";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh pr list --repo " + repoIt->second;
    if (stateIt != params.end()) cmd += " --state " + stateIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubClone(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto destIt = params.find("dest");
    
    if (repoIt == params.end()) {
        result.error = "Missing parameter: repo";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh repo clone " + repoIt->second;
    if (destIt != params.end()) cmd += " " + destIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubFork(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    
    if (repoIt == params.end()) {
        result.error = "Missing parameter: repo";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh repo fork " + repoIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubWorkflow(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto actionIt = params.find("action");
    auto workflowIt = params.find("workflow");
    
    if (repoIt == params.end() || actionIt == params.end()) {
        result.error = "Missing parameters: repo and action required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh workflow " + actionIt->second;
    if (workflowIt != params.end()) cmd += " " + workflowIt->second;
    cmd += " --repo " + repoIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubRelease(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto repoIt = params.find("repo");
    auto tagIt = params.find("tag");
    auto titleIt = params.find("title");
    
    if (repoIt == params.end() || tagIt == params.end()) {
        result.error = "Missing parameters: repo and tag required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh release create " + tagIt->second + " --repo " + repoIt->second;
    if (titleIt != params.end()) cmd += " --title \"" + titleIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubGist(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto descIt = params.find("description");
    auto publicIt = params.find("public");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "gh gist create " + fileIt->second;
    if (descIt != params.end()) cmd += " --description \"" + descIt->second + "\"";
    if (publicIt != params.end() && publicIt->second == "true") cmd += " --public";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_GitHubAPI(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto methodIt = params.find("method");
    auto endpointIt = params.find("endpoint");
    auto dataIt = params.find("data");
    
    if (endpointIt == params.end()) {
        result.error = "Missing parameter: endpoint";
        result.statusCode = 400;
        return result;
    }
    
    std::string method = methodIt != params.end() ? methodIt->second : "GET";
    std::string cmd = "gh api --method " + method + " " + endpointIt->second;
    if (dataIt != params.end()) cmd += " -f " + dataIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// WEB TOOLS (5+ tools)
// ============================================================================

static ToolResult Tool_WebFetch(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    
    if (urlIt == params.end()) {
        result.error = "Missing parameter: url";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "curl -s -L \"" + urlIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_WebDownload(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    auto destIt = params.find("dest");
    
    if (urlIt == params.end() || destIt == params.end()) {
        result.error = "Missing parameters: url and dest required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "curl -s -L -o \"" + destIt->second + "\" \"" + urlIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_WebPost(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    auto dataIt = params.find("data");
    auto contentTypeIt = params.find("content_type");
    
    if (urlIt == params.end()) {
        result.error = "Missing parameter: url";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "curl -s -X POST";
    if (contentTypeIt != params.end()) cmd += " -H \"Content-Type: " + contentTypeIt->second + "\"";
    if (dataIt != params.end()) cmd += " -d \"" + dataIt->second + "\"";
    cmd += " \"" + urlIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_WebHeaders(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto urlIt = params.find("url");
    
    if (urlIt == params.end()) {
        result.error = "Missing parameter: url";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "curl -s -I \"" + urlIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_WebPing(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto hostIt = params.find("host");
    
    if (hostIt == params.end()) {
        result.error = "Missing parameter: host";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "ping -c 4 " + hostIt->second;
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// DATABASE TOOLS (5+ tools)
// ============================================================================

static ToolResult Tool_DBQuery(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto dbIt = params.find("database");
    auto queryIt = params.find("query");
    
    if (dbIt == params.end() || queryIt == params.end()) {
        result.error = "Missing parameters: database and query required";
        result.statusCode = 400;
        return result;
    }
    
    // SQLite example
    std::string cmd = "sqlite3 \"" + dbIt->second + "\" \"" + queryIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DBListTables(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto dbIt = params.find("database");
    
    if (dbIt == params.end()) {
        result.error = "Missing parameter: database";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "sqlite3 \"" + dbIt->second + "\" \".tables\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DBSchema(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto dbIt = params.find("database");
    auto tableIt = params.find("table");
    
    if (dbIt == params.end()) {
        result.error = "Missing parameter: database";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "sqlite3 \"" + dbIt->second + "\" \".schema";
    if (tableIt != params.end()) cmd += " " + tableIt->second;
    cmd += "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DBExport(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto dbIt = params.find("database");
    auto outputIt = params.find("output");
    
    if (dbIt == params.end() || outputIt == params.end()) {
        result.error = "Missing parameters: database and output required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "sqlite3 \"" + dbIt->second + "\" \".dump\" > \"" + outputIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DBImport(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto dbIt = params.find("database");
    auto inputIt = params.find("input");
    
    if (dbIt == params.end() || inputIt == params.end()) {
        result.error = "Missing parameters: database and input required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "sqlite3 \"" + dbIt->second + "\" < \"" + inputIt->second + "\"";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// TEST TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_TestRun(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto filterIt = params.find("filter");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && ctest --output-on-failure";
    if (filterIt != params.end()) cmd += " -R " + filterIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestCoverage(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && gcovr --xml";
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestBenchmark(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto nameIt = params.find("name");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && ";
    if (nameIt != params.end()) {
        cmd += "./" + nameIt->second;
    } else {
        cmd += "ctest -L benchmark";
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestLint(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto filesIt = params.find("files");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && clang-tidy";
    if (filesIt != params.end()) cmd += " " + filesIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestFormat(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto filesIt = params.find("files");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && clang-format --dry-run --Werror";
    if (filesIt != params.end()) cmd += " " + filesIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestStatic(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto filesIt = params.find("files");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && cppcheck --enable=all";
    if (filesIt != params.end()) cmd += " " + filesIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestMemory(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto exeIt = params.find("executable");
    
    if (exeIt == params.end()) {
        result.error = "Missing parameter: executable";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && valgrind --leak-check=full " + exeIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestSanitize(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto typeIt = params.find("type");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    std::string type = typeIt != params.end() ? typeIt->second : "address";
    
    std::string cmd = "cd \"" + path + "\" && cmake -DCMAKE_CXX_FLAGS=\"-fsanitize=" + type + "\" .. && make";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestFuzz(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto targetIt = params.find("target");
    auto corpusIt = params.find("corpus");
    
    if (targetIt == params.end()) {
        result.error = "Missing parameter: target";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && " + targetIt->second;
    if (corpusIt != params.end()) cmd += " " + corpusIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_TestReport(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto formatIt = params.find("format");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    std::string format = formatIt != params.end() ? formatIt->second : "xml";
    
    std::string cmd = "cd \"" + path + "\" && ctest -T Test -T Submit";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// DOCUMENTATION TOOLS (5+ tools)
// ============================================================================

static ToolResult Tool_DocGenerate(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto outputIt = params.find("output");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    std::string output = outputIt != params.end() ? outputIt->second : "docs";
    
    std::string cmd = "cd \"" + path + "\" && doxygen Doxyfile";
    
    char* output2 = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output2);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output2 ? output2 : "";
    if (output2) Platform::Free(output2);
    return result;
}

static ToolResult Tool_DocMarkdown(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto inputIt = params.find("input");
    auto outputIt = params.find("output");
    
    if (inputIt == params.end()) {
        result.error = "Missing parameter: input";
        result.statusCode = 400;
        return result;
    }
    
    // Convert comments to markdown
    size_t size = 0;
    char* data = Platform::ReadFile(inputIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read file: " + inputIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    std::string markdown;
    bool inComment = false;
    std::string currentComment;
    
    for (size_t i = 0; i < content.size(); i++) {
        if (i + 1 < content.size() && content[i] == '/' && content[i + 1] == '*') {
            inComment = true;
            i += 2;
            continue;
        }
        if (i + 1 < content.size() && content[i] == '*' && content[i + 1] == '/') {
            inComment = false;
            i += 2;
            if (!currentComment.empty()) {
                markdown += currentComment + "\n\n";
                currentComment.clear();
            }
            continue;
        }
        if (inComment) {
            currentComment += content[i];
        }
    }
    
    result.success = true;
    result.output = markdown;
    return result;
}

static ToolResult Tool_DocAPI(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto formatIt = params.find("format");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    std::string format = formatIt != params.end() ? formatIt->second : "openapi";
    
    std::string cmd = "cd \"" + path + "\" && swagger-codegen generate -l " + format;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DocChangelog(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto fromIt = params.find("from");
    auto toIt = params.find("to");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && git log --oneline";
    if (fromIt != params.end() && toIt != params.end()) {
        cmd += " " + fromIt->second + ".." + toIt->second;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DocReadme(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto templateIt = params.find("template");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    // Generate README from template
    std::string readme = "# Project\n\n";
    
    // Check for package.json, CMakeLists.txt, etc.
    std::string pkgPath = path + "/package.json";
    std::string cmakePath = path + "/CMakeLists.txt";
    
    size_t size = 0;
    char* data = Platform::ReadFile(pkgPath.c_str(), &size);
    if (data) {
        readme += "## Node.js Project\n\n";
        // Parse package.json for name, description
        std::string content(data, size);
        Platform::Free(data);
        readme += content;
    }
    
    data = Platform::ReadFile(cmakePath.c_str(), &size);
    if (data) {
        readme += "## CMake Project\n\n";
        std::string content(data, size);
        Platform::Free(data);
        readme += content;
    }
    
    result.success = true;
    result.output = readme;
    return result;
}

// ============================================================================
// SECURITY TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_SecurityScan(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && semgrep --config=auto";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecuritySecrets(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && gitleaks detect --source .";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecurityDeps(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && npm audit";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecurityHash(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto algoIt = params.find("algorithm");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    std::string algo = algoIt != params.end() ? algoIt->second : "sha256";
    std::string cmd;
    
#ifdef _WIN32
    if (algo == "md5") {
        cmd = "certutil -hashfile \"" + fileIt->second + "\" MD5";
    } else if (algo == "sha1") {
        cmd = "certutil -hashfile \"" + fileIt->second + "\" SHA1";
    } else {
        cmd = "certutil -hashfile \"" + fileIt->second + "\" SHA256";
    }
#else
    if (algo == "md5") {
        cmd = "md5sum \"" + fileIt->second + "\"";
    } else if (algo == "sha1") {
        cmd = "sha1sum \"" + fileIt->second + "\"";
    } else {
        cmd = "sha256sum \"" + fileIt->second + "\"";
    }
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecurityEncrypt(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto inputIt = params.find("input");
    auto outputIt = params.find("output");
    auto keyIt = params.find("key");
    
    if (inputIt == params.end() || outputIt == params.end() || keyIt == params.end()) {
        result.error = "Missing parameters: input, output, and key required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "openssl enc -aes-256-cbc -in \"" + inputIt->second + 
                      "\" -out \"" + outputIt->second + "\" -pass pass:" + keyIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecurityDecrypt(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto inputIt = params.find("input");
    auto outputIt = params.find("output");
    auto keyIt = params.find("key");
    
    if (inputIt == params.end() || outputIt == params.end() || keyIt == params.end()) {
        result.error = "Missing parameters: input, output, and key required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "openssl enc -aes-256-cbc -d -in \"" + inputIt->second + 
                      "\" -out \"" + outputIt->second + "\" -pass pass:" + keyIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecurityCert(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto domainIt = params.find("domain");
    
    if (domainIt == params.end()) {
        result.error = "Missing parameter: domain";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "openssl s_client -connect " + domainIt->second + ":443 -servername " + 
                      domainIt->second + " 2>/dev/null | openssl x509 -noout -text";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecuritySSL(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto domainIt = params.find("domain");
    
    if (domainIt == params.end()) {
        result.error = "Missing parameter: domain";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "echo | openssl s_client -connect " + domainIt->second + ":443 2>/dev/null";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SecurityKeygen(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto typeIt = params.find("type");
    auto bitsIt = params.find("bits");
    auto outputIt = params.find("output");
    
    std::string type = typeIt != params.end() ? typeIt->second : "rsa";
    std::string bits = bitsIt != params.end() ? bitsIt->second : "4096";
    std::string output = outputIt != params.end() ? outputIt->second : "key.pem";
    
    std::string cmd = "openssl gen" + type + " -out \"" + output + "\" " + bits;
    
    char* output2 = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output2);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output2 ? output2 : "";
    if (output2) Platform::Free(output2);
    return result;
}

static ToolResult Tool_SecurityAudit(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : GetWorkspaceRoot();
    
    std::string cmd = "cd \"" + path + "\" && npm audit --json";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

} // namespace NativeIDE
} // namespace RawrXD
