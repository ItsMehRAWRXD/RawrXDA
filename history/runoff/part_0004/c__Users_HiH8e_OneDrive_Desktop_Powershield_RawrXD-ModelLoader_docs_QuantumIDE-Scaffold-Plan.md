# QuantumIDE UI Scaffold Plan (Qt Widgets)

Objective: Add Goal Bar, Agent Control Panel dock, Editor stub (FILLAME.JS), Proposal Review dock, and QShell tab into the existing Qt app. Keep pure Qt Widgets (no new deps). Build with MSVC.

Proposed file targets (create/patch as needed):
- src/qtapp/MainWindow.h
- src/qtapp/MainWindow.cpp
- src/qtapp/AgentPanelWidget.h / .cpp (optional helper)
- src/qtapp/ProposalReviewWidget.h / .cpp (optional helper)
- src/qtapp/QShellWidget.h / .cpp (optional helper)

UI layout:
- Top: Goal Bar (QWidget with QHBoxLayout) -> QLineEdit + QPushButton (mic). Signal: onGoalSubmitted(QString).
- Left Dock: Agent Control Panel (QDockWidget + QWidget): ACTIVE GOAL progress (QProgressBar), agent rows (Feature/Security/Performance) with status labels.
- Center: Tabbed editor (QTabWidget) with a read-only QTextEdit showing FILLAME.JS sample lines. Overlay stub label near line 9.
- Right Dock: Proposal Review (QDockWidget + QListWidget + QPushButtons). Actions are no-op stubs.
- Bottom: QShell tab (QTabWidget page) using a simple QTextEdit for command input/output and a parser for Invoke-QAgent/Get-QContext/Set-QConfig that echoes.

Signals & slots:
- MainWindow::onGoalSubmitted(QString goal): updates Agent Panel (progress to 10%, shows goal text) and Proposal list with mock items.
- QShellWidget::executeCommand(QString cmd): parses known commands and emits output text; unknown commands return help.

Build:
- Use MSVC generator. No external libs.

Next steps:
1) If `src/qtapp/MainWindow.*` exists, patch it; else create minimal files and add to CMake.
2) Add stubs for AgentPanelWidget, ProposalReviewWidget, QShellWidget if helpful; otherwise inline in MainWindow.
3) Build and run to verify the layout renders.
