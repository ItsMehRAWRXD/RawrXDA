// RawrXD Error Handler - Pure Win32/C++ Implementation
// Replaces Qt error handling with Win32 error codes + simple logging

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <memory>
#include <queue>

// ============================================================================
// ERROR STRUCTURES
// ============================================================================

enum class ErrorLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR_LEVEL = 3,
    CRITICAL = 4
};

struct ErrorRecord {
    int id;
    int level;
    wchar_t code[64];
    wchar_t message[512];
    wchar_t context[256];
    DWORD timestamp;
};

// ============================================================================
// ERROR HANDLER
// ============================================================================

class ErrorHandler {
private:
    CRITICAL_SECTION m_cs;
    std::vector<ErrorRecord> m_history;
    int m_nextErrorId;
    DWORD m_lastErrorCode;
    
public:
    ErrorHandler()
        : m_nextErrorId(1),
          m_lastErrorCode(0) {
        InitializeCriticalSection(&m_cs);
    }
    
    ~ErrorHandler() {
        EnterCriticalSection(&m_cs);
        m_history.clear();
        LeaveCriticalSection(&m_cs);
        DeleteCriticalSection(&m_cs);
    }
    
    int LogError(int level, const wchar_t* code, const wchar_t* message, const wchar_t* context) {
        if (!code || !message) return -1;
        
        EnterCriticalSection(&m_cs);
        
        ErrorRecord record;
        record.id = m_nextErrorId++;
        record.level = level;
        record.timestamp = GetTickCount();
        
        wcscpy_s(record.code, 64, code);
        wcscpy_s(record.message, 512, message);
        wcscpy_s(record.context, 256, context ? context : L"");
        
        m_history.push_back(record);
        int errorId = record.id;
        
        // Keep history size manageable
        if (m_history.size() > 1000) {
            m_history.erase(m_history.begin());
        }
        
        LeaveCriticalSection(&m_cs);
        
        // Output to debugger
        OutputDebugStringW((std::wstring(L"[ERROR] ") + code + L": " + message + L"\n").c_str());
        
        return errorId;
    }
    
    int LogWarning(const wchar_t* code, const wchar_t* message, const wchar_t* context = L"") {
        return LogError((int)ErrorLevel::WARNING, code, message, context);
    }
    
    int LogInfo(const wchar_t* code, const wchar_t* message, const wchar_t* context = L"") {
        return LogError((int)ErrorLevel::INFO, code, message, context);
    }
    
    int ReportWindowsError(const wchar_t* operation, const wchar_t* context = L"") {
        DWORD dwError = GetLastError();
        wchar_t buffer[512];
        
        FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dwError,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer,
            sizeof(buffer) / sizeof(wchar_t),
            NULL);
        
        wchar_t code[64];
        swprintf_s(code, 64, L"WIN32_%lu", dwError);
        
        m_lastErrorCode = dwError;
        
        return LogError((int)ErrorLevel::ERROR_LEVEL, code, buffer, operation ? operation : context);
    }
    
    const wchar_t* GetLastErrorString() {
        static wchar_t buffer[512];
        
        FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            m_lastErrorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer,
            sizeof(buffer) / sizeof(wchar_t),
            NULL);
        
        return buffer;
    }
    
    DWORD GetLastErrorCode() const {
        return m_lastErrorCode;
    }
    
    int GetErrorCount() {
        EnterCriticalSection(&m_cs);
        int count = (int)m_history.size();
        LeaveCriticalSection(&m_cs);
        return count;
    }
    
    ErrorRecord GetError(int errorId) {
        EnterCriticalSection(&m_cs);
        
        ErrorRecord result;
        ZeroMemory(&result, sizeof(result));
        result.id = -1;
        
        for (const auto& record : m_history) {
            if (record.id == errorId) {
                result = record;
                break;
            }
        }
        
        LeaveCriticalSection(&m_cs);
        return result;
    }
    
    std::vector<ErrorRecord> GetRecentErrors(int count) {
        std::vector<ErrorRecord> result;
        
        EnterCriticalSection(&m_cs);
        
        int start = (int)m_history.size() - count;
        if (start < 0) start = 0;
        
        for (int i = start; i < (int)m_history.size(); i++) {
            result.push_back(m_history[i]);
        }
        
        LeaveCriticalSection(&m_cs);
        return result;
    }
    
    void Clear() {
        EnterCriticalSection(&m_cs);
        m_history.clear();
        m_nextErrorId = 1;
        LeaveCriticalSection(&m_cs);
    }
};

// ============================================================================
// C INTERFACE
// ============================================================================

extern "C" {

__declspec(dllexport) ErrorHandler* __stdcall CreateErrorHandler(void) {
    return new ErrorHandler();
}

__declspec(dllexport) void __stdcall DestroyErrorHandler(ErrorHandler* handler) {
    if (handler) delete handler;
}

__declspec(dllexport) int __stdcall ErrorHandler_LogError(ErrorHandler* handler,
    int level, const wchar_t* code, const wchar_t* message, const wchar_t* context) {
    if (!handler) return -1;
    return handler->LogError(level, code, message, context);
}

__declspec(dllexport) int __stdcall ErrorHandler_LogWarning(ErrorHandler* handler,
    const wchar_t* code, const wchar_t* message) {
    if (!handler) return -1;
    return handler->LogWarning(code, message);
}

__declspec(dllexport) int __stdcall ErrorHandler_LogInfo(ErrorHandler* handler,
    const wchar_t* code, const wchar_t* message) {
    if (!handler) return -1;
    return handler->LogInfo(code, message);
}

__declspec(dllexport) int __stdcall ErrorHandler_ReportWindowsError(ErrorHandler* handler,
    const wchar_t* operation, const wchar_t* context) {
    if (!handler) return -1;
    return handler->ReportWindowsError(operation, context);
}

__declspec(dllexport) const wchar_t* __stdcall ErrorHandler_GetLastErrorString(ErrorHandler* handler) {
    return handler ? handler->GetLastErrorString() : L"Unknown error";
}

__declspec(dllexport) DWORD __stdcall ErrorHandler_GetLastErrorCode(ErrorHandler* handler) {
    return handler ? handler->GetLastErrorCode() : 0;
}

__declspec(dllexport) int __stdcall ErrorHandler_GetErrorCount(ErrorHandler* handler) {
    return handler ? handler->GetErrorCount() : 0;
}

__declspec(dllexport) void __stdcall ErrorHandler_Clear(ErrorHandler* handler) {
    if (handler) handler->Clear();
}

} // extern "C"

// ============================================================================
// DLL ENTRY
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_ErrorHandler] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_ErrorHandler] DLL unloaded\n");
        break;
    }
    return TRUE;
}
