/**
 * @file MainWindow.h
 * @brief Main application window with dock widgets and menu system
 *
 * Responsibilities:
 * - Main application frame (QMainWindow)
 * - Menu bar (File, Edit, View, Help)
 * - Dock widgets (file browser, chat, terminal, output)
 * - Status bar with progress indicator
 * - Keyboard shortcuts
 * - State persistence
 *
 * Architecture:
 * - PIMPL pattern for encapsulation
 * - Signal/slot for dock visibility control
 * - QSettings for persistence
 *
 * @author RawrXD Team
 * @version 1.0.0
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QJsonArray>
#include <memory>

class MultiTabEditor;
class FileBrowser;
class TerminalWidget;
class QTextEdit;
class QSettings;
class QLabel;
class QProgressBar;
class QAction;
class QDockWidget;
class Phase5ModelRouter;
class Phase5ChatInterface;
class Phase5AnalyticsDashboard;
class REPane;

// Forward declare with complete struct visible to unique_ptr deleter
struct MainWindowPrivate;

/**
 * @brief Main application window
 *
 * Features:
 * - Central editor with multi-tab support
 * - Dockable file browser (left)
 * - Dockable chat interface (right)
 * - Dockable terminal (bottom)
 * - Dockable output pane (bottom, tabified)
 * - View menu for dock visibility control
 * - Keyboard shortcuts for all operations
 * - State persistence (window geometry, dock positions, visibility)
 * - Status bar with progress indicator
 * - Toolbar with common actions
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~MainWindow();

    /**
     * @brief Initialize window (deferred, after QApplication)
     *
     * Initializes all menus, docks, and status bar.
     * Should be called after QApplication is created.
     */
    void initialize();

    // CLI-accessible methods (IDECommandServer)
    Q_INVOKABLE bool loadModelFromCLI(const QString& path);
    Q_INVOKABLE bool openFileFromCLI(const QString& path);
    Q_INVOKABLE bool toggleDockFromCLI(const QString& name);
    Q_INVOKABLE bool sendChatMessageFromCLI(const QString& message, QString& response);
    Q_INVOKABLE QString getIDEStatusFromCLI();
    Q_INVOKABLE QJsonArray listFilesFromCLI(const QString& directory);

    // Accessors
    /**
     * @brief Get the central editor widget
     */
    MultiTabEditor* getEditor() const;

    /**
     * @brief Get the file browser widget
     */
    FileBrowser* getFileBrowser() const;

    /**
     * @brief Get the terminal widget
     */
    TerminalWidget* getTerminal() const;

    /**
     * @brief Get the output pane
     */
    QTextEdit* getOutputPane() const;

    // Terminal integration
    /**
     * @brief Execute a command in the integrated terminal
     * @param command Command to execute
     * @param showInOutput Whether to show the command in terminal output
     */
    void executeTerminalCommand(const QString& command, bool showInOutput = true);

    /**
     * @brief Get current terminal output
     */
    QString getTerminalOutput() const;

    // Status bar
    /**
     * @brief Set status message
     */
    void setStatusMessage(const QString& message);

    /**
     * @brief Show progress bar
     * @param value Current value
     * @param maximum Maximum value
     */
    void showProgress(int value, int maximum);

    /**
     * @brief Hide progress bar
     */
    void hideProgress();

    // Output pane
    /**
     * @brief Add message to output pane
     */
    void addOutputMessage(const QString& message);

    /**
     * @brief Clear output pane
     */
    void clearOutput();

    /**
     * @brief Add RE pane integration
     */
    void addREPane();

    /**
     * @brief Update RE pane causal graph statistics
     */
    void updateREPaneCausalGraphStats();

protected:
    /**
     * @brief Close event - save state before closing
     */
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief Reset dock layout to defaults
     */
    void resetDockLayout();

private:
    /**
     * @brief Create menu bar with all menus
     */
    void createMenuBar();

    /**
     * @brief Create central widget (multi-tab editor)
     */
    void createCentralWidget();

    /**
     * @brief Create dock widgets
     */
    void createDockWidgets();

    /**
     * @brief Create status bar
     */
    void createStatusBar();

    /**
     * @brief Create toolbars
     */
    void createToolBars();

    /**
     * @brief Save window state (geometry, dock positions, visibility)
     */
    void saveWindowState();

    /**
     * @brief Restore window state
     */
    void restoreWindowState();

    // Private implementation
    std::unique_ptr<MainWindowPrivate> m_private;
    REPane *rePane;
};

#endif // MAINWINDOW_H
