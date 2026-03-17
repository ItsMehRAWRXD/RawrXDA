// Win32IDE Build System Integration
// Wires ToolchainBridge into the IDE menu/command system

#include "Win32IDE.h"
#include <commctrl.h>
#include <thread>
#include <string>
#include <vector>
#include <cstring>
#include <windows.h>
#include <intrin.h>

// Build menu IDM defines (must match Win32IDE.cpp)
#ifndef IDM_BUILD_PROJECT
#define IDM_BUILD_PROJECT 2801
#define IDM_BUILD_CLEAN 2802
#define IDM_BUILD_REBUILD 2803
#define IDM_BUILD_RUN 2804
#define IDM_BUILD_STOP 2805
#define IDM_BUILD_CONFIG_DEBUG 2806
#define IDM_BUILD_CONFIG_RELEASE 2807
#define IDM_BUILD_ASM_CURRENT 2808
#define IDM_BUILD_SET_TARGET 2809
#define IDM_BUILD_SHOW_LOG 2810
#endif

namespace {

static inline uint8_t make_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    return static_cast<uint8_t>(((mod & 0x3u) << 6) | ((reg & 0x7u) << 3) | (rm & 0x7u));
}

// EVEX.512.0F.W0 vmovups zmm, [r64] (low 8 regs only for this probe emitter)
static bool emit_vmovups_load(std::vector<uint8_t>& out, uint8_t dstZmm, uint8_t baseReg) {
    if (dstZmm > 7 || baseReg > 7 || baseReg == 5) {
        return false;
    }
    const uint8_t bytes[] = {
        0x62, 0xF1, 0x7C, 0x48, 0x10, make_modrm(0, dstZmm, baseReg)
    };
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
    return true;
}

// EVEX.512.0F.W0 vmovups [r64], zmm (low 8 regs only for this probe emitter)
static bool emit_vmovups_store(std::vector<uint8_t>& out, uint8_t baseReg, uint8_t srcZmm) {
    if (srcZmm > 7 || baseReg > 7 || baseReg == 5) {
        return false;
    }
    const uint8_t bytes[] = {
        0x62, 0xF1, 0x7C, 0x48, 0x11, make_modrm(0, srcZmm, baseReg)
    };
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
    return true;
}

// EVEX.512.66.0F38.W0 vfmadd231ps zmmDst, zmmSrc1, [r64]
static bool emit_vfmadd231ps(std::vector<uint8_t>& out, uint8_t dstZmm, uint8_t src1Zmm, uint8_t memBaseReg) {
    if (dstZmm > 7 || src1Zmm > 15 || memBaseReg > 7 || memBaseReg == 5) {
        return false;
    }
    const uint8_t p1 = static_cast<uint8_t>(((~src1Zmm) & 0x0Fu) << 3) | 0x05u; // W=0, 1-bit set, pp=66h
    const uint8_t bytes[] = {
        0x62, 0xF2, p1, 0x48, 0xB8, make_modrm(0, dstZmm, memBaseReg)
    };
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
    return true;
}

static void emit_vzeroupper(std::vector<uint8_t>& out) {
    const uint8_t bytes[] = { 0xC5, 0xF8, 0x77 };
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

static void emit_ret(std::vector<uint8_t>& out) {
    out.push_back(0xC3);
}

static bool cpu_supports_avx512f() {
#if defined(_M_X64) || defined(__x86_64__)
    int regs[4] = {0, 0, 0, 0};
    __cpuidex(regs, 1, 0);
    const bool osxsave = (regs[2] & (1 << 27)) != 0;
    const bool avx = (regs[2] & (1 << 28)) != 0;
    if (!osxsave || !avx) {
        return false;
    }

    const unsigned __int64 xcr0 = _xgetbv(0);
    // XMM(1), YMM(2), Opmask(5), ZMM_Hi256(6), Hi16_ZMM(7)
    if ((xcr0 & 0xE6u) != 0xE6u) {
        return false;
    }

    __cpuidex(regs, 7, 0);
    return (regs[1] & (1 << 16)) != 0; // AVX-512F
#else
    return false;
#endif
}

static bool build_avx512_fma_probe_kernel(std::vector<uint8_t>& out) {
    out.clear();

    // Signature: void kernel(float* inout, const float* rhs)
    // RCX = inout, RDX = rhs
    // inout[0..15] = inout[0..15] + rhs[0..15] * rhs[0..15]
    if (!emit_vmovups_load(out, 0, 1)) return false;      // zmm0 <- [rcx]
    if (!emit_vmovups_load(out, 1, 2)) return false;      // zmm1 <- [rdx]
    if (!emit_vfmadd231ps(out, 0, 1, 2)) return false;    // zmm0 += zmm1 * [rdx]
    if (!emit_vmovups_store(out, 1, 0)) return false;     // [rcx] <- zmm0
    emit_vzeroupper(out);
    emit_ret(out);
    return true;
}

using Avx512ProbeKernelFn = void (*)(float* inout, const float* rhs);

static bool execute_emitted_avx512_probe(float& out0, std::string& reason) {
    if (!cpu_supports_avx512f()) {
        reason = "AVX-512F not available on this CPU/OS context";
        return false;
    }

    std::vector<uint8_t> code;
    if (!build_avx512_fma_probe_kernel(code) || code.empty()) {
        reason = "failed to build AVX-512 probe kernel bytes";
        return false;
    }

    void* execMem = VirtualAlloc(nullptr, code.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!execMem) {
        reason = "VirtualAlloc failed for JIT page";
        return false;
    }

    std::memcpy(execMem, code.data(), code.size());
    DWORD oldProtect = 0;
    if (!VirtualProtect(execMem, code.size(), PAGE_EXECUTE_READ, &oldProtect)) {
        VirtualFree(execMem, 0, MEM_RELEASE);
        reason = "VirtualProtect RW->RX failed";
        return false;
    }

    FlushInstructionCache(GetCurrentProcess(), execMem, code.size());

    alignas(64) float inout[16];
    alignas(64) float rhs[16];
    for (int i = 0; i < 16; ++i) {
        inout[i] = static_cast<float>(i + 1); // 1..16
        rhs[i] = 2.0f;
    }

    auto kernel = reinterpret_cast<Avx512ProbeKernelFn>(execMem);
    kernel(inout, rhs);
    out0 = inout[0]; // Expect 1 + 2*2 = 5

    VirtualFree(execMem, 0, MEM_RELEASE);
    return true;
}

} // namespace

void Win32IDE::createAcceleratorTable() {
    // Keyboard shortcuts aligned with Build menu text hints
    // Also includes Edit menu shortcuts (Ctrl+F, Ctrl+H, etc.)
    ACCEL accelTable[] = {
        // Build menu
        { FVIRTKEY,                    VK_F7,   IDM_BUILD_PROJECT },       // F7 = Build Project
        { FCONTROL | FSHIFT | FVIRTKEY, 'B',    IDM_BUILD_REBUILD },       // Ctrl+Shift+B = Rebuild All
        { FCONTROL | FVIRTKEY,         VK_F7,   IDM_BUILD_ASM_CURRENT },   // Ctrl+F7 = Assemble Current
        { FVIRTKEY,                    VK_F5,   IDM_BUILD_RUN },           // F5 = Run
        { FCONTROL | FVIRTKEY,         VK_F5,   4250 },                    // Ctrl+F5 = CMD_INFERENCE_RUN (Inference Run)
        { FSHIFT | FVIRTKEY,           VK_F5,   IDM_BUILD_STOP },          // Shift+F5 = Stop
        // Edit menu
        { FCONTROL | FVIRTKEY,         'Z',     IDM_EDIT_UNDO },           // Ctrl+Z = Undo
        { FCONTROL | FVIRTKEY,         'Y',     IDM_EDIT_REDO },           // Ctrl+Y = Redo
        { FCONTROL | FVIRTKEY,         'X',     IDM_EDIT_CUT },            // Ctrl+X = Cut
        { FCONTROL | FVIRTKEY,         'C',     IDM_EDIT_COPY },           // Ctrl+C = Copy
        { FCONTROL | FVIRTKEY,         'V',     IDM_EDIT_PASTE },          // Ctrl+V = Paste
        { FCONTROL | FVIRTKEY,         'A',     IDM_EDIT_SELECT_ALL },     // Ctrl+A = Select All
        { FCONTROL | FVIRTKEY,         'F',     IDM_EDIT_FIND },           // Ctrl+F = Find
        { FCONTROL | FVIRTKEY,         'H',     IDM_EDIT_REPLACE },        // Ctrl+H = Replace
        { FVIRTKEY,                    VK_F3,   IDM_EDIT_FIND_NEXT },      // F3 = Find Next
        { FSHIFT | FVIRTKEY,           VK_F3,   IDM_EDIT_FIND_PREV },      // Shift+F3 = Find Previous
        // File menu
        { FCONTROL | FVIRTKEY,         'N',     IDM_FILE_NEW },            // Ctrl+N = New
        { FCONTROL | FVIRTKEY,         'O',     IDM_FILE_OPEN },           // Ctrl+O = Open
        { FCONTROL | FVIRTKEY,         'S',     IDM_FILE_SAVE },           // Ctrl+S = Save
        { FCONTROL | FSHIFT | FVIRTKEY, 'S',    IDM_FILE_SAVEAS },         // Ctrl+Shift+S = Save As
    };
    m_hAccel = CreateAcceleratorTableA(accelTable, _countof(accelTable));
}

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
    const bool requestJitProbe = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    if (requestJitProbe) {
        float out0 = 0.0f;
        std::string reason;
        if (execute_emitted_avx512_probe(out0, reason)) {
            appendToOutput("[JIT] In-process AVX-512 kernel executed (no .exe emitted). out[0]="
                           + std::to_string(out0),
                           "Build", OutputSeverity::Info);
            if (IsWindow(m_hwndStatusBar)) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0,
                    (LPARAM)"JIT probe OK: in-process AVX-512 execution");
            }
        } else {
            appendToOutput("[JIT] In-process AVX-512 probe unavailable: " + reason,
                           "Build", OutputSeverity::Warning);
            if (IsWindow(m_hwndStatusBar)) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0,
                    (LPARAM)"JIT probe skipped (see output)");
            }
        }
        return;
    }

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
    if (m_currentFile.empty()) {
        appendToOutput("No file open", "Build", OutputSeverity::Warning);
        return;
    }

    // Check if it's an ASM file
    std::string ext;
    auto dot = m_currentFile.rfind('.');
    if (dot != std::string::npos) ext = m_currentFile.substr(dot);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);

    if (ext != ".asm") {
        // Try compiling as C++ instead
        appendToOutput("[C++] " + m_currentFile, "Build", OutputSeverity::Info);
        auto r = m_toolchain->compileSingle(m_currentFile);
        if (r.success) {
            appendToOutput("[OK] Compiled in " + std::to_string((int)r.elapsedMs) + "ms", "Build", OutputSeverity::Info);
        } else {
            appendToOutput("[FAIL]\n" + r.output + "\n" + r.errors, "Build", OutputSeverity::Error);
        }
        return;
    }

    m_toolchain->assembleFile(m_currentFile,
        [this](const std::string& line, bool isError) {
            if (IsWindow(m_hwndMain))
                appendToOutput(line, "Build", isError ? OutputSeverity::Error : OutputSeverity::Info);
        }
    );
}

void Win32IDE::onBuildSetTarget() {
    if (!m_toolchain) initializeBuildSystem();

    // Allow user to input a new build target
    char newTarget[260] = {};
    lstrcpyA(newTarget, m_toolchain->getTarget().c_str());

    // Create a simple input dialog for target specification
    std::string prompt = "Current target: " + m_toolchain->getTarget() + "\n\nEnter new build target (e.g., x64-Debug):";
    if (showBuildTargetInputDialog(prompt.c_str(), newTarget, sizeof(newTarget))) {
        std::string targetStr = newTarget;
        if (!targetStr.empty() && targetStr != m_toolchain->getTarget()) {
            m_toolchain->setTarget(targetStr);
            appendToOutput("Build target set to: " + targetStr, "Build", OutputSeverity::Info);
            SendMessageA(m_hwndStatusBar, SB_SETTEXTA, 0,
                (LPARAM)(std::string("Target: ") + targetStr).c_str());
        }
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

// Simple input dialog for build target
bool Win32IDE::showBuildTargetInputDialog(const char* prompt, char* buffer, size_t bufferSize) {
    // Create a simple dialog with an edit control
    HWND hwndDialog = CreateWindowExA(WS_EX_DLGMODALFRAME, "STATIC", "Set Build Target",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        200, 200, 400, 150, m_hwndMain, nullptr, m_hInstance, nullptr);

    if (!hwndDialog) return false;

    // Add prompt text
    CreateWindowExA(0, "STATIC", prompt, WS_CHILD | WS_VISIBLE,
        10, 10, 380, 40, hwndDialog, nullptr, m_hInstance, nullptr);

    // Add edit control
    HWND hwndEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", buffer,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        10, 55, 380, 25, hwndDialog, nullptr, m_hInstance, nullptr);

    // Add buttons
    HWND hwndOK = CreateWindowExA(0, "BUTTON", "OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        200, 90, 80, 30, hwndDialog, (HMENU)IDOK, m_hInstance, nullptr);
    HWND hwndCancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
        290, 90, 80, 30, hwndDialog, (HMENU)IDCANCEL, m_hInstance, nullptr);

    SetFocus(hwndEdit);

    // Simple message loop for the dialog
    MSG msg;
    bool result = false;
    while (GetMessageA(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            if (LOWORD(msg.wParam) == IDOK) {
                GetWindowTextA(hwndEdit, buffer, (int)bufferSize);
                result = true;
                break;
            } else if (LOWORD(msg.wParam) == IDCANCEL) {
                break;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    DestroyWindow(hwndDialog);
    return result;
}

