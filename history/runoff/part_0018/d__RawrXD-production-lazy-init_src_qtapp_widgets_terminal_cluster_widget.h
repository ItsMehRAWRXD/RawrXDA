/**
 * @file terminal_cluster_widget.h
 * @brief Header for TerminalClusterWidget - Multi-terminal management interface
 */

#ifndef TERMINAL_CLUSTER_WIDGET_H
#define TERMINAL_CLUSTER_WIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTabBar>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QProgressBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QSettings>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QTextCharFormat>
#include <QFont>
#include <QColor>
#include <QBrush>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDateTime>
#include <QUuid>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QTemporaryFile>
#include <QDebug>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextBlock>
#include <QAbstractScrollArea>
#include <QFontMetrics>
#include <QPalette>

// ============================================================================
// Forward Declarations and Type Definitions
// ============================================================================

class TerminalWidget;
class TerminalProcess;
class TerminalClusterWidget;

// ============================================================================
// TerminalProcess Class
// ============================================================================

class TerminalProcess : public QObject
{
    Q_OBJECT

public:
    explicit TerminalProcess(QObject* parent = nullptr);
    ~TerminalProcess();

    void start(const QString& program, const QStringList& arguments = QStringList(),
               const QString& workingDirectory = QString());
    void stop();
    bool isRunning() const { return process_ && process_->state() == QProcess::Running; }

    void write(const QByteArray& data);
    void sendCommand(const QString& command);

    QString getWorkingDirectory() const;
    void setWorkingDirectory(const QString& dir);

    QString getShell() const { return shell_; }
    void setShell(const QString& shell) { shell_ = shell; }

    // History management
    void addToHistory(const QString& command);
    QStringList getHistory() const { return history_; }
    QString getHistoryItem(int index) const;
    int historySize() const { return history_.size(); }

signals:
    void readyReadStandardOutput(const QByteArray& data);
    void readyReadStandardError(const QByteArray& data);
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void errorOccurred(QProcess::ProcessError error);
    void stateChanged(QProcess::ProcessState state);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onStarted();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onErrorOccurred(QProcess::ProcessError error);
    void onStateChanged(QProcess::ProcessState state);

private:
    QProcess* process_;
    QString shell_;
    QString workingDirectory_;
    QStringList history_;
    int historyIndex_;
};

// ============================================================================
// TerminalWidget Class
// ============================================================================

class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget();

    void initialize(const QString& shell = QString(), const QString& workingDirectory = QString());
    void terminate();

    void sendInput(const QString& text);
    void sendCommand(const QString& command);

    void clear();
    void scrollToBottom();

    // Appearance
    void setFont(const QFont& font);
    QFont getFont() const { return terminal_->font(); }

    void setColors(const QColor& foreground, const QColor& background);
    QColor getForegroundColor() const { return palette().color(QPalette::Text); }
    QColor getBackgroundColor() const { return palette().color(QPalette::Base); }

    // History
    void addToHistory(const QString& command);
    QStringList getHistory() const;
    void showHistory();

    // State
    bool isActive() const { return process_ && process_->isRunning(); }
    QString getTitle() const;
    QString getWorkingDirectory() const;

    // Configuration
    void setShell(const QString& shell);
    QString getShell() const;

    void setScrollbackLines(int lines);
    int getScrollbackLines() const { return scrollbackLines_; }

signals:
    void titleChanged(const QString& title);
    void activated(TerminalWidget* terminal);
    void closed(TerminalWidget* terminal);
    void commandExecuted(const QString& command);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onProcessOutput(const QByteArray& data);
    void onProcessError(const QByteArray& data);
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

    void updateDisplay();
    void handleInput(const QString& input);

private:
    void setupUI();
    void setupProcess();
    void updatePrompt();
    void appendOutput(const QString& text, const QColor& color = QColor());
    void processEscapeSequence(const QString& sequence);

    // UI components
    QVBoxLayout* layout_;
    QPlainTextEdit* terminal_;
    QLineEdit* inputLine_;

    // Process
    TerminalProcess* process_;

    // State
    QString currentDirectory_;
    QString shell_;
    QString prompt_;
    QString currentInput_;
    QStringList history_;
    int historyIndex_;
    int scrollbackLines_;

    // Display
    QColor outputColor_;
    QColor errorColor_;
    QColor inputColor_;

    // Buffer for escape sequence processing
    QString escapeBuffer_;
    bool inEscapeSequence_;
};

// ============================================================================
// TerminalClusterWidget Class
// ============================================================================

class TerminalClusterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalClusterWidget(QWidget* parent = nullptr, QSettings* settings = nullptr);
    ~TerminalClusterWidget();

    // Terminal management
    TerminalWidget* createTerminal(const QString& name = QString(),
                                 const QString& shell = QString(),
                                 const QString& workingDirectory = QString());
    void closeTerminal(TerminalWidget* terminal);
    void closeTerminal(int index);
    TerminalWidget* getCurrentTerminal() const;
    TerminalWidget* getTerminal(int index) const;
    int terminalCount() const { return terminals_.size(); }

    // Layout management
    void setLayoutMode(int mode); // 0=tabs, 1=split horizontal, 2=split vertical, 3=grid
    int getLayoutMode() const { return layoutMode_; }

    // Global operations
    void sendToAllTerminals(const QString& command);
    void clearAllTerminals();
    void restartAllTerminals();

    // Configuration
    void setDefaultShell(const QString& shell) { defaultShell_ = shell; }
    QString getDefaultShell() const { return defaultShell_; }

    void setDefaultWorkingDirectory(const QString& dir) { defaultWorkingDirectory_ = dir; }
    QString getDefaultWorkingDirectory() const { return defaultWorkingDirectory_; }

    void setFont(const QFont& font);
    QFont getFont() const { return font_; }

    void setColors(const QColor& foreground, const QColor& background);
    QColor getForegroundColor() const { return foregroundColor_; }
    QColor getBackgroundColor() const { return backgroundColor_; }

    // UI operations
    void refresh();
    void setReadOnly(bool readOnly);
    bool isReadOnly() const { return readOnly_; }

    QString getTitle() const;
    void appendLog(const QString& message);
    void appendError(const QString& error);

signals:
    void terminalCreated(TerminalWidget* terminal);
    void terminalClosed(TerminalWidget* terminal);
    void currentTerminalChanged(TerminalWidget* terminal);
    void titleChanged(const QString& title);

protected:
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onNewTerminal();
    void onCloseTerminal();
    void onCloseAllTerminals();
    void onRenameTerminal();
    void onDuplicateTerminal();
    void onSplitTerminal();
    void onLayoutChanged(int mode);
    void onFontChanged();
    void onColorsChanged();
    void onSettingsChanged();

    void onTerminalActivated(TerminalWidget* terminal);
    void onTerminalClosed(TerminalWidget* terminal);
    void onTabCloseRequested(int index);
    void onCurrentTabChanged(int index);

    void updateToolbar();
    void updateWindowTitle();

private:
    void setupUI();
    void setupToolbar();
    void setupTabWidget();
    void setupConnections();

    void updateLayout();
    void createTabLayout();
    void createSplitLayout();
    void createGridLayout();

    QString generateTerminalName() const;
    void saveTerminalState();
    void restoreTerminalState();

    // UI components
    QVBoxLayout* mainLayout_;
    QWidget* toolbarWidget_;
    QHBoxLayout* toolbarLayout_;

    // Toolbar buttons
    QPushButton* newTerminalBtn_;
    QPushButton* closeTerminalBtn_;
    QPushButton* closeAllBtn_;
    QPushButton* renameBtn_;
    QPushButton* duplicateBtn_;
    QPushButton* splitBtn_;
    QPushButton* clearBtn_;
    QPushButton* restartBtn_;

    // Layout controls
    QLabel* layoutLabel_;
    QComboBox* layoutCombo_;

    // Appearance controls
    QPushButton* fontBtn_;
    QPushButton* colorsBtn_;
    QPushButton* settingsBtn_;

    // Terminal container
    QTabWidget* tabWidget_;
    QWidget* splitWidget_;
    QSplitter* splitter_;

    // State
    QList<TerminalWidget*> terminals_;
    TerminalWidget* currentTerminal_;
    int layoutMode_; // 0=tabs, 1=horizontal split, 2=vertical split, 3=grid

    // Configuration
    QString defaultShell_;
    QString defaultWorkingDirectory_;
    QFont font_;
    QColor foregroundColor_;
    QColor backgroundColor_;
    bool readOnly_;

    // Settings
    QSettings* settings_;
};

#endif // TERMINAL_CLUSTER_WIDGET_H