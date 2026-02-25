// Win32IDE_BuildRunner.cpp — Unified Build Pipeline (P0)
// Runs CMake/Ninja build, parses output, pushes errors to ProblemsAggregator.
#include "Win32IDE.h"
#include "core/problems_aggregator.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <regex>
#include <thread>
#include <atomic>

namespace {

// Avoid clashing with Win32IDE::appendToOutput (member) inside lambdas that capture `this`.
static void appendBuildOutput(Win32IDE* ide, const std::string& line) {
    if (ide && line.size())
        ide->appendToOutput(line + "\n");
}

void parseAndReport(const std::string& line, Win32IDE* ide) {
    using namespace RawrXD;
    auto& agg = ProblemsAggregator::instance();

    // MSVC: file(line,col): error C1234: message
    std::regex msvcRe(R"((.+?)\((\d+)(?:,(\d+))?\)\s*:\s*(error|warning|note)\s+(\w+\d+)?\s*:\s*(.+))");
    // GCC/Clang: file:line:column: error: message
    std::regex gccRe(R"(([^:]+):(\d+):(\d+):\s*(error|warning|note):\s*(.+))");
    // CMake Error at file:line (message):
    std::regex cmakeRe(R"(CMake Error at (.+?):(\d+))");

    std::smatch m;
    std::string path, code, msg;
    int lineNo = 0, col = 0;
    int severity = 2; // 1=Error, 2=Warning, 3=Info

    if (std::regex_match(line, m, msvcRe) && m.size() >= 5) {
        path = m[1].str();
        lineNo = std::stoi(m[2].str());
        if (m[3].matched && m[3].length()) col = std::stoi(m[3].str());
        std::string kind = m[4].str();
        code = m[5].matched ? m[5].str() : "MSVC";
        msg = m[6].str();
        if (kind == "error") severity = 1;
        else if (kind == "warning") severity = 2;
        else severity = 3;
        agg.add("Build", path, lineNo, col, severity, code, msg, code);
        return;
    }
    if (std::regex_match(line, m, gccRe) && m.size() >= 5) {
        path = m[1].str();
        lineNo = std::stoi(m[2].str());
        col = std::stoi(m[3].str());
        std::string kind = m[4].str();
        msg = m[5].str();
        if (kind == "error") severity = 1;
        else if (kind == "warning") severity = 2;
        else severity = 3;
        agg.add("Build", path, lineNo, col, severity, "GCC", msg, "GCC");
        return;
    }
    if (std::regex_match(line, m, cmakeRe) && m.size() >= 3) {
        path = m[1].str();
        lineNo = std::stoi(m[2].str());
        agg.add("Build", path, lineNo, 0, 1, "CMAKE", "CMake configuration error", "CMAKE");
    }
}

} // namespace

void Win32IDE::runBuildInBackground(const std::string& workingDir, const std::string& buildCommand) {
    RawrXD::ProblemsAggregator::instance().clear("Build");

    std::string cmd = buildCommand.empty()
        ? "cmake --build build --config Release"
        : buildCommand;

    std::thread([this, workingDir, cmd]() {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        HANDLE hRead = nullptr, hWrite = nullptr;
        if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return;
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        std::string cmdLine = "cmd /c " + cmd;
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');
        BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
            0, nullptr, workingDir.empty() ? nullptr : workingDir.c_str(), &si, &pi);

        CloseHandle(hWrite);
        if (!ok) {
            CloseHandle(hRead);
            appendBuildOutput(this, "[Build] CreateProcess failed.");
            return;
        }

        CloseHandle(pi.hThread);
        char buf[1024];
        DWORD read = 0;
        std::string lineBuf;

        while (ReadFile(hRead, buf, sizeof(buf) - 1, &read, nullptr) && read > 0) {
            buf[read] = '\0';
            lineBuf += buf;
            size_t pos;
            while ((pos = lineBuf.find('\n')) != std::string::npos) {
                std::string line = lineBuf.substr(0, pos);
                lineBuf.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                appendBuildOutput(this, line);
                parseAndReport(line, this);
            }
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(hRead);
        appendBuildOutput(this, "[Build] Done.");
        refreshProblemsView();
    }).detach();
}
