// Comprehensive Logging System for Win32IDE
#include "Win32IDE.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <windows.h>
#include <vector>

// Global log file
static std::ofstream g_logFile;
static bool g_logInitialized = false;
static CRITICAL_SECTION g_logMutex;

void Win32IDE::initializeLogging() {
    if (g_logInitialized) return;
    
    InitializeCriticalSection(&g_logMutex);
    
    // Attempt to initialize logging with multiple fallback strategies
    std::vector<std::string> candidateDirs = {
        "logs",                    // Primary: local logs directory
        ".",                       // Fallback 1: current directory
        ""                         // Fallback 2: no subdirectory
    };
    
    // Try to get TEMP directory as additional fallback
    char tempPath[MAX_PATH] = {};
    if (GetTempPathA(sizeof(tempPath), tempPath)) {
        std::string tempDir = tempPath;
        size_t lastSlash = tempDir.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            tempDir = tempDir.substr(0, lastSlash);
        }
        candidateDirs.push_back(tempDir);
    }
    
    char filename[MAX_PATH] = {};
    bool logFileOpened = false;
    
    for (const auto& dir : candidateDirs) {
        // Generate timestamp once
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        
        char logFilename[256];
        strftime(logFilename, sizeof(logFilename), "RawrXD_IDE_%Y%m%d_%H%M%S.log", &timeinfo);
        
        // Build full path
        if (dir.empty()) {
            strncpy_s(filename, sizeof(filename), logFilename, _TRUNCATE);
        } else {
            DWORD dirAttrs = GetFileAttributesA(dir.c_str());
            
            // If directory doesn't exist, try to create it (but don't fail if it already exists)
            if (dirAttrs == INVALID_FILE_ATTRIBUTES) {
                if (!CreateDirectoryA(dir.c_str(), NULL)) {
                    DWORD createErr = GetLastError();
                    // ERROR_ALREADY_EXISTS is OK
                    if (createErr != ERROR_ALREADY_EXISTS) {
                        continue;  // Try next directory
                    }
                }
            } else if (!(dirAttrs & FILE_ATTRIBUTE_DIRECTORY)) {
                continue;  // Path exists but is not a directory
            }
            
            // Check write permissions by testing file creation
            char testFilePath[MAX_PATH] = {};
            snprintf(testFilePath, sizeof(testFilePath), "%s\\.rawrxd_write_test", dir.c_str());
            HANDLE testHandle = CreateFileA(testFilePath, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, NULL);
            if (testHandle != INVALID_HANDLE_VALUE) {
                CloseHandle(testHandle);
                DeleteFileA(testFilePath);  // Clean up test file
            } else {
                continue;  // No write permissions in this directory
            }
            
            snprintf(filename, sizeof(filename), "%s\\%s", dir.c_str(), logFilename);
        }
        
        // Attempt to open log file
        g_logFile.open(filename, std::ios::out | std::ios::app);
        if (g_logFile.is_open() && g_logFile.good()) {
            logFileOpened = true;
            break;  // Success!
        } else {
            g_logFile.clear();  // Clear any error flags
        }
    }
    
    g_logInitialized = true;
    
    if (logFileOpened) {
        logMessage("SYSTEM", "=== RawrXD IDE Logging Initialized ===");
        logMessage("SYSTEM", "Log file: " + std::string(filename));
    } else {
        // Even if file couldn't be opened, logging is still "initialized" (will use debug output only)
        OutputDebugStringA("WARNING: Could not open log file in any candidate directory; using debug output only\n");
        logMessage("SYSTEM", "=== RawrXD IDE Logging Initialized (Debug Output Mode) ===");
    }
}

void Win32IDE::logMessage(const std::string& category, const std::string& message) {
    if (!g_logInitialized) initializeLogging();
    
    EnterCriticalSection(&g_logMutex);
    
    // Get timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    // Format: [TIMESTAMP] [CATEGORY] Message
    std::string logEntry = "[" + std::string(timestamp) + "] [" + category + "] " + message;
    
    // Write to file
    if (g_logFile.is_open()) {
        g_logFile << logEntry << std::endl;
        g_logFile.flush();
    }
    
    // Also write to debug output
    OutputDebugStringA((logEntry + "\n").c_str());
    
    // Write to Output panel if available
    if (m_hwndMain && IsWindow(m_hwndMain)) {
        OutputSeverity severity = OutputSeverity::Debug;
        if (category == "ERROR") severity = OutputSeverity::Error;
        else if (category == "WARNING") severity = OutputSeverity::Warning;
        else if (category == "INFO") severity = OutputSeverity::Info;
        
        appendToOutput(logEntry, "Debug", severity);
    }
    
    LeaveCriticalSection(&g_logMutex);
}

void Win32IDE::logFunction(const std::string& functionName) {
    logMessage("FUNC", ">>> " + functionName);
}

void Win32IDE::logError(const std::string& functionName, const std::string& error) {
    logMessage("ERROR", functionName + ": " + error);
}

void Win32IDE::logWarning(const std::string& functionName, const std::string& warning) {
    logMessage("WARNING", functionName + ": " + warning);
}

void Win32IDE::logInfo(const std::string& message) {
    logMessage("INFO", message);
}

void Win32IDE::logWindowCreate(const std::string& windowName, HWND hwnd) {
    std::ostringstream oss;
    oss << "Window created: " << windowName << " (HWND: 0x" << std::hex << (UINT_PTR)hwnd << ")";
    logMessage("WINDOW", oss.str());
}

void Win32IDE::logWindowDestroy(const std::string& windowName, HWND hwnd) {
    std::ostringstream oss;
    oss << "Window destroyed: " << windowName << " (HWND: 0x" << std::hex << (UINT_PTR)hwnd << ")";
    logMessage("WINDOW", oss.str());
}

void Win32IDE::logFileOperation(const std::string& operation, const std::string& filePath, bool success) {
    std::string status = success ? "SUCCESS" : "FAILED";
    logMessage("FILE", operation + ": " + filePath + " - " + status);
}

void Win32IDE::logUIEvent(const std::string& event, const std::string& details) {
    logMessage("UI", event + ": " + details);
}

void Win32IDE::shutdownLogging() {
    if (!g_logInitialized) return;
    
    logMessage("SYSTEM", "=== RawrXD IDE Shutting Down ===");
    
    if (g_logFile.is_open()) {
        g_logFile.close();
    }
    
    DeleteCriticalSection(&g_logMutex);
    g_logInitialized = false;
}
