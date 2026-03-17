// TerminalTab.cpp - Individual Terminal Tab Implementation
// Complete terminal session management with shell execution

#include "TerminalTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QClipboard>
#include <QApplication>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
#include <QScrollBar>
#include <QTimer>
#include <QProcessEnvironment>
#include <QDir>
#include <QDebug>
#include <algorithm>

// ============================================================================
// Constructor & Initialization
// ============================================================================

TerminalTab::TerminalTab(const TerminalConfig& config, QWidget* parent)
    : QWidget(parent)
    , m_config(config)
    , m_workingDirectory(config.workingDirectory.isEmpty() ? QDir::homePath() : config.workingDirectory)
    , m_shellProcess(nullptr)
    , m_historyIndex(-1)
    , m_isRunning(false)
    , m_isWaitingForPrompt(false)
    , m_commandStartLine(0)
    , m_fontFamily("Courier New")
    , m_fontSize(10)
{
    setupUI();
    connectSignals();
    
    // Start the terminal
    start();
}

TerminalTab::~TerminalTab()
{
    if (m_shellProcess) {
        if (m_shellProcess->state() != QProcess::NotRunning) {
            m_shellProcess->terminate();
            m_shellProcess->waitForFinished(1000);
            if (m_shellProcess->state() != QProcess::NotRunning) {
                m_shellProcess->kill();
            }
        }
        delete m_shellProcess;
    }
    if (m_outputTimer) {
        delete m_outputTimer;
    }
}

// ============================================================================
// UI Setup
// ============================================================================

void TerminalTab::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_shellLabel = new QLabel(m_config.shell, this);
    m_shellLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    m_shellLabel->setMinimumWidth(100);
    toolbarLayout->addWidget(m_shellLabel);
    
    toolbarLayout->addWidget(new QLabel(" | ", this));
    
    m_dirLabel = new QLabel(m_workingDirectory, this);
    m_dirLabel->setStyleSheet("color: #666;");
    m_dirLabel->setElideMode(Qt::ElideLeft);
    toolbarLayout->addWidget(m_dirLabel, 1);
    
    toolbarLayout->addWidget(new QLabel(" | ", this));
    
    m_clearBtn = new QPushButton("Clear", this);
    m_clearBtn->setMaximumWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &TerminalTab::onClearClicked);
    toolbarLayout->addWidget(m_clearBtn);
    
    m_copyBtn = new QPushButton("Copy", this);
    m_copyBtn->setMaximumWidth(60);
    connect(m_copyBtn, &QPushButton::clicked, this, &TerminalTab::onCopyClicked);
    toolbarLayout->addWidget(m_copyBtn);
    
    m_pasteBtn = new QPushButton("Paste", this);
    m_pasteBtn->setMaximumWidth(60);
    connect(m_pasteBtn, &QPushButton::clicked, this, &TerminalTab::onPasteClicked);
    toolbarLayout->addWidget(m_pasteBtn);
    
    m_stopBtn = new QPushButton("Stop", this);
    m_stopBtn->setMaximumWidth(60);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        if (m_shellProcess) {
            m_shellProcess->terminate();
        }
    });
    toolbarLayout->addWidget(m_stopBtn);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Output display
    m_outputDisplay = new QTextEdit(this);
    m_outputDisplay->setReadOnly(true);
    m_outputDisplay->setFont(QFont(m_fontFamily, m_fontSize));
    m_outputDisplay->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_outputDisplay, &QTextEdit::customContextMenuRequested,
            this, &TerminalTab::onContextMenuRequested);
    mainLayout->addWidget(m_outputDisplay, 1);
    
    // Input line
    QHBoxLayout* inputLayout = new QHBoxLayout();
    
    QLabel* promptLabel = new QLabel(">>> ", this);
    promptLabel->setStyleSheet("color: #2196F3; font-weight: bold;");
    inputLayout->addWidget(promptLabel);
    
    m_inputLine = new QLineEdit(this);
    m_inputLine->setFont(QFont(m_fontFamily, m_fontSize));
    connect(m_inputLine, &QLineEdit::returnPressed, this, &TerminalTab::onInputLineEnter);
    connect(m_inputLine, &QLineEdit::textChanged, this, [this](const QString& text) {
        // Handle command history navigation with Ctrl+P/N
    });
    inputLayout->addWidget(m_inputLine);
    
    mainLayout->addLayout(inputLayout);
    
    // Setup output timer for buffering
    m_outputTimer = new QTimer(this);
    connect(m_outputTimer, &QTimer::timeout, this, &TerminalTab::onOutputTimeout);
    m_outputTimer->start(m_config.updateInterval);
}

void TerminalTab::connectSignals()
{
    // Signals connected in setupUI and slot implementations
}

// ============================================================================
// Terminal Control
// ============================================================================

void TerminalTab::start()
{
    if (m_isRunning) {
        return;
    }
    
    m_shellProcess = new QProcess(this);
    m_shellProcess->setWorkingDirectory(m_workingDirectory);
    m_shellProcess->setProcessChannelMode(QProcess::MergedChannels);
    
    // Setup environment
    QProcessEnvironment env;
    if (m_config.inheritParentEnv) {
        env = QProcessEnvironment::systemEnvironment();
    }
    for (auto it = m_config.environment.begin(); it != m_config.environment.end(); ++it) {
        env.insert(it.key(), it.value());
    }
    m_shellProcess->setProcessEnvironment(env);
    
    // Determine shell
    QString shell = m_config.shell;
    if (shell.isEmpty()) {
#ifdef Q_OS_WIN
        shell = "cmd.exe";
#else
        shell = "/bin/bash";
#endif
    }
    
    // Connect signals
    connect(m_shellProcess, &QProcess::readyRead, this, &TerminalTab::onProcessReadyRead);
    connect(m_shellProcess, QOverload<QProcess::ProcessError>::of(&QProcess::error),
            this, &TerminalTab::onProcessError);
    connect(m_shellProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalTab::onProcessFinished);
    connect(m_shellProcess, &QProcess::started, this, &TerminalTab::onProcessStarted);
    
    // Start process
    m_shellProcess->start(shell, m_config.initialArgs);
    m_isRunning = true;
    m_stopBtn->setEnabled(true);
    
    emit processStarted();
}

void TerminalTab::stop()
{
    if (!m_shellProcess) {
        return;
    }
    
    m_shellProcess->terminate();
    if (!m_shellProcess->waitForFinished(1000)) {
        m_shellProcess->kill();
        m_shellProcess->waitForFinished(500);
    }
    
    m_isRunning = false;
    m_stopBtn->setEnabled(false);
}

void TerminalTab::kill()
{
    if (!m_shellProcess) {
        return;
    }
    
    m_shellProcess->kill();
    m_shellProcess->waitForFinished(500);
    
    m_isRunning = false;
    m_stopBtn->setEnabled(false);
}

bool TerminalTab::isRunning() const
{
    return m_isRunning && m_shellProcess && m_shellProcess->state() == QProcess::Running;
}

// ============================================================================
// Input/Output
// ============================================================================

void TerminalTab::sendCommand(const QString& command)
{
    if (!isRunning()) {
        start();
    }
    
    m_currentCommand = command;
    m_commandHistory.append(command);
    m_historyIndex = m_commandHistory.size();
    
    // Limit history size
    if (m_commandHistory.size() > MAX_HISTORY) {
        m_commandHistory.removeFirst();
    }
    
    m_shellProcess->write(command.toUtf8() + "\n");
    m_shellProcess->waitForBytesWritten(100);
    
    emit commandExecuted(command);
}

void TerminalTab::sendInput(const QString& input)
{
    if (!isRunning()) {
        return;
    }
    
    m_shellProcess->write(input.toUtf8());
    m_shellProcess->waitForBytesWritten(100);
}

void TerminalTab::sendKey(Qt::Key key)
{
    if (!isRunning()) {
        return;
    }
    
    QByteArray bytes;
    switch (key) {
        case Qt::Key_Up:
            bytes = "\x1b[A";
            break;
        case Qt::Key_Down:
            bytes = "\x1b[B";
            break;
        case Qt::Key_Right:
            bytes = "\x1b[C";
            break;
        case Qt::Key_Left:
            bytes = "\x1b[D";
            break;
        case Qt::Key_Home:
            bytes = "\x1b[H";
            break;
        case Qt::Key_End:
            bytes = "\x1b[F";
            break;
        case Qt::Key_Delete:
            bytes = "\x1b[3~";
            break;
        case Qt::Key_Backspace:
            bytes = "\x7f";
            break;
        case Qt::Key_Tab:
            bytes = "\t";
            break;
        default:
            return;
    }
    
    m_shellProcess->write(bytes);
    m_shellProcess->waitForBytesWritten(100);
}

QString TerminalTab::getOutput() const
{
    return m_outputDisplay->toPlainText();
}

QString TerminalTab::getSelectedText() const
{
    return m_outputDisplay->textCursor().selectedText();
}

// ============================================================================
// Configuration
// ============================================================================

void TerminalTab::setWorkingDirectory(const QString& dir)
{
    m_workingDirectory = dir;
    m_dirLabel->setText(dir);
    m_dirLabel->setToolTip(dir);
    
    emit workingDirectoryChanged(dir);
}

void TerminalTab::setEnvironmentVariable(const QString& name, const QString& value)
{
    m_config.environment[name] = value;
}

// ============================================================================
// History Management
// ============================================================================

void TerminalTab::clearHistory()
{
    m_commandHistory.clear();
    m_historyIndex = -1;
}

void TerminalTab::clearOutput()
{
    m_outputDisplay->clear();
    m_outputLines.clear();
}

// ============================================================================
// Display Settings
// ============================================================================

void TerminalTab::setFontSize(int size)
{
    m_fontSize = size;
    QFont font(m_fontFamily, size);
    m_outputDisplay->setFont(font);
    m_inputLine->setFont(font);
}

int TerminalTab::getFontSize() const
{
    return m_fontSize;
}

void TerminalTab::setFontFamily(const QString& family)
{
    m_fontFamily = family;
    QFont font(family, m_fontSize);
    m_outputDisplay->setFont(font);
    m_inputLine->setFont(font);
}

QString TerminalTab::getFontFamily() const
{
    return m_fontFamily;
}

void TerminalTab::clear()
{
    clearOutput();
}

// ============================================================================
// State Query
// ============================================================================

int TerminalTab::getLineCount() const
{
    return m_outputLines.size();
}

QString TerminalTab::getLastLine() const
{
    if (m_outputLines.isEmpty()) {
        return "";
    }
    return m_outputLines.last();
}

// ============================================================================
// Copy/Paste
// ============================================================================

void TerminalTab::copy()
{
    if (m_outputDisplay->textCursor().hasSelection()) {
        m_outputDisplay->copy();
    }
}

void TerminalTab::paste()
{
    if (isRunning()) {
        QClipboard* clipboard = QApplication::clipboard();
        QString text = clipboard->text();
        sendInput(text);
    }
}

void TerminalTab::selectAll()
{
    m_outputDisplay->selectAll();
}

// ============================================================================
// Event Handlers
// ============================================================================

void TerminalTab::keyPressEvent(QKeyEvent* event)
{
    if (!event) {
        return;
    }
    
    QWidget::keyPressEvent(event);
}

void TerminalTab::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_inputLine->setFocus();
}

void TerminalTab::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    selectAll();
}

void TerminalTab::contextMenuEvent(QContextMenuEvent* event)
{
    onContextMenuRequested(event->pos());
}

// ============================================================================
// Slot Implementations
// ============================================================================

void TerminalTab::onProcessReadyRead()
{
    if (!m_shellProcess) {
        return;
    }
    
    QByteArray data = m_shellProcess->readAll();
    if (!data.isEmpty()) {
        QString output = QString::fromUtf8(data);
        m_outputBuffer += output;
        
        emit outputReceived(output);
    }
}

void TerminalTab::onProcessError()
{
    if (!m_shellProcess) {
        return;
    }
    
    QString errorStr;
    switch (m_shellProcess->error()) {
        case QProcess::FailedToStart:
            errorStr = "Failed to start shell";
            break;
        case QProcess::Crashed:
            errorStr = "Shell crashed";
            break;
        case QProcess::Timedout:
            errorStr = "Shell timeout";
            break;
        case QProcess::WriteError:
            errorStr = "Write error";
            break;
        case QProcess::ReadError:
            errorStr = "Read error";
            break;
        default:
            errorStr = "Unknown error";
            break;
    }
    
    appendOutput(QString("\n[ERROR] %1\n").arg(errorStr), false);
    emit processError(errorStr);
}

void TerminalTab::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_isRunning = false;
    m_stopBtn->setEnabled(false);
    
    QString message = QString("\n[Process finished with exit code %1]\n").arg(exitCode);
    appendOutput(message, false);
    
    emit processFinished(exitCode);
}

void TerminalTab::onProcessStarted()
{
    m_isRunning = true;
    m_stopBtn->setEnabled(true);
    
    updatePrompt();
}

void TerminalTab::onInputLineEnter()
{
    QString command = m_inputLine->text();
    if (command.isEmpty()) {
        return;
    }
    
    // Append to output
    appendOutput(QString("%1%2\n").arg(getPrompt(), command), false);
    
    // Send command
    sendCommand(command);
    
    // Clear input
    m_inputLine->clear();
    m_historyIndex = m_commandHistory.size();
}

void TerminalTab::onClearClicked()
{
    clearOutput();
}

void TerminalTab::onCopyClicked()
{
    copy();
}

void TerminalTab::onPasteClicked()
{
    paste();
}

void TerminalTab::onHistoryUp()
{
    if (m_commandHistory.isEmpty()) {
        return;
    }
    
    if (m_historyIndex > 0) {
        m_historyIndex--;
        m_inputLine->setText(m_commandHistory[m_historyIndex]);
    }
}

void TerminalTab::onHistoryDown()
{
    if (m_commandHistory.isEmpty()) {
        return;
    }
    
    if (m_historyIndex < m_commandHistory.size() - 1) {
        m_historyIndex++;
        m_inputLine->setText(m_commandHistory[m_historyIndex]);
    } else if (m_historyIndex == m_commandHistory.size() - 1) {
        m_historyIndex++;
        m_inputLine->clear();
    }
}

void TerminalTab::onOutputTimeout()
{
    if (!m_outputBuffer.isEmpty()) {
        parseAndAppendOutput(m_outputBuffer);
        m_outputBuffer.clear();
    }
}

void TerminalTab::onContextMenuRequested(const QPoint& pos)
{
    QMenu menu;
    
    QAction* copyAction = menu.addAction("Copy");
    connect(copyAction, &QAction::triggered, this, &TerminalTab::copy);
    
    QAction* pasteAction = menu.addAction("Paste");
    connect(pasteAction, &QAction::triggered, this, &TerminalTab::paste);
    
    menu.addSeparator();
    
    QAction* selectAllAction = menu.addAction("Select All");
    connect(selectAllAction, &QAction::triggered, this, &TerminalTab::selectAll);
    
    menu.addSeparator();
    
    QAction* clearAction = menu.addAction("Clear");
    connect(clearAction, &QAction::triggered, this, &TerminalTab::clearOutput);
    
    menu.exec(m_outputDisplay->mapToGlobal(pos));
}

// ============================================================================
// Helper Methods
// ============================================================================

void TerminalTab::appendOutput(const QString& text, bool parsed)
{
    if (parsed) {
        parseAndAppendOutput(text);
    } else {
        m_outputDisplay->append(text);
        m_outputLines.append(text);
        
        // Limit output buffer
        if (m_outputLines.size() > m_config.bufferSize) {
            m_outputLines.removeFirst();
        }
    }
    
    scrollToBottom();
}

void TerminalTab::parseAndAppendOutput(const QString& text)
{
    QList<ANSISegment> segments = m_ansiParser.parseLine(text);
    
    QTextCursor cursor = m_outputDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    for (const auto& segment : segments) {
        if (!segment.text.isEmpty()) {
            cursor.insertText(segment.text, segment.state.toFormat());
        }
    }
    
    m_outputDisplay->setTextCursor(cursor);
    
    QStringList lines = text.split('\n');
    for (const QString& line : lines) {
        if (!line.isEmpty()) {
            m_outputLines.append(line);
        }
    }
    
    // Limit output buffer
    if (m_outputLines.size() > m_config.bufferSize) {
        int excess = m_outputLines.size() - m_config.bufferSize;
        for (int i = 0; i < excess; ++i) {
            m_outputLines.removeFirst();
        }
    }
}

void TerminalTab::updatePrompt()
{
    // Update prompt display
}

void TerminalTab::executeCommand(const QString& cmd)
{
    sendCommand(cmd);
}

QString TerminalTab::getPrompt() const
{
    return ">>> ";
}

QString TerminalTab::extractWorkingDirectory(const QString& output)
{
    // Parse working directory from output if available
    return m_workingDirectory;
}

void TerminalTab::updateButtonStates()
{
    m_stopBtn->setEnabled(isRunning());
}

void TerminalTab::scrollToBottom()
{
    QScrollBar* scrollBar = m_outputDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void TerminalTab::handleCommandHistory(bool up)
{
    if (up) {
        onHistoryUp();
    } else {
        onHistoryDown();
    }
}
