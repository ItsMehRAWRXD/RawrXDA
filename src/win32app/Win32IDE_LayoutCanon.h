#include <cassert>
#pragma once

// =============================================================================
// RawrXD IDE Layout Canon — Four Pane Rule (canonical)
// =============================================================================
// The IDE has exactly FOUR main panes. Everything else is a POP UP.
// Command palette, settings, dialogs, floating panel, help, Git panel,
// hotpatch panel, modals, tooltips — pop ups only. They overlay the panes.
// See ARCHITECTURE.md §6.5.
// =============================================================================

namespace RawrXD
{

/// The four main panes. No more, no fewer.
enum class MainPane : int
{
    FileExplorer = 0,   // Left sidebar — back↔front wiring, C++/Win32 only
    TerminalDebug = 1,  // Bottom — Codex CLI, RE, Ghidra/IDA-level
    Editor = 2,         // Main frame — central editing surface
    AIChat = 3          // Right sidebar — heart; loadable models, streaming
};

/// Canonical pane count. Layout logic must not exceed this.
inline constexpr int MAIN_PANE_COUNT = 4;

static_assert(MAIN_PANE_COUNT == 4, "RawrXD IDE: exactly four main panes; no more, no fewer.");

/// Control ID ranges for each pane (used by IsMainPaneControlId, GetMainPaneFromControlId)
inline constexpr int IDC_FILE_EXPLORER_MIN = 1025;
inline constexpr int IDC_FILE_EXPLORER_MAX = 1026;
inline constexpr int IDC_PANEL_MIN = 1300;
inline constexpr int IDC_PANEL_MAX = 1320;
inline constexpr int IDC_EDITOR_MIN = 1001;
inline constexpr int IDC_EDITOR_MAX = 1024;  // editor + misc main-frame controls
inline constexpr int IDC_COPILOT_MIN = 1200;
inline constexpr int IDC_COPILOT_MAX = 1207;

/// Returns true if the control ID belongs to one of the four main panes.
inline bool IsMainPaneControlId(int id)
{
    return (id >= IDC_FILE_EXPLORER_MIN && id <= IDC_FILE_EXPLORER_MAX) ||
           (id >= IDC_PANEL_MIN && id <= IDC_PANEL_MAX) || (id >= IDC_EDITOR_MIN && id <= IDC_EDITOR_MAX) ||
           (id >= IDC_COPILOT_MIN && id <= IDC_COPILOT_MAX);
}

/// Returns the main pane for the given control ID, or -1 if not a main pane.
inline int GetMainPaneFromControlId(int id)
{
    if (id >= IDC_FILE_EXPLORER_MIN && id <= IDC_FILE_EXPLORER_MAX)
        return static_cast<int>(MainPane::FileExplorer);
    if (id >= IDC_PANEL_MIN && id <= IDC_PANEL_MAX)
        return static_cast<int>(MainPane::TerminalDebug);
    if (id >= IDC_EDITOR_MIN && id <= IDC_EDITOR_MAX)
        return static_cast<int>(MainPane::Editor);
    if (id >= IDC_COPILOT_MIN && id <= IDC_COPILOT_MAX)
        return static_cast<int>(MainPane::AIChat);
    return -1;
}

/// Pop-ups: command palette, settings, dialogs, floating panel, help (1008),
/// Git panel, hotpatch panel, modals, tooltips. Not in main pane set.
/// Use !IsMainPaneControlId(id) to treat as pop-up.
///
/// Pop-up rules:
/// - Qt builds: pop ups = QWidget.
/// - Pop ups are NOT embedded; they float in their own pane/layer over the IDE.
/// - Other widgets must NOT overwrite the pop-up layer (dedicated z-order).

}  // namespace RawrXD

