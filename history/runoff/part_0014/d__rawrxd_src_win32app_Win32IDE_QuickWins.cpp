// ============================================================================
// Win32IDE Quick-Win Systems Integration (Phase 33)
// Wires ShortcutManager, BackupManager, AlertSystem, SLOTracker into IDE
// ============================================================================

#include "Win32IDE.h"
#include "../core/shortcut_manager.hpp"
#include "../core/backup_manager.hpp"
#include "../core/alert_system.hpp"
#include "../core/slo_tracker.hpp"

#include <memory>
#include <string>
#include <sstream>

// ============================================================================
// Static globals (same pattern as Win32IDE_VoiceChat.cpp)
// ============================================================================
static std::unique_ptr<ShortcutManager> g_shortcutMgr;
static std::unique_ptr<BackupManager>   g_backupMgr;
static std::unique_ptr<AlertSystem>     g_alertSystem;
static std::unique_ptr<SLOTracker>      g_sloTracker;

// ============================================================================
// Alert callback — forward to status bar via OutputDebugStringA
// ============================================================================
static void alertCallback(const AlertEvent* evt, void* /*userData*/)
{
    if (!evt) return;
    std::string status = "[" + evt->source + "] " + evt->title + ": " + evt->message + "\n";
    OutputDebugStringA(status.c_str());
}

// ============================================================================
// Lifecycle
// ============================================================================
void Win32IDE::initQuickWinSystems()
{
    if (m_quickWinInitialized) return;

    // ── Shortcut Manager ────────────────────────────────────────────────
    g_shortcutMgr = std::make_unique<ShortcutManager>();

    // Register default shortcuts (using ShortcutModifiers enum values)
    g_shortcutMgr->registerDefault(IDM_QW_BACKUP_CREATE,   "Backup: Create",       MOD_CTRL_KEY | MOD_SHIFT_KEY, 'B', ShortcutContext::Global);
    g_shortcutMgr->registerDefault(IDM_VOICE_RECORD,       "Voice: Record",        0, VK_F9, ShortcutContext::Global);
    g_shortcutMgr->registerDefault(IDM_VOICE_TOGGLE_PANEL, "Voice: Toggle Panel",  MOD_CTRL_KEY | MOD_SHIFT_KEY, 'U', ShortcutContext::Global);
    g_shortcutMgr->registerDefault(IDM_QW_SHORTCUT_EDITOR, "Shortcuts Editor",     MOD_CTRL_KEY, 'K', ShortcutContext::Global);

    // Load user overrides from disk
    g_shortcutMgr->loadFromFile("shortcuts.json");

    // ── Backup Manager ──────────────────────────────────────────────────
    g_backupMgr = std::make_unique<BackupManager>();

    BackupConfig backupCfg;
    backupCfg.projectRoot = ".";
    backupCfg.backupRoot = "backups";
    backupCfg.maxBackups = 20;
    backupCfg.autoBackupIntervalMs = 300000;  // 5 minutes
    backupCfg.enableChecksums = true;
    backupCfg.includePatterns = {"*.cpp", "*.hpp", "*.h", "*.asm", "*.cmake", "*.json", "*.txt"};
    backupCfg.excludePatterns = {"build/*", "backups/*", ".git/*", "*.obj", "*.exe", "*.pdb"};
    g_backupMgr->configure(backupCfg);

    // ── Alert System ────────────────────────────────────────────────────
    g_alertSystem = std::make_unique<AlertSystem>();

    AlertSystemConfig alertCfg;
    alertCfg.enableTrayIcon = true;
    alertCfg.enableResourceWatch = true;
    alertCfg.resourcePollMs = 5000;
    alertCfg.deduplicateWindowMs = 30000;
    alertCfg.maxAlertHistory = 200;
    alertCfg.logToFile = true;
    alertCfg.logPath = "alerts.log";
    g_alertSystem->configure(alertCfg);
    g_alertSystem->setAlertCallback(alertCallback, this);

    // Init tray icon
    g_alertSystem->initTrayIcon(m_hwndMain);

    // Start resource monitoring
    g_alertSystem->startResourceMonitor();

    // ── SLO Tracker ─────────────────────────────────────────────────────
    g_sloTracker = std::make_unique<SLOTracker>();

    // Define standard SLOs
    g_sloTracker->defineSLO({"inference_latency", 0.999, 3600000, 5000, 1});
    g_sloTracker->defineSLO({"build_success_rate", 0.95, 86400000, 60000, 0});
    g_sloTracker->defineSLO({"agent_response_quality", 0.90, 3600000, 10000, 1});

    m_quickWinInitialized = true;

    g_alertSystem->emitInfo("Quick-Win Systems", "Shortcuts, Backups, Alerts, SLO tracking initialized", "IDE");
}

void Win32IDE::shutdownQuickWinSystems()
{
    if (!m_quickWinInitialized) return;

    if (g_alertSystem) {
        g_alertSystem->stopResourceMonitor();
        g_alertSystem->removeTrayIcon();
    }

    if (g_backupMgr) {
        g_backupMgr->stopAutoBackup();
    }

    if (g_shortcutMgr) {
        g_shortcutMgr->saveToFile("shortcuts.json");
    }

    g_sloTracker.reset();
    g_alertSystem.reset();
    g_backupMgr.reset();
    g_shortcutMgr.reset();

    m_quickWinInitialized = false;
}

// ============================================================================
// Accessors
// ============================================================================
ShortcutManager* Win32IDE::getShortcutManager()   { return g_shortcutMgr.get(); }
BackupManager*   Win32IDE::getBackupManager()      { return g_backupMgr.get(); }
AlertSystem*     Win32IDE::getAlertSystem()         { return g_alertSystem.get(); }

// ============================================================================
// Shortcut Commands
// ============================================================================
void Win32IDE::cmdShortcutEditor()
{
    if (!g_shortcutMgr) return;

    auto bindings = g_shortcutMgr->getAllBindings();
    std::ostringstream ss;
    ss << "Keyboard Shortcuts (" << bindings.size() << " bindings)\n\n";

    for (const auto& binding : bindings) {
        ss << "  " << binding.toDisplayString()
           << "  ->  " << binding.commandName
           << " (cmd " << binding.commandId << ")\n";
    }

    auto conflicts = g_shortcutMgr->detectConflicts();
    if (!conflicts.empty()) {
        ss << "\nConflicts:\n";
        for (const auto& c : conflicts) {
            ss << "  " << c.description << "\n";
        }
    }

    MessageBoxA(m_hwndMain, ss.str().c_str(), "Keyboard Shortcuts", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdShortcutReset()
{
    if (!g_shortcutMgr) return;

    if (MessageBoxA(m_hwndMain, "Reset all shortcuts to defaults?",
                     "Reset Shortcuts", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        g_shortcutMgr->resetToDefaults();
        if (g_alertSystem) {
            g_alertSystem->emitInfo("Shortcuts", "All shortcuts reset to defaults", "ShortcutManager");
        }
    }
}

// ============================================================================
// Backup Commands
// ============================================================================
void Win32IDE::cmdBackupCreate()
{
    if (!g_backupMgr) return;

    auto result = g_backupMgr->createBackup(BackupType::Full, "Manual backup from IDE");
    if (result.success) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Backup created: %s (%zu files, %zu bytes)",
                 result.entry.id.c_str(), result.entry.fileCount, result.entry.totalBytes);
        if (g_alertSystem) g_alertSystem->emitInfo("Backup", msg, "BackupManager");
        MessageBoxA(m_hwndMain, msg, "Backup Created", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxA(m_hwndMain, result.detail, "Backup Failed", MB_OK | MB_ICONERROR);
    }
}

void Win32IDE::cmdBackupRestore()
{
    if (!g_backupMgr) return;

    auto backups = g_backupMgr->listBackups();
    if (backups.empty()) {
        MessageBoxA(m_hwndMain, "No backups available.", "Restore", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const auto& latest = backups.back();
    char msg[512];
    snprintf(msg, sizeof(msg),
             "Restore from latest backup?\n\n"
             "ID: %s\nTimestamp: %s\nFiles: %zu\nSize: %zu bytes\n\n"
             "This will overwrite current files!",
             latest.id.c_str(), latest.timestamp.c_str(),
             latest.fileCount, latest.totalBytes);

    if (MessageBoxA(m_hwndMain, msg, "Confirm Restore",
                     MB_YESNO | MB_ICONWARNING) == IDYES) {
        auto result = g_backupMgr->restoreBackup(latest.id);
        if (result.success) {
            if (g_alertSystem) g_alertSystem->emitInfo("Backup", "Restore completed", "BackupManager");
            MessageBoxA(m_hwndMain, "Restore completed.", "Success", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(m_hwndMain, result.detail, "Restore Failed", MB_OK | MB_ICONERROR);
        }
    }
}

void Win32IDE::cmdBackupAutoToggle()
{
    if (!g_backupMgr) return;

    if (g_backupMgr->isAutoBackupRunning()) {
        g_backupMgr->stopAutoBackup();
        if (g_alertSystem) g_alertSystem->emitInfo("Backup", "Auto-backup stopped", "BackupManager");
    } else {
        g_backupMgr->startAutoBackup();
        if (g_alertSystem) g_alertSystem->emitInfo("Backup", "Auto-backup started (5 min interval)", "BackupManager");
    }
}

void Win32IDE::cmdBackupList()
{
    if (!g_backupMgr) return;

    auto backups = g_backupMgr->listBackups();
    std::ostringstream ss;
    ss << "Backups (" << backups.size() << " total, "
       << (g_backupMgr->getTotalBackupSize() / 1024) << " KB total)\n\n";

    for (const auto& b : backups) {
        ss << "  [" << b.id << "] " << b.timestamp
           << " - " << b.fileCount << " files, " << (b.totalBytes / 1024) << " KB";
        if (b.verified) ss << " OK";
        ss << "\n";
    }

    if (backups.empty()) {
        ss << "  (no backups)\n";
    }

    MessageBoxA(m_hwndMain, ss.str().c_str(), "Backup List", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdBackupPrune()
{
    if (!g_backupMgr) return;

    int pruned = g_backupMgr->pruneOldBackups();
    char msg[128];
    snprintf(msg, sizeof(msg), "Pruned %d old backup(s).", pruned);
    if (g_alertSystem) g_alertSystem->emitInfo("Backup", msg, "BackupManager");
    MessageBoxA(m_hwndMain, msg, "Prune Complete", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Alert Commands
// ============================================================================
void Win32IDE::cmdAlertToggleMonitor()
{
    if (!g_alertSystem) return;

    if (g_alertSystem->isMonitoring()) {
        g_alertSystem->stopResourceMonitor();
        MessageBoxA(m_hwndMain, "Resource monitor stopped.", "Alerts", MB_OK | MB_ICONINFORMATION);
    } else {
        g_alertSystem->startResourceMonitor();
        MessageBoxA(m_hwndMain, "Resource monitor started.", "Alerts", MB_OK | MB_ICONINFORMATION);
    }
}

void Win32IDE::cmdAlertShowHistory()
{
    if (!g_alertSystem) return;

    auto history = g_alertSystem->getAlertHistory();
    std::ostringstream ss;
    ss << "Alert History (" << history.size() << " total, "
       << g_alertSystem->getActiveCount() << " active)\n\n";

    int shown = 0;
    for (auto it = history.rbegin(); it != history.rend() && shown < 50; ++it, ++shown) {
        const char* prio = "INFO";
        switch (it->priority) {
            case AlertPriority::Warning:  prio = "WARN"; break;
            case AlertPriority::Error:    prio = "ERR "; break;
            case AlertPriority::Critical: prio = "CRIT"; break;
            default: break;
        }
        ss << (it->dismissed ? "  " : "* ")
           << "[" << prio << "] " << it->timestamp
           << " [" << it->source << "] " << it->title;
        if (it->duplicateCount > 1)
            ss << " (x" << it->duplicateCount << ")";
        ss << "\n";
    }

    MessageBoxA(m_hwndMain, ss.str().c_str(), "Alert History", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAlertDismissAll()
{
    if (!g_alertSystem) return;

    g_alertSystem->dismissAll();
    MessageBoxA(m_hwndMain, "All alerts dismissed.", "Alerts", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdAlertResourceStatus()
{
    if (!g_alertSystem) return;

    auto resources = g_alertSystem->getAllResourceStatus();
    std::ostringstream ss;
    ss << "Resource Status\n\n";

    for (const auto& r : resources) {
        const char* name = "Unknown";
        switch (r.type) {
            case ResourceType::CPU:      name = "CPU"; break;
            case ResourceType::Memory:   name = "Memory"; break;
            case ResourceType::Disk:     name = "Disk"; break;
            case ResourceType::GPU_VRAM: name = "GPU VRAM"; break;
            case ResourceType::Network:  name = "Network"; break;
            case ResourceType::TokenRate:name = "Token Rate"; break;
        }
        ss << "  " << name << ": ";
        ss.precision(1);
        ss << std::fixed << r.currentPct << "%";
        if (r.inCritical) ss << "  CRITICAL";
        else if (r.inWarning) ss << "  WARNING";
        else ss << "  OK";
        ss << "\n";
    }

    if (resources.empty()) {
        ss << "  (no data yet - monitor may still be initializing)\n";
    }

    MessageBoxA(m_hwndMain, ss.str().c_str(), "Resource Status", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// SLO Dashboard
// ============================================================================
void Win32IDE::cmdSLODashboard()
{
    if (!g_sloTracker) return;

    auto statuses = g_sloTracker->getAllStatuses();
    std::ostringstream ss;
    ss << "SLO Dashboard\n\n";

    for (const auto& s : statuses) {
        ss << "  " << s.serviceName << "\n";
        ss << "     Total: " << s.totalRequests
           << " | Success: " << s.successCount
           << " | Failures: " << s.failureCount << "\n";
        ss.precision(3);
        ss << std::fixed;
        ss << "     Availability: " << (s.currentAvailability * 100.0) << "%"
           << " (target: " << (s.targetAvailability * 100.0) << "%)\n";
        ss.precision(1);
        ss << "     P99 Latency:  " << s.p99LatencyMs << " ms\n";
        ss << "     Error Budget: " << s.errorBudgetRemaining << " remaining";
        if (s.breached) ss << "  BREACH";
        ss << "\n\n";
    }

    if (statuses.empty()) {
        ss << "  (no SLOs defined or no events recorded)\n";
    }

    if (g_sloTracker->hasBreaches()) {
        ss << "Active SLO breaches detected!\n";
    }

    MessageBoxA(m_hwndMain, ss.str().c_str(), "SLO Dashboard", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Command Router (9800 range)
// ============================================================================
bool Win32IDE::handleQuickWinCommand(int commandId)
{
    switch (commandId) {
        case IDM_QW_SHORTCUT_EDITOR:       cmdShortcutEditor();       return true;
        case IDM_QW_SHORTCUT_RESET:        cmdShortcutReset();        return true;
        case IDM_QW_BACKUP_CREATE:         cmdBackupCreate();         return true;
        case IDM_QW_BACKUP_RESTORE:        cmdBackupRestore();        return true;
        case IDM_QW_BACKUP_AUTO_TOGGLE:    cmdBackupAutoToggle();     return true;
        case IDM_QW_BACKUP_LIST:           cmdBackupList();           return true;
        case IDM_QW_BACKUP_PRUNE:          cmdBackupPrune();          return true;
        case IDM_QW_ALERT_TOGGLE_MONITOR:  cmdAlertToggleMonitor();   return true;
        case IDM_QW_ALERT_SHOW_HISTORY:    cmdAlertShowHistory();     return true;
        case IDM_QW_ALERT_DISMISS_ALL:     cmdAlertDismissAll();      return true;
        case IDM_QW_ALERT_RESOURCE_STATUS: cmdAlertResourceStatus();  return true;
        case IDM_QW_SLO_DASHBOARD:         cmdSLODashboard();         return true;
        default: return false;
    }
}
