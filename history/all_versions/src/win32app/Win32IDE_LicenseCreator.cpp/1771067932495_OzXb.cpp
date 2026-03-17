// Win32IDE_LicenseCreator.cpp — Enterprise License Creator & Feature Dashboard
// ============================================================================
// Tools > License Creator — Shows all 8 enterprise features (locked/unlocked),
// license status, Dev Unlock (when RAWRXD_ENTERPRISE_DEV=1), Install License,
// and KeyGen instructions. Integrates with RawrXD_KeyGen and EnterpriseLicense.
// ============================================================================

#include "Win32IDE.h"
#include "../core/enterprise_license.h"
#include <commdlg.h>
#include <sstream>
#include <cstdlib>
#include <cstring>

namespace {

#define IDC_LC_STATUS       7001
#define IDC_LC_FEATURES     7002
#define IDC_LC_DEV_UNLOCK   7010
#define IDC_LC_INSTALL      7011
#define IDC_LC_COPY_HWID    7012
#define IDC_LC_LAUNCH_KEYGEN 7013
#define IDC_LC_GEN_TRIAL    7015
#define IDC_LC_CLOSE        7014

struct LicenseFeatureRow {
    uint64_t mask;
    const wchar_t* name;
    const wchar_t* wiring;  // Where the feature is gated (for tracking)
};

static const LicenseFeatureRow s_features[] = {
    { 0x01, L"800B Dual-Engine",       L"AgentCommands, streaming_engine_registry, g_800B_Unlocked" },
    { 0x02, L"AVX-512 Premium",        L"production_release, StreamingEngineRegistry" },
    { 0x04, L"Distributed Swarm",      L"swarm_orchestrator.initialize(), Win32IDE_SwarmPanel" },
    { 0x08, L"GPU Quant 4-bit",        L"gpu_kernel_autotuner.initialize(), production_release" },
    { 0x10, L"Enterprise Support",     L"Support tier, audit differentiation" },
    { 0x20, L"Unlimited Context",      L"enterprise_license.cpp GetMaxContextLength" },
    { 0x40, L"Flash Attention",        L"streaming_engine_registry, flash_attention" },
    { 0x80, L"Multi-GPU",              L"production_release" },
};

static std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int need = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    if (need <= 0) return L"";
    std::wstring out(need, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], need);
    return out;
}

static std::string wideToUtf8(const wchar_t* w) {
    if (!w || !*w) return "";
    int need = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (need <= 0) return "";
    std::string out(need, 0);
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &out[0], need, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

static void buildFeatureText(std::wstringstream& ss) {
    auto& lic = RawrXD::EnterpriseLicense::Instance();
    uint64_t mask = lic.GetFeatureMask();
    const char* edition = lic.GetEditionName();

    ss << L"Edition: " << utf8ToWide(edition ? edition : "Unknown") << L"\r\n";
    ss << L"HWID: 0x" << std::hex << lic.GetHardwareHash() << std::dec << L"\r\n\r\n";
    ss << L"Enterprise Features (see ENTERPRISE_FEATURES_MANIFEST.md):\r\n";
    for (const auto& f : s_features) {
        bool enabled = (mask & f.mask) != 0;
        ss << (enabled ? L"  [UNLOCKED] " : L"  [locked]   ") << f.name << L"\r\n";
        ss << L"    -> " << f.wiring << L"\r\n";
    }
    ss << L"\r\n--- Backend Phases (see ENTERPRISE_LICENSE_CREATOR_AUDIT.md) ---\r\n";
    ss << L"  Phase 1 (Foundation):  Arena, NUMA, timing — optional in main build\r\n";
    ss << L"  Phase 2 (Model Loader): GGUF/strategies — optional; main path: gguf_loader\r\n";
    ss << L"  Phase 3 (Inference):   C++ cpu_inference_engine, transformer, flash_attention\r\n";

    ss << L"\r\nKeyGen: RawrXD_KeyGen.exe --genkey|--hwid|--issue|--sign\r\n";
    ss << L"Dev unlock: RAWRXD_ENTERPRISE_DEV=1";
}

} // namespace

LRESULT Win32IDE::LicenseCreatorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            ide = reinterpret_cast<Win32IDE*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(ide));

            CreateWindowExW(0, L"STATIC", L"Enterprise License Creator",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                12, 10, 400, 20, hwnd, nullptr, nullptr, nullptr);

            CreateWindowExW(0, L"STATIC", L"Status:",
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                12, 38, 80, 18, hwnd, nullptr, nullptr, nullptr);

            {
                const char* ed = RawrXD::EnterpriseLicense::Instance().GetEditionName();
                CreateWindowExW(0, L"EDIT", utf8ToWide(ed ? ed : "Unknown").c_str(),
                WS_CHILD | WS_VISIBLE | ES_READONLY | WS_BORDER,
                100, 35, 300, 22, hwnd, (HMENU)(UINT_PTR)IDC_LC_STATUS, nullptr, nullptr);
            }

            CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER,
                12, 65, 510, 220, hwnd, (HMENU)(UINT_PTR)IDC_LC_FEATURES, nullptr, nullptr);

            std::wstringstream ss;
            buildFeatureText(ss);
            SetDlgItemTextW(hwnd, IDC_LC_FEATURES, ss.str().c_str());

            int y = 295;
            if (getenv("RAWRXD_ENTERPRISE_DEV")) {
                CreateWindowExW(0, L"BUTTON", L"Dev Unlock",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    12, y, 100, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_DEV_UNLOCK, nullptr, nullptr);
                y += 32;
            }

            CreateWindowExW(0, L"BUTTON", L"Install License...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                12, y, 120, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_INSTALL, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Copy HWID",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                140, y, 90, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_COPY_HWID, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Launch KeyGen",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                240, y, 110, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_LAUNCH_KEYGEN, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Generate Trial (30d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                12, y + 32, 140, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_GEN_TRIAL, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                360, y, 100, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_CLOSE, nullptr, nullptr);

            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDC_LC_DEV_UNLOCK && ide) {
                int64_t r = RawrXD::Enterprise_DevUnlock();
                if (r == 1) {
                    ide->appendToOutput("[License] Dev Unlock succeeded. Enterprise features enabled.\n", "Output", OutputSeverity::Info);
                    std::wstringstream ss;
                    buildFeatureText(ss);
                    SetDlgItemTextW(hwnd, IDC_LC_FEATURES, ss.str().c_str());
                    const char* ed = RawrXD::EnterpriseLicense::Instance().GetEditionName();
                    SetDlgItemTextW(hwnd, IDC_LC_STATUS, utf8ToWide(ed ? ed : "Unknown").c_str());
                } else {
                    ide->appendToOutput("[License] Dev Unlock failed. Set RAWRXD_ENTERPRISE_DEV=1 and restart.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_INSTALL && ide) {
                wchar_t path[MAX_PATH] = {};
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"License files (*.rawrlic)\0*.rawrlic\0All (*.*)\0*.*\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileNameW(&ofn)) {
                    std::string narrowPath = wideToUtf8(path);
                    bool ok = RawrXD::EnterpriseLicense::Instance().InstallLicenseFromFile(narrowPath);
                    if (ok) {
                        ide->appendToOutput("[License] License installed successfully.\n", "Output", OutputSeverity::Info);
                        std::wstringstream ss;
                        buildFeatureText(ss);
                        SetDlgItemTextW(hwnd, IDC_LC_FEATURES, ss.str().c_str());
                        const char* ed = RawrXD::EnterpriseLicense::Instance().GetEditionName();
                        SetDlgItemTextW(hwnd, IDC_LC_STATUS, utf8ToWide(ed ? ed : "Unknown").c_str());
                    } else {
                        ide->appendToOutput("[License] Install failed. Check file format.\n", "Output", OutputSeverity::Warning);
                    }
                }
                return 0;
            }
            if (id == IDC_LC_COPY_HWID) {
                uint64_t hwid = RawrXD::EnterpriseLicense::Instance().GetHardwareHash();
                wchar_t buf[32];
                swprintf_s(buf, L"0x%llX", (unsigned long long)hwid);
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    size_t len = (wcslen(buf) + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
                    if (h) {
                        memcpy(GlobalLock(h), buf, len);
                        GlobalUnlock(h);
                        SetClipboardData(CF_UNICODETEXT, h);
                    }
                    CloseClipboard();
                }
                return 0;
            }
            if (id == IDC_LC_GEN_TRIAL && ide) {
                wchar_t keygenPath[MAX_PATH] = {};
                wchar_t dirPath[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, keygenPath, MAX_PATH);
                wchar_t* lastSlash = keygenPath + wcslen(keygenPath);
                while (lastSlash > keygenPath && *lastSlash != L'\\') --lastSlash;
                if (lastSlash > keygenPath) {
                    *lastSlash = L'\0';
                    wcscpy_s(dirPath, keygenPath);
                    wcscat_s(keygenPath, L"\\RawrXD_KeyGen.exe");
                } else {
                    wcscpy_s(keygenPath, L"RawrXD_KeyGen.exe");
                }
                uint64_t hwid = RawrXD::EnterpriseLicense::Instance().GetHardwareHash();
                wchar_t cmdLine[512];
                swprintf_s(cmdLine, L"\"%s\" --issue --type trial --days 30 --features 0xFF --hwid 0x%llX --output license.rawrlic",
                    keygenPath, (unsigned long long)hwid);
                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = { 0 };
                if (CreateProcessW(keygenPath[0] ? keygenPath : nullptr, cmdLine, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        dirPath[0] ? dirPath : nullptr, &si, &pi)) {
                    WaitForSingleObject(pi.hProcess, 15000);
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    wchar_t licPath[MAX_PATH];
                    swprintf_s(licPath, L"%s\\license.rawrlic", dirPath[0] ? dirPath : L".");
                    if (GetFileAttributesW(licPath) != INVALID_FILE_ATTRIBUTES) {
                        if (RawrXD::EnterpriseLicense::Instance().InstallLicenseFromFile(std::wstring(licPath))) {
                            ide->appendToOutput("[License] Trial license installed successfully.\n", "Output", OutputSeverity::Info);
                            std::wstringstream ss;
                            buildFeatureText(ss);
                            SetDlgItemTextW(hwnd, IDC_LC_FEATURES, ss.str().c_str());
                            const char* ed = RawrXD::EnterpriseLicense::Instance().GetEditionName();
                            SetDlgItemTextW(hwnd, IDC_LC_STATUS, utf8ToWide(ed ? ed : "Unknown").c_str());
                        } else {
                            ide->appendToOutput("[License] Trial license generated but install failed.\n", "Output", OutputSeverity::Warning);
                        }
                    } else {
                        ide->appendToOutput("[License] KeyGen ran but license.rawrlic not found. Run --genkey first.\n", "Output", OutputSeverity::Warning);
                    }
                } else {
                    ide->appendToOutput("[License] RawrXD_KeyGen not found. Run --genkey first to create keys.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_LAUNCH_KEYGEN) {
                wchar_t exePath[MAX_PATH] = {};
                wchar_t dirPath[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                wchar_t* lastSlash = exePath + wcslen(exePath);
                while (lastSlash > exePath && *lastSlash != L'\\') --lastSlash;
                if (lastSlash > exePath) {
                    *lastSlash = L'\0';
                    wcscpy_s(dirPath, exePath);
                    wcscat_s(exePath, L"\\RawrXD_KeyGen.exe");
                } else {
                    wcscpy_s(exePath, L"RawrXD_KeyGen.exe");
                }
                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi = { 0 };
                if (CreateProcessW(exePath[0] ? exePath : nullptr, nullptr, nullptr, nullptr, FALSE, 0, nullptr,
                        dirPath[0] ? dirPath : nullptr, &si, &pi)) {
                    CloseHandle(pi.hThread);
                    CloseHandle(pi.hProcess);
                    if (ide) ide->appendToOutput("[License] RawrXD_KeyGen launched.\n", "Output", OutputSeverity::Info);
                } else {
                    wcscpy_s(exePath, L"RawrXD_KeyGen.exe");
                    if (CreateProcessW(nullptr, exePath, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                        CloseHandle(pi.hThread);
                        CloseHandle(pi.hProcess);
                        if (ide) ide->appendToOutput("[License] RawrXD_KeyGen launched.\n", "Output", OutputSeverity::Info);
                    } else if (ide) ide->appendToOutput("[License] RawrXD_KeyGen not found. Build from src/tools/RawrXD_KeyGen.cpp\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_CLOSE || id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Win32IDE::showLicenseCreatorDialog() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Win32IDE::LicenseCreatorWndProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"RawrXD_LicenseCreator";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassExW(&wc);
        registered = true;
    }

    RECT mainRect;
    GetWindowRect(m_hwndMain, &mainRect);
    int w = 540;
    int h = 460;
    int x = mainRect.left + (mainRect.right - mainRect.left - w) / 2;
    int y = mainRect.top + (mainRect.bottom - mainRect.top - h) / 2;

    HWND child = CreateWindowExW(WS_EX_DLGMODALFRAME,
        L"RawrXD_LicenseCreator", L"Enterprise License Creator",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, w, h, m_hwndMain, nullptr, m_hInstance, this);
    if (child) {
        ShowWindow(child, SW_SHOW);
        SetForegroundWindow(child);
    }
}
