#include "Win32IDE.h"
#include "HeadlessIDE.h"
#include "../../include/rawrxd_version.h"
#include "../../include/final_gauntlet.h"
#include "../modules/vsix_loader.h"
#include "../modules/memory_manager.h"
#include "../modules/engine_manager.h"
#include "../modules/codex_ultimate.h"
#include <commctrl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================================
// Headless mode detection — scans argv for --headless flag
// ============================================================================
static bool hasHeadlessFlag(LPSTR lpCmdLine) {
    if (!lpCmdLine) return false;
    return strstr(lpCmdLine, "--headless") != nullptr;
}

// ============================================================================
// Parse WinMain command line into argc/argv for headless mode
// ============================================================================
static void parseCmdLine(LPSTR lpCmdLine, int& argc, char**& argv) {
    static std::vector<std::string> args;
    static std::vector<char*> ptrs;

    args.clear();
    ptrs.clear();
    args.push_back("RawrXD-Win32IDE.exe");

    if (lpCmdLine && lpCmdLine[0]) {
        std::string cmdLine(lpCmdLine);
        std::istringstream iss(cmdLine);
        std::string token;
        while (iss >> token) {
            // Handle quoted arguments
            if (!token.empty() && token.front() == '"') {
                token = token.substr(1);
                std::string rest;
                while (token.back() != '"' && std::getline(iss, rest, '"')) {
                    token += " " + rest;
                }
                if (!token.empty() && token.back() == '"') {
                    token.pop_back();
                }
            }
            args.push_back(token);
        }
    }

    for (auto& a : args) ptrs.push_back(&a[0]);
    ptrs.push_back(nullptr);

    argc = static_cast<int>(args.size());
    argv = ptrs.data();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nCmdShow) {

    // ========================================================================
    // HEADLESS MODE — Phase 19C
    // If --headless is present, skip all GUI initialization and run the
    // HeadlessIDE surface with console I/O + HTTP server.
    // ========================================================================
    if (hasHeadlessFlag(lpCmdLine)) {
        // Allocate a console for stdout/stderr (WinMain doesn't have one)
        AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);

        int argc = 0;
        char** argv = nullptr;
        parseCmdLine(lpCmdLine, argc, argv);

        HeadlessIDE headless;
        HeadlessResult r = headless.initialize(argc, argv);
        if (!r.success) {
            if (r.errorCode == 0) return 0;  // --help requested
            fprintf(stderr, "Headless init failed: %s (code %d)\n", r.detail, r.errorCode);
            return r.errorCode;
        }

        int exitCode = headless.run();

        FreeConsole();
        return exitCode;
    }

    // ========================================================================
    // GUI MODE — original Win32IDE path (unchanged)
    // ========================================================================
    // Initialize common controls
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_WIN95_CLASSES | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);
    
    // ========================================================================
    // FIRST-RUN GAUNTLET GATE (Phase 33: Gold Master)
    // On very first launch, run the full system verification gauntlet.
    // If it fails, warn the user but allow them to continue (non-blocking).
    // The flag file prevents re-running on subsequent launches.
    // ========================================================================
    {
        const char* gauntletFlag = "config\\first_run.flag";
        DWORD attrs = GetFileAttributesA(gauntletFlag);
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            // First run — execute gauntlet
            bool passed = RawrXD::Final::Gauntlet::RunAll(nullptr);
            if (!passed) {
                MessageBoxA(nullptr,
                    "System validation completed with warnings.\n"
                    "Check the Audit Dashboard (Ctrl+Shift+A) for details.\n\n"
                    "The IDE will continue to start normally.",
                    "RawrXD — First Run Check", MB_OK | MB_ICONWARNING);
            }
            // Create the flag file so we don't re-run
            CreateDirectoryA("config", nullptr);
            HANDLE hFlag = CreateFileA(gauntletFlag, GENERIC_WRITE, 0, nullptr,
                                       CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hFlag != INVALID_HANDLE_VALUE) {
                const char* stamp = RAWRXD_VERSION_STR " " __DATE__ " " __TIME__ "\n";
                DWORD written = 0;
                WriteFile(hFlag, stamp, (DWORD)strlen(stamp), &written, nullptr);
                CloseHandle(hFlag);
            }
        }
    }

    // Initialize managers
    VSIXLoader::GetInstance().Initialize("plugins");
    
    // Create IDE window
    Win32IDE ide(hInstance);
    
    if (!ide.createWindow()) {
        MessageBoxW(nullptr, L"Failed to initialize IDE", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Initialize engine manager (safe — LoadEngine may fail for missing DLLs)
    auto* engine_mgr = new EngineManager();
    try { engine_mgr->LoadEngine("engines/800b-5drive/800b_engine.dll", "800b-5drive"); } catch (...) {}
    try { engine_mgr->LoadEngine("engines/codex-ultimate/codex.dll", "codex-ultimate"); } catch (...) {}
    try { engine_mgr->LoadEngine("engines/rawrxd-compiler/compiler.dll", "rawrxd-compiler"); } catch (...) {}
    
    // Initialize Codex Ultimate
    auto* codex = new CodexUltimate();
    
    ide.setEngineManager(engine_mgr);
    ide.setCodexUltimate(codex);

    // Show window and force layout
    ide.showWindow();

    // Force layout — SendMessage WM_SIZE with current client rect
    {
        HWND hwnd = ide.getMainWindow();
        if (hwnd) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            SendMessage(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
            InvalidateRect(hwnd, nullptr, TRUE);
            UpdateWindow(hwnd);
        }
    }

    // Set focus to the editor so the caret appears and keyboard input works immediately
    {
        HWND editor = ide.getEditor();
        if (editor && IsWindow(editor)) {
            SetFocus(editor);
        }
    }

    // Diagnostic: dump child window info
    {
        std::ofstream diag("C:\\Users\\HiH8e\\Desktop\\WINDOW_DIAG.txt");
        HWND hwnd = ide.getMainWindow();
        RECT mainRc;
        GetClientRect(hwnd, &mainRc);
        diag << "Main window client: " << mainRc.right << "x" << mainRc.bottom << std::endl;
        diag << "m_hwndMain: " << (void*)hwnd << std::endl;

        auto dumpCtrl = [&](const char* name, HWND h) {
            if (!h) { diag << name << ": NULL" << std::endl; return; }
            RECT r; GetWindowRect(h, &r);
            int w = r.right - r.left, ht = r.bottom - r.top;
            BOOL vis = IsWindowVisible(h);
            diag << name << ": " << (void*)h << " pos=(" << r.left << "," << r.top << ") size=" << w << "x" << ht << " visible=" << vis << std::endl;
        };

        dumpCtrl("Toolbar", ide.getToolbar());
        dumpCtrl("Sidebar", ide.getSidebar());
        dumpCtrl("TabBar", ide.getTabBar());
        dumpCtrl("LineNumbers", ide.getLineNumbers());
        dumpCtrl("Editor", ide.getEditor());
        dumpCtrl("StatusBar", ide.getStatusBar());
        dumpCtrl("ActivityBar", ide.getActivityBar());
        diag << "Done." << std::endl;
    }
    
    // Run message loop
    return ide.runMessageLoop();
}
