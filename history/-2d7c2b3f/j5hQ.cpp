#include "rollback.hpp"
#include "meta_learn.hpp"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

#pragma comment(lib, "winhttp.lib")

// Helper: WinHTTP POST
static bool SendGitHubIssue(const std::string& token, const std::string& title, const std::string& body) {
    HINTERNET hSession = WinHttpOpen(L"RawrXD-Rollback/1.0", 
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
                                     WINHTTP_NO_PROXY_NAME, 
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/repos/ItsMehRAWRXD/RawrXD-ModelLoader/issues",
                                            NULL, WINHTTP_NO_REFERER, 
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    // Headers
    std::string authHeader = "Authorization: Bearer " + token + "\r\ncontent-type: application/json";
    std::wstring wHeader(authHeader.begin(), authHeader.end());

    // Body
    nlohmann::json jsonBody;
    jsonBody["title"] = title;
    jsonBody["body"] = body;
    jsonBody["labels"] = {"regression", "auto"};
    std::string sBody = jsonBody.dump();

    bool bResults = WinHttpSendRequest(hRequest,
                                       wHeader.c_str(), (DWORD)wHeader.length(),
                                       (LPVOID)sBody.c_str(), (DWORD)sBody.length(),
                                       (DWORD)sBody.length(), 0);

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    // We could check status code here, but for now we assume success if response received
    
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return bResults;
}

// Reusing helper function pattern
static bool runProcess(const std::string& cmd, const std::vector<std::string>& args, int timeoutMs = 60000) {
    std::string commandLine = cmd;
    for (const auto& arg : args) {
        commandLine += " \"" + arg + "\"";
    }
    
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    
    char* cmdLine = _strdup(commandLine.c_str());
    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        free(cmdLine);
        return false;
    }
    free(cmdLine);
    
    DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
    bool success = false;
    
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
    } else {
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        success = (exitCode == 0);
    }
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return success;
}

// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    // load before/after from perf_db.json via MetaLearn
    bool ok = false;
    MetaLearn ml;
    auto db = ml.getHistory(""); // Load all
    
    if (db.size() < 2)
        return false; // need at least 2 records

    // last commit = most recent (assuming order or check timestamp)
    // The previous implementation assumed order in list.
    // m_records is appended, so last is most recent.
    
    const auto& last = db.back();
    const auto& prev = db[db.size() - 2];

    double lastTPS = last.tps;
    double prevTPS = prev.tps;
    double lastPPL = last.ppl;
    double prevPPL = prev.ppl;

    // regression: TPS drop > 5 % OR PPL increase > 2 %
    bool tpsReg = lastTPS < prevTPS * 0.95;
    bool pplReg = lastPPL > prevPPL * 1.02;


    return tpsReg || pplReg;
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    if (!runProcess("git", {"revert", "--no-edit", "HEAD"}, 60000)) {
        
        return false;
    }
    
    return true;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const std::string& title, const std::string& body) {
    char* token = nullptr;
    size_t len = 0;
    _dupenv_s(&token, &len, "GITHUB_TOKEN");
    
    if (!token || len == 0) {
        
        if (token) free(token);
        return true; // allow in dev
    }
    std::string tokenStr(token);
    free(token);

    // Native implementation replacing fallback/mock
    return SendGitHubIssue(tokenStr, title, body);
}
