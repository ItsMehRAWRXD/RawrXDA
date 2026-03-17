// Win32IDE Build System Integration
// Wires ToolchainBridge into the IDE menu/command system

#include "Win32IDE.h"
#include <commctrl.h>

// Build menu IDM defines (must match Win32IDE.cpp)
#ifndef IDM_BUILD_PROJECT
#define IDM_BUILD_PROJECT 5001
#define IDM_BUILD_CLEAN 5002
#define IDM_BUILD_REBUILD 5003
#define IDM_BUILD_RUN 5004
#define IDM_BUILD_STOP 5005
#define IDM_BUILD_CONFIG_DEBUG 5006
#define IDM_BUILD_CONFIG_RELEASE 5007
#define IDM_BUILD_ASM_CURRENT 5008
#define IDM_BUILD_SET_TARGET 5009
#define IDM_BUILD_SHOW_LOG 5010
#endif

void Win32IDE::initializeBuildSystem() {
    m_toolchain = std::make_unique<RawrXD::ToolchainBridge>();

    // Resolve project root from current working directory or loaded file
    char cwd[MAX_PATH] = {};
    GetCurrentDirectoryA(MAX_PATH, cwd);
    m_toolchain->setProjectRoot(cwd);

    // Discover toolchain in background
    std::thread([this]() {
        if (m_toolchain->discoverToolchain()) {
            // Post message to UI thread to update status bar
            if (IsWindow(m_hwndMain)) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0,
                    (LPARAM)"[+] Toolchain: VS2022 ready");
            }
        } else {
            if (IsWindow(m_hwndMain)) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0,
                    (LPARAM)"[!] Toolchain: Not found");
            }
        }
    }).detach();

    appendToOutput("Build system initialized", "Build", OutputSeverity::Info);
}

void Win32IDE::onBuildProject() {
    if (!m_toolchain) initializeBuildSystem();
    if (m_toolchain->isBuilding()) {
        appendToOutput("Build already in progress", "Build", OutputSeverity::Warning);
        return;
    }

    appendToOutput("=== Build Started ===", "Build", OutputSeverity::Info);

    m_toolchain->buildProject(
        // Output callback — runs on build thread, appends to output panel
        [this](const std::string& line, bool isError) {
            if (IsWindow(m_hwndMain)) {
                appendToOutput(line, "Build",
                    isError ? OutputSeverity::Error : OutputSeverity::Info);
            }
        },
        // Phase callback — updates status bar
        [this](RawrXD::BuildPhase phase, const std::string& detail) {
            if (!IsWindow(m_hwndMain)) return;
            std::string status;
            switch (phase) {
                case RawrXD::BuildPhase::Discovering: status = "Detecting toolchain..."; break;
                case RawrXD::BuildPhase::Assembling:  status = "MASM: " + detail; break;
                case RawrXD::BuildPhase::Compiling:   status = "C++: " + detail; break;
                case RawrXD::BuildPhase::Linking:     status = "Link: " + detail; break;
                case RawrXD::BuildPhase::Done:        status = "Build OK: " + detail; break;
                case RawrXD::BuildPhase::Failed:      status = "BUILD FAILED: " + detail; break;
                default: status = detail; break;
            }
            SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)status.c_str());
        }
    );
}

void Win32IDE::onBuildClean() {
    if (!m_toolchain) initializeBuildSystem();
    m_toolchain->buildClean();
    appendToOutput("Clean complete", "Build", OutputSeverity::Info);
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)"Cleaned");
}

void Win32IDE::onBuildRebuild() {
    if (!m_toolchain) initializeBuildSystem();
    if (m_toolchain->isBuilding()) {
        appendToOutput("Build already in progress", "Build", OutputSeverity::Warning);
        return;
    }

    appendToOutput("=== Rebuild All ===", "Build", OutputSeverity::Info);

    m_toolchain->rebuildAll(
        [this](const std::string& line, bool isError) {
            if (IsWindow(m_hwndMain))
                appendToOutput(line, "Build", isError ? OutputSeverity::Error : OutputSeverity::Info);
        },
        [this](RawrXD::BuildPhase phase, const std::string& detail) {
            if (IsWindow(m_hwndMain)) {
                std::string s = (phase == RawrXD::BuildPhase::Done) ? "Rebuild OK" :
                                (phase == RawrXD::BuildPhase::Failed) ? "REBUILD FAILED" : detail;
                SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0, (LPARAM)s.c_str());
            }
        }
    );
}

void Win32IDE::onBuildRun() {
    if (!m_toolchain) initializeBuildSystem();

    m_toolchain->runTarget(
        [this](const std::string& line, bool isError) {
            if (IsWindow(m_hwndMain))
                appendToOutput(line, "Build", isError ? OutputSeverity::Error : OutputSeverity::Info);
        }
    );
}

void Win32IDE::onBuildStop() {
    if (m_toolchain && m_toolchain->isBuilding()) {
        m_toolchain->stopBuild();
        appendToOutput("Build cancelled", "Build", OutputSeverity::Warning);
    }
}

void Win32IDE::onBuildAsmCurrent() {
    if (!m_toolchain) initializeBuildSystem();

    // Get current file from editor
    if (m_currentFilePath.empty()) {
        appendToOutput("No file open", "Build", OutputSeverity::Warning);
        return;
    }

    // Check if it's an ASM file
    std::string ext;
    auto dot = m_currentFilePath.rfind('.');
    if (dot != std::string::npos) ext = m_currentFilePath.substr(dot);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);

    if (ext != ".asm") {
        // Try compiling as C++ instead
        appendToOutput("[C++] " + m_currentFilePath, "Build", OutputSeverity::Info);
        auto r = m_toolchain->compileSingle(m_currentFilePath);
        if (r.success) {
            appendToOutput("[OK] Compiled in " + std::to_string((int)r.elapsedMs) + "ms", "Build", OutputSeverity::Info);
        } else {
            appendToOutput("[FAIL]\n" + r.output + "\n" + r.errors, "Build", OutputSeverity::Error);
        }
        return;
    }

    m_toolchain->assembleFile(m_currentFilePath,
        [this](const std::string& line, bool isError) {
            if (IsWindow(m_hwndMain))
                appendToOutput(line, "Build", isError ? OutputSeverity::Error : OutputSeverity::Info);
        }
    );
}

void Win32IDE::onBuildSetTarget() {
    if (!m_toolchain) initializeBuildSystem();

    // Simple input via message box with edit control
    char buf[260] = {};
    lstrcpyA(buf, m_toolchain->getTarget().c_str());

    // Use a simple prompt — in production this would be a proper dialog
    if (MessageBoxA(m_hwndMain,
            ("Current target: " + m_toolchain->getTarget() + "\n\nChange via command palette > Build: Set Target").c_str(),
            "Build Target", MB_OK | MB_ICONINFORMATION) == IDOK) {
        // For now, leave as-is since we need a proper input dialog
        // Users can set via command palette or config
    }
}

void Win32IDE::onBuildShowLog() {
    if (!m_toolchain) {
        appendToOutput("No build log available", "Build", OutputSeverity::Info);
        return;
    }
    appendToOutput("=== Build Log ===\n" + m_toolchain->lastBuildLog(), "Build", OutputSeverity::Info);
}

void Win32IDE::onBuildSetConfig(RawrXD::BuildConfig config) {
    if (!m_toolchain) initializeBuildSystem();
    m_toolchain->setConfig(config);
    const char* name = (config == RawrXD::BuildConfig::Debug) ? "Debug" : "Release";
    appendToOutput(std::string("Build config: ") + name, "Build", OutputSeverity::Info);
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0,
        (LPARAM)(std::string("Config: ") + name).c_str());
}
