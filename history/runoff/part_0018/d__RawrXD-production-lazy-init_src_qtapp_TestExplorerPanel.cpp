#include "TestExplorerPanel.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QHeaderView>
#include <QTreeWidgetItem>

TestExplorerPanel::TestExplorerPanel(QWidget* parent) : QWidget(parent) {
    RAWRXD_INIT_TIMED("TestExplorerPanel");
    setupUI();
}

void TestExplorerPanel::setupUI() {
    RAWRXD_TIMED_FUNC();
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Toolbar
    m_toolBar = new QToolBar(this);
    m_runAllAction = m_toolBar->addAction("▶ Run All");
    m_runSelectedAction = m_toolBar->addAction("⏵ Run Selected");
    m_toolBar->addSeparator();
    m_generateTestsAction = m_toolBar->addAction("🧪 Generate Tests");
    m_showCoverageAction = m_toolBar->addAction("📊 Show Coverage");

    connect(m_runAllAction, &QAction::triggered, this, &TestExplorerPanel::onRunAll);
    connect(m_runSelectedAction, &QAction::triggered, this, &TestExplorerPanel::onRunSelected);
    connect(m_generateTestsAction, &QAction::triggered, this, &TestExplorerPanel::onGenerateTests);
    connect(m_showCoverageAction, &QAction::triggered, this, &TestExplorerPanel::onShowCoverage);

    mainLayout->addWidget(m_toolBar);

    // Progress bar
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setFixedHeight(15);
    m_overallProgress->setTextVisible(false);
    mainLayout->addWidget(m_overallProgress);

    // Stats label
    m_statsLabel = new QLabel("Tests: 0 | Passed: 0 | Failed: 0", this);
    m_statsLabel->setStyleSheet("padding: 2px 5px; font-size: 10px;");
    mainLayout->addWidget(m_statsLabel);

    // Test Tree
    m_testTree = new QTreeWidget(this);
    m_testTree->setHeaderLabels({"Test", "Status", "Duration"});
    m_testTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_testTree);
}

void TestExplorerPanel::initialize() {
    RAWRXD_TIMED_FUNC();
    auto& runner = RawrXD::TestRunnerIntegration::instance();
    runner.setTestTree(m_testTree);
    
    connect(&runner, &RawrXD::TestRunnerIntegration::testStarted, this, &TestExplorerPanel::onTestStarted);
    connect(&runner, &RawrXD::TestRunnerIntegration::testFinished, this, &TestExplorerPanel::onTestFinished);
    connect(&runner, &RawrXD::TestRunnerIntegration::suiteFinished, this, &TestExplorerPanel::onSuiteFinished);
}

void TestExplorerPanel::onRunAll() {
    RAWRXD_TIMED_FUNC();
    emit runAllRequested();
}

void TestExplorerPanel::onRunSelected() {
    RAWRXD_TIMED_FUNC();
    emit runSelectedRequested();
}

void TestExplorerPanel::onGenerateTests() {
    RAWRXD_TIMED_FUNC();
    emit generateTestsRequested();
}

void TestExplorerPanel::onShowCoverage() {
    RAWRXD_TIMED_FUNC();
    emit showCoverageRequested();
}

void TestExplorerPanel::onTestStarted(const QString& suite, const QString& test) {
    m_totalTests++;
    updateStats();
}

void TestExplorerPanel::onTestFinished(const QString& suite, const QString& test, bool success) {
    if (success) m_passedTests++;
    else m_failedTests++;
    updateStats();
}

void TestExplorerPanel::onSuiteFinished(const QString& suite, int passed, int failed, int total) {
    m_overallProgress->setMaximum(total);
    m_overallProgress->setValue(passed + failed);
}

void TestExplorerPanel::updateStats() {
    m_statsLabel->setText(QString("Tests: %1 | Passed: %2 | Failed: %3")
        .arg(m_totalTests).arg(m_passedTests).arg(m_failedTests));
    
    if (m_totalTests > 0) {
        m_overallProgress->setMaximum(m_totalTests);
        m_overallProgress->setValue(m_passedTests + m_failedTests);
    }
}
