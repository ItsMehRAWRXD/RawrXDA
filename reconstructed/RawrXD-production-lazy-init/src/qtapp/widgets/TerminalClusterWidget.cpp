/**
 * @file TerminalClusterWidget.cpp
 * @brief Production implementation of TerminalClusterWidget
 * 
 * Replaces bespoke terminal panel with production-grade terminal components
 */

#include "TerminalClusterWidget.h"
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QDebug>
#include <QDateTime>

TerminalClusterWidget::TerminalClusterWidget(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_powerShellTerminal(nullptr)
    , m_cmdTerminal(nullptr)
    , m_fixButton(nullptr)
    , m_autoHealCheckbox(nullptr)
    , m_autonomousMode(false)
    , m_currentShellType(TerminalManager::PowerShell)
{
    qDebug() << "[TerminalClusterWidget] Constructor";
    setupUI();
}

TerminalClusterWidget::~TerminalClusterWidget()
{
    qDebug() << "[TerminalClusterWidget] Destructor";
    stopShells();
}

void TerminalClusterWidget::initialize()
{
    qDebug() << "[TerminalClusterWidget] Initializing";
    
    if (m_powerShellTerminal) {
        m_powerShellTerminal->initialize();
        m_powerShellTerminal->startShell(TerminalManager::PowerShell);
    }
    
    if (m_cmdTerminal) {
        m_cmdTerminal->initialize();
        m_cmdTerminal->startShell(TerminalManager::CommandPrompt);
    }
    
    setupConnections();
}

void TerminalClusterWidget::startShells()
{
    qDebug() << "[TerminalClusterWidget] Starting shells";
    
    if (m_powerShellTerminal && !m_powerShellTerminal->isRunning()) {
        m_powerShellTerminal->startShell(TerminalManager::PowerShell);
    }
    
    if (m_cmdTerminal && !m_cmdTerminal->isRunning()) {
        m_cmdTerminal->startShell(TerminalManager::CommandPrompt);
    }
}

void TerminalClusterWidget::stopShells()
{
    qDebug() << "[TerminalClusterWidget] Stopping shells";
    
    if (m_powerShellTerminal) {
        m_powerShellTerminal->stopShell();
    }
    
    if (m_cmdTerminal) {
        m_cmdTerminal->stopShell();
    }
}

void TerminalClusterWidget::askAIToFix(TerminalManager::ShellType shellType)
{
    qDebug() << "[TerminalClusterWidget] Asking AI to fix shell:" << shellType;
    
    QString errorText;
    if (shellType == TerminalManager::PowerShell && m_powerShellTerminal) {
        errorText = "PowerShell terminal error";
    } else if (shellType == TerminalManager::CommandPrompt && m_cmdTerminal) {
        errorText = "CMD terminal error";
    }
    
    if (!errorText.isEmpty()) {
        emit errorDetected(errorText, shellType);
    }
}

void TerminalClusterWidget::setupUI()
{
    qDebug() << "[TerminalClusterWidget] Setting up UI";
    
    setObjectName("TerminalClusterWidget");
    setStyleSheet("QWidget#TerminalClusterWidget { background-color: #1e1e1e; }");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Tab widget for multiple terminals
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setObjectName("TerminalTabs");
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: none; background-color: #1e1e1e; } "
        "QTabBar { background-color: #252526; } "
        "QTabBar::tab { background-color: #252526; color: #969696; padding: 6px 12px; } "
        "QTabBar::tab:selected { background-color: #1e1e1e; color: #ffffff; border-top: 1px solid #007acc; }"
    );
    
    // PowerShell terminal
    m_powerShellTerminal = new TerminalWidget(this);
    setupTerminalTab("PowerShell", TerminalManager::PowerShell);
    
    // CMD terminal
    m_cmdTerminal = new TerminalWidget(this);
    setupTerminalTab("CMD", TerminalManager::CommandPrompt);
    
    mainLayout->addWidget(m_tabWidget);
    
    // AI Fix controls
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    controlsLayout->setContentsMargins(5, 5, 5, 5);
    controlsLayout->setSpacing(10);
    
    m_fixButton = new QPushButton("✨ Fix", this);
    m_fixButton->setToolTip("Autonomous AI Fix");
    m_fixButton->setEnabled(false);
    m_fixButton->setFixedWidth(80);
    m_fixButton->setStyleSheet("QPushButton { background-color: #333; color: #fff; border: 1px solid #555; }");
    
    m_autoHealCheckbox = new QCheckBox("Auto-Heal", this);
    m_autoHealCheckbox->setStyleSheet("QCheckBox { color: #888; font-size: 8pt; }");
    m_autoHealCheckbox->setToolTip("Automatically trigger AI fix when errors are detected");
    
    controlsLayout->addWidget(m_fixButton);
    controlsLayout->addWidget(m_autoHealCheckbox);
    controlsLayout->addStretch();
    
    mainLayout->addLayout(controlsLayout);
}

void TerminalClusterWidget::setupConnections()
{
    qDebug() << "[TerminalClusterWidget] Setting up connections";
    
    // Connect terminal signals
    if (m_powerShellTerminal) {
        connect(m_powerShellTerminal, &TerminalWidget::errorDetected,
                this, &TerminalClusterWidget::onPowerShellErrorDetected);
        connect(m_powerShellTerminal, &TerminalWidget::fixSuggested,
                this, &TerminalClusterWidget::onPowerShellFixSuggested);
    }
    
    if (m_cmdTerminal) {
        connect(m_cmdTerminal, &TerminalWidget::errorDetected,
                this, &TerminalClusterWidget::onCmdErrorDetected);
        connect(m_cmdTerminal, &TerminalWidget::fixSuggested,
                this, &TerminalClusterWidget::onCmdFixSuggested);
    }
    
    // Connect UI controls
    connect(m_fixButton, &QPushButton::clicked,
            this, &TerminalClusterWidget::onFixButtonClicked);
    connect(m_autoHealCheckbox, &QCheckBox::toggled,
            this, &TerminalClusterWidget::onAutoHealToggled);
    
    // Connect tab change to update current shell type
    connect(m_tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        QString tabName = m_tabWidget->tabText(index);
        if (tabName == "PowerShell") {
            m_currentShellType = TerminalManager::PowerShell;
        } else if (tabName == "CMD") {
            m_currentShellType = TerminalManager::CommandPrompt;
        }
    });
}

void TerminalClusterWidget::setupTerminalTab(const QString& tabName, TerminalManager::ShellType shellType)
{
    qDebug() << "[TerminalClusterWidget] Setting up terminal tab:" << tabName;
    
    TerminalWidget* terminal = nullptr;
    if (shellType == TerminalManager::PowerShell) {
        terminal = m_powerShellTerminal;
    } else if (shellType == TerminalManager::CommandPrompt) {
        terminal = m_cmdTerminal;
    }
    
    if (terminal) {
        // TerminalWidget is two-phase; ensure widgets exist before any output is appended.
        terminal->initialize();
        terminal->setObjectName(tabName + "Terminal");
        m_tabWidget->addTab(terminal, tabName);
        appendWelcomeMessage(shellType);
    }
}

void TerminalClusterWidget::appendWelcomeMessage(TerminalManager::ShellType shellType)
{
    QString welcomeMessage;
    if (shellType == TerminalManager::PowerShell) {
        welcomeMessage = "PowerShell 7.x\nCopyright (c) Microsoft Corporation. All rights reserved.\n";
    } else if (shellType == TerminalManager::CommandPrompt) {
        welcomeMessage = "Microsoft Windows [Version 10.0.xxxxx]\n(c) Microsoft Corporation. All rights reserved.\n";
    }
    
    TerminalWidget* terminal = nullptr;
    if (shellType == TerminalManager::PowerShell) {
        terminal = m_powerShellTerminal;
    } else if (shellType == TerminalManager::CommandPrompt) {
        terminal = m_cmdTerminal;
    }
    
    if (terminal) {
        terminal->initialize();
        terminal->appendOutput(welcomeMessage);
    }
}

void TerminalClusterWidget::onPowerShellErrorDetected(const QString& errorText)
{
    qDebug() << "[TerminalClusterWidget] PowerShell error detected:" << errorText;
    emit errorDetected(errorText, TerminalManager::PowerShell);
    
    if (m_autonomousMode) {
        askAIToFix(TerminalManager::PowerShell);
    }
}

void TerminalClusterWidget::onPowerShellFixSuggested(const QString& fixCommand)
{
    qDebug() << "[TerminalClusterWidget] PowerShell fix suggested:" << fixCommand;
    emit fixSuggested(fixCommand, TerminalManager::PowerShell);
}

void TerminalClusterWidget::onCmdErrorDetected(const QString& errorText)
{
    qDebug() << "[TerminalClusterWidget] CMD error detected:" << errorText;
    emit errorDetected(errorText, TerminalManager::CommandPrompt);
    
    if (m_autonomousMode) {
        askAIToFix(TerminalManager::CommandPrompt);
    }
}

void TerminalClusterWidget::onCmdFixSuggested(const QString& fixCommand)
{
    qDebug() << "[TerminalClusterWidget] CMD fix suggested:" << fixCommand;
    emit fixSuggested(fixCommand, TerminalManager::CommandPrompt);
}

void TerminalClusterWidget::onFixButtonClicked()
{
    qDebug() << "[TerminalClusterWidget] Fix button clicked for shell:" << m_currentShellType;
    askAIToFix(m_currentShellType);
}

void TerminalClusterWidget::onAutoHealToggled(bool checked)
{
    qDebug() << "[TerminalClusterWidget] Auto-heal toggled:" << checked;
    m_autonomousMode = checked;
    emit terminalCommand(QString("Autonomous Mode %1").arg(checked ? "ON" : "OFF"), m_currentShellType);
}