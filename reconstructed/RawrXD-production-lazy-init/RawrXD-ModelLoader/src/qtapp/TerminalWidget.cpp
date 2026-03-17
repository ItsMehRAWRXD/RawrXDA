#include "TerminalWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QDateTime>
#include <QKeyEvent>
#include <QScrollBar>

TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent)
    , m_process(std::make_unique<QProcess>(this))
{
    setupUi();
    initializeProcess();
}

TerminalWidget::~TerminalWidget()
{
    terminateProcess();
}

void TerminalWidget::setupUi()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    // Status bar
    m_statusLabel = new QLabel("PowerShell Ready", this);
    m_statusLabel->setStyleSheet("color: #888; font-size: 10px;");
    mainLayout->addWidget(m_statusLabel);

    // Output display
    m_outputDisplay = new QPlainTextEdit(this);
    m_outputDisplay->setReadOnly(true);
    m_outputDisplay->setStyleSheet(
        "background-color: #1e1e1e; color: #d4d4d4; font-family: 'Courier New', monospace; font-size: 10pt;"
    );
    m_outputDisplay->setPlaceholderText("PowerShell terminal output...\n");
    mainLayout->addWidget(m_outputDisplay);

    // Input area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);

    m_inputField = new QLineEdit(this);
    m_inputField->setPlaceholderText("Enter PowerShell command...");
    m_inputField->setStyleSheet("background-color: #252526; color: #d4d4d4;");

    m_executeButton = new QPushButton("Execute", this);
    m_executeButton->setStyleSheet("background-color: #007acc; color: white; padding: 3px 10px;");
    m_executeButton->setMaximumWidth(80);

    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setStyleSheet("background-color: #555; color: white; padding: 3px 10px;");
    m_clearButton->setMaximumWidth(60);

    inputLayout->addWidget(m_inputField, 1);
    inputLayout->addWidget(m_executeButton);
    inputLayout->addWidget(m_clearButton);

    mainLayout->addLayout(inputLayout);

    // Connections
    connect(m_inputField, &QLineEdit::returnPressed, this, &TerminalWidget::onCommandEntered);
    connect(m_executeButton, &QPushButton::clicked, this, &TerminalWidget::onExecuteButtonClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &TerminalWidget::onClearClicked);
}

void TerminalWidget::initializeProcess()
{
    // Use PowerShell on Windows
#ifdef Q_OS_WIN
    m_process->setProgram("powershell.exe");
    m_process->setArguments(QStringList() << "-NoExit" << "-NoProfile");
#else
    m_process->setProgram("/bin/bash");
#endif

    connect(m_process.get(), &QProcess::readyReadStandardOutput, this, &TerminalWidget::onProcessOutput);
    connect(m_process.get(), &QProcess::readyReadStandardError, this, &TerminalWidget::onProcessError);
    connect(m_process.get(), QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TerminalWidget::onProcessFinished);

    // Start the process
    m_process->start();
    if (m_process->waitForStarted()) {
        m_processRunning = true;
        m_statusLabel->setText("PowerShell Connected (PID: " + QString::number(m_process->processId()) + ")");
        appendOutput("PowerShell terminal initialized successfully\n");
    } else {
        m_statusLabel->setText("Failed to start PowerShell");
        appendOutput("ERROR: Failed to start PowerShell process\n", true);
    }
}

void TerminalWidget::onCommandEntered()
{
    onExecuteButtonClicked();
}

void TerminalWidget::onExecuteButtonClicked()
{
    QString command = m_inputField->text().trimmed();
    if (command.isEmpty()) return;

    executeCommand(command);
    addToHistory(command);
    m_inputField->clear();
    m_historyIndex = 0;
}

void TerminalWidget::executeCommand(const QString& command)
{
    if (!m_processRunning) {
        appendOutput("ERROR: PowerShell process not running\n", true);
        return;
    }

    // Write command to PowerShell process
    QString cmdWithNewline = command + "\n";
    m_process->write(cmdWithNewline.toUtf8());

    appendOutput("\n> " + command + "\n");
}

void TerminalWidget::onProcessOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromLocal8Bit(data);
    appendOutput(output);
}

void TerminalWidget::onProcessError()
{
    QByteArray data = m_process->readAllStandardError();
    QString error = QString::fromLocal8Bit(data);
    appendOutput(error, true);
}

void TerminalWidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_processRunning = false;
    m_statusLabel->setText(QString("PowerShell terminated (exit code: %1)").arg(exitCode));
    appendOutput("\n[Process terminated with exit code: " + QString::number(exitCode) + "]\n", true);
    emit processFinished(exitCode);
}

void TerminalWidget::onClearClicked()
{
    clearTerminal();
}

void TerminalWidget::onHistoryUp()
{
    if (m_commandHistory.isEmpty()) return;
    if (m_historyIndex < m_commandHistory.size()) {
        m_historyIndex++;
        m_inputField->setText(m_commandHistory[m_commandHistory.size() - m_historyIndex]);
    }
}

void TerminalWidget::onHistoryDown()
{
    if (m_historyIndex > 1) {
        m_historyIndex--;
        m_inputField->setText(m_commandHistory[m_commandHistory.size() - m_historyIndex]);
    } else if (m_historyIndex == 1) {
        m_historyIndex = 0;
        m_inputField->clear();
    }
}

void TerminalWidget::clearTerminal()
{
    m_outputDisplay->clear();
    appendOutput("Terminal cleared\n");
}

void TerminalWidget::appendOutput(const QString& text, bool isError)
{
    if (isError) {
        m_outputDisplay->setTextColor(QColor(255, 100, 100));
    } else {
        m_outputDisplay->setTextColor(QColor(212, 212, 212));
    }

    m_outputDisplay->insertPlainText(text);

    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_outputDisplay->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

QString TerminalWidget::getHistory() const
{
    return m_commandHistory.join("\n");
}

bool TerminalWidget::isProcessRunning() const
{
    return m_processRunning;
}

void TerminalWidget::terminateProcess()
{
    if (m_processRunning && m_process) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
        }
        m_processRunning = false;
    }
}

void TerminalWidget::addToHistory(const QString& command)
{
    if (!command.isEmpty()) {
        m_commandHistory.append(command);
        if (m_commandHistory.size() > m_maxHistorySize) {
            m_commandHistory.removeFirst();
        }
    }
}
