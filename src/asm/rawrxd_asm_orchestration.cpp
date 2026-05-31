// rawrxd_asm_orchestration.cpp — Production ASM orchestration implementation

#include "rawrxd_asm_orchestration.h"
#include <windows.h>
#include <cstring>
#include <cstdio>

extern "C" DWORD PipeCapture_Run(char* pCmdLine, unsigned char* pOutBuf,
                                  DWORD dwBufSize, DWORD* pdwBytesRead) {
    if (!pCmdLine || !pOutBuf || dwBufSize == 0) {
        if (pdwBytesRead) *pdwBytesRead = 0;
        return ERROR_INVALID_PARAMETER;
    }
    
    SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        if (pdwBytesRead) *pdwBytesRead = 0;
        return GetLastError();
    }
    
    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = nullptr;
    
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessA(nullptr, pCmdLine, nullptr, nullptr, TRUE, 
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        if (pdwBytesRead) *pdwBytesRead = 0;
        return GetLastError();
    }
    
    CloseHandle(hWritePipe);
    
    DWORD totalRead = 0;
    DWORD bytesRead = 0;
    
    while (totalRead < dwBufSize - 1) {
        if (!ReadFile(hReadPipe, pOutBuf + totalRead, dwBufSize - 1 - totalRead, &bytesRead, nullptr)) {
            break;
        }
        if (bytesRead == 0) break;
        totalRead += bytesRead;
    }
    
    pOutBuf[totalRead] = '\0';
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    
    if (pdwBytesRead) *pdwBytesRead = totalRead;
    return 0;
}

// Context buffer for multi-step tool execution tracking
static constexpr size_t CONTEXT_BUF_SIZE = 65536;
static char g_contextBuf[CONTEXT_BUF_SIZE] = {0};
static size_t g_contextLen = 0;
static constexpr size_t MAX_STEPS = 256;
static char g_stepNames[MAX_STEPS][64] = {0};
static size_t g_stepCount = 0;

extern "C" void ContextBuf_Reset() {
    g_contextBuf[0] = '\0';
    g_contextLen = 0;
    g_stepCount = 0;
}

extern "C" void ContextBuf_AppendStep(const char* pToolName) {
    if (!pToolName) return;
    
    // Store step name for tracking
    if (g_stepCount < MAX_STEPS) {
        strncpy_s(g_stepNames[g_stepCount], sizeof(g_stepNames[0]), pToolName, _TRUNCATE);
        g_stepCount++;
    }
    
    // Append to context buffer
    size_t nameLen = strlen(pToolName);
    if (g_contextLen + nameLen + 4 < CONTEXT_BUF_SIZE) {
        if (g_contextLen > 0) {
            strcat_s(g_contextBuf + g_contextLen, CONTEXT_BUF_SIZE - g_contextLen, " -> ");
            g_contextLen += 4;
        }
        strcat_s(g_contextBuf + g_contextLen, CONTEXT_BUF_SIZE - g_contextLen, pToolName);
        g_contextLen += nameLen;
    }
}

extern "C" void ContextBuf_AppendResult(const char* pResult) {
    if (!pResult || g_contextLen >= CONTEXT_BUF_SIZE - 1) return;
    
    size_t resultLen = strlen(pResult);
    size_t remaining = CONTEXT_BUF_SIZE - g_contextLen - 1;
    
    if (resultLen > remaining) {
        resultLen = remaining;
    }
    
    if (g_contextLen > 0 && g_contextBuf[g_contextLen - 1] != '\n') {
        strncat_s(g_contextBuf, CONTEXT_BUF_SIZE, "\n", _TRUNCATE);
        g_contextLen++;
    }
    
    strncat_s(g_contextBuf, CONTEXT_BUF_SIZE, pResult, resultLen);
    g_contextLen = strlen(g_contextBuf);
}

extern "C" const char* ContextBuf_Get() {
    return g_contextBuf[0] ? g_contextBuf : "";
}

extern "C" unsigned int ContextBuf_GetLen() {
    return static_cast<unsigned int>(g_contextLen);
}

extern "C" void ContextBuf_BuildPrompt(const char* pGoal, const char* pSystemPrompt,
                                        char* pOutBuf, DWORD dwOutSize) {
    if (!pOutBuf || dwOutSize == 0) return;
    
    if (pSystemPrompt) {
        strncpy_s(pOutBuf, dwOutSize, pSystemPrompt, _TRUNCATE);
        strncat_s(pOutBuf, dwOutSize, "\r\n\r\n", _TRUNCATE);
    }
    
    if (pGoal) {
        strncat_s(pOutBuf, dwOutSize, "Goal: ", _TRUNCATE);
        strncat_s(pOutBuf, dwOutSize, pGoal, _TRUNCATE);
        strncat_s(pOutBuf, dwOutSize, "\r\n\r\n", _TRUNCATE);
    }
    
    strncat_s(pOutBuf, dwOutSize, "Previous execution context:\r\n", _TRUNCATE);
    strncat_s(pOutBuf, dwOutSize, ContextBuf_Get(), _TRUNCATE);
}
