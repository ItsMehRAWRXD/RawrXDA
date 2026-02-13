// ============================================================================
// RawrXD_PipeClient.h - Complete Win32 Named Pipe Client
// Version: 1.1.0 - Production Ready
// Requires: Windows SDK, C++17
// ============================================================================
//
// Usage:
//   #include "RawrXD_PipeClient.h"
//   
//   // On file save in your IDE:
//   RawrXD::PipeClient client;
//   if (client.Connect()) {
//       auto result = client.ClassifyFile("D:\\project\\file.cpp");
//       if (result.IsCritical()) {
//           ShowError(result.pattern, result.line);
//       } else if (result.IsHighConfidence()) {
//           AddTODO(result.pattern, result.line, result.priority);
//       }
//   }
//
// ============================================================================

#pragma once

#ifndef RAWRXD_PIPE_CLIENT_H
#define RAWRXD_PIPE_CLIENT_H

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace RawrXD {

// Pattern types matching the MASM engine
enum class PatternType : int {
    Unknown = 0,
    TODO    = 1,
    FIXME   = 2,
    XXX     = 3,
    HACK    = 4,
    BUG     = 5,
    NOTE    = 6,
    IDEA    = 7,
    REVIEW  = 8
};

// Priority levels
enum class Priority : int {
    Low      = 0,
    Medium   = 1,
    High     = 2,
    Critical = 3
};

// Result structure for pattern classification
struct PatternResult {
    PatternType type = PatternType::Unknown;
    std::string typeName = "Unknown";
    double confidence = 0.0;
    int lineNumber = 0;
    Priority priority = Priority::Low;
    std::string content;
    bool success = false;
    std::string errorMessage;
    
    // Convenience methods
    bool IsCritical() const { return priority == Priority::Critical; }
    bool IsHigh() const { return priority >= Priority::High; }
    bool HasMatch() const { return type != PatternType::Unknown && success; }
};

// Callback for async operations
using PatternCallback = std::function<void(const PatternResult&)>;

// ============================================================================
// PipeClient - Main client class
// ============================================================================
class PipeClient {
public:
    // Default pipe name matches RawrXD-IDE-Bridge.ps1
    static constexpr const char* DEFAULT_PIPE_NAME = "\\\\.\\pipe\\RawrXD_PatternBridge";
    static constexpr DWORD DEFAULT_TIMEOUT_MS = 5000;
    
private:
    HANDLE m_hPipe = INVALID_HANDLE_VALUE;
    bool m_connected = false;
    std::string m_pipeName;
    DWORD m_timeoutMs;
    std::string m_lastError;
    
    // Statistics
    uint64_t m_totalRequests = 0;
    uint64_t m_totalMatches = 0;
    uint64_t m_totalBytes = 0;
    
    // Internal helpers
    bool SendCommand(const std::string& cmd) {
        if (!m_connected) return false;
        
        DWORD len = static_cast<DWORD>(cmd.length());
        DWORD written;
        
        // Send length prefix
        if (!WriteFile(m_hPipe, &len, sizeof(len), &written, NULL) || written != sizeof(len)) {
            m_lastError = "Failed to write command length";
            return false;
        }
        
        // Send command data
        if (!WriteFile(m_hPipe, cmd.data(), len, &written, NULL) || written != len) {
            m_lastError = "Failed to write command data";
            return false;
        }
        
        FlushFileBuffers(m_hPipe);
        return true;
    }
    
    bool SendData(const void* data, DWORD size) {
        if (!m_connected) return false;
        
        DWORD written;
        
        // Send size prefix
        if (!WriteFile(m_hPipe, &size, sizeof(size), &written, NULL) || written != sizeof(size)) {
            m_lastError = "Failed to write data size";
            return false;
        }
        
        // Send data
        if (!WriteFile(m_hPipe, data, size, &written, NULL) || written != size) {
            m_lastError = "Failed to write data";
            return false;
        }
        
        FlushFileBuffers(m_hPipe);
        return true;
    }
    
    std::optional<std::string> ReadResponse() {
        if (!m_connected) return std::nullopt;
        
        DWORD len;
        DWORD bytesRead;
        
        // Read length prefix
        if (!ReadFile(m_hPipe, &len, sizeof(len), &bytesRead, NULL) || bytesRead != sizeof(len)) {
            m_lastError = "Failed to read response length";
            return std::nullopt;
        }
        
        // Sanity check
        if (len > 1024 * 1024) { // 1MB max
            m_lastError = "Response too large";
            return std::nullopt;
        }
        
        // Read response data
        std::vector<char> buffer(len + 1);
        if (!ReadFile(m_hPipe, buffer.data(), len, &bytesRead, NULL) || bytesRead != len) {
            m_lastError = "Failed to read response data";
            return std::nullopt;
        }
        
        buffer[len] = '\0';
        return std::string(buffer.data());
    }
    
    PatternResult ParseJsonResponse(const std::string& json) {
        PatternResult result;
        result.success = false;
        
        // Simple JSON parsing (no external dependencies)
        // Format: {"Pattern":"TODO","Confidence":0.85,"Line":42,"Priority":1,"Content":"..."}
        
        auto extractString = [&json](const char* key) -> std::string {
            std::string searchKey = std::string("\"") + key + "\":\"";
            size_t pos = json.find(searchKey);
            if (pos == std::string::npos) return "";
            pos += searchKey.length();
            size_t end = json.find("\"", pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        };
        
        auto extractNumber = [&json](const char* key) -> double {
            std::string searchKey = std::string("\"") + key + "\":";
            size_t pos = json.find(searchKey);
            if (pos == std::string::npos) return 0.0;
            pos += searchKey.length();
            // Skip whitespace
            while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
            size_t end = pos;
            while (end < json.length() && (isdigit(json[end]) || json[end] == '.' || json[end] == '-')) end++;
            if (end == pos) return 0.0;
            return std::stod(json.substr(pos, end - pos));
        };
        
        result.typeName = extractString("Pattern");
        result.confidence = extractNumber("Confidence");
        result.lineNumber = static_cast<int>(extractNumber("Line"));
        result.priority = static_cast<Priority>(static_cast<int>(extractNumber("Priority")));
        result.content = extractString("Content");
        
        // Map type name to enum
        if (result.typeName == "TODO")        result.type = PatternType::TODO;
        else if (result.typeName == "FIXME")  result.type = PatternType::FIXME;
        else if (result.typeName == "XXX")    result.type = PatternType::XXX;
        else if (result.typeName == "HACK")   result.type = PatternType::HACK;
        else if (result.typeName == "BUG")    result.type = PatternType::BUG;
        else if (result.typeName == "NOTE")   result.type = PatternType::NOTE;
        else if (result.typeName == "IDEA")   result.type = PatternType::IDEA;
        else if (result.typeName == "REVIEW") result.type = PatternType::REVIEW;
        else                                  result.type = PatternType::Unknown;
        
        result.success = !result.typeName.empty();
        return result;
    }
    
public:
    PipeClient(const char* pipeName = DEFAULT_PIPE_NAME, DWORD timeoutMs = DEFAULT_TIMEOUT_MS)
        : m_pipeName(pipeName), m_timeoutMs(timeoutMs) {}
    
    ~PipeClient() {
        Disconnect();
    }
    
    // Disable copy
    PipeClient(const PipeClient&) = delete;
    PipeClient& operator=(const PipeClient&) = delete;
    
    // Enable move
    PipeClient(PipeClient&& other) noexcept {
        m_hPipe = other.m_hPipe;
        m_connected = other.m_connected;
        m_pipeName = std::move(other.m_pipeName);
        m_timeoutMs = other.m_timeoutMs;
        other.m_hPipe = INVALID_HANDLE_VALUE;
        other.m_connected = false;
    }
    
    // ========================================================================
    // Connection Management
    // ========================================================================
    
    bool Connect() {
        if (m_connected) return true;
        
        // Wait for pipe to be available
        if (!WaitNamedPipeA(m_pipeName.c_str(), m_timeoutMs)) {
            // Pipe doesn't exist, try direct connect anyway
        }
        
        m_hPipe = CreateFileA(
            m_pipeName.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (m_hPipe == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if (err == ERROR_PIPE_BUSY) {
                m_lastError = "Pipe is busy - another client connected";
            } else if (err == ERROR_FILE_NOT_FOUND) {
                m_lastError = "Pipe not found - is the server running?";
            } else {
                m_lastError = "Failed to connect: error " + std::to_string(err);
            }
            return false;
        }
        
        // Set pipe mode to message
        DWORD mode = PIPE_READMODE_MESSAGE;
        if (!SetNamedPipeHandleState(m_hPipe, &mode, NULL, NULL)) {
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
            m_lastError = "Failed to set pipe mode";
            return false;
        }
        
        m_connected = true;
        m_lastError.clear();
        return true;
    }
    
    void Disconnect() {
        if (m_hPipe != INVALID_HANDLE_VALUE) {
            // Send shutdown signal
            SendCommand("DISCONNECT");
            CloseHandle(m_hPipe);
            m_hPipe = INVALID_HANDLE_VALUE;
        }
        m_connected = false;
    }
    
    bool IsConnected() const { return m_connected; }
    
    const std::string& GetLastError() const { return m_lastError; }
    
    // ========================================================================
    // Classification Methods
    // ========================================================================
    
    PatternResult ClassifyBuffer(const std::vector<BYTE>& data) {
        PatternResult result;
        result.success = false;
        
        if (!m_connected) {
            result.errorMessage = "Not connected";
            return result;
        }
        
        if (data.empty()) {
            result.errorMessage = "Empty buffer";
            return result;
        }
        
        m_totalRequests++;
        m_totalBytes += data.size();
        
        // Send CLASSIFY command
        if (!SendCommand("CLASSIFY")) {
            result.errorMessage = m_lastError;
            return result;
        }
        
        // Send buffer data
        if (!SendData(data.data(), static_cast<DWORD>(data.size()))) {
            result.errorMessage = m_lastError;
            return result;
        }
        
        // Read response
        auto response = ReadResponse();
        if (!response.has_value()) {
            result.errorMessage = m_lastError;
            return result;
        }
        
        result = ParseJsonResponse(response.value());
        
        if (result.HasMatch()) {
            m_totalMatches++;
        }
        
        return result;
    }
    
    PatternResult ClassifyString(const std::string& text) {
        std::vector<BYTE> data(text.begin(), text.end());
        return ClassifyBuffer(data);
    }
    
    PatternResult ClassifyFile(const char* filePath) {
        PatternResult result;
        result.success = false;
        
        HANDLE hFile = CreateFileA(
            filePath,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (hFile == INVALID_HANDLE_VALUE) {
            result.errorMessage = "Failed to open file: " + std::string(filePath);
            return result;
        }
        
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == INVALID_FILE_SIZE || fileSize == 0) {
            CloseHandle(hFile);
            result.errorMessage = "Invalid file size";
            return result;
        }
        
        // Limit to 10MB
        if (fileSize > 10 * 1024 * 1024) {
            CloseHandle(hFile);
            result.errorMessage = "File too large (>10MB)";
            return result;
        }
        
        std::vector<BYTE> buffer(fileSize);
        DWORD bytesRead;
        
        if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
            CloseHandle(hFile);
            result.errorMessage = "Failed to read file";
            return result;
        }
        
        CloseHandle(hFile);
        return ClassifyBuffer(buffer);
    }
    
    // Classify file by wide string path
    PatternResult ClassifyFile(const wchar_t* filePath) {
        // Convert to narrow string
        int size = WideCharToMultiByte(CP_UTF8, 0, filePath, -1, NULL, 0, NULL, NULL);
        std::string narrow(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, filePath, -1, &narrow[0], size, NULL, NULL);
        return ClassifyFile(narrow.c_str());
    }
    
    // ========================================================================
    // Batch Operations
    // ========================================================================
    
    std::vector<PatternResult> ClassifyFiles(const std::vector<std::string>& filePaths) {
        std::vector<PatternResult> results;
        results.reserve(filePaths.size());
        
        for (const auto& path : filePaths) {
            results.push_back(ClassifyFile(path.c_str()));
        }
        
        return results;
    }
    
    // ========================================================================
    // Server Commands
    // ========================================================================
    
    bool Ping() {
        if (!m_connected) return false;
        
        if (!SendCommand("PING")) return false;
        
        auto response = ReadResponse();
        return response.has_value() && response.value().find("PONG") != std::string::npos;
    }
    
    std::string GetServerStats() {
        if (!m_connected) return "Not connected";
        
        if (!SendCommand("STATS")) return "";
        
        auto response = ReadResponse();
        return response.value_or("");
    }
    
    std::string GetEngineInfo() {
        if (!m_connected) return "";
        
        if (!SendCommand("INFO")) return "";
        
        auto response = ReadResponse();
        return response.value_or("");
    }
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    uint64_t GetTotalRequests() const { return m_totalRequests; }
    uint64_t GetTotalMatches() const { return m_totalMatches; }
    uint64_t GetTotalBytes() const { return m_totalBytes; }
    
    void ResetStats() {
        m_totalRequests = 0;
        m_totalMatches = 0;
        m_totalBytes = 0;
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

inline const char* PatternTypeToString(PatternType type) {
    switch (type) {
        case PatternType::TODO:   return "TODO";
        case PatternType::FIXME:  return "FIXME";
        case PatternType::XXX:    return "XXX";
        case PatternType::HACK:   return "HACK";
        case PatternType::BUG:    return "BUG";
        case PatternType::NOTE:   return "NOTE";
        case PatternType::IDEA:   return "IDEA";
        case PatternType::REVIEW: return "REVIEW";
        default:                  return "Unknown";
    }
}

inline const char* PriorityToString(Priority priority) {
    switch (priority) {
        case Priority::Low:      return "Low";
        case Priority::Medium:   return "Medium";
        case Priority::High:     return "High";
        case Priority::Critical: return "Critical";
        default:                 return "Unknown";
    }
}

} // namespace RawrXD

// ============================================================================
// C-Style API for Plugin Integration
// ============================================================================
// These functions provide a simple C interface for IDE plugins that may not
// support C++ directly (e.g., Lua, Python FFI, C# P/Invoke)

#ifdef RAWRXD_EXPORT_C_API

// Global client instance for C API
static RawrXD::PipeClient* g_pipeClient = nullptr;

extern "C" {

// Initialize the global client
__declspec(dllexport) int RawrXD_Init(const char* pipeName = nullptr) {
    if (g_pipeClient) return 1; // Already initialized
    
    if (pipeName && pipeName[0]) {
        g_pipeClient = new RawrXD::PipeClient(pipeName);
    } else {
        g_pipeClient = new RawrXD::PipeClient();
    }
    
    return g_pipeClient->Connect() ? 0 : -1;
}

// Shutdown and cleanup
__declspec(dllexport) void RawrXD_Shutdown() {
    if (g_pipeClient) {
        g_pipeClient->Disconnect();
        delete g_pipeClient;
        g_pipeClient = nullptr;
    }
}

// Check if connected
__declspec(dllexport) int RawrXD_IsConnected() {
    return (g_pipeClient && g_pipeClient->IsConnected()) ? 1 : 0;
}

// Classify a buffer, returns pattern type (0-8), or -1 on error
__declspec(dllexport) int RawrXD_ClassifyBuffer(
    const unsigned char* buffer,
    unsigned int length,
    double* outConfidence,
    int* outLineNumber,
    int* outPriority
) {
    if (!g_pipeClient || !g_pipeClient->IsConnected()) return -1;
    if (!buffer || length == 0) return -1;
    
    std::vector<BYTE> data(buffer, buffer + length);
    auto result = g_pipeClient->ClassifyBuffer(data);
    
    if (!result.success) return -1;
    
    if (outConfidence) *outConfidence = result.confidence;
    if (outLineNumber) *outLineNumber = result.lineNumber;
    if (outPriority) *outPriority = static_cast<int>(result.priority);
    
    return static_cast<int>(result.type);
}

// Classify a file, returns pattern type (0-8), or -1 on error
__declspec(dllexport) int RawrXD_ClassifyFile(
    const char* filePath,
    double* outConfidence,
    int* outLineNumber,
    int* outPriority
) {
    if (!g_pipeClient || !g_pipeClient->IsConnected()) return -1;
    if (!filePath) return -1;
    
    auto result = g_pipeClient->ClassifyFile(filePath);
    
    if (!result.success) return -1;
    
    if (outConfidence) *outConfidence = result.confidence;
    if (outLineNumber) *outLineNumber = result.lineNumber;
    if (outPriority) *outPriority = static_cast<int>(result.priority);
    
    return static_cast<int>(result.type);
}

// Get pattern type name (thread-safe, returns static string)
__declspec(dllexport) const char* RawrXD_GetPatternName(int patternType) {
    return RawrXD::PatternTypeToString(static_cast<RawrXD::PatternType>(patternType));
}

// Get priority name (thread-safe, returns static string)
__declspec(dllexport) const char* RawrXD_GetPriorityName(int priority) {
    return RawrXD::PriorityToString(static_cast<RawrXD::Priority>(priority));
}

// Get last error message
__declspec(dllexport) const char* RawrXD_GetLastError() {
    static std::string lastError;
    if (g_pipeClient) {
        lastError = g_pipeClient->GetLastError();
    } else {
        lastError = "Client not initialized";
    }
    return lastError.c_str();
}

// Ping the server
__declspec(dllexport) int RawrXD_Ping() {
    return (g_pipeClient && g_pipeClient->Ping()) ? 0 : -1;
}

// Get statistics
__declspec(dllexport) void RawrXD_GetStats(
    unsigned long long* outRequests,
    unsigned long long* outMatches,
    unsigned long long* outBytes
) {
    if (!g_pipeClient) return;
    
    if (outRequests) *outRequests = g_pipeClient->GetTotalRequests();
    if (outMatches) *outMatches = g_pipeClient->GetTotalMatches();
    if (outBytes) *outBytes = g_pipeClient->GetTotalBytes();
}

} // extern "C"

#endif // RAWRXD_EXPORT_C_API

#endif // RAWRXD_PIPE_CLIENT_H
