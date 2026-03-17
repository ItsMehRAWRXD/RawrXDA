/**
 * @file compiler_panel.cpp
 * @brief Implementation of Compiler Panel for PowerShell Compiler Integration
 */

#include "compiler_panel.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDateTime>
#include <QClipboard>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include "logging/logger.h"
#include "metrics/metrics.h"

// Global singleton instance
static std::unique_ptr<CompilerPanel> g_compilerPanel;

CompilerPanel::CompilerPanel(QWidget *parent)
    : QWidget(parent)
    , m_compilerManager(getPowerShellCompilerManager())
    , m_currentSourceCode("")
    , m_currentFilePath("")
{
    setupUI();
    setupConnections();
    loadConfiguration();
    
    // Auto-load compilers from default path
    QString defaultPath = QApplication::applicationDirPath() + "/compilers";
    if (QDir(defaultPath).exists()) {
        m_compilerManager->loadCompilers(defaultPath);
    }
    
    populateLanguageComboBox();
    updateCompilerInfo();
}

CompilerPanel::~CompilerPanel()
{
    saveConfiguration();
}

void CompilerPanel::setCurrentSourceCode(const QString &code)
{
    m_currentSourceCode = code;
}

void CompilerPanel::setCurrentFilePath(const QString &filePath)
{
    m_currentFilePath = filePath;
}

QString CompilerPanel::getSelectedLanguage() const
{
    return m_languageComboBox->currentData().toString();
}

void CompilerPanel::loadConfiguration()
{
    QSettings settings("RawrXD", "AgenticIDE");
    
    settings.beginGroup("CompilerPanel");
    m_timeoutSpinBox->setValue(settings.value("timeout", 30000).toInt());
    m_compilersPathEdit->setText(settings.value("compilersPath", "").toString());
    m_autoDetectCheckBox->setChecked(settings.value("autoDetect", true).toBool());
    m_showOutputCheckBox->setChecked(settings.value("showOutput", true).toBool());
    settings.endGroup();
}

void CompilerPanel::saveConfiguration()
{
    QSettings settings("RawrXD", "AgenticIDE");
    
    settings.beginGroup("CompilerPanel");
    settings.setValue("timeout", m_timeoutSpinBox->value());
    settings.setValue("compilersPath", m_compilersPathEdit->text());
    settings.setValue("autoDetect", m_autoDetectCheckBox->isChecked());
    settings.setValue("showOutput", m_showOutputCheckBox->isChecked());
    settings.endGroup();
}

void CompilerPanel::onCompileClicked()
{
    if (m_currentSourceCode.isEmpty()) {
        QMessageBox::warning(this, "Compile Error", "No source code available for compilation.");
        return;
    }
    
    QString language = getSelectedLanguage();
    if (language.isEmpty()) {
        QMessageBox::warning(this, "Compile Error", "Please select a programming language.");
        return;
    }
    
    // Get output path
    QString outputPath = QFileDialog::getSaveFileName(this, "Save Compiled Output", 
                                                     QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                     "Executable Files (*.exe);;All Files (*.*)");
    
    if (outputPath.isEmpty()) {
        return; // User cancelled
    }
    
    // Build compiler options
    QJsonObject options;
    options["timeout"] = m_timeoutSpinBox->value();
    options["verbose"] = m_showOutputCheckBox->isChecked();
    
    logOutput(QString("Starting compilation for %1 language...").arg(language));
    
    // Start compilation
    CompilationResult result = m_compilerManager->compile(language, m_currentSourceCode, outputPath, options);
    onCompilationFinished(result);
}

void CompilerPanel::onCompileFileClicked()
{
    if (m_currentFilePath.isEmpty()) {
        QMessageBox::warning(this, "Compile Error", "No file selected for compilation.");
        return;
    }
    
    QString language = getSelectedLanguage();
    if (language.isEmpty()) {
        QMessageBox::warning(this, "Compile Error", "Please select a programming language.");
        return;
    }
    
    // Get output path
    QString outputPath = QFileDialog::getSaveFileName(this, "Save Compiled Output", 
                                                     QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
                                                     "Executable Files (*.exe);;All Files (*.*)");
    
    if (outputPath.isEmpty()) {
        return; // User cancelled
    }
    
    // Build compiler options
    QJsonObject options;
    options["timeout"] = m_timeoutSpinBox->value();
    options["verbose"] = m_showOutputCheckBox->isChecked();
    
    logOutput(QString("Starting compilation of file: %1").arg(m_currentFilePath));
    
    // Start compilation
    CompilationResult result = m_compilerManager->compileFile(language, m_currentFilePath, outputPath, options);
    onCompilationFinished(result);
}

void CompilerPanel::onLanguageChanged(int index)
{
    Q_UNUSED(index);
    updateCompilerInfo();
}

void CompilerPanel::onCompilerSettingsChanged()
{
    saveConfiguration();
}

void CompilerPanel::onCompilationFinished(const CompilationResult &result)
{
    if (result.success) {
        logOutput(QString("Compilation completed successfully in %1 ms").arg(result.durationMs));
        logOutput(QString("Output file: %1").arg(result.outputFile));
        if (!result.output.isEmpty()) {
            logOutput("Compiler output:");
            logOutput(result.output);
        }
        
        QMessageBox::information(this, "Compilation Success", 
                                QString("Compilation completed successfully!\nOutput: %1").arg(result.outputFile));
    } else {
        logOutput(QString("Compilation failed after %1 ms").arg(result.durationMs), true);
        logOutput(QString("Error: %1").arg(result.error), true);
        
        QMessageBox::critical(this, "Compilation Failed", 
                             QString("Compilation failed:\n%1").arg(result.error));
    }
    
    updateStats();
}

void CompilerPanel::onCompilationError(const QString &language, const QString &error)
{
    logOutput(QString("Compilation error for %1: %2").arg(language).arg(error), true);
}

void CompilerPanel::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    
    // Create tab widget
    m_tabWidget = new QTabWidget(this);
    m_mainLayout->addWidget(m_tabWidget);
    
    // Setup individual tabs
    setupCompilerTab();
    setupSettingsTab();
    setupOutputTab();
    setupStatsTab();
}

void CompilerPanel::setupCompilerTab()
{
    m_compilerTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_compilerTab);
    
    // Language selection
    QHBoxLayout *languageLayout = new QHBoxLayout();
    languageLayout->addWidget(new QLabel("Language:"));
    m_languageComboBox = new QComboBox();
    languageLayout->addWidget(m_languageComboBox);
    languageLayout->addStretch();
    layout->addLayout(languageLayout);
    
    // Compiler info
    m_compilerInfoLabel = new QLabel("Select a language to see compiler information");
    m_compilerInfoLabel->setWordWrap(true);
    layout->addWidget(m_compilerInfoLabel);
    
    // Compile buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_compileButton = new QPushButton("Compile Current Code");
    m_compileFileButton = new QPushButton("Compile Current File");
    buttonLayout->addWidget(m_compileButton);
    buttonLayout->addWidget(m_compileFileButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    // Progress and status
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
    
    m_statusLabel = new QLabel("Ready");
    layout->addWidget(m_statusLabel);
    
    layout->addStretch();
    m_tabWidget->addTab(m_compilerTab, "Compiler");
}

void CompilerPanel::setupSettingsTab()
{
    m_settingsTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_settingsTab);
    
    // Timeout settings
    QHBoxLayout *timeoutLayout = new QHBoxLayout();
    timeoutLayout->addWidget(new QLabel("Timeout (ms):"));
    m_timeoutSpinBox = new QSpinBox();
    m_timeoutSpinBox->setRange(1000, 300000);
    m_timeoutSpinBox->setValue(30000);
    timeoutLayout->addWidget(m_timeoutSpinBox);
    timeoutLayout->addStretch();
    layout->addLayout(timeoutLayout);
    
    // Compilers path
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel("Compilers Path:"));
    m_compilersPathEdit = new QLineEdit();
    pathLayout->addWidget(m_compilersPathEdit);
    m_browsePathButton = new QPushButton("Browse...");
    pathLayout->addWidget(m_browsePathButton);
    layout->addLayout(pathLayout);
    
    // Reload button
    m_reloadCompilersButton = new QPushButton("Reload Compilers");
    layout->addWidget(m_reloadCompilersButton);
    
    // Checkboxes
    m_autoDetectCheckBox = new QCheckBox("Auto-detect language from file extension");
    m_autoDetectCheckBox->setChecked(true);
    layout->addWidget(m_autoDetectCheckBox);
    
    m_showOutputCheckBox = new QCheckBox("Show verbose compiler output");
    m_showOutputCheckBox->setChecked(true);
    layout->addWidget(m_showOutputCheckBox);
    
    layout->addStretch();
    m_tabWidget->addTab(m_settingsTab, "Settings");
}

void CompilerPanel::setupOutputTab()
{
    m_outputTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_outputTab);
    
    m_outputTextEdit = new QTextEdit();
    m_outputTextEdit->setReadOnly(true);
    m_outputTextEdit->setFont(QFont("Consolas", 9));
    layout->addWidget(m_outputTextEdit);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_clearOutputButton = new QPushButton("Clear Output");
    m_copyOutputButton = new QPushButton("Copy Output");
    buttonLayout->addWidget(m_clearOutputButton);
    buttonLayout->addWidget(m_copyOutputButton);
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout);
    
    m_tabWidget->addTab(m_outputTab, "Output");
}

void CompilerPanel::setupStatsTab()
{
    m_statsTab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_statsTab);
    
    m_statsListWidget = new QListWidget();
    layout->addWidget(m_statsListWidget);
    
    m_resetStatsButton = new QPushButton("Reset Statistics");
    layout->addWidget(m_resetStatsButton);
    
    m_tabWidget->addTab(m_statsTab, "Statistics");
}

void CompilerPanel::setupConnections()
{
    // Compiler manager signals
    connect(m_compilerManager, &PowerShellCompilerManager::compilationStarted,
            this, &CompilerPanel::onCompilationStarted);
    connect(m_compilerManager, &PowerShellCompilerManager::compilationFinished,
            this, &CompilerPanel::onCompilationFinished);
    connect(m_compilerManager, &PowerShellCompilerManager::compilationError,
            this, &CompilerPanel::onCompilationError);
    connect(m_compilerManager, &PowerShellCompilerManager::compilerLoaded,
            this, &CompilerPanel::onCompilerLoaded);
    
    // UI signals
    connect(m_languageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CompilerPanel::onLanguageChanged);
    connect(m_compileButton, &QPushButton::clicked,
            this, &CompilerPanel::onCompileClicked);
    connect(m_compileFileButton, &QPushButton::clicked,
            this, &CompilerPanel::onCompileFileClicked);
    connect(m_browsePathButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, "Select Compilers Directory");
        if (!path.isEmpty()) {
            m_compilersPathEdit->setText(path);
            m_compilerManager->loadCompilers(path);
            populateLanguageComboBox();
        }
    });
    connect(m_reloadCompilersButton, &QPushButton::clicked, this, [this]() {
        QString path = m_compilersPathEdit->text();
        if (!path.isEmpty() && QDir(path).exists()) {
            m_compilerManager->loadCompilers(path);
            populateLanguageComboBox();
        }
    });
    connect(m_clearOutputButton, &QPushButton::clicked, this, [this]() {
        m_outputTextEdit->clear();
    });
    connect(m_copyOutputButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_outputTextEdit->toPlainText());
    });
    connect(m_resetStatsButton, &QPushButton::clicked, this, [this]() {
        m_compilerManager->resetStats();
        updateStats();
    });
    
    // Settings changes
    connect(m_timeoutSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CompilerPanel::onCompilerSettingsChanged);
    connect(m_autoDetectCheckBox, &QCheckBox::stateChanged,
            this, &CompilerPanel::onCompilerSettingsChanged);
    connect(m_showOutputCheckBox, &QCheckBox::stateChanged,
            this, &CompilerPanel::onCompilerSettingsChanged);
}

void CompilerPanel::populateLanguageComboBox()
{
    m_languageComboBox->clear();
    
    QList<CompilerInfo> compilers = m_compilerManager->getAvailableCompilers();
    for (const CompilerInfo &compiler : compilers) {
        m_languageComboBox->addItem(compiler.name, compiler.language);
    }
    
    if (compilers.isEmpty()) {
        m_languageComboBox->addItem("No compilers available", "");
    }
}

void CompilerPanel::updateCompilerInfo()
{
    QString language = getSelectedLanguage();
    if (language.isEmpty()) {
        m_compilerInfoLabel->setText("Select a language to see compiler information");
        return;
    }
    
    CompilerInfo info = m_compilerManager->getCompilerInfo(language);
    if (info.language.isEmpty()) {
        m_compilerInfoLabel->setText("Compiler information not available");
        return;
    }
    
    QString infoText = QString("Language: %1\nCompiler: %2\nVersion: %3\nDescription: %4\nExtensions: %5")
                          .arg(info.language)
                          .arg(info.name)
                          .arg(info.version)
                          .arg(info.description)
                          .arg(info.extensions.join(", "));
    
    m_compilerInfoLabel->setText(infoText);
}

void CompilerPanel::updateStats()
{
    m_statsListWidget->clear();
    
    QMap<QString, int> stats = m_compilerManager->getCompilationStats();
    
    m_statsListWidget->addItem(QString("Total Compilations: %1").arg(stats.value("total", 0)));
    m_statsListWidget->addItem(QString("Successful: %1").arg(stats.value("successful", 0)));
    m_statsListWidget->addItem(QString("Failed: %1").arg(stats.value("failed", 0)));
    
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        if (it.key() != "total" && it.key() != "successful" && it.key() != "failed") {
            m_statsListWidget->addItem(QString("%1: %2").arg(it.key()).arg(it.value()));
        }
    }
}

void CompilerPanel::logOutput(const QString &message, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMessage = QString("[%1] %2").arg(timestamp).arg(message);
    
    if (isError) {
        m_outputTextEdit->setTextColor(Qt::red);
    } else {
        m_outputTextEdit->setTextColor(Qt::black);
    }
    
    m_outputTextEdit->append(formattedMessage);
    
    // Auto-scroll to bottom
    QTextCursor cursor = m_outputTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_outputTextEdit->setTextCursor(cursor);
}

void CompilerPanel::onCompilationStarted(const QString &language, const QString &sourceFile)
{
    Q_UNUSED(sourceFile);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate progress
    m_statusLabel->setText(QString("Compiling %1...").arg(language));
    logOutput(QString("Started compilation for %1").arg(language));
}

void CompilerPanel::onCompilerLoaded(const QString &language, const CompilerInfo &info)
{
    Q_UNUSED(info);
    logOutput(QString("Loaded compiler for %1").arg(language));
    populateLanguageComboBox();
}

CompilerPanel* getCompilerPanel()
{
    if (!g_compilerPanel) {
        g_compilerPanel = std::make_unique<CompilerPanel>();
    }
    return g_compilerPanel.get();
}