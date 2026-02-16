// ============================================================================
// cli_main_headless.cpp — Pure CLI entry point for RawrXD (101% parity)
// ============================================================================
//
// Standalone console binary that mirrors the Win32 GUI IDE in full parity.
// Uses HeadlessIDE with the exact same engine: chat, agentic autonomous loop,
// swarm, CoT, tools, Ollama/Local GGUF, HTTP API, LSP, etc.
//
// Default: Interactive REPL (chat + agentic). Use --prompt, --input for batch.
// NO GUI. NO WinMain. Console subsystem only.
//
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "HeadlessIDE.h"

// ============================================================================
// Set CWD to exe directory (same as main_win32)
// ============================================================================
static void setCwdToExeDirectory() {
    char exePath[MAX_PATH];
    if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) == 0) return;
    std::string exeDir(exePath);
    size_t last = exeDir.find_last_of("\\/");
    if (last != std::string::npos) exeDir = exeDir.substr(0, last);
    if (!exeDir.empty()) SetCurrentDirectoryA(exeDir.c_str());
}

// ============================================================================
// Inject --repl when no mode-affecting args (default: interactive chat)
// Returns possibly-modified argc (caller passes to initialize).
// ============================================================================
static int ensureDefaultReplMode(int argc, char* argv[], std::vector<std::string>& replArgs) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--prompt" || a == "--input" || a == "--repl" ||
            a == "--help" || a == "-h") {
            return argc;  // Mode already specified, use original args
        }
    }
    replArgs.push_back(argv[0]);
    replArgs.push_back("--repl");
    for (int i = 1; i < argc; ++i) replArgs.push_back(argv[i]);
    return static_cast<int>(replArgs.size());
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    setCwdToExeDirectory();

    std::vector<std::string> replArgs;
    int effectiveArgc = ensureDefaultReplMode(argc, argv, replArgs);

    char** effectiveArgv = argv;
    std::vector<char*> replArgv;
    if (!replArgs.empty()) {
        for (auto& s : replArgs) replArgv.push_back(&s[0]);
        replArgv.push_back(nullptr);
        effectiveArgv = replArgv.data();
    }

    HeadlessIDE headless;
    HeadlessResult r = headless.initialize(effectiveArgc, effectiveArgv);
    if (!r.success) {
        if (r.errorCode == 0) return 0;  // --help requested
        fprintf(stderr, "RawrXD CLI init failed: %s (code %d)\n", r.detail, r.errorCode);
        return r.errorCode;
    }

    return headless.run();
}
