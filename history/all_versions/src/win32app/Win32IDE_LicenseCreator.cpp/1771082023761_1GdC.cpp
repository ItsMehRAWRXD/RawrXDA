// Win32IDE_LicenseCreator.cpp — Enterprise License Creator & Feature Dashboard
// ============================================================================
// Tools > License Creator — Shows all 8 enterprise features (locked/unlocked),
// license status, Dev Unlock (when RAWRXD_ENTERPRISE_DEV=1), Install License,
// and KeyGen instructions. Integrates with RawrXD_KeyGen and EnterpriseLicense.
// ============================================================================

#include "Win32IDE.h"
#include "../core/enterprise_license.h"
#include "enterprise_feature_manager.hpp"
#include "../../include/enterprise_license.h"
#include "feature_registry_panel.h"
#include <commdlg.h>
#include <shlobj.h>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>

namespace {

#define IDC_LC_STATUS       7001
#define IDC_LC_FEATURES     7002
#define IDC_LC_DEV_UNLOCK   7010
#define IDC_LC_INSTALL      7011
#define IDC_LC_COPY_HWID    7012
#define IDC_LC_LAUNCH_KEYGEN 7013
#define IDC_LC_GEN_TRIAL    7015
#define IDC_LC_CLOSE        7014
#define IDC_LC_GEN_ENTERPRISE 7016
#define IDC_LC_GEN_PRO      7017
#define IDC_LC_REFRESH      7018
#define IDC_LC_STATUS_V2    7020
#define IDC_LC_FEATURES_V2  7021
#define IDC_LC_V2_DEV_UNLOCK 7022
#define IDC_LC_V2_INSTALL   7023
#define IDC_LC_V2_GEN_PRO   7024
#define IDC_LC_V2_GEN_ENT   7025
#define IDC_LC_V2_GEN_SOV   7026
#define IDC_LC_V2_REFRESH   7027
#define IDC_LC_V2_SECRET    7030
#define IDC_LC_V2_DAYS      7031
#define IDC_LC_V2_OUTPUT    7032
#define IDC_LC_V2_BROWSE    7033
#define IDC_LC_V2_BIND      7034

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

static std::wstring getDlgItemTextSafe(HWND hwnd, int id) {
    wchar_t buf[512] = {};
    GetDlgItemTextW(hwnd, id, buf, (int)(sizeof(buf) / sizeof(buf[0])));
    return std::wstring(buf);
}

static uint32_t parseDaysFromDialog(HWND hwnd, uint32_t fallbackDays) {
    std::wstring daysText = getDlgItemTextSafe(hwnd, IDC_LC_V2_DAYS);
    if (daysText.empty()) return fallbackDays;
    wchar_t* end = nullptr;
    unsigned long days = wcstoul(daysText.c_str(), &end, 10);
    if (end == daysText.c_str()) return fallbackDays;
    return (uint32_t)days;
}

// ============================================================================
// Inline License Header (matches enterprise_license_creator.cpp / ASM format)
// ============================================================================
#pragma pack(push, 1)
struct InlineLicenseHeader {
    uint32_t magic;         // 0x4C445852 = "RXDL"
    uint16_t version;       // 0x0200 = v2.0
    uint16_t headerSize;    // sizeof = 64
    uint64_t hwid;          // Hardware fingerprint
    uint64_t features;      // Feature bitmask (0x00–0xFF)
    uint32_t issueDate;     // Unix timestamp
    uint32_t expiryDate;    // Unix timestamp (0 = perpetual)
    uint8_t  tier;          // 0=Community, 1=Trial, 2=Pro, 3=Enterprise
    uint8_t  reserved[31];  // Padding to 64 bytes
};
#pragma pack(pop)

// Forward declarations
static void buildFeatureText(std::wstringstream& ss);
static void buildFeatureTextV2(std::wstringstream& ss);
static void refreshLicenseUI(HWND hwnd);

// Create a .rawrlic file inline (no external tools needed)
// tier: 1=Trial, 2=Pro, 3=Enterprise; days: 0=perpetual; features: bitmask
static bool createLicenseInline(uint8_t tier, int days, uint64_t features,
                                 const wchar_t* outputPath) {
    auto& lic = RawrXD::EnterpriseLicense::Instance();

    InlineLicenseHeader hdr{};
    hdr.magic      = 0x4C445852;  // "RXDL"
    hdr.version    = 0x0200;
    hdr.headerSize = sizeof(InlineLicenseHeader);
    hdr.hwid       = lic.GetHardwareHash();
    hdr.features   = features;
    hdr.issueDate  = static_cast<uint32_t>(time(nullptr));
    hdr.expiryDate = (days == 0) ? 0 : hdr.issueDate + (days * 86400);
    hdr.tier       = tier;
    memset(hdr.reserved, 0, sizeof(hdr.reserved));

    // Build output path
    wchar_t path[MAX_PATH] = {};
    if (outputPath && outputPath[0]) {
        wcscpy_s(path, outputPath);
    } else {
        // Default: %APPDATA%\RawrXD\license.rawrlic
        wchar_t appData[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
            swprintf_s(path, L"%s\\RawrXD\\license.rawrlic", appData);
            // Ensure directory exists
            wchar_t dir[MAX_PATH];
            swprintf_s(dir, L"%s\\RawrXD", appData);
            CreateDirectoryW(dir, nullptr);
        } else {
            wcscpy_s(path, L"license.rawrlic");
        }
    }

    // Write header + 512-byte dummy signature
    std::string narrowPath = wideToUtf8(path);
    std::ofstream f(narrowPath, std::ios::binary);
    if (!f.is_open()) return false;

    f.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));

    // Deterministic signature pattern (for dev/trial format compatibility)
    uint8_t sig[512] = {};
    for (int i = 0; i < 512; i++) {
        sig[i] = static_cast<uint8_t>((hdr.hwid >> (i % 8 * 8)) ^ i);
    }
    f.write(reinterpret_cast<const char*>(sig), sizeof(sig));
    f.close();

    // Auto-install the created license
    return lic.InstallLicenseFromFile(std::wstring(path));
}

static bool createV2LicenseInline(RawrXD::License::LicenseTierV2 tier,
                                  uint32_t days,
                                  const wchar_t* outputPath,
                                  const char* signingSecret,
                                  bool bindMachine) {
    using namespace RawrXD::License;
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    if (!signingSecret || !*signingSecret) {
        return false;
    }

    LicenseKeyV2 key{};
    LicenseResult r = lic.createKey(tier, days, signingSecret, &key);
    if (!r.success) return false;

    if (!bindMachine) {
        key.hwid = 0; // unbound license
    }

    wchar_t path[MAX_PATH] = {};
    if (outputPath && outputPath[0]) {
        wcscpy_s(path, outputPath);
    } else {
        wchar_t appData[MAX_PATH] = {};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
            swprintf_s(path, L"%s\\RawrXD\\license_v2.rawrlic", appData);
            wchar_t dir[MAX_PATH];
            swprintf_s(dir, L"%s\\RawrXD", appData);
            CreateDirectoryW(dir, nullptr);
        } else {
            wcscpy_s(path, L"license_v2.rawrlic");
        }
    }

    std::string narrowPath = wideToUtf8(path);
    std::ofstream f(narrowPath, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(&key), sizeof(key));
    f.close();

    LicenseResult load = lic.loadKeyFromFile(narrowPath.c_str());
    return load.success;
}

static bool createV2LicenseFromDialog(HWND hwnd, Win32IDE* ide,
                                      RawrXD::License::LicenseTierV2 tier,
                                      uint32_t fallbackDays) {
    std::wstring secretW = getDlgItemTextSafe(hwnd, IDC_LC_V2_SECRET);
    std::string secret = wideToUtf8(secretW.c_str());
    if (secret.empty()) {
        const char* envSecret = std::getenv("RAWRXD_LICENSE_SECRET");
        if (envSecret && *envSecret) secret = envSecret;
    }

    if (secret.empty()) {
        if (ide) {
            ide->appendToOutput("[License] V2 signing secret missing. Set RAWRXD_LICENSE_SECRET or enter a secret in the dialog.\n",
                                "Output", OutputSeverity::Warning);
        }
        return false;
    }

    uint32_t days = parseDaysFromDialog(hwnd, fallbackDays);
    std::wstring outputPathW = getDlgItemTextSafe(hwnd, IDC_LC_V2_OUTPUT);
    const wchar_t* outputPath = outputPathW.empty() ? nullptr : outputPathW.c_str();
    bool bindMachine = (SendMessageW(GetDlgItem(hwnd, IDC_LC_V2_BIND), BM_GETCHECK, 0, 0) == BST_CHECKED);

    bool ok = createV2LicenseInline(tier, days, outputPath, secret.c_str(), bindMachine);

    // Best-effort secret scrub
    std::fill(secret.begin(), secret.end(), '\0');

    return ok;
}

static void refreshLicenseUI(HWND hwnd) {
    std::wstringstream ss;
    buildFeatureText(ss);
    SetDlgItemTextW(hwnd, IDC_LC_FEATURES, ss.str().c_str());
    const char* ed = RawrXD::EnterpriseLicense::Instance().GetEditionName();
    SetDlgItemTextW(hwnd, IDC_LC_STATUS, utf8ToWide(ed ? ed : "Unknown").c_str());

    std::wstringstream ssV2;
    buildFeatureTextV2(ssV2);
    SetDlgItemTextW(hwnd, IDC_LC_FEATURES_V2, ssV2.str().c_str());
    const char* tierName = RawrXD::License::tierName(
        RawrXD::License::EnterpriseLicenseV2::Instance().currentTier());
    SetDlgItemTextW(hwnd, IDC_LC_STATUS_V2, utf8ToWide(tierName ? tierName : "Unknown").c_str());
}

static void buildFeatureText(std::wstringstream& ss) {
    auto& lic = RawrXD::EnterpriseLicense::Instance();
    uint64_t mask = lic.GetFeatureMask();
    const char* edition = lic.GetEditionName();

    ss << L"Edition: " << utf8ToWide(edition ? edition : "Unknown") << L"\r\n";
    ss << L"HWID: 0x" << std::hex << lic.GetHardwareHash() << std::dec << L"\r\n";
    ss << L"Max Model: " << lic.GetMaxModelSizeGB() << L"GB | Max Context: "
       << lic.GetMaxContextLength() << L" tokens\r\n\r\n";
    ss << L"Enterprise Features:\r\n";
    for (const auto& f : s_features) {
        bool enabled = (mask & f.mask) != 0;
        ss << (enabled ? L"  [UNLOCKED] " : L"  [locked]   ") << f.name << L"\r\n";
        ss << L"    -> " << f.wiring << L"\r\n";
    }

    // Count active vs locked
    int active = 0, locked = 0;
    for (const auto& f : s_features) {
        if (mask & f.mask) active++; else locked++;
    }
    ss << L"\r\nSummary: " << active << L" active, " << locked << L" locked\r\n";

    ss << L"\r\n--- License Tiers ---\r\n";
    ss << L"  Community: 70B models, 32K context, 4GB budget\r\n";
    ss << L"  Trial:     180B models, 128K context, 16GB budget (30 days)\r\n";
    ss << L"  Pro:       400B models, 128K context, 32GB budget\r\n";
    ss << L"  Enterprise: 800B models, 200K context, unlimited budget\r\n";

    ss << L"\r\nDev unlock: set RAWRXD_ENTERPRISE_DEV=1 and click Dev Unlock\r\n";
    ss << L"Or use inline buttons below to create a license directly.";
}

static void buildFeatureTextV2(std::wstringstream& ss) {
    using namespace RawrXD::License;
    auto& lic = EnterpriseLicenseV2::Instance();
    lic.initialize();

    LicenseTierV2 tier = lic.currentTier();
    const TierLimits::Limits& limits = lic.currentLimits();

    ss << L"V2 Tier: " << utf8ToWide(tierName(tier)) << L"\r\n";
    ss << L"Enabled: " << lic.enabledFeatureCount() << L" / " << TOTAL_FEATURES
       << L" | Implemented: " << lic.countImplemented()
       << L" | UI Wired: " << lic.countWiredToUI()
       << L" | Tested: " << lic.countTested() << L"\r\n";
    ss << L"Max Model: " << limits.maxModelGB << L"GB | Max Context: "
       << limits.maxContextTokens << L" tokens\r\n\r\n";

    ss << L"Enterprise V2 Features (61):\r\n";
    for (uint32_t i = 0; i < TOTAL_FEATURES; ++i) {
        const FeatureDefV2& def = g_FeatureManifest[i];
        bool enabled = lic.isFeatureEnabled(def.id);
        bool licensed = lic.isFeatureLicensed(def.id);
        ss << (enabled ? L"  [ON]  " : (licensed ? L"  [LCK] " : L"  [OFF] "))
           << utf8ToWide(def.name ? def.name : "Unknown")
           << L" | " << utf8ToWide(tierName(def.minTier))
           << L" | " << (def.implemented ? L"IMPL" : L"----")
           << L" | " << (def.wiredToUI ? L"UI" : L"--")
           << L" | " << (def.tested ? L"TEST" : L"----")
           << L"\r\n";
    }

    ss << L"\r\nV2 Dev unlock: set RAWRXD_ENTERPRISE_DEV=1 then click V2 Dev Unlock\r\n";
    ss << L"V2 Create uses RAWRXD_LICENSE_SECRET (default dev secret if unset).";
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
                12, 65, 510, 180, hwnd, (HMENU)(UINT_PTR)IDC_LC_FEATURES, nullptr, nullptr);

            int y = 255;
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

            CreateWindowExW(0, L"BUTTON", L"Refresh",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                240, y, 80, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_REFRESH, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Close",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                430, y, 80, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_CLOSE, nullptr, nullptr);

            // Row 2: License creation buttons (V1)
            y += 32;
            CreateWindowExW(0, L"BUTTON", L"Create Trial (30d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                12, y, 130, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_GEN_TRIAL, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Create Pro (180d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                148, y, 126, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_GEN_PRO, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Create Enterprise (365d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                280, y, 160, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_GEN_ENTERPRISE, nullptr, nullptr);

            // V2 section
            y += 40;
            CreateWindowExW(0, L"STATIC", L"Enterprise V2 License Status:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                12, y, 240, 18, hwnd, nullptr, nullptr, nullptr);

            CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_READONLY | WS_BORDER,
                250, y - 3, 272, 22, hwnd, (HMENU)(UINT_PTR)IDC_LC_STATUS_V2, nullptr, nullptr);

            y += 26;
            CreateWindowExW(0, L"STATIC", L"V2 Secret:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                12, y, 80, 18, hwnd, nullptr, nullptr, nullptr);
            CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_PASSWORD | WS_BORDER,
                95, y - 3, 220, 22, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_SECRET, nullptr, nullptr);
            CreateWindowExW(0, L"STATIC", L"Days:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                325, y, 40, 18, hwnd, nullptr, nullptr, nullptr);
            CreateWindowExW(0, L"EDIT", L"365",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                370, y - 3, 60, 22, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_DAYS, nullptr, nullptr);

            y += 26;
            CreateWindowExW(0, L"STATIC", L"Output:",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                12, y, 80, 18, hwnd, nullptr, nullptr, nullptr);
            CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                95, y - 3, 300, 22, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_OUTPUT, nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                405, y - 3, 90, 22, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_BROWSE, nullptr, nullptr);

            y += 26;
            HWND hBind = CreateWindowExW(0, L"BUTTON", L"Bind to this machine (HWID)",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                12, y, 220, 20, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_BIND, nullptr, nullptr);
            if (hBind) SendMessageW(hBind, BM_SETCHECK, BST_CHECKED, 0);

            y += 30;
            CreateWindowExW(0, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER,
                12, y, 510, 180, hwnd, (HMENU)(UINT_PTR)IDC_LC_FEATURES_V2, nullptr, nullptr);

            y += 190;
            if (getenv("RAWRXD_ENTERPRISE_DEV")) {
                CreateWindowExW(0, L"BUTTON", L"V2 Dev Unlock",
                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    12, y, 110, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_DEV_UNLOCK, nullptr, nullptr);
            }

            CreateWindowExW(0, L"BUTTON", L"V2 Install Key...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                130, y, 120, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_INSTALL, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"V2 Refresh",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                260, y, 90, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_REFRESH, nullptr, nullptr);

            y += 32;
            CreateWindowExW(0, L"BUTTON", L"V2 Create Pro (180d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                12, y, 160, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_GEN_PRO, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"V2 Create Ent (365d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                180, y, 170, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_GEN_ENT, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"V2 Create Sov (365d)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                360, y, 170, 26, hwnd, (HMENU)(UINT_PTR)IDC_LC_V2_GEN_SOV, nullptr, nullptr);

            refreshLicenseUI(hwnd);

            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDC_LC_DEV_UNLOCK && ide) {
                int64_t r = RawrXD::Enterprise_DevUnlock();
                if (r == 1) {
                    ide->appendToOutput("[License] Dev Unlock succeeded. Enterprise features enabled.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
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
                        refreshLicenseUI(hwnd);
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
                // Inline trial license: 30 days, all features (0xFF), tier=1
                if (createLicenseInline(1, 30, 0xFF, nullptr)) {
                    ide->appendToOutput("[License] Trial license (30 days, all features) created and installed.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] Failed to create trial license.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_GEN_PRO && ide) {
                // Inline Pro license: 180 days, Pro features (0x4A), tier=2
                if (createLicenseInline(2, 180, 0x4A, nullptr)) {
                    ide->appendToOutput("[License] Pro license (180 days) created and installed.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] Failed to create Pro license.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_GEN_ENTERPRISE && ide) {
                // Inline Enterprise license: 365 days, all features (0xFF), tier=3
                if (createLicenseInline(3, 365, 0xFF, nullptr)) {
                    ide->appendToOutput("[License] Enterprise license (365 days, all features) created and installed.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] Failed to create Enterprise license.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_V2_DEV_UNLOCK && ide) {
                auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
                auto r = lic.devUnlock();
                if (r.success) {
                    ide->appendToOutput("[License] V2 Dev Unlock succeeded.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] V2 Dev Unlock failed. Set RAWRXD_ENTERPRISE_DEV=1 and restart.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_V2_INSTALL && ide) {
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
                    auto& lic = RawrXD::License::EnterpriseLicenseV2::Instance();
                    auto r = lic.loadKeyFromFile(narrowPath.c_str());
                    if (r.success) {
                        ide->appendToOutput("[License] V2 license installed successfully.\n", "Output", OutputSeverity::Info);
                        refreshLicenseUI(hwnd);
                    } else {
                        ide->appendToOutput("[License] V2 install failed. Check file format/secret.\n", "Output", OutputSeverity::Warning);
                    }
                }
                return 0;
            }
            if (id == IDC_LC_V2_REFRESH) {
                refreshLicenseUI(hwnd);
                return 0;
            }
            if (id == IDC_LC_V2_GEN_PRO && ide) {
                if (createV2LicenseFromDialog(hwnd, ide, RawrXD::License::LicenseTierV2::Professional, 180)) {
                    ide->appendToOutput("[License] V2 Pro license created and installed.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] Failed to create V2 Pro license.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_V2_GEN_ENT && ide) {
                if (createV2LicenseFromDialog(hwnd, ide, RawrXD::License::LicenseTierV2::Enterprise, 365)) {
                    ide->appendToOutput("[License] V2 Enterprise license created and installed.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] Failed to create V2 Enterprise license.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_V2_GEN_SOV && ide) {
                if (createV2LicenseFromDialog(hwnd, ide, RawrXD::License::LicenseTierV2::Sovereign, 365)) {
                    ide->appendToOutput("[License] V2 Sovereign license created and installed.\n", "Output", OutputSeverity::Info);
                    refreshLicenseUI(hwnd);
                } else {
                    ide->appendToOutput("[License] Failed to create V2 Sovereign license.\n", "Output", OutputSeverity::Warning);
                }
                return 0;
            }
            if (id == IDC_LC_V2_BROWSE) {
                wchar_t path[MAX_PATH] = {};
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hwnd;
                ofn.lpstrFilter = L"License files (*.rawrlic)\0*.rawrlic\0All (*.*)\0*.*\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.lpstrTitle = L"Save V2 License";
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                if (GetSaveFileNameW(&ofn)) {
                    SetDlgItemTextW(hwnd, IDC_LC_V2_OUTPUT, path);
                }
                return 0;
            }
            if (id == IDC_LC_REFRESH) {
                refreshLicenseUI(hwnd);
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
    int w = 560;
    int h = 760;
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

LRESULT CALLBACK Win32IDE::FeatureRegistryHostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Win32IDE* ide = reinterpret_cast<Win32IDE*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            ide = reinterpret_cast<Win32IDE*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(ide));

            if (ide) {
                if (!ide->m_featureRegistryPanel) {
                    ide->m_featureRegistryPanel = std::make_unique<FeatureRegistryPanel>();
                }

                RECT rc;
                GetClientRect(hwnd, &rc);
                ide->m_featureRegistryPanel->create(hwnd, 0, 0, rc.right - rc.left, rc.bottom - rc.top);
            }
            return 0;
        }
        case WM_SIZE: {
            if (ide && ide->m_featureRegistryPanel) {
                int w = LOWORD(lp);
                int h = HIWORD(lp);
                ide->m_featureRegistryPanel->resize(0, 0, w, h);
            }
            return 0;
        }
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_DESTROY:
            if (ide) {
                ide->m_hwndFeatureRegistryHost = nullptr;
            }
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void Win32IDE::showFeatureRegistryDialog() {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Win32IDE::FeatureRegistryHostProc;
        wc.hInstance = m_hInstance;
        wc.lpszClassName = L"RawrXD_FeatureRegistryHost";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        RegisterClassExW(&wc);
        registered = true;
    }

    if (m_hwndFeatureRegistryHost && IsWindow(m_hwndFeatureRegistryHost)) {
        ShowWindow(m_hwndFeatureRegistryHost, SW_SHOW);
        SetForegroundWindow(m_hwndFeatureRegistryHost);
        if (m_featureRegistryPanel) {
            m_featureRegistryPanel->refreshFeatures();
            m_featureRegistryPanel->refreshLicenseStatus();
        }
        return;
    }

    RECT mainRect;
    GetWindowRect(m_hwndMain, &mainRect);
    int w = 860;
    int h = 620;
    int x = mainRect.left + (mainRect.right - mainRect.left - w) / 2;
    int y = mainRect.top + (mainRect.bottom - mainRect.top - h) / 2;

    m_hwndFeatureRegistryHost = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"RawrXD_FeatureRegistryHost",
        L"Enterprise Feature Registry",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        x, y, w, h,
        m_hwndMain, nullptr, m_hInstance, this);

    if (m_hwndFeatureRegistryHost) {
        ShowWindow(m_hwndFeatureRegistryHost, SW_SHOW);
        SetForegroundWindow(m_hwndFeatureRegistryHost);
    }
}
