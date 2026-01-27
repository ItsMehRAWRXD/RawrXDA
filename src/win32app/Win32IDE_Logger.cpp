// Comprehensive Logging System for Win32IDE
#include "Win32IDE.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>

// Global log file
static std::ofstream g_logFile;
static bool g_logInitialized = false;
static CRITICAL_SECTION g_logMutex;

void Win32IDE::initializeLogging() {
    if (g_logInitialized) return;
    
    InitializeCriticalSection(&g_logMutex);
    
    // Create logs directory
    CreateDirectoryA("logs", NULL);
    
    // Open log file with timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    
    char filename[256];
    strftime(filename, sizeof(filename), "logs\\RawrXD_IDE_%Y%m%d_%H%M%S.log", &timeinfo);
    
    g_logFile.open(filename, std::ios::out | std::ios::app);
    g_logInitialized = true;
    
    logMessage("SYSTEM", "=== RawrXD IDE Logging Initialized ===");
    logMessage("SYSTEM", "Log file: " + std::string(filename));
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
