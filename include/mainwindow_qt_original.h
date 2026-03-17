#pragma once

// ============================================================================
// MainWindow — Qt-free stub (replaces QMainWindow-based MainWindow)
// ============================================================================
// Original Qt: QMainWindow, QWidget, QString, signals. This header provides
// a C++20 stub for pure Win32/MASM builds. Use Win32 main window (e.g. Win32IDE)
// for actual UI.
// ============================================================================

#include <memory>
#include <string>

#include "agentic_controller.hpp"

namespace RawrXD::IDE {

class AgenticController;

class MainWindow {
public:
    MainWindow() = default;
    explicit MainWindow(void* /*parent*/) {}
    ~MainWindow() = default;

    AgenticResult initialize();

private:
    AgenticResult centerOnPrimaryDisplay();
    void wireSignals();
    void restoreLayout();
    AgenticResult hydrateLayout(const std::string& snapshotHint, std::string& outSnapshotId);

    std::unique_ptr<AgenticController> m_agenticController;
    std::string m_lastHydratedSnapshot;
};

} // namespace RawrXD::IDE
