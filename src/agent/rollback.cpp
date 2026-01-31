#include "rollback.hpp"
#include "meta_learn.hpp"
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <nlohmann/json.hpp>

// Assuming WinHTTP helper is available (e.g. from a shared utility or copied)
// For now I'll use a mocked/simplified http post or system call if needed, 
// or reimplement minimal WinHTTP POST here.

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

    std::cout << "[Rollback] detectRegression" << " tpsReg=" << tpsReg << " pplReg=" << pplReg
            << " lastTPS=" << lastTPS << " prevTPS=" << prevTPS
            << " lastPPL=" << lastPPL << " prevPPL=" << prevPPL << std::endl;

    return tpsReg || pplReg;
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    if (!runProcess("git", {"revert", "--no-edit", "HEAD"}, 60000)) {
        std::cerr << "[Rollback] git revert failed or timed out" << std::endl;
        return false;
    }
    std::cout << "[Rollback] git revert SUCCESS" << std::endl;
    return true;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const std::string& title, const std::string& body) {
    char* token = nullptr;
    size_t len = 0;
    _dupenv_s(&token, &len, "GITHUB_TOKEN");
    
    if (!token || len == 0) {
        std::cerr << "[Rollback] GITHUB_TOKEN not set - skipping issue" << std::endl;
        if (token) free(token);
        return true; // allow in dev
    }
    std::string tokenStr(token);
    free(token);

    nlohmann::json issue;
    issue["title"] = title;
    issue["body"] = body;
    issue["labels"] = {"regression", "auto"};

    // Using CURL via system for quickly opening issue without full WinHTTP implementation redundancy
    // or re-implement WinHTTP POST.
    // Given usage of "github-pull-request" tools in the instruction, maybe the agent should rely on tools?
    // But this is runtime code.
    // Let's implement a simple WinHTTP POST.
    
    // ... Mocking/Simplifying for now as this is a "SelfCode" agent that edits code.
    // std::cout << "Would post issue to GitHub: " << title << std::endl;
    
    std::string jsonStr = issue.dump();
    // Escape quotes for command line
    std::string escapedJson = ""; 
    for(char c : jsonStr) {
        if(c == '"') escapedJson += "\\\"";
        else escapedJson += c;
    }
    
    // Fallback to curl if available in path (common in Windows Git Bash/PowerShell env)
    // curl -X POST -H "Authorization: Bearer <token>" -H "Content-Type: application/json" -d <json> URL
    std::string cmd = "curl -X POST -H \"Authorization: Bearer " + tokenStr + "\" -H \"Content-Type: application/json\" -d \"" + escapedJson + "\" https://api.github.com/repos/ItsMehRAWRXD/RawrXD-ModelLoader/issues";
    
    if (system(cmd.c_str()) == 0) {
        std::cout << "[Rollback] GitHub issue opened: " << title << std::endl;
        return true;
    } else {
        std::cerr << "[Rollback] GitHub issue failed" << std::endl;
        return false;
    }
}
