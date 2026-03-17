// TerminalPanel.cpp - Main Terminal Panel Implementation
// Complete terminal panel with multi-tab support and shell management

#include "TerminalPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QDebug>

#ifdef Q_OS_WIN
    #include <windows.h>
    #include <shlobj.h>
#else
    #include <unistd.h>
    #include <pwd.h>
#endif

// ============================================================================
// Constructor & Initialization
// ============================================================================

TerminalPanel::TerminalPanel(QWidget* parent)
    : QDockWidget("Terminal", parent)
    , m_nextTabId(0)
    , m_tabRightClickIndex(-1)
    , m_fontSize(10)
    , m_fontFamily("Courier New")
    , m_bufferSize(10000)
{
    setupUI();
    connectSignals();
    detectAvailableShells();
    
    // Create default terminal
    addNewTerminal("Terminal 1");
}

TerminalPanel::~TerminalPanel()
{
    // Cleanup handled by Qt parent ownership
}

// ============================================================================
// UI Setup
// ============================================================================

void TerminalPanel::setupUI()
{
    QWidget* mainWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_addBtn = new QPushButton("+", this);
    m_addBtn->setMaximumWidth(40);
    connect(m_addBtn, &QPushButton::clicked, this, &TerminalPanel::onAddTerminalClicked);
    toolbarLayout->addWidget(m_addBtn);
    
    m_removeBtn = new QPushButton("-", this);
    m_removeBtn->setMaximumWidth(40);
    connect(m_removeBtn, &QPushButton::clicked, this, &TerminalPanel::onRemoveTerminalClicked);
    toolbarLayout->addWidget(m_removeBtn);
    
    toolbarLayout->addWidget(new QLabel("Shell:", this));
    
    m_shellCombo = new QComboBox(this);
    m_shellCombo->setMinimumWidth(120);
    connect(m_shellCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TerminalPanel::onShellComboChanged);
    toolbarLayout->addWidget(m_shellCombo);
    
    toolbarLayout->addWidget(new QLabel("|", this));
    
    m_clearBtn = new QPushButton("Clear", this);
    m_clearBtn->setMaximumWidth(60);
    connect(m_clearBtn, &QPushButton::clicked, this, &TerminalPanel::onClearClicked);
    toolbarLayout->addWidget(m_clearBtn);
    
    m_stopBtn = new QPushButton("Stop", this);
    m_stopBtn->setMaximumWidth(60);
    connect(m_stopBtn, &QPushButton::clicked, this, &TerminalPanel::onStopClicked);
    toolbarLayout->addWidget(m_stopBtn);
    
    m_killBtn = new QPushButton("Kill", this);
    m_killBtn->setMaximumWidth(60);
    connect(m_killBtn, &QPushButton::clicked, this, &TerminalPanel::onKillClicked);
    toolbarLayout->addWidget(m_killBtn);
    
    m_splitBtn = new QPushButton("Split", this);
    m_splitBtn->setMaximumWidth(60);
    connect(m_splitBtn, &QPushButton::clicked, this, &TerminalPanel::onSplitTerminal);
    toolbarLayout->addWidget(m_splitBtn);
    
    toolbarLayout->addStretch();
    
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("color: #2196F3;");
    toolbarLayout->addWidget(m_statusLabel);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    m_tabWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, &TerminalPanel::onTabCloseRequested);
    connect(m_tabWidget, &QTabWidget::currentChanged, this, &TerminalPanel::onTabChanged);
    connect(m_tabWidget, &QTabWidget::customContextMenuRequested,
            this, &TerminalPanel::onTabRightClicked);
    mainLayout->addWidget(m_tabWidget);
    
    setWidget(mainWidget);
    setMinimumHeight(200);
}

void TerminalPanel::connectSignals()
{
    // Signals connected in setupUI
}

// ============================================================================
// Tab Management
// ============================================================================

TerminalTab* TerminalPanel::addNewTerminal(const QString& name, const QString& shell)
{
    TerminalConfig config;
    config.name = name.isEmpty() ? QString("Terminal %1").arg(m_nextTabId + 1) : name;
    config.shell = shell.isEmpty() ? m_defaultShell : shell;
    config.workingDirectory = QDir::homePath();
    
    return addTerminal(config, config.name);
}

TerminalTab* TerminalPanel::addTerminal(const TerminalTab::TerminalConfig& config, const QString& name)
{
    TerminalTab* tab = new TerminalTab(config, this);
    
    QString tabName = name.isEmpty() ? QString("Terminal %1").arg(m_nextTabId + 1) : name;
    int index = m_tabWidget->addTab(tab, tabName);
    
    m_tabNames[index] = tabName;
    m_tabConfigs[index] = config;
    
    // Connect signals
    connect(tab, &TerminalTab::outputReceived, this, [this, index](const QString& output) {
        emit outputReceived(output, index);
    });
    connect(tab, &TerminalTab::processFinished, this, [this, index](int exitCode) {
        emit processFinished(exitCode, index);
        m_statusLabel->setText(QString("Process %1 finished [%2]").arg(index).arg(exitCode));
    });
    connect(tab, &TerminalTab::processError, this, [this, index](const QString& error) {
        m_statusLabel->setText(QString("Terminal %1 error: %2").arg(index).arg(error));
    });
    connect(tab, &TerminalTab::commandExecuted, this, [this, index](const QString& command) {
        emit commandExecuted(command, index);
        m_statusLabel->setText(QString("Terminal %1: %2").arg(index).arg(command));
    });
    
    m_nextTabId++;
    emit terminalCreated(tab);
    
    return tab;
}

void TerminalPanel::removeTerminal(int index)
{
    if (index < 0 || index >= m_tabWidget->count()) {
        return;
    }
    
    TerminalTab* tab = dynamic_cast<TerminalTab*>(m_tabWidget->widget(index));
    if (tab) {
        if (tab->isRunning()) {
            tab->stop();
        }
        m_tabWidget->removeTab(index);
        m_tabNames.remove(index);
        m_tabConfigs.remove(index);
        tab->deleteLater();
        
        emit terminalClosed(index);
    }
}

void TerminalPanel::removeAllTerminals()
{
    while (m_tabWidget->count() > 0) {
        removeTerminal(0);
    }
}

TerminalTab* TerminalPanel::getCurrentTerminal() const
{
    return dynamic_cast<TerminalTab*>(m_tabWidget->currentWidget());
}

TerminalTab* TerminalPanel::getTerminal(int index) const
{
    return dynamic_cast<TerminalTab*>(m_tabWidget->widget(index));
}

int TerminalPanel::getTerminalCount() const
{
    return m_tabWidget->count();
}

int TerminalPanel::getCurrentTabIndex() const
{
    return m_tabWidget->currentIndex();
}

void TerminalPanel::setCurrentTab(int index)
{
    if (index >= 0 && index < m_tabWidget->count()) {
        m_tabWidget->setCurrentIndex(index);
    }
}

// ============================================================================
// Terminal Control
// ============================================================================

void TerminalPanel::execute(const QString& command)
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->sendCommand(command);
    }
}

void TerminalPanel::sendInput(const QString& input)
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->sendInput(input);
    }
}

void TerminalPanel::sendKey(Qt::Key key)
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->sendKey(key);
    }
}

void TerminalPanel::stopCurrent()
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->stop();
    }
}

void TerminalPanel::killCurrent()
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->kill();
    }
}

void TerminalPanel::clearCurrent()
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->clearOutput();
    }
}

void TerminalPanel::clearAll()
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TerminalTab* tab = getTerminal(i);
        if (tab) {
            tab->clearOutput();
        }
    }
}

// ============================================================================
// Shell Management
// ============================================================================

QStringList TerminalPanel::getAvailableShells() const
{
    return m_availableShells;
}

QString TerminalPanel::getDefaultShell() const
{
    return m_defaultShell;
}

void TerminalPanel::setDefaultShell(const QString& shell)
{
    m_defaultShell = shell;
}

bool TerminalPanel::isShellAvailable(const QString& shell) const
{
    return m_availableShells.contains(shell);
}

// ============================================================================
// Session Management
// ============================================================================

void TerminalPanel::saveSessionTabs(const QString& filename)
{
    QJsonArray tabsArray;
    
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TerminalTab* tab = getTerminal(i);
        if (tab) {
            QJsonObject tabObj;
            tabObj["name"] = m_tabNames[i];
            tabObj["shell"] = m_tabConfigs[i].shell;
            tabObj["workdir"] = m_tabConfigs[i].workingDirectory;
            
            tabsArray.append(tabObj);
        }
    }
    
    QJsonObject root;
    root["tabs"] = tabsArray;
    
    QJsonDocument doc(root);
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void TerminalPanel::loadSessionTabs(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return;
    }
    
    QJsonArray tabsArray = doc.object()["tabs"].toArray();
    
    removeAllTerminals();
    
    for (const QJsonValue& value : tabsArray) {
        QJsonObject tabObj = value.toObject();
        
        TerminalConfig config;
        config.name = tabObj["name"].toString();
        config.shell = tabObj["shell"].toString();
        config.workingDirectory = tabObj["workdir"].toString();
        
        addTerminal(config, config.name);
    }
}

QList<TerminalConfig> TerminalPanel::getSessionTabs() const
{
    QList<TerminalConfig> configs;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        configs.append(m_tabConfigs[i]);
    }
    return configs;
}

void TerminalPanel::setSessionTabs(const QList<TerminalConfig>& configs)
{
    removeAllTerminals();
    for (const auto& config : configs) {
        addTerminal(config, config.name);
    }
}

// ============================================================================
// Display Settings
// ============================================================================

void TerminalPanel::setFontSize(int size)
{
    m_fontSize = size;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TerminalTab* tab = getTerminal(i);
        if (tab) {
            tab->setFontSize(size);
        }
    }
}

void TerminalPanel::setFontFamily(const QString& family)
{
    m_fontFamily = family;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TerminalTab* tab = getTerminal(i);
        if (tab) {
            tab->setFontFamily(family);
        }
    }
}

void TerminalPanel::setBufferSize(int lines)
{
    m_bufferSize = lines;
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TerminalTab* tab = getTerminal(i);
        if (tab) {
            tab->m_config.bufferSize = lines;
        }
    }
}

// ============================================================================
// Configuration
// ============================================================================

void TerminalPanel::setTerminalConfig(const TerminalConfig& config)
{
    m_defaultConfig = config;
    applyConfigToAll(config);
}

TerminalConfig TerminalPanel::getTerminalConfig() const
{
    return m_defaultConfig;
}

// ============================================================================
// Working Directory
// ============================================================================

QString TerminalPanel::getCurrentDirectory() const
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        return tab->getWorkingDirectory();
    }
    return QDir::homePath();
}

void TerminalPanel::setCurrentDirectory(const QString& dir)
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        tab->setWorkingDirectory(dir);
    }
}

// ============================================================================
// History Management
// ============================================================================

void TerminalPanel::saveCommandHistory(const QString& filename)
{
    TerminalTab* tab = getCurrentTerminal();
    if (!tab) {
        return;
    }
    
    QStringList history = tab->getCommandHistory();
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        for (const QString& cmd : history) {
            file.write(cmd.toUtf8() + "\n");
        }
        file.close();
    }
}

void TerminalPanel::loadCommandHistory(const QString& filename)
{
    TerminalTab* tab = getCurrentTerminal();
    if (!tab) {
        return;
    }
    
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QStringList history;
        while (!file.atEnd()) {
            QString line = file.readLine().trimmed();
            if (!line.isEmpty()) {
                history.append(line);
            }
        }
        file.close();
        
        tab->setCommandHistory(history);
    }
}

QStringList TerminalPanel::getCommandHistory() const
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        return tab->getCommandHistory();
    }
    return QStringList();
}

// ============================================================================
// Slot Implementations
// ============================================================================

void TerminalPanel::onTabChanged(int index)
{
    Q_UNUSED(index);
    m_statusLabel->setText("Ready");
    emit terminalSwitched(index);
}

void TerminalPanel::onTabCloseRequested(int index)
{
    removeTerminal(index);
}

void TerminalPanel::onTabRightClicked(const QPoint& pos)
{
    m_tabRightClickIndex = m_tabWidget->tabBar()->tabAt(pos);
    if (m_tabRightClickIndex < 0) {
        return;
    }
    
    QMenu menu;
    
    QAction* renameAction = menu.addAction("Rename");
    connect(renameAction, &QAction::triggered, this, &TerminalPanel::onRenameTerminal);
    
    QAction* duplicateAction = menu.addAction("Duplicate");
    connect(duplicateAction, &QAction::triggered, this, &TerminalPanel::onDuplicateTerminal);
    
    menu.addSeparator();
    
    QAction* closeAction = menu.addAction("Close");
    connect(closeAction, &QAction::triggered, this, &TerminalPanel::onCloseTerminal);
    
    menu.exec(m_tabWidget->tabBar()->mapToGlobal(pos));
}

void TerminalPanel::onAddTerminalClicked()
{
    addNewTerminal();
}

void TerminalPanel::onAddTerminalWithShell()
{
    QString shell = QInputDialog::getItem(this, "Select Shell", "Shell:",
                                         m_availableShells, 0, false);
    if (!shell.isEmpty()) {
        addNewTerminal(QString(), shell);
    }
}

void TerminalPanel::onRemoveTerminalClicked()
{
    int current = m_tabWidget->currentIndex();
    if (current >= 0) {
        removeTerminal(current);
    }
}

void TerminalPanel::onClearClicked()
{
    clearCurrent();
}

void TerminalPanel::onStopClicked()
{
    stopCurrent();
}

void TerminalPanel::onKillClicked()
{
    killCurrent();
}

void TerminalPanel::onSplitTerminal()
{
    TerminalTab* current = getCurrentTerminal();
    if (current) {
        TerminalConfig config = m_tabConfigs[m_tabWidget->currentIndex()];
        addTerminal(config, QString("%1 (split)").arg(m_tabNames[m_tabWidget->currentIndex()]));
    }
}

void TerminalPanel::onShellSelected(const QString& shell)
{
    // Shell selected from combo box
    Q_UNUSED(shell);
}

void TerminalPanel::onShellComboChanged(int index)
{
    if (index >= 0 && index < m_availableShells.size()) {
        m_defaultShell = m_availableShells[index];
    }
}

void TerminalPanel::onRenameTerminal()
{
    if (m_tabRightClickIndex < 0) {
        return;
    }
    
    QString oldName = m_tabWidget->tabText(m_tabRightClickIndex);
    bool ok = false;
    QString newName = QInputDialog::getText(this, "Rename Terminal", "New name:",
                                           QLineEdit::Normal, oldName, &ok);
    if (ok && !newName.isEmpty()) {
        m_tabWidget->setTabText(m_tabRightClickIndex, newName);
        m_tabNames[m_tabRightClickIndex] = newName;
    }
}

void TerminalPanel::onDuplicateTerminal()
{
    if (m_tabRightClickIndex < 0) {
        return;
    }
    
    TerminalConfig config = m_tabConfigs[m_tabRightClickIndex];
    QString name = m_tabNames[m_tabRightClickIndex] + " (copy)";
    addTerminal(config, name);
}

void TerminalPanel::onCloseTerminal()
{
    if (m_tabRightClickIndex >= 0) {
        removeTerminal(m_tabRightClickIndex);
    }
}

void TerminalPanel::onTerminalProperties()
{
    // Show terminal properties dialog
}

void TerminalPanel::onFontSizeChanged(int size)
{
    setFontSize(size);
}

void TerminalPanel::onFontFamilyChanged(const QString& family)
{
    setFontFamily(family);
}

void TerminalPanel::onBufferSizeChanged(int size)
{
    setBufferSize(size);
}

// ============================================================================
// Helper Methods
// ============================================================================

void TerminalPanel::setupUI()
{
    // UI setup already done in TerminalPanel::setupUI()
}

void TerminalPanel::connectSignals()
{
    // Signals connected in setupUI()
}

void TerminalPanel::detectAvailableShells()
{
    m_availableShells.clear();
    
#ifdef Q_OS_WIN
    // Windows shells
    m_availableShells << "cmd.exe" << "powershell.exe";
    
    // Check for Git Bash
    QFile gitBash("C:/Program Files/Git/bin/bash.exe");
    if (gitBash.exists()) {
        m_availableShells << "bash.exe (Git)";
    }
    
    // Check for WSL
    QFile wsl("C:/Windows/System32/wsl.exe");
    if (wsl.exists()) {
        m_availableShells << "wsl.exe";
    }
    
    m_defaultShell = "cmd.exe";
#else
    // Unix-like shells
    m_availableShells << "/bin/bash" << "/bin/sh" << "/bin/zsh";
    m_defaultShell = "/bin/bash";
#endif
    
    // Update shell combo
    m_shellCombo->clear();
    m_shellCombo->addItems(m_availableShells);
}

QString TerminalPanel::getDefaultShellForOS() const
{
#ifdef Q_OS_WIN
    return "cmd.exe";
#else
    return "/bin/bash";
#endif
}

void TerminalPanel::createContextMenu()
{
    // Context menu created dynamically on right-click
}

void TerminalPanel::applyConfigToAll(const TerminalConfig& config)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        TerminalTab* tab = getTerminal(i);
        if (tab) {
            if (!config.shell.isEmpty()) {
                // Would need to restart terminal
            }
            if (config.bufferSize > 0) {
                tab->m_config.bufferSize = config.bufferSize;
            }
        }
    }
}

void TerminalPanel::updateTabTitle(int index)
{
    if (index >= 0 && index < m_tabWidget->count()) {
        TerminalTab* tab = getTerminal(index);
        if (tab) {
            QString title = m_tabNames[index];
            if (!tab->isRunning()) {
                title += " [stopped]";
            }
            m_tabWidget->setTabText(index, title);
        }
    }
}

void TerminalPanel::updateToolbarState()
{
    TerminalTab* tab = getCurrentTerminal();
    if (tab) {
        m_stopBtn->setEnabled(tab->isRunning());
        m_killBtn->setEnabled(tab->isRunning());
    } else {
        m_stopBtn->setEnabled(false);
        m_killBtn->setEnabled(false);
    }
}

TerminalConfig TerminalPanel::createConfigForShell(const QString& shell) const
{
    TerminalConfig config;
    config.shell = shell;
    config.workingDirectory = QDir::homePath();
    config.inheritParentEnv = true;
    return config;
}

void TerminalPanel::closeEvent(QCloseEvent* event)
{
    // Cleanup terminals if needed
    QDockWidget::closeEvent(event);
}
