#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>
#include <fstream>
#include <cwchar>

static std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static void parseArgsAndMaybeRunDigest(Win32IDE& ide, const std::string& cmd) {
    // Very simple argument parser for: --auto-digest --src <path> --out <path>
    auto hasFlag = [&](const char* flag) {
        return cmd.find(flag) != std::string::npos;
    };
    if (!hasFlag("--auto-digest")) return;

    std::string src;
    std::string out;
    // extract tokens
    // support both --src="..." and --src ... forms
    auto extract = [&](const char* key) -> std::string {
        size_t pos = cmd.find(key);
        if (pos == std::string::npos) return std::string();
        pos += strlen(key);
        // skip space or equals
        while (pos < cmd.size() && (cmd[pos] == ' ' || cmd[pos] == '=')) ++pos;
        if (pos >= cmd.size()) return std::string();
        // quoted value
        if (cmd[pos] == '"') {
            ++pos;
            size_t end = cmd.find('"', pos);
            if (end == std::string::npos) return std::string();
            return cmd.substr(pos, end - pos);
        }
        // until next space
        size_t end = cmd.find(' ', pos);
        if (end == std::string::npos) end = cmd.size();
        return cmd.substr(pos, end - pos);
    };

    src = extract("--src");
    out = extract("--out");
    if (src.empty() || out.empty()) {
        LOG_ERROR("--auto-digest requires --src and --out");
        return;
    }
    ide.queueDigestionJob(widen(src), widen(out));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Initialize logger early
    try {
        IDELogger::getInstance().initialize("C:\\RawrXD_IDE.log");
        IDELogger::getInstance().setLevel(IDELogger::Level::DEBUG);
        LOG_INFO("WinMain started - RawrXD Win32 IDE initializing");
    } catch (...) {
        // Fallback to file diagnostic if logger fails
        std::ofstream errLog("C:\\LOGGER_INIT_FAILED.txt");
        errLog << "Logger initialization threw exception" << std::endl;
        errLog.close();
    }
    
    LOG_DEBUG("Creating Win32IDE instance");
    Win32IDE ide(hInstance);
    LOG_DEBUG("Win32IDE constructor completed");

    if (!ide.createWindow()) {
        LOG_ERROR("createWindow() failed");
        MessageBoxA(nullptr, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    LOG_INFO("Main window created successfully");
    // Optional auto-digest on startup for smoke tests
    try {
        std::string cmd = lpCmdLine ? std::string(lpCmdLine) : std::string();
        parseArgsAndMaybeRunDigest(ide, cmd);
    } catch(...) {
        // ignore arg parsing errors
    }
    ide.showWindow();
    LOG_INFO("Entering message loop");
    int rc = ide.runMessageLoop();
    LOG_INFO("Message loop exited with code " + std::to_string(rc));
    return rc;
}
