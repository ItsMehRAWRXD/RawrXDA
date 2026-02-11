// zero_touch.cpp – Qt-free ZeroTouch implementation (C++20 / Win32)
// Provides automated dev-environment bootstrapping: file watchers, git hooks, voice triggers
#include "zero_touch.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void zt_log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[ZeroTouch] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static bool writeTextFile(const fs::path& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << content;
    return ofs.good();
}

// ---------------------------------------------------------------------------
// ZeroTouch
// ---------------------------------------------------------------------------

ZeroTouch::ZeroTouch() = default;

void ZeroTouch::installAll() {
    zt_log("Installing all zero-touch components...");
    installFileWatcher();
    installGitHook();
    installVoiceTrigger();
    zt_log("All zero-touch components installed.");
}

// ---------------------------------------------------------------------------
// File watcher – creates a .vscode/settings.json entry for watcherExclude
// (actual directory monitoring is handled by the Win32IDE / HeadlessIDE loop)
// ---------------------------------------------------------------------------
void ZeroTouch::installFileWatcher() {
    zt_log("Installing file watcher configuration...");

    fs::path vscodePath = fs::current_path() / ".vscode";
    std::error_code ec;
    fs::create_directories(vscodePath, ec);
    if (ec) {
        zt_log("  WARNING: Could not create .vscode directory: %s", ec.message().c_str());
        return;
    }

    fs::path settingsPath = vscodePath / "settings.json";
    if (fs::exists(settingsPath)) {
        zt_log("  .vscode/settings.json already exists – skipping watcher config.");
        return;
    }

    const char* defaultSettings =
        "{\n"
        "  \"files.watcherExclude\": {\n"
        "    \"**/build/**\": true,\n"
        "    \"**/.git/objects/**\": true,\n"
        "    \"**/node_modules/**\": true\n"
        "  }\n"
        "}\n";

    if (writeTextFile(settingsPath, defaultSettings)) {
        zt_log("  Created %s", settingsPath.string().c_str());
    } else {
        zt_log("  WARNING: Failed to write %s", settingsPath.string().c_str());
    }
}

// ---------------------------------------------------------------------------
// Git hook – installs a pre-commit hook that runs self_test_gate
// ---------------------------------------------------------------------------
void ZeroTouch::installGitHook() {
    zt_log("Installing git pre-commit hook...");

    fs::path gitHooksDir = fs::current_path() / ".git" / "hooks";
    if (!fs::is_directory(gitHooksDir)) {
        zt_log("  No .git/hooks directory found – not a git repo? Skipping.");
        return;
    }

    fs::path hookPath = gitHooksDir / "pre-commit";
    if (fs::exists(hookPath)) {
        // Check if it's ours
        std::ifstream existing(hookPath);
        std::string firstLine;
        if (std::getline(existing, firstLine) && firstLine.find("RawrXD") != std::string::npos) {
            zt_log("  RawrXD pre-commit hook already installed – skipping.");
            return;
        }
        zt_log("  WARNING: pre-commit hook already exists (third-party) – not overwriting.");
        return;
    }

    const char* hookScript =
        "#!/bin/sh\n"
        "# RawrXD auto-installed pre-commit hook\n"
        "# Runs the self-test gate before allowing commits\n"
        "echo \"[RawrXD] Running self-test gate...\"\n"
        "if command -v ./build/self_test_gate >/dev/null 2>&1; then\n"
        "  ./build/self_test_gate\n"
        "  exit $?\n"
        "elif command -v ./build/Release/self_test_gate.exe >/dev/null 2>&1; then\n"
        "  ./build/Release/self_test_gate.exe\n"
        "  exit $?\n"
        "else\n"
        "  echo \"[RawrXD] self_test_gate not found – skipping.\"\n"
        "  exit 0\n"
        "fi\n";

    if (writeTextFile(hookPath, hookScript)) {
        zt_log("  Installed pre-commit hook at %s", hookPath.string().c_str());
    } else {
        zt_log("  WARNING: Failed to write pre-commit hook.");
    }
}

// ---------------------------------------------------------------------------
// Voice trigger – placeholder for future Web Speech / audio integration
// ---------------------------------------------------------------------------
void ZeroTouch::installVoiceTrigger() {
    zt_log("Voice trigger stub installed (no-op until audio subsystem is integrated).");
    m_lastVoiceWish.clear();
    // Future: register a named-pipe or localhost HTTP endpoint that the
    // GUI voice engine can POST transcribed commands to.
    // For now the IDE chatbot handles voice via the HTML frontend.
}
