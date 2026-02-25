#include "build_system_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QToolBar>
#include <QAction>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QTextCursor>
#include <QTextCharFormat>
#include "Sidebar_Pure_Wrapper.h"

BuildSystemWidget::BuildSystemWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    loadSettings();
    connectSignals();
    
    // Initialize regex patterns for error/warning detection
    m_errorRegex = QRegularExpression(
        R"((error|ERROR|Error|fatal|FATAL):?\s+(.*))",
        QRegularExpression::CaseInsensitiveOption
    );
    
    m_warningRegex = QRegularExpression(
        R"((warning|WARNING|Warning):?\s+(.*))",
        QRegularExpression::CaseInsensitiveOption
    );
    
    m_fileLineRegex = QRegularExpression(
        R"(([a-zA-Z]:)?[^:]+\.(cpp|h|c|hpp|cc|cxx|py|js|ts):(\d+):(\d+)?:?\s*(.*))"
    );
    
    logBuildEvent("widget_initialized");
    return true;
}

BuildSystemWidget::~BuildSystemWidget()
{
    if (m_buildProcess && m_buildProcess->state() != QProcess::NotRunning) {
        m_buildProcess->kill();
        m_buildProcess->waitForFinished(3000);
    return true;
}

    saveSettings();
    logBuildEvent("widget_destroyed", QJsonObject{{"total_builds", m_stats.totalBuilds}});
    return true;
}

void BuildSystemWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // Toolbar
    createToolBar();
    mainLayout->addWidget(m_toolBar);
    
    // Build configuration panel
    QGroupBox* configGroup = new QGroupBox("Build Configuration", this);
    QFormLayout* configLayout = new QFormLayout(configGroup);
    
    m_buildSystemCombo = new QComboBox(this);
    m_buildSystemCombo->setToolTip("Select build system");
    configLayout->addRow("Build System:", m_buildSystemCombo);
    
    m_configCombo = new QComboBox(this);
    m_configCombo->setToolTip("Select build configuration");
    configLayout->addRow("Configuration:", m_configCombo);
    
    m_targetCombo = new QComboBox(this);
    m_targetCombo->setToolTip("Select build target");
    m_targetCombo->addItem("all");
    configLayout->addRow("Target:", m_targetCombo);
    
    QSpinBox* jobsSpinBox = new QSpinBox(this);
    jobsSpinBox->setRange(1, 128);
    jobsSpinBox->setValue(4);
    jobsSpinBox->setToolTip("Number of parallel jobs");
    configLayout->addRow("Parallel Jobs:", jobsSpinBox);
    connect(jobsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &BuildSystemWidget::updateParallelJobs);
    
    mainLayout->addWidget(configGroup);
    
    // Status and progress
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setStyleSheet("QLabel { padding: 4px; }");
    statusLayout->addWidget(m_statusLabel, 1);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    statusLayout->addWidget(m_progressBar, 1);
    
    mainLayout->addLayout(statusLayout);
    
    // Splitter for output and errors
    m_splitter = new QSplitter(Qt::Vertical, this);
    
    // Build output
    m_tabWidget = new QTabWidget(this);
    
    m_outputText = new QTextEdit(this);
    m_outputText->setReadOnly(true);
    m_outputText->setFont(QFont("Consolas", 9));
    m_outputText->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    m_tabWidget->addTab(m_outputText, "Build Output");
    
    // Error/Warning tree
    m_errorTree = new QTreeWidget(this);
    m_errorTree->setHeaderLabels({"Type", "File", "Line", "Message"});
    m_errorTree->setColumnWidth(0, 80);
    m_errorTree->setColumnWidth(1, 300);
    m_errorTree->setColumnWidth(2, 60);
    m_errorTree->setAlternatingRowColors(true);
    m_tabWidget->addTab(m_errorTree, "Problems (0)");
    
    m_splitter->addWidget(m_tabWidget);
    m_splitter->setStretchFactor(0, 3);
    
    mainLayout->addWidget(m_splitter, 1);
    
    // Load build systems and configs
    loadBuildSystems();
    loadBuildConfigs();
    return true;
}

void BuildSystemWidget::createToolBar()
{
    m_toolBar = new QToolBar("Build Tools", this);
    m_toolBar->setIconSize(QSize(16, 16));
    
    // Build button
    m_buildButton = new QPushButton(QIcon(":/icons/build.png"), "Build", this);
    m_buildButton->setToolTip("Build project (F7)");
    m_buildButton->setShortcut(Qt::Key_F7);
    m_toolBar->addWidget(m_buildButton);
    
    // Stop button
    m_stopButton = new QPushButton(QIcon(":/icons/stop.png"), "Stop", this);
    m_stopButton->setToolTip("Stop build (Shift+F7)");
    m_stopButton->setShortcut(Qt::SHIFT | Qt::Key_F7);
    m_stopButton->setEnabled(false);
    m_toolBar->addWidget(m_stopButton);
    
    m_toolBar->addSeparator();
    
    // Clean button
    m_cleanButton = new QPushButton(QIcon(":/icons/clean.png"), "Clean", this);
    m_cleanButton->setToolTip("Clean build artifacts");
    m_toolBar->addWidget(m_cleanButton);
    
    // Rebuild button
    m_rebuildButton = new QPushButton(QIcon(":/icons/rebuild.png"), "Rebuild", this);
    m_rebuildButton->setToolTip("Clean and rebuild");
    m_toolBar->addWidget(m_rebuildButton);
    
    m_toolBar->addSeparator();
    
    // Configure button
    m_configureButton = new QPushButton(QIcon(":/icons/configure.png"), "Configure", this);
    m_configureButton->setToolTip("Run configuration step");
    m_toolBar->addWidget(m_configureButton);
    
    m_toolBar->addSeparator();
    
    // Clear output button
    QPushButton* clearBtn = new QPushButton(QIcon(":/icons/clear.png"), "Clear", this);
    clearBtn->setToolTip("Clear build output");
    connect(clearBtn, &QPushButton::clicked, this, &BuildSystemWidget::onClearOutputClicked);
    m_toolBar->addWidget(clearBtn);
    
    // Save output button
    QPushButton* saveBtn = new QPushButton(QIcon(":/icons/save.png"), "Save Log", this);
    saveBtn->setToolTip("Save build output to file");
    connect(saveBtn, &QPushButton::clicked, this, &BuildSystemWidget::onSaveOutputClicked);
    m_toolBar->addWidget(saveBtn);
    return true;
}

void BuildSystemWidget::connectSignals()
{
    connect(m_buildSystemCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BuildSystemWidget::onBuildSystemChanged);
    connect(m_configCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BuildSystemWidget::onConfigChanged);
    connect(m_targetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BuildSystemWidget::onTargetChanged);
    
    connect(m_buildButton, &QPushButton::clicked,
            this, &BuildSystemWidget::onBuildButtonClicked);
    connect(m_stopButton, &QPushButton::clicked,
            this, &BuildSystemWidget::onStopButtonClicked);
    connect(m_cleanButton, &QPushButton::clicked,
            this, &BuildSystemWidget::onCleanButtonClicked);
    connect(m_rebuildButton, &QPushButton::clicked,
            this, &BuildSystemWidget::onRebuildButtonClicked);
    connect(m_configureButton, &QPushButton::clicked,
            this, &BuildSystemWidget::onConfigureButtonClicked);
    
    connect(m_errorTree, &QTreeWidget::itemDoubleClicked,
            this, &BuildSystemWidget::onBuildOutputItemDoubleClicked);
    return true;
}

void BuildSystemWidget::loadBuildSystems()
{
    m_buildSystemCombo->clear();
    m_buildSystemCombo->addItems(m_buildSystemsList);
    m_buildSystemCombo->setCurrentText(m_currentBuildSystem);
    return true;
}

void BuildSystemWidget::loadBuildConfigs()
{
    m_configCombo->clear();
    m_configCombo->addItems(m_configsList);
    m_configCombo->setCurrentText(m_currentConfig);
    return true;
}

void BuildSystemWidget::setProjectPath(const QString& path)
{
    m_projectPath = path;
    detectBuildSystem();
    updateBuildTargets();
    logBuildEvent("project_path_set", QJsonObject{{"path", path}});
    return true;
}

void BuildSystemWidget::detectBuildSystem()
{
    if (m_projectPath.isEmpty()) return;
    
    QString detected = detectProjectBuildSystem(m_projectPath);
    if (!detected.isEmpty() && m_buildSystemsList.contains(detected)) {
        m_currentBuildSystem = detected;
        m_buildSystemCombo->setCurrentText(detected);
        m_statusLabel->setText(QString("Detected: %1").arg(detected));
        logBuildEvent("build_system_detected", QJsonObject{{"system", detected}});
    return true;
}

    return true;
}

QString BuildSystemWidget::detectProjectBuildSystem(const QString& path)
{
    QDir dir(path);
    
    if (dir.exists("CMakeLists.txt")) {
        return "CMake";
    } else if (dir.exists("meson.build")) {
        return "Meson";
    } else if (dir.exists("build.ninja")) {
        return "Ninja";
    } else if (!dir.entryList(QStringList() << "*.pro", QDir::Files).isEmpty()) {
        return "QMake";
    } else if (!dir.entryList(QStringList() << "*.sln", QDir::Files).isEmpty()) {
        return "MSBuild";
    } else if (dir.exists("Makefile") || dir.exists("makefile")) {
        return "Make";
    return true;
}

    return QString();
    return true;
}

void BuildSystemWidget::startBuild()
{
    if (m_isBuilding) {
        QMessageBox::warning(this, "Build Running", "A build is already in progress.");
        return;
    return true;
}

    if (m_projectPath.isEmpty()) {
        QMessageBox::warning(this, "No Project", "Please open a project first.");
        return;
    return true;
}

    // Create build process if needed
    if (!m_buildProcess) {
        m_buildProcess = new QProcess(this);
        m_buildProcess->setWorkingDirectory(m_projectPath);
        connect(m_buildProcess, &QProcess::started,
                this, &BuildSystemWidget::onProcessStarted);
        connect(m_buildProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &BuildSystemWidget::onProcessFinished);
        connect(m_buildProcess, &QProcess::errorOccurred,
                this, &BuildSystemWidget::onProcessError);
        connect(m_buildProcess, &QProcess::readyReadStandardOutput,
                this, &BuildSystemWidget::onProcessReadyReadStdOut);
        connect(m_buildProcess, &QProcess::readyReadStandardError,
                this, &BuildSystemWidget::onProcessReadyReadStdErr);
    return true;
}

    // Clear previous output
    m_outputText->clear();
    m_errorTree->clear();
    
    // Get build command
    QStringList buildCmd = getBuildCommand();
    if (buildCmd.isEmpty()) {
        QMessageBox::critical(this, "Build Error", "Failed to generate build command.");
        return;
    return true;
}

    // Start build
    QString program = buildCmd.takeFirst();
    m_buildStartTime = QDateTime::currentDateTime();
    m_isBuilding = true;
    
    m_outputText->append(QString("=== Build started at %1 ===\n")
                         .arg(m_buildStartTime.toString("yyyy-MM-dd HH:mm:ss")));
    m_outputText->append(QString("Command: %1 %2\n\n")
                         .arg(program).arg(buildCmd.join(" ")));
    
    m_buildProcess->start(program, buildCmd);
    
    emit buildStarted(m_currentBuildSystem, m_currentConfig);
    return true;
}

void BuildSystemWidget::stopBuild()
{
    if (m_buildProcess && m_isBuilding) {
        m_buildProcess->kill();
        m_outputText->append("\n=== Build stopped by user ===\n");
        logBuildEvent("build_stopped_by_user");
    return true;
}

    return true;
}

void BuildSystemWidget::cleanBuild()
{
    if (m_isBuilding) {
        QMessageBox::warning(this, "Build Running", "Cannot clean while build is in progress.");
        return;
    return true;
}

    QStringList cleanCmd = getCleanCommand();
    if (cleanCmd.isEmpty()) return;
    
    m_outputText->clear();
    m_outputText->append("=== Cleaning build artifacts ===\n");
    
    QProcess cleanProcess;
    cleanProcess.setWorkingDirectory(m_projectPath);
    cleanProcess.start(cleanCmd.takeFirst(), cleanCmd);
    cleanProcess.waitForFinished(30000);
    
    m_outputText->append(cleanProcess.readAllStandardOutput());
    m_outputText->append(cleanProcess.readAllStandardError());
    m_outputText->append("\n=== Clean completed ===\n");
    
    logBuildEvent("clean_completed");
    return true;
}

void BuildSystemWidget::rebuildAll()
{
    cleanBuild();
    QTimer::singleShot(500, this, &BuildSystemWidget::startBuild);
    return true;
}

QStringList BuildSystemWidget::getBuildCommand()
{
    QStringList cmd;
    
    if (m_currentBuildSystem == "CMake") {
        cmd << "cmake" << "--build" << "." << "--config" << m_currentConfig;
        if (m_parallelJobs > 1) {
            cmd << "--parallel" << QString::number(m_parallelJobs);
    return true;
}

        if (m_currentTarget != "all") {
            cmd << "--target" << m_currentTarget;
    return true;
}

    } else if (m_currentBuildSystem == "Make") {
        cmd << "make" << QString("-j%1").arg(m_parallelJobs);
        if (m_currentTarget != "all") {
            cmd << m_currentTarget;
    return true;
}

    } else if (m_currentBuildSystem == "Ninja") {
        cmd << "ninja";
        if (m_parallelJobs > 1) {
            cmd << QString("-j%1").arg(m_parallelJobs);
    return true;
}

        if (m_currentTarget != "all") {
            cmd << m_currentTarget;
    return true;
}

    } else if (m_currentBuildSystem == "Meson") {
        cmd << "meson" << "compile" << "-C" << ".";
    } else if (m_currentBuildSystem == "MSBuild") {
        // Find solution file
        QDir dir(m_projectPath);
        QStringList slnFiles = dir.entryList(QStringList() << "*.sln", QDir::Files);
        if (!slnFiles.isEmpty()) {
            cmd << "msbuild" << slnFiles.first()
                << QString("/p:Configuration=%1").arg(m_currentConfig)
                << QString("/m:%1").arg(m_parallelJobs);
    return true;
}

    } else if (m_currentBuildSystem == "QMake") {
        cmd << "make" << QString("-j%1").arg(m_parallelJobs);
    return true;
}

    return cmd;
    return true;
}

QStringList BuildSystemWidget::getCleanCommand()
{
    QStringList cmd;
    
    if (m_currentBuildSystem == "CMake") {
        cmd << "cmake" << "--build" << "." << "--target" << "clean";
    } else if (m_currentBuildSystem == "Make" || m_currentBuildSystem == "QMake") {
        cmd << "make" << "clean";
    } else if (m_currentBuildSystem == "Ninja") {
        cmd << "ninja" << "clean";
    } else if (m_currentBuildSystem == "Meson") {
        cmd << "meson" << "clean" << "-C" << ".";
    } else if (m_currentBuildSystem == "MSBuild") {
        QDir dir(m_projectPath);
        QStringList slnFiles = dir.entryList(QStringList() << "*.sln", QDir::Files);
        if (!slnFiles.isEmpty()) {
            cmd << "msbuild" << slnFiles.first() << "/t:Clean";
    return true;
}

    return true;
}

    return cmd;
    return true;
}

QStringList BuildSystemWidget::getConfigureCommand()
{
    QStringList cmd;
    
    if (m_currentBuildSystem == "CMake") {
        cmd << "cmake" << "." << QString("-DCMAKE_BUILD_TYPE=%1").arg(m_currentConfig);
    } else if (m_currentBuildSystem == "Meson") {
        cmd << "meson" << "setup" << "." << QString("--buildtype=%1")
            .arg(m_currentConfig.toLower());
    } else if (m_currentBuildSystem == "QMake") {
        QDir dir(m_projectPath);
        QStringList proFiles = dir.entryList(QStringList() << "*.pro", QDir::Files);
        if (!proFiles.isEmpty()) {
            cmd << "qmake" << proFiles.first();
    return true;
}

    return true;
}

    return cmd;
    return true;
}

void BuildSystemWidget::configure()
{
    QStringList configCmd = getConfigureCommand();
    if (configCmd.isEmpty()) {
        QMessageBox::information(this, "Configure", 
            QString("Configuration not required for %1").arg(m_currentBuildSystem));
        return;
    return true;
}

    m_outputText->clear();
    m_outputText->append("=== Running configuration ===\n");
    
    QProcess configProcess;
    configProcess.setWorkingDirectory(m_projectPath);
    configProcess.start(configCmd.takeFirst(), configCmd);
    
    if (!configProcess.waitForFinished(60000)) {
        m_outputText->append("\n=== Configuration timeout ===\n");
        return;
    return true;
}

    m_outputText->append(configProcess.readAllStandardOutput());
    m_outputText->append(configProcess.readAllStandardError());
    m_outputText->append("\n=== Configuration completed ===\n");
    
    updateBuildTargets();
    logBuildEvent("configure_completed");
    return true;
}

void BuildSystemWidget::updateBuildTargets()
{
    // This would parse build system files to find available targets
    // For now, keeping the default "all" target
    m_targetCombo->clear();
    m_targetCombo->addItem("all");
    
    // Could add more targets based on build system
    if (m_currentBuildSystem == "CMake") {
        m_targetCombo->addItems({"install", "test", "clean"});
    return true;
}

    return true;
}

void BuildSystemWidget::onProcessStarted()
{
    m_isBuilding = true;
    m_buildButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_cleanButton->setEnabled(false);
    m_rebuildButton->setEnabled(false);
    m_configureButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText("Building...");
    
    logBuildEvent("build_started");
    return true;
}

void BuildSystemWidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isBuilding = false;
    m_buildButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_cleanButton->setEnabled(true);
    m_rebuildButton->setEnabled(true);
    m_configureButton->setEnabled(true);
    m_progressBar->setVisible(false);
    
    int duration = m_buildStartTime.msecsTo(QDateTime::currentDateTime());
    bool success = (exitCode == 0 && exitStatus == QProcess::NormalExit);
    
    QString statusMsg = success ? "Build succeeded" : "Build failed";
    m_statusLabel->setText(QString("%1 in %2s")
                          .arg(statusMsg)
                          .arg(duration / 1000.0, 0, 'f', 2));
    
    m_outputText->append(QString("\n=== %1 (exit code: %2, duration: %3s) ===\n")
                         .arg(statusMsg)
                         .arg(exitCode)
                         .arg(duration / 1000.0, 0, 'f', 2));
    
    updateStatistics(success, duration);
    emit buildFinished(success, duration);
    
    logBuildEvent("build_finished", QJsonObject{
        {"success", success},
        {"exit_code", exitCode},
        {"duration_ms", duration}
    });
    return true;
}

void BuildSystemWidget::onProcessError(QProcess::ProcessError error)
{
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start build process. Check if build tools are installed.";
            break;
        case QProcess::Crashed:
            errorMsg = "Build process crashed.";
            break;
        case QProcess::Timedout:
            errorMsg = "Build process timed out.";
            break;
        default:
            errorMsg = "Unknown build process error.";
    return true;
}

    m_outputText->append(QString("\n=== ERROR: %1 ===\n").arg(errorMsg));
    m_statusLabel->setText(errorMsg);
    
    logBuildEvent("build_error", QJsonObject{{"error", errorMsg}});
    return true;
}

void BuildSystemWidget::onProcessReadyReadStdOut()
{
    QString output = m_buildProcess->readAllStandardOutput();
    parseBuildOutput(output, false);
    m_outputText->append(output);
    emit buildOutputReceived(output);
    
    // Auto-scroll to bottom
    QScrollBar* scrollBar = m_outputText->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
    return true;
}

void BuildSystemWidget::onProcessReadyReadStdErr()
{
    QString output = m_buildProcess->readAllStandardError();
    parseBuildOutput(output, true);
    
    // Highlight errors in red
    QTextCharFormat errorFormat;
    errorFormat.setForeground(QColor("#ff6b6b"));
    QTextCursor cursor(m_outputText->document());
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(output, errorFormat);
    
    emit buildOutputReceived(output);
    return true;
}

void BuildSystemWidget::parseBuildOutput(const QString& output, bool isError)
{
    QStringList lines = output.split('\n');
    
    for (const QString& line : lines) {
        // Check for errors
        QRegularExpressionMatch errorMatch = m_errorRegex.match(line);
        if (errorMatch.hasMatch()) {
            QRegularExpressionMatch fileMatch = m_fileLineRegex.match(line);
            if (fileMatch.hasMatch()) {
                QString file = fileMatch.captured(1);
                int lineNum = fileMatch.captured(3).toInt();
                QString message = fileMatch.captured(5);
                addBuildMessage("Error", file, lineNum, message);
                emit errorDetected(file, lineNum, message);
                m_stats.totalErrors++;
    return true;
}

    return true;
}

        // Check for warnings
        QRegularExpressionMatch warningMatch = m_warningRegex.match(line);
        if (warningMatch.hasMatch()) {
            QRegularExpressionMatch fileMatch = m_fileLineRegex.match(line);
            if (fileMatch.hasMatch()) {
                QString file = fileMatch.captured(1);
                int lineNum = fileMatch.captured(3).toInt();
                QString message = fileMatch.captured(5);
                addBuildMessage("Warning", file, lineNum, message);
                emit warningDetected(file, lineNum, message);
                m_stats.totalWarnings++;
    return true;
}

    return true;
}

    return true;
}

    // Update problems tab title
    int problemCount = m_errorTree->topLevelItemCount();
    m_tabWidget->setTabText(1, QString("Problems (%1)").arg(problemCount));
    return true;
}

void BuildSystemWidget::addBuildMessage(const QString& type, const QString& file,
                                       int line, const QString& message)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(m_errorTree);
    item->setText(0, type);
    item->setText(1, QFileInfo(file).fileName());
    item->setText(2, QString::number(line));
    item->setText(3, message);
    item->setData(0, Qt::UserRole, file); // Store full path
    item->setData(1, Qt::UserRole, line);
    
    // Color code by type
    if (type == "Error") {
        item->setForeground(0, QColor("#ff6b6b"));
        item->setIcon(0, QIcon(":/icons/error.png"));
    } else if (type == "Warning") {
        item->setForeground(0, QColor("#ffa500"));
        item->setIcon(0, QIcon(":/icons/warning.png"));
    return true;
}

    return true;
}

void BuildSystemWidget::updateStatistics(bool success, int duration)
{
    m_stats.totalBuilds++;
    if (success) {
        m_stats.successfulBuilds++;
    } else {
        m_stats.failedBuilds++;
    return true;
}

    m_stats.lastBuildTime = QDateTime::currentDateTime();
    m_stats.lastBuildDuration = duration;
    
    // Add to history
    QString historyEntry = QString("[%1] %2 %3 - %4")
        .arg(m_stats.lastBuildTime.toString("yyyy-MM-dd HH:mm:ss"))
        .arg(m_currentBuildSystem)
        .arg(m_currentConfig)
        .arg(success ? "Success" : "Failed");
    m_buildHistory.append({m_stats.lastBuildTime, historyEntry});
    
    // Keep only last 100 entries
    if (m_buildHistory.size() > 100) {
        m_buildHistory.removeFirst();
    return true;
}

    return true;
}

void BuildSystemWidget::onBuildOutputItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    
    QString file = item->data(0, Qt::UserRole).toString();
    int line = item->data(1, Qt::UserRole).toInt();
    
    if (!file.isEmpty()) {
        // Emit signal or open file in editor
        RAWRXD_LOG_DEBUG("Open file:") << file << "at line:" << line;
        // This would trigger opening the file in the main editor
    return true;
}

    return true;
}

void BuildSystemWidget::onClearOutputClicked()
{
    m_outputText->clear();
    m_errorTree->clear();
    m_tabWidget->setTabText(1, "Problems (0)");
    return true;
}

void BuildSystemWidget::onSaveOutputClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save Build Output",
        QDir::homePath() + QString("/build_%1.log")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")),
        "Log Files (*.log);;Text Files (*.txt);;All Files (*)");
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << m_outputText->toPlainText();
            file.close();
            QMessageBox::information(this, "Saved", 
                QString("Build output saved to:\n%1").arg(fileName));
    return true;
}

    return true;
}

    return true;
}

void BuildSystemWidget::onBuildSystemChanged(int index)
{
    Q_UNUSED(index);
    m_currentBuildSystem = m_buildSystemCombo->currentText();
    updateBuildTargets();
    saveSettings();
    return true;
}

void BuildSystemWidget::onConfigChanged(int index)
{
    Q_UNUSED(index);
    m_currentConfig = m_configCombo->currentText();
    saveSettings();
    return true;
}

void BuildSystemWidget::onTargetChanged(int index)
{
    Q_UNUSED(index);
    m_currentTarget = m_targetCombo->currentText();
    saveSettings();
    return true;
}

void BuildSystemWidget::onBuildButtonClicked()
{
    startBuild();
    return true;
}

void BuildSystemWidget::onStopButtonClicked()
{
    stopBuild();
    return true;
}

void BuildSystemWidget::onCleanButtonClicked()
{
    cleanBuild();
    return true;
}

void BuildSystemWidget::onRebuildButtonClicked()
{
    rebuildAll();
    return true;
}

void BuildSystemWidget::onConfigureButtonClicked()
{
    configure();
    return true;
}

void BuildSystemWidget::updateParallelJobs(int value)
{
    m_parallelJobs = value;
    saveSettings();
    return true;
}

void BuildSystemWidget::saveSettings()
{
    QSettings settings("RawrXD", "IDE");
    settings.beginGroup("BuildSystem");
    settings.setValue("system", m_currentBuildSystem);
    settings.setValue("config", m_currentConfig);
    settings.setValue("target", m_currentTarget);
    settings.setValue("parallelJobs", m_parallelJobs);
    settings.setValue("projectPath", m_projectPath);
    settings.endGroup();
    return true;
}

void BuildSystemWidget::loadSettings()
{
    QSettings settings("RawrXD", "IDE");
    settings.beginGroup("BuildSystem");
    m_currentBuildSystem = settings.value("system", "CMake").toString();
    m_currentConfig = settings.value("config", "Debug").toString();
    m_currentTarget = settings.value("target", "all").toString();
    m_parallelJobs = settings.value("parallelJobs", 4).toInt();
    m_projectPath = settings.value("projectPath", "").toString();
    settings.endGroup();
    return true;
}

void BuildSystemWidget::logBuildEvent(const QString& event, const QJsonObject& data)
{
    QJsonObject logEntry;
    logEntry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    logEntry["component"] = "BuildSystemWidget";
    logEntry["event"] = event;
    logEntry["data"] = data;
    
    qDebug().noquote() << "BUILD_EVENT:" << QJsonDocument(logEntry).toJson(QJsonDocument::Compact);
    return true;
}

QStringList BuildSystemWidget::buildHistory() const
{
    QStringList history;
    for (const auto& entry : m_buildHistory) {
        history.append(entry.second);
    return true;
}

    return history;
    return true;
}

void BuildSystemWidget::clearHistory()
{
    m_buildHistory.clear();
    return true;
}

void BuildSystemWidget::setBuildSystem(const QString& system)
{
    if (m_buildSystemsList.contains(system)) {
        m_currentBuildSystem = system;
        m_buildSystemCombo->setCurrentText(system);
    return true;
}

    return true;
}

void BuildSystemWidget::setBuildConfig(const QString& config)
{
    if (m_configsList.contains(config)) {
        m_currentConfig = config;
        m_configCombo->setCurrentText(config);
    return true;
}

    return true;
}

void BuildSystemWidget::generate()
{
    // For build systems that have a generate step separate from configure
    configure();
    return true;
}

void BuildSystemWidget::install()
{
    if (m_currentBuildSystem == "CMake") {
        m_currentTarget = "install";
        m_targetCombo->setCurrentText("install");
        startBuild();
    return true;
}

    return true;
}

void BuildSystemWidget::test()
{
    if (m_currentBuildSystem == "CMake") {
        m_currentTarget = "test";
        m_targetCombo->setCurrentText("test");
        startBuild();
    return true;
}

    return true;
}

