// =============================================================================
// Win32IDE_DiskRecovery.cpp — Disk Recovery Panel for Win32IDE
// =============================================================================
// Sidebar view + bottom panel for the DiskRecoveryAgent.
// Provides scan, probe, start/pause/abort, key extraction, bad-map export.
// No exceptions. No std::function in hot path. PatchResult pattern.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include "Win32IDE.h"
#include "../agent/DiskRecoveryAgent.h"

#include <cstdio>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace RawrXD::Recovery;

// ---------------------------------------------------------------------------
// Control IDs unique to DiskRecovery panel
// ---------------------------------------------------------------------------
static constexpr int IDC_RECOVERY_SCAN       = 7001;
static constexpr int IDC_RECOVERY_PROBE      = 7002;
static constexpr int IDC_RECOVERY_START      = 7003;
static constexpr int IDC_RECOVERY_PAUSE      = 7004;
static constexpr int IDC_RECOVERY_ABORT      = 7005;
static constexpr int IDC_RECOVERY_KEY_EXTRACT= 7006;
static constexpr int IDC_RECOVERY_BADMAP     = 7007;
static constexpr int IDC_RECOVERY_DRIVELIST  = 7008;
static constexpr int IDC_RECOVERY_LOG        = 7009;
static constexpr int IDC_RECOVERY_OUTPATH    = 7010;
static constexpr int IDC_RECOVERY_BROWSE     = 7011;
static constexpr int IDC_RECOVERY_PROGRESS   = 7012;

// ---------------------------------------------------------------------------
// Singleton recovery agent for IDE
// ---------------------------------------------------------------------------
static DiskRecoveryAgent& ideRecoveryAgent() {
    static DiskRecoveryAgent s_agent;
    return s_agent;
}

// ---------------------------------------------------------------------------
// createDiskRecoveryView — Creates all child controls in the sidebar
// ---------------------------------------------------------------------------
void Win32IDE::createDiskRecoveryView(HWND hwndParent)
{
    int y = 4;
    const int w = 250;  // fits in default sidebar

    // Title label
    m_hwndRecoveryTitle = CreateWindowExA(
        0, "STATIC", "DISK RECOVERY",
        WS_CHILD | SS_LEFT,
        8, y, w, 20,
        hwndParent, nullptr, m_hInstance, nullptr);
    y += 26;

    // Drive list (listbox)
    m_hwndRecoveryDriveList = CreateWindowExA(
        WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
        8, y, w - 16, 100,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_DRIVELIST, m_hInstance, nullptr);
    y += 108;

    // Button row 1: Scan | Probe
    CreateWindowExA(0, "BUTTON", "Scan",
        WS_CHILD | BS_PUSHBUTTON,
        8, y, 60, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_SCAN, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Probe",
        WS_CHILD | BS_PUSHBUTTON,
        76, y, 60, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_PROBE, m_hInstance, nullptr);
    y += 32;

    // Button row 2: Start | Pause | Abort
    CreateWindowExA(0, "BUTTON", "Start",
        WS_CHILD | BS_PUSHBUTTON,
        8, y, 55, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_START, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Pause",
        WS_CHILD | BS_PUSHBUTTON,
        68, y, 55, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_PAUSE, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Abort",
        WS_CHILD | BS_PUSHBUTTON,
        128, y, 55, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_ABORT, m_hInstance, nullptr);
    y += 32;

    // Button row 3: Key Extract | Bad Map
    CreateWindowExA(0, "BUTTON", "Key Extract",
        WS_CHILD | BS_PUSHBUTTON,
        8, y, 85, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_KEY_EXTRACT, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "Bad Map",
        WS_CHILD | BS_PUSHBUTTON,
        100, y, 75, 26,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_BADMAP, m_hInstance, nullptr);
    y += 32;

    // Output path row
    CreateWindowExA(0, "STATIC", "Output:",
        WS_CHILD,
        8, y + 3, 50, 18,
        hwndParent, nullptr, m_hInstance, nullptr);
    m_hwndRecoveryOutPath = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "D:\\recovery_output",
        WS_CHILD | ES_AUTOHSCROLL,
        60, y, w - 110, 22,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_OUTPATH, m_hInstance, nullptr);
    CreateWindowExA(0, "BUTTON", "...",
        WS_CHILD | BS_PUSHBUTTON,
        w - 42, y, 30, 22,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_BROWSE, m_hInstance, nullptr);
    y += 28;

    // Status / progress label
    m_hwndRecoveryStatus = CreateWindowExA(
        0, "STATIC", "Status: Idle",
        WS_CHILD | SS_LEFT,
        8, y, w, 18,
        hwndParent, nullptr, m_hInstance, nullptr);
    y += 22;

    // Progress bar
    m_hwndRecoveryProgress = CreateWindowExA(
        0, PROGRESS_CLASSA, "",
        WS_CHILD | PBS_SMOOTH,
        8, y, w - 16, 16,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_PROGRESS, m_hInstance, nullptr);
    SendMessageA(m_hwndRecoveryProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
    y += 22;

    // Log / output area (read-only edit)
    m_hwndRecoveryLog = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        8, y, w - 16, 180,
        hwndParent, (HMENU)(INT_PTR)IDC_RECOVERY_LOG, m_hInstance, nullptr);

    // Dark theme colors
    setDarkThemeForWindow(m_hwndRecoveryDriveList);
    setDarkThemeForWindow(m_hwndRecoveryLog);
    setDarkThemeForWindow(m_hwndRecoveryOutPath);

    m_recoveryTimerActive = false;

    appendToOutput("Disk Recovery view created\n", "Output", OutputSeverity::Info);
}

// ---------------------------------------------------------------------------
// setDarkThemeForWindow — helper to set basic dark colors on a control
// ---------------------------------------------------------------------------
void Win32IDE::setDarkThemeForWindow(HWND hwnd)
{
    if (!hwnd) return;
    // Dark theme handled by WM_CTLCOLOR* messages in the parent proc
    InvalidateRect(hwnd, nullptr, TRUE);
}

// ---------------------------------------------------------------------------
// recoveryAppendLog — add text to the recovery log edit control
// ---------------------------------------------------------------------------
void Win32IDE::recoveryAppendLog(const char* msg)
{
    if (!m_hwndRecoveryLog) return;
    int len = GetWindowTextLengthA(m_hwndRecoveryLog);
    SendMessageA(m_hwndRecoveryLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(m_hwndRecoveryLog, EM_REPLACESEL, FALSE, (LPARAM)msg);
}

// ---------------------------------------------------------------------------
// onRecoveryScan — enumerate physical drives
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryScan()
{
    recoveryAppendLog("[SCAN] Scanning physical drives...\r\n");

    auto& agent = ideRecoveryAgent();
    auto result = agent.ScanDrives();
    if (!result.success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[SCAN] FAILED: %s\r\n",
                 result.message.c_str());
        recoveryAppendLog(buf);
        return;
    }

    // Populate drive listbox
    SendMessageA(m_hwndRecoveryDriveList, LB_RESETCONTENT, 0, 0);

    const auto& drives = agent.GetDetectedDrives();
    for (size_t i = 0; i < drives.size(); i++) {
        char entry[256];
        snprintf(entry, sizeof(entry), "Drive %d: %s (%s, %.1f GB)",
                 drives[i].driveNumber,
                 drives[i].model,
                 drives[i].serial,
                 drives[i].totalBytes / (1024.0 * 1024.0 * 1024.0));
        SendMessageA(m_hwndRecoveryDriveList, LB_ADDSTRING, 0, (LPARAM)entry);
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "[SCAN] Found %zu drive(s)\r\n", drives.size());
    recoveryAppendLog(buf);

    SetWindowTextA(m_hwndRecoveryStatus, "Status: Scan complete");
}

// ---------------------------------------------------------------------------
// onRecoveryProbe — identify USB bridge chipset
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryProbe()
{
    int sel = (int)SendMessageA(m_hwndRecoveryDriveList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        recoveryAppendLog("[PROBE] No drive selected. Click Scan first.\r\n");
        return;
    }

    auto& agent = ideRecoveryAgent();
    const auto& drives = agent.GetDetectedDrives();
    if (sel >= (int)drives.size()) return;

    recoveryAppendLog("[PROBE] Probing USB bridge...\r\n");
    auto result = agent.ProbeBridge(drives[sel].driveNumber);
    if (!result.success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[PROBE] FAILED: %s\r\n", result.message.c_str());
        recoveryAppendLog(buf);
        return;
    }

    const char* bridgeName = "Unknown";
    switch (agent.GetBridgeType()) {
        case BridgeType::JMS567:   bridgeName = "JMicron JMS567"; break;
        case BridgeType::NS1066:   bridgeName = "Norelsys NS1066"; break;
        case BridgeType::ASM1153E: bridgeName = "ASMedia ASM1153E"; break;
        case BridgeType::VL716:    bridgeName = "VIA VL716"; break;
        default: break;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "[PROBE] Bridge: %s\r\n", bridgeName);
    recoveryAppendLog(buf);

    char status[128];
    snprintf(status, sizeof(status), "Status: Bridge=%s", bridgeName);
    SetWindowTextA(m_hwndRecoveryStatus, status);
}

// ---------------------------------------------------------------------------
// onRecoveryStart — begin imaging
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryStart()
{
    int sel = (int)SendMessageA(m_hwndRecoveryDriveList, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) {
        recoveryAppendLog("[START] No drive selected.\r\n");
        return;
    }

    // Get output path
    char outPath[MAX_PATH] = {};
    GetWindowTextA(m_hwndRecoveryOutPath, outPath, MAX_PATH);
    if (outPath[0] == '\0') {
        recoveryAppendLog("[START] Output path is empty.\r\n");
        return;
    }

    auto& agent = ideRecoveryAgent();
    const auto& drives = agent.GetDetectedDrives();
    if (sel >= (int)drives.size()) return;

    RecoveryConfig config = {};
    config.driveNumber = drives[sel].driveNumber;
    config.outputPath = outPath;
    config.retryCount = 3;
    config.skipBadSectors = true;

    recoveryAppendLog("[START] Beginning recovery imaging...\r\n");
    auto result = agent.StartRecovery(config);
    if (!result.success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[START] FAILED: %s\r\n", result.message.c_str());
        recoveryAppendLog(buf);
        return;
    }

    // Start progress timer (poll every 500ms)
    SetTimer(m_hwndMain, 0xDC01, 500, nullptr);
    m_recoveryTimerActive = true;

    SetWindowTextA(m_hwndRecoveryStatus, "Status: Imaging in progress...");
    SendMessageA(m_hwndRecoveryProgress, PBM_SETPOS, 0, 0);
}

// ---------------------------------------------------------------------------
// onRecoveryPause — pause/resume imaging
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryPause()
{
    auto& agent = ideRecoveryAgent();
    auto state = agent.GetState();

    if (state == RecoveryState::Imaging) {
        agent.PauseRecovery();
        recoveryAppendLog("[PAUSE] Recovery paused.\r\n");
        SetWindowTextA(m_hwndRecoveryStatus, "Status: PAUSED");
    } else if (state == RecoveryState::Paused) {
        agent.ResumeRecovery();
        recoveryAppendLog("[RESUME] Recovery resumed.\r\n");
        SetWindowTextA(m_hwndRecoveryStatus, "Status: Imaging in progress...");
    } else {
        recoveryAppendLog("[PAUSE] Not in imaging state.\r\n");
    }
}

// ---------------------------------------------------------------------------
// onRecoveryAbort — stop imaging
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryAbort()
{
    auto& agent = ideRecoveryAgent();
    agent.AbortRecovery();
    recoveryAppendLog("[ABORT] Recovery aborted.\r\n");
    SetWindowTextA(m_hwndRecoveryStatus, "Status: Aborted");
    SendMessageA(m_hwndRecoveryProgress, PBM_SETPOS, 0, 0);

    if (m_recoveryTimerActive) {
        KillTimer(m_hwndMain, 0xDC01);
        m_recoveryTimerActive = false;
    }
}

// ---------------------------------------------------------------------------
// onRecoveryKeyExtract — extract WD encryption key from bridge RAM
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryKeyExtract()
{
    recoveryAppendLog("[KEY] Extracting encryption key from bridge RAM...\r\n");

    auto& agent = ideRecoveryAgent();
    auto result = agent.ExtractEncryptionKey();
    if (!result.success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[KEY] FAILED: %s\r\n", result.message.c_str());
        recoveryAppendLog(buf);
        return;
    }

    recoveryAppendLog("[KEY] Encryption key extracted successfully.\r\n");
    SetWindowTextA(m_hwndRecoveryStatus, "Status: Key extracted");
}

// ---------------------------------------------------------------------------
// onRecoveryBadMapExport — export bad sector map
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryBadMapExport()
{
    char outPath[MAX_PATH] = {};
    GetWindowTextA(m_hwndRecoveryOutPath, outPath, MAX_PATH);

    std::string mapFile = std::string(outPath) + "\\bad_sectors.map";
    recoveryAppendLog("[MAP] Exporting bad sector map...\r\n");

    auto& agent = ideRecoveryAgent();
    auto result = agent.ExportBadSectorMap(mapFile.c_str());
    if (!result.success) {
        char buf[256];
        snprintf(buf, sizeof(buf), "[MAP] FAILED: %s\r\n", result.message.c_str());
        recoveryAppendLog(buf);
        return;
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "[MAP] Bad sector map exported to: %s\r\n", mapFile.c_str());
    recoveryAppendLog(buf);
}

// ---------------------------------------------------------------------------
// onRecoveryBrowse — pick output directory
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryBrowse()
{
    BROWSEINFOA bi = {};
    bi.hwndOwner = m_hwndMain;
    bi.lpszTitle = "Select Recovery Output Directory";
    bi.ulFlags   = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            SetWindowTextA(m_hwndRecoveryOutPath, path);
        }
        CoTaskMemFree(pidl);
    }
}

// ---------------------------------------------------------------------------
// onRecoveryTimer — polled from WM_TIMER (timer ID 42)
// ---------------------------------------------------------------------------
void Win32IDE::onRecoveryTimer()
{
    auto& agent = ideRecoveryAgent();
    auto state  = agent.GetState();
    auto stats  = agent.GetStats();

    // Update progress bar (0..1000 range)
    int progress = 0;
    if (stats.totalSectors > 0) {
        progress = (int)((stats.recoveredSectors * 1000ULL) / stats.totalSectors);
    }
    SendMessageA(m_hwndRecoveryProgress, PBM_SETPOS, (WPARAM)progress, 0);

    // Update status line
    char status[256];
    snprintf(status, sizeof(status),
             "Status: %s | %.1f%% | %llu/%llu sectors | %llu bad | %.1f MB/s",
             (state == RecoveryState::Imaging)   ? "Imaging" :
             (state == RecoveryState::Paused)     ? "PAUSED" :
             (state == RecoveryState::Completed)  ? "DONE" :
             (state == RecoveryState::Failed)     ? "FAILED" :
             (state == RecoveryState::Aborted)    ? "Aborted" : "Idle",
             stats.totalSectors > 0 ? (stats.recoveredSectors * 100.0 / stats.totalSectors) : 0.0,
             (unsigned long long)stats.recoveredSectors,
             (unsigned long long)stats.totalSectors,
             (unsigned long long)stats.badSectors,
             stats.throughputMBps);
    SetWindowTextA(m_hwndRecoveryStatus, status);

    // Stop timer when done
    if (state == RecoveryState::Completed ||
        state == RecoveryState::Failed ||
        state == RecoveryState::Aborted ||
        state == RecoveryState::Idle) {
        KillTimer(m_hwndMain, 42);
        m_recoveryTimerActive = false;

        char buf[128];
        snprintf(buf, sizeof(buf), "[DONE] Final state: %s, %llu sectors recovered, %llu bad.\r\n",
                 (state == RecoveryState::Completed) ? "Completed" : "Stopped",
                 (unsigned long long)stats.recoveredSectors,
                 (unsigned long long)stats.badSectors);
        recoveryAppendLog(buf);
    }
}

// ---------------------------------------------------------------------------
// handleRecoveryCommand — route WM_COMMAND from sidebar or panel
// ---------------------------------------------------------------------------
void Win32IDE::handleRecoveryCommand(int commandId)
{
    switch (commandId) {
    case IDC_RECOVERY_SCAN:         onRecoveryScan();       break;
    case IDC_RECOVERY_PROBE:        onRecoveryProbe();      break;
    case IDC_RECOVERY_START:        onRecoveryStart();      break;
    case IDC_RECOVERY_PAUSE:        onRecoveryPause();      break;
    case IDC_RECOVERY_ABORT:        onRecoveryAbort();      break;
    case IDC_RECOVERY_KEY_EXTRACT:  onRecoveryKeyExtract(); break;
    case IDC_RECOVERY_BADMAP:       onRecoveryBadMapExport(); break;
    case IDC_RECOVERY_BROWSE:       onRecoveryBrowse();     break;
    default: break;
    }
}
