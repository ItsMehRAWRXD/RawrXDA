#include "TestRunnerPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>

namespace RawrXD {

TestRunnerPanel::TestRunnerPanel(QWidget* parent)
    : QDockWidget("Test Runner", parent),
      m_discovery(new TestDiscovery(this)),
      m_executor(new TestExecutor(this)) {
    
    setObjectName("TestRunnerPanel");
    setupUi();
    
    // Connect discovery signals
    connect(m_discovery, &TestDiscovery::discoveryStarted,
            this, &TestRunnerPanel::onDiscoveryStarted);
    connect(m_discovery, &TestDiscovery::discoveryFinished,
            this, &TestRunnerPanel::onDiscoveryFinished);
    connect(m_discovery, &TestDiscovery::error,
            this, &TestRunnerPanel::onDiscoveryError);
    
    // Connect executor signals
    connect(m_executor, &TestExecutor::executionStarted,
            this, &TestRunnerPanel::onExecutionStarted);
    connect(m_executor, &TestExecutor::testStarted,
            this, &TestRunnerPanel::onTestStarted);
    connect(m_executor, &TestExecutor::testFinished,
            this, &TestRunnerPanel::onTestFinished);
    connect(m_executor, &TestExecutor::executionFinished,
            this, &TestRunnerPanel::onExecutionFinished);
    connect(m_executor, &TestExecutor::progress,
            this, &TestRunnerPanel::onExecutionProgress);
    connect(m_executor, &TestExecutor::testOutput,
            this, &TestRunnerPanel::onTestOutput);
}

TestRunnerPanel::~TestRunnerPanel() {
}

void TestRunnerPanel::setupUi() {
    m_mainWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(m_mainWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    createToolbar();
    mainLayout->addWidget(m_toolbar);
    
    m_splitter = new QSplitter(Qt::Vertical, m_mainWidget);
    
    createTreeView();
    m_splitter->addWidget(m_testTree);
    
    createOutputView();
    m_splitter->addWidget(m_bottomTabs);
    
    m_splitter->setStretchFactor(0, 2);
    m_splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_splitter);
    
    createStatusBar();
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statsLabel);
    
    setWidget(m_mainWidget);
}

void TestRunnerPanel::createToolbar() {
    m_toolbar = new QToolBar(m_mainWidget);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    
    // Run buttons
    m_runBtn = new QPushButton(QIcon::fromTheme("media-playback-start"), "Run All", m_toolbar);
    connect(m_runBtn, &QPushButton::clicked, this, &TestRunnerPanel::onRunButtonClicked);
    m_toolbar->addWidget(m_runBtn);
    
    m_runSelectedBtn = new QPushButton(QIcon::fromTheme("media-playback-start"), "Run Selected", m_toolbar);
    connect(m_runSelectedBtn, &QPushButton::clicked, this, &TestRunnerPanel::runSelectedTests);
    m_toolbar->addWidget(m_runSelectedBtn);
    
    m_runFailedBtn = new QPushButton(QIcon::fromTheme("media-playback-start"), "Run Failed", m_toolbar);
    connect(m_runFailedBtn, &QPushButton::clicked, this, &TestRunnerPanel::runFailedTests);
    m_toolbar->addWidget(m_runFailedBtn);
    
    m_toolbar->addSeparator();
    
    // Debug button
    m_debugBtn = new QPushButton(QIcon::fromTheme("debug-run"), "Debug", m_toolbar);
    connect(m_debugBtn, &QPushButton::clicked, this, &TestRunnerPanel::onDebugButtonClicked);
    m_toolbar->addWidget(m_debugBtn);
    
    // Stop button
    m_stopBtn = new QPushButton(QIcon::fromTheme("process-stop"), "Stop", m_toolbar);
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &TestRunnerPanel::onStopButtonClicked);
    m_toolbar->addWidget(m_stopBtn);
    
    m_toolbar->addSeparator();
    
    // Refresh button
    m_refreshBtn = new QPushButton(QIcon::fromTheme("view-refresh"), "Refresh", m_toolbar);
    connect(m_refreshBtn, &QPushButton::clicked, this, &TestRunnerPanel::onRefreshButtonClicked);
    m_toolbar->addWidget(m_refreshBtn);
    
    m_toolbar->addSeparator();
    
    // Filters
    m_filterEdit = new QLineEdit(m_toolbar);
    m_filterEdit->setPlaceholderText("Filter tests...");
    m_filterEdit->setMinimumWidth(150);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &TestRunnerPanel::onFilterTextChanged);
    m_toolbar->addWidget(m_filterEdit);
    
    m_frameworkFilter = new QComboBox(m_toolbar);
    m_frameworkFilter->addItems({"All Frameworks", "GoogleTest", "Catch2", "PyTest", "Jest", "Go", "Cargo", "CTest"});
    connect(m_frameworkFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TestRunnerPanel::onFrameworkFilterChanged);
    m_toolbar->addWidget(m_frameworkFilter);
    
    m_statusFilter = new QComboBox(m_toolbar);
    m_statusFilter->addItems({"All Tests", "Not Run", "Passed", "Failed", "Skipped"});
    connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TestRunnerPanel::onStatusFilterChanged);
    m_toolbar->addWidget(m_statusFilter);
}

void TestRunnerPanel::createTreeView() {
    m_testTree = new QTreeWidget(m_mainWidget);
    m_testTree->setHeaderLabels({"Test", "Status", "Duration", "Framework"});
    m_testTree->setColumnWidth(0, 300);
    m_testTree->setColumnWidth(1, 100);
    m_testTree->setColumnWidth(2, 100);
    m_testTree->setAlternatingRowColors(true);
    m_testTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(m_testTree, &QTreeWidget::itemClicked,
            this, &TestRunnerPanel::onTreeItemClicked);
    connect(m_testTree, &QTreeWidget::itemDoubleClicked,
            this, &TestRunnerPanel::onTreeItemDoubleClicked);
    connect(m_testTree, &QTreeWidget::customContextMenuRequested,
            this, &TestRunnerPanel::onTreeContextMenu);
}

void TestRunnerPanel::createOutputView() {
    m_bottomTabs = new QTabWidget(m_mainWidget);

    // Raw output tab (legacy)
    m_outputView = new QTextEdit(m_bottomTabs);
    m_outputView->setReadOnly(true);
    m_outputView->setFont(QFont("Consolas", 9));
    m_bottomTabs->addTab(m_outputView, "Output");

    // Rich details tab
    m_outputDetailsPanel = new TestOutputPanel(m_bottomTabs);
    connect(m_outputDetailsPanel, &TestOutputPanel::linkActivated, this, [this](const QUrl& url){
        // Future: route to editor to open source at line
        Q_UNUSED(url);
    });
    m_bottomTabs->addTab(m_outputDetailsPanel, "Details");

    // Coverage tab
    m_coverageTree = new QTreeWidget(m_bottomTabs);
    m_coverageTree->setHeaderLabels({"File", "Lines", "Covered", "%", "Funcs %", "Branches %"});
    m_coverageTree->setSortingEnabled(true);
    m_bottomTabs->addTab(m_coverageTree, "Coverage");

    // Coverage collector
    m_coverageCollector = new CoverageCollector(this);
    connect(m_coverageCollector, &CoverageCollector::coverageLoaded, this, [this](const CoverageReport& rep){
        m_coverageTree->clear();
        for (auto it = rep.files.constBegin(); it != rep.files.constEnd(); ++it) {
            const auto& fm = it.value();
            auto* item = new QTreeWidgetItem(m_coverageTree);
            item->setText(0, it.key());
            item->setText(1, QString::number(fm.linesTotal));
            item->setText(2, QString::number(fm.linesCovered));
            item->setText(3, QString::number(fm.linePercent(), 'f', 1));
            item->setText(4, QString::number(fm.funcPercent(), 'f', 1));
            item->setText(5, QString::number(fm.branchPercent(), 'f', 1));
        }
        m_coverageTree->sortItems(3, Qt::DescendingOrder);
    });
}

void TestRunnerPanel::createStatusBar() {
    m_statusLabel = new QLabel("Ready", m_mainWidget);
    m_progressBar = new QProgressBar(m_mainWidget);
    m_progressBar->setVisible(false);
    m_statsLabel = new QLabel("0 tests", m_mainWidget);
}

void TestRunnerPanel::setWorkspace(const QString& path) {
    if (m_workspacePath == path) return;

    m_workspacePath = path;
    emit workspaceChanged(path);

    if (m_coverageCollector) m_coverageCollector->setWorkspace(path);

    // Auto-discover tests
    if (!path.isEmpty()) {
        discoverTests();
        refreshCoverage();
    }
}

void TestRunnerPanel::discoverTests() {
    if (m_workspacePath.isEmpty()) {
        m_statusLabel->setText("No workspace set");
        return;
    }
    
    m_discovering = true;
    m_statusLabel->setText("Discovering tests...");
    m_testTree->clear();
    m_treeItemMap.clear();
    
    // Run discovery in background
    QMetaObject::invokeMethod(m_discovery, [this]() {
        DiscoveryResult result = m_discovery->discoverTests(m_workspacePath);
        QMetaObject::invokeMethod(this, [this, result]() {
            onDiscoveryFinished(result);
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}

void TestRunnerPanel::refreshTests() {
    clearResults();
    discoverTests();
}

void TestRunnerPanel::runAllTests() {
    if (m_testSuites.isEmpty()) {
        QMessageBox::information(this, "No Tests", "No tests discovered. Try refreshing.");
        return;
    }
    
    QList<TestCase> allTests;
    for (const TestSuite& suite : m_testSuites) {
        allTests.append(suite.cases);
    }
    
    TestExecutionOptions options;
    options.runInParallel = true;
    options.maxParallelJobs = 4;
    
    m_executor->executeTests(allTests, options);
}

void TestRunnerPanel::runSelectedTests() {
    QList<QTreeWidgetItem*> selected = m_testTree->selectedItems();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Selection", "Please select tests to run.");
        return;
    }
    
    QList<TestCase> tests;
    for (QTreeWidgetItem* item : selected) {
        QString testId = item->data(0, Qt::UserRole).toString();
        if (testId.isEmpty()) continue;
        
        // Find test case
        for (const TestSuite& suite : m_testSuites) {
            for (const TestCase& tc : suite.cases) {
                if (tc.id == testId) {
                    tests.append(tc);
                    break;
                }
            }
        }
    }
    
    if (tests.isEmpty()) return;
    
    TestExecutionOptions options;
    m_executor->executeTests(tests, options);
}

void TestRunnerPanel::runFailedTests() {
    QList<TestCase> failedTests;
    
    for (const auto& result : m_testResults) {
        if (result.isFailed() || result.testCase.status == TestStatus::Error) {
            failedTests.append(result.testCase);
        }
    }
    
    if (failedTests.isEmpty()) {
        QMessageBox::information(this, "No Failed Tests", "No failed tests to rerun.");
        return;
    }
    
    TestExecutionOptions options;
    m_executor->executeTests(failedTests, options);
}

void TestRunnerPanel::debugSelectedTest() {
    // Debug mode - would integrate with debugger panel
    QList<QTreeWidgetItem*> selected = m_testTree->selectedItems();
    if (selected.isEmpty() || selected.size() > 1) {
        QMessageBox::information(this, "Debug Test", "Please select exactly one test to debug.");
        return;
    }
    
    QString testId = selected.first()->data(0, Qt::UserRole).toString();
    
    // Find test case
    for (const TestSuite& suite : m_testSuites) {
        for (const TestCase& tc : suite.cases) {
            if (tc.id == testId) {
                TestExecutionOptions options;
                options.debugMode = true;
                m_executor->executeTest(tc, options);
                return;
            }
        }
    }
}

void TestRunnerPanel::stopExecution() {
    m_executor->cancel();
}

void TestRunnerPanel::clearResults() {
    m_testResults.clear();
    m_passedTests = 0;
    m_failedTests = 0;
    m_skippedTests = 0;
    m_totalDurationMs = 0.0;
    m_outputView->clear();
    if (m_outputDetailsPanel) m_outputDetailsPanel->clear();
    updateTreeIcons();
    updateStatusLabel();
}

// Discovery slots
void TestRunnerPanel::onDiscoveryStarted(TestFramework framework, const QString& path) {
    m_statusLabel->setText(QString("Discovering %1 tests...").arg(static_cast<int>(framework)));
}

void TestRunnerPanel::onDiscoveryFinished(const DiscoveryResult& result) {
    m_discovering = false;
    
    if (!result.success) {
        m_statusLabel->setText(QString("Discovery failed: %1").arg(result.errorMessage));
        return;
    }
    
    m_testSuites = result.suites;
    m_totalTests = result.totalTests;
    
    populateTree(result.suites);
    updateStatusLabel();
    
    m_statusLabel->setText(QString("Discovered %1 tests").arg(m_totalTests));
}

void TestRunnerPanel::onDiscoveryError(const QString& message) {
    m_discovering = false;
    m_statusLabel->setText(QString("Discovery error: %1").arg(message));
}

// Execution slots
void TestRunnerPanel::onExecutionStarted(int totalTests) {
    m_running = true;
    m_runBtn->setEnabled(false);
    m_runSelectedBtn->setEnabled(false);
    m_runFailedBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(totalTests);
    m_progressBar->setValue(0);
    m_outputView->clear();
    m_statusLabel->setText(QString("Running %1 tests...").arg(totalTests));
}

void TestRunnerPanel::onTestStarted(const TestCase& test) {
    QTreeWidgetItem* item = findTreeItem(test.id);
    if (item) {
        item->setIcon(1, getStatusIcon(TestStatus::Running));
        item->setText(1, "Running");
    }
    m_runningTests++;
}

void TestRunnerPanel::onTestFinished(const TestExecutionResult& result) {
    m_testResults[result.testCase.id] = result;
    m_runningTests--;
    
    if (result.isPassed()) m_passedTests++;
    else if (result.isFailed()) m_failedTests++;
    else if (result.isSkipped()) m_skippedTests++;
    
    m_totalDurationMs += result.durationMs;
    
    updateTreeItem(result);
    updateStatusLabel();
    
    // Append output
    if (!result.output.isEmpty()) {
        m_outputView->append(QString("\n=== %1 ===").arg(result.testCase.name));
        m_outputView->append(result.output);
        if (!result.failureMessage.isEmpty()) {
            m_outputView->append(QString("FAILURE: %1").arg(result.failureMessage));
        }
    }
    if (m_outputDetailsPanel) {
        m_outputDetailsPanel->showResult(result);
    }
}

void TestRunnerPanel::onExecutionFinished(int passed, int failed, int skipped) {
    m_running = false;
    m_runBtn->setEnabled(true);
    m_runSelectedBtn->setEnabled(true);
    m_runFailedBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_progressBar->setVisible(false);

    QString summary = QString("Completed: %1 passed, %2 failed, %3 skipped in %4")
        .arg(passed).arg(failed).arg(skipped)
        .arg(formatDuration(m_totalDurationMs));

    m_statusLabel->setText(summary);
    m_outputView->append(QString("\n\n%1").arg(summary));

    // Refresh coverage if available
    refreshCoverage();
}

void TestRunnerPanel::onExecutionProgress(int current, int total) {
    m_progressBar->setValue(current);
    updateStatusLabel();
}

void TestRunnerPanel::onTestOutput(const QString& testId, const QString& output) {
    // Real-time output append (could be heavy, consider buffering)
    m_outputView->append(output);
    if (m_outputDetailsPanel) m_outputDetailsPanel->appendLiveOutput(testId, output);
}

// UI slots
void TestRunnerPanel::onTreeItemClicked(QTreeWidgetItem* item, int column) {
    QString testId = item->data(0, Qt::UserRole).toString();
    if (testId.isEmpty()) return;
    
    // Find and emit test case
    for (const TestSuite& suite : m_testSuites) {
        for (const TestCase& tc : suite.cases) {
            if (tc.id == testId) {
                emit testSelected(tc);
                return;
            }
        }
    }
}

void TestRunnerPanel::onTreeItemDoubleClicked(QTreeWidgetItem* item, int column) {
    QString testId = item->data(0, Qt::UserRole).toString();
    if (testId.isEmpty()) return;
    
    for (const TestSuite& suite : m_testSuites) {
        for (const TestCase& tc : suite.cases) {
            if (tc.id == testId) {
                emit testDoubleClicked(tc);
                return;
            }
        }
    }
}

void TestRunnerPanel::onTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_testTree->itemAt(pos);
    if (!item) return;
    
    QMenu menu(this);
    menu.addAction("Run Test", this, &TestRunnerPanel::runSelectedTests);
    menu.addAction("Debug Test", this, &TestRunnerPanel::debugSelectedTest);
    menu.addSeparator();
    menu.addAction("Go to Source", [this, item]() {
        // Navigate to source (would integrate with editor)
    });
    menu.exec(m_testTree->mapToGlobal(pos));
}

void TestRunnerPanel::onFilterTextChanged(const QString& text) {
    applyFilters();
}

void TestRunnerPanel::onFrameworkFilterChanged(int index) {
    applyFilters();
}

void TestRunnerPanel::onStatusFilterChanged(int index) {
    applyFilters();
}

void TestRunnerPanel::onRunButtonClicked() {
    runAllTests();
}

void TestRunnerPanel::onDebugButtonClicked() {
    debugSelectedTest();
}

void TestRunnerPanel::onStopButtonClicked() {
    stopExecution();
}

void TestRunnerPanel::onRefreshButtonClicked() {
    refreshTests();
}

// Tree management
void TestRunnerPanel::populateTree(const QList<TestSuite>& suites) {
    m_testTree->clear();
    m_treeItemMap.clear();
    
    for (const TestSuite& suite : suites) {
        auto* suiteItem = new QTreeWidgetItem(m_testTree);
        suiteItem->setText(0, suite.name);
        suiteItem->setText(3, QString::number(static_cast<int>(suite.framework)));
        suiteItem->setIcon(0, QIcon::fromTheme("folder"));
        
        for (const TestCase& tc : suite.cases) {
            auto* testItem = new QTreeWidgetItem(suiteItem);
            testItem->setText(0, tc.name);
            testItem->setText(1, "Not Run");
            testItem->setText(3, QString::number(static_cast<int>(tc.framework)));
            testItem->setData(0, Qt::UserRole, tc.id);
            testItem->setIcon(0, QIcon::fromTheme("application-x-executable"));
            testItem->setIcon(1, getStatusIcon(TestStatus::NotRun));
            
            m_treeItemMap[tc.id] = testItem;
        }
        
        suiteItem->setExpanded(true);
    }
}

void TestRunnerPanel::updateTreeItem(const TestExecutionResult& result) {
    QTreeWidgetItem* item = findTreeItem(result.testCase.id);
    if (!item) return;
    
    item->setIcon(1, getStatusIcon(result.testCase.status));
    item->setText(1, QString::number(static_cast<int>(result.testCase.status)));
    item->setText(2, formatDuration(result.durationMs));
    
    // Color code by status
    QColor color = getStatusColor(result.testCase.status);
    for (int col = 0; col < m_testTree->columnCount(); ++col) {
        item->setForeground(col, color);
    }
}

QTreeWidgetItem* TestRunnerPanel::findTreeItem(const QString& testId) {
    return m_treeItemMap.value(testId, nullptr);
}

void TestRunnerPanel::updateTreeIcons() {
    for (auto it = m_treeItemMap.begin(); it != m_treeItemMap.end(); ++it) {
        if (m_testResults.contains(it.key())) {
            updateTreeItem(m_testResults[it.key()]);
        } else {
            it.value()->setIcon(1, getStatusIcon(TestStatus::NotRun));
            it.value()->setText(1, "Not Run");
            it.value()->setText(2, "");
        }
    }
}

void TestRunnerPanel::applyFilters() {
    QString filterText = m_filterEdit->text().toLower();
    int frameworkIdx = m_frameworkFilter->currentIndex();
    int statusIdx = m_statusFilter->currentIndex();

    for (int i = 0; i < m_testTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* suiteItem = m_testTree->topLevelItem(i);
        bool suiteVisible = false;

        for (int j = 0; j < suiteItem->childCount(); ++j) {
            QTreeWidgetItem* testItem = suiteItem->child(j);
            QString testName = testItem->text(0).toLower();

            bool matchesText = filterText.isEmpty() || testName.contains(filterText);

            bool matchesFramework = true;
            if (frameworkIdx > 0) { // 1..7 map to our enum order in combo
                int fw = testItem->text(3).toInt();
                // Combo order: 1:GTest 2:Catch2 3:PyTest 4:Jest 5:Go 6:Cargo 7:CTest
                int desired = frameworkIdx - 1; // assuming enum order matches
                matchesFramework = (fw == desired);
            }

            bool matchesStatus = true;
            if (statusIdx > 0) { // 1:NotRun 2:Passed 3:Failed 4:Skipped
                int status = testItem->text(1) == "Not Run" ? (int)TestStatus::NotRun : testItem->text(1).toInt();
                switch (statusIdx) {
                    case 1: matchesStatus = (status == (int)TestStatus::NotRun); break;
                    case 2: matchesStatus = (status == (int)TestStatus::Passed); break;
                    case 3: matchesStatus = (status == (int)TestStatus::Failed); break;
                    case 4: matchesStatus = (status == (int)TestStatus::Skipped); break;
                    default: break;
                }
            }

            testItem->setHidden(!(matchesText && matchesFramework && matchesStatus));
            if (!testItem->isHidden()) suiteVisible = true;
        }
        suiteItem->setHidden(!suiteVisible);
    }
}

// UI helpers
QIcon TestRunnerPanel::getStatusIcon(TestStatus status) {
    switch (status) {
        case TestStatus::Passed:
            return QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton);
        case TestStatus::Failed:
            return QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton);
        case TestStatus::Skipped:
            return QApplication::style()->standardIcon(QStyle::SP_DialogResetButton);
        case TestStatus::Running:
            return QApplication::style()->standardIcon(QStyle::SP_BrowserReload);
        case TestStatus::Timeout:
            return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
        case TestStatus::Error:
            return QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
        default:
            return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
}

QColor TestRunnerPanel::getStatusColor(TestStatus status) {
    switch (status) {
        case TestStatus::Passed:
            return QColor(0, 180, 0);
        case TestStatus::Failed:
        case TestStatus::Error:
        case TestStatus::Timeout:
            return QColor(220, 0, 0);
        case TestStatus::Skipped:
            return QColor(200, 160, 0);
        default:
            return QColor(200, 200, 200);
    }
}

QString TestRunnerPanel::formatDuration(double ms) {
    if (ms < 1000) {
        return QString("%1 ms").arg(static_cast<int>(ms));
    } else {
        return QString("%1 s").arg(ms / 1000.0, 0, 'f', 2);
    }
}

void TestRunnerPanel::updateStatusLabel() {
    m_statsLabel->setText(QString("%1 tests | %2 passed | %3 failed | %4 skipped | %5")
        .arg(m_totalTests)
        .arg(m_passedTests)
        .arg(m_failedTests)
        .arg(m_skippedTests)
        .arg(formatDuration(m_totalDurationMs)));
}

void TestRunnerPanel::refreshCoverage() {
    if (!m_coverageCollector) return;
    m_coverageCollector->setWorkspace(m_workspacePath);
    // Run in background to avoid UI stall
    QMetaObject::invokeMethod(this, [this]{
        auto rep = m_coverageCollector->discoverAndLoad();
        if (rep.files.isEmpty()) {
            // Keep tree as-is if no report found
        }
    }, Qt::QueuedConnection);
}

void TestRunnerPanel::expandAll() {
    m_testTree->expandAll();
}

void TestRunnerPanel::collapseAll() {
    m_testTree->collapseAll();
}

} // namespace RawrXD
