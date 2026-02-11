#pragma once

#include <memory>
#include <string>
#include <windows.h>

#include "agentic_controller.hpp"

namespace RawrXD::IDE {

class AgenticController;

class MainWindow {
public:
    explicit MainWindow(void* parent = nullptr);
    ~MainWindow() = default;

    AgenticResult initialize();

    // Callback placeholders (would be called via Win32 messages)
    void layoutRestored(const std::string& snapshotId) { /* Win32 message post */ }
    void layoutHydrationFailed(const std::string& snapshotHint, const std::string& reason) { /* Win32 message post */ }

private:
    AgenticResult centerOnPrimaryDisplay();
    void wireSignals();
    void restoreLayout();
    AgenticResult hydrateLayout(const std::string& snapshotHint, std::string& outSnapshotId);

    std::unique_ptr<AgenticController> m_agenticController;
    std::string m_lastHydratedSnapshot;
    
    // Win32 window state
    HWND m_hwnd;
    int m_x, m_y, m_width, m_height;
};

} // namespace RawrXD::IDE
