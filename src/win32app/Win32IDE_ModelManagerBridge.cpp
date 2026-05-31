// ============================================================================
// Win32IDE_ModelManagerBridge.cpp — Bridge from Win32IDE to ModelManager dialog
// ============================================================================

#include "Win32IDE.h"
#include "Win32IDE_ModelManager.h"
#include "Win32IDE_DownloadsPanel.h"
#include "model_puller/model_puller.h"
#include "../../include/model_puller/download_manager.h"
#include <cstdio>

// Status bar part index for download progress (part 8 of 12)
static constexpr int kStatusBarDownloadPart = 8;

void Win32IDE::showModelManager() {
    LOG_INFO("Opening Model Manager dialog");

    ModelManagerDialog dlg(m_hwndMain, m_hInstance);
    dlg.Show();

    std::string selectedPath = dlg.GetSelectedModelPath();
    if (!selectedPath.empty()) {
        LOG_INFO("Model Manager selected: " + selectedPath);
        // Trigger model reload if active model changed
        scanForModels();
    }
}

// ============================================================================
// Status Bar Download Progress Integration
// ============================================================================
// Call this from model pull operations to show progress on status bar part 8.
// Thread-safe: uses PostMessage/SendMessage which are thread-safe to other threads.
void Win32IDE::updateDownloadProgress(const RawrXD::DownloadProgress& progress) {
    if (!m_hwndStatusBar) return;

    char buf[128];
    if (progress.totalBytes > 0 && progress.progressPercent > 0.0) {
        double mbDone = static_cast<double>(progress.bytesDownloaded) / (1024.0 * 1024.0);
        double mbTotal = static_cast<double>(progress.totalBytes) / (1024.0 * 1024.0);
        double speedMBs = progress.speedBytesPerSec / (1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "DL: %.0f%% (%.0f/%.0f MB, %.1f MB/s)",
                 progress.progressPercent, mbDone, mbTotal, speedMBs);
    } else if (progress.bytesDownloaded > 0) {
        double mbDone = static_cast<double>(progress.bytesDownloaded) / (1024.0 * 1024.0);
        snprintf(buf, sizeof(buf), "DL: %.0f MB", mbDone);
    } else {
        snprintf(buf, sizeof(buf), "Connecting...");
    }

    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, kStatusBarDownloadPart, reinterpret_cast<LPARAM>(buf));

    // Mirror to Downloads panel registry
    if (!progress.currentFile.empty()) {
        RawrXD::DownloadItemStatus st =
            (progress.progressPercent <= 0.0)
                ? RawrXD::DownloadItemStatus::Connecting
                : RawrXD::DownloadItemStatus::InProgress;
        RawrXD::DownloadRegistry::Instance().UpdateProgress(
            progress.currentFile,
            progress.progressPercent,
            progress.speedBytesPerSec,
            st);
    }
}

void Win32IDE::clearDownloadProgress() {
    if (!m_hwndStatusBar) return;
    SendMessageA(m_hwndStatusBar, SB_SETTEXTA, kStatusBarDownloadPart, reinterpret_cast<LPARAM>(""));
}

// ============================================================================
// Downloads Panel
// ============================================================================

static DownloadsPanelWindow* GetOrCreateDownloadsPanel(Win32IDE* ide, HWND hwndMain, HINSTANCE hInst) {
    // Lazy-create on first call
    static DownloadsPanelWindow* s_panel = nullptr;
    if (!s_panel) {
        s_panel = new DownloadsPanelWindow(hwndMain, hInst);
        s_panel->Create();

        // Seed with completed local model list so panel shows history on first open
        try {
            auto models = RawrXD::ModelPuller::Instance().ListLocalModels();
            for (auto& m : models) {
                RawrXD::DownloadItem item;
                item.kind      = RawrXD::DownloadItemKind::Model;
                item.status    = RawrXD::DownloadItemStatus::Completed;
                item.name      = m.name.empty() ? m.id : m.name;
                item.sizeBytes = m.sizeBytes;
                item.destPath  = m.absolutePath;
                item.progressPct = 100.0;
                RawrXD::DownloadRegistry::Instance().AddOrUpdate(item);
            }
        } catch (...) { /* model index not ready yet — silent */ }
    }
    (void)ide;
    return s_panel;
}

void Win32IDE::showDownloadsPanel() {
    DownloadsPanelWindow* panel = GetOrCreateDownloadsPanel(this, m_hwndMain, m_hInstance);
    if (panel) panel->Show();
}

void Win32IDE::hideDownloadsPanel() {
    DownloadsPanelWindow* panel = GetOrCreateDownloadsPanel(this, m_hwndMain, m_hInstance);
    if (panel) panel->Hide();
}

void Win32IDE::toggleDownloadsPanel() {
    DownloadsPanelWindow* panel = GetOrCreateDownloadsPanel(this, m_hwndMain, m_hInstance);
    if (!panel) return;
    if (panel->IsVisible()) panel->Hide();
    else                    panel->Show();
}

// ============================================================================
// Extension Panel
// ============================================================================

static RawrXD::ExtensionPanelWindow* GetOrCreateExtensionPanel(Win32IDE* ide, HWND hwndMain, HINSTANCE hInst) {
    static RawrXD::ExtensionPanelWindow* s_panel = nullptr;
    if (!s_panel) {
        s_panel = new RawrXD::ExtensionPanelWindow(hwndMain, hInst);
        s_panel->Create();

        // Wire action buttons to ExtensionInstaller
        s_panel->SetActionCallback([ide](const std::string& extId, const std::string& action) {
            auto installer = ide->getExtensionInstaller();
            if (!ide || !installer) return;
            std::string err;
            if (action == "activate") {
                if (installer->activate(extId, &err)) {
                    LOG_INFO("[ExtensionPanel] Activated: " + extId);
                } else {
                    LOG_ERROR("[ExtensionPanel] Activate failed: " + extId + " — " + err);
                }
            } else if (action == "deactivate") {
                if (installer->deactivate(extId, &err)) {
                    LOG_INFO("[ExtensionPanel] Deactivated: " + extId);
                } else {
                    LOG_ERROR("[ExtensionPanel] Deactivate failed: " + extId + " — " + err);
                }
            } else if (action == "remove") {
                if (installer->uninstall(extId, &err)) {
                    LOG_INFO("[ExtensionPanel] Uninstalled: " + extId);
                } else {
                    LOG_ERROR("[ExtensionPanel] Uninstall failed: " + extId + " — " + err);
                }
            }
        });
    }
    (void)ide;
    return s_panel;
}

void Win32IDE::showExtensionPanel() {
    RawrXD::ExtensionPanelWindow* panel = GetOrCreateExtensionPanel(this, m_hwndMain, m_hInstance);
    if (panel) panel->Show();
}

void Win32IDE::hideExtensionPanel() {
    RawrXD::ExtensionPanelWindow* panel = GetOrCreateExtensionPanel(this, m_hwndMain, m_hInstance);
    if (panel) panel->Hide();
}

void Win32IDE::toggleExtensionPanel() {
    RawrXD::ExtensionPanelWindow* panel = GetOrCreateExtensionPanel(this, m_hwndMain, m_hInstance);
    if (!panel) return;
    if (panel->IsVisible()) panel->Hide();
    else                    panel->Show();
}
