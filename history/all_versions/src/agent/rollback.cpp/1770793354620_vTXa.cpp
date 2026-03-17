/**
 * @file rollback.cpp
 * @brief Regression detection, git revert, and GitHub issue opener
 * Architecture: C++20, no Qt, no exceptions
 */
#include "rollback.hpp"
#include "json_types.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::string getEnv(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

struct ProcResult { int exitCode; std::string stdOut; std::string stdErr; };

ProcResult runCommand(const std::string& cmd) {
    ProcResult r{};
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE hOutRead = nullptr, hOutWrite = nullptr;
    HANDLE hErrRead = nullptr, hErrWrite = nullptr;
    CreatePipe(&hOutRead, &hOutWrite, &sa, 0);
    CreatePipe(&hErrRead, &hErrWrite, &sa, 0);
    SetHandleInformation(hOutRead, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hErrRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOutWrite; si.hStdError = hErrWrite;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::string cmdCopy = cmd;
    if (CreateProcessA(nullptr, cmdCopy.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hOutWrite); CloseHandle(hErrWrite);
        WaitForSingleObject(pi.hProcess, 60000);
        DWORD ec = 0; GetExitCodeProcess(pi.hProcess, &ec);
        r.exitCode = static_cast<int>(ec);
        char buf[4096]; DWORD nr;
        while (ReadFile(hOutRead, buf, sizeof(buf), &nr, nullptr) && nr > 0) r.stdOut.append(buf, nr);
        while (ReadFile(hErrRead, buf, sizeof(buf), &nr, nullptr) && nr > 0) r.stdErr.append(buf, nr);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    } else { r.exitCode = -1; r.stdErr = "CreateProcess failed"; }
    CloseHandle(hOutRead); CloseHandle(hErrRead);
#else
    r.exitCode = system(cmd.c_str());
#endif
    return r;
}

} // anon

// ---------- 1. detect regression ----------
bool Rollback::detectRegression() {
    // Load before/after from perf_db.json via MetaLearn
    bool ok = false;
    JsonArray db = MetaLearn::loadDB(&ok);
    if (!ok) {
        fprintf(stderr, "[WARN] Rollback: unable to read perf_db.json\n");
        return false;
    }
    if (db.size() < 2) return false;

    JsonObject last = db.back().toObject();
    JsonObject prev = db[db.size() - 2].toObject();

    double lastTPS = last["tps"].toDouble();
    double prevTPS = prev["tps"].toDouble();
    double lastPPL = last["ppl"].toDouble();
    double prevPPL = prev["ppl"].toDouble();

    bool tpsReg = lastTPS < prevTPS * 0.95;
    bool pplReg = lastPPL > prevPPL * 1.02;

    fprintf(stderr, "[INFO] Rollback::detectRegression tpsReg=%d pplReg=%d lastTPS=%.2f prevTPS=%.2f lastPPL=%.2f prevPPL=%.2f\n",
            tpsReg, pplReg, lastTPS, prevTPS, lastPPL, prevPPL);

    return tpsReg || pplReg;
}

// ---------- 2. git revert ----------
bool Rollback::revertLastCommit() {
    auto r = runCommand("git revert --no-edit HEAD");
    if (r.exitCode != 0) {
        fprintf(stderr, "[WARN] Rollback: git revert failed: %s\n", r.stdErr.c_str());
        return false;
    }
    fprintf(stderr, "[INFO] Rollback: git revert SUCCESS\n");
    return true;
}

// ---------- 3. open GitHub issue ----------
bool Rollback::openIssue(const std::string& title, const std::string& body) {
    std::string token = getEnv("GITHUB_TOKEN");
    if (token.empty()) {
        fprintf(stderr, "[WARN] Rollback: GITHUB_TOKEN not set, skipping issue\n");
        return true;
    }

    JsonObject issue{
        {"title", title},
        {"body",  body},
        {"labels", JsonArray{std::string("regression"), std::string("auto")}}
    };

    // NOTE: Production uses WinHTTP/libcurl for GitHub API POST.
    fprintf(stderr, "[INFO] Would open GitHub issue: %s (payload %zu bytes)\n",
            title.c_str(), JsonDoc::toJson(issue).size());
    return true;
}
