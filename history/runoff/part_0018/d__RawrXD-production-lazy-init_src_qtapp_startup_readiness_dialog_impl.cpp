/**
 * @file startup_readiness_dialog_impl.cpp
 * @brief Production implementation of StartupReadinessDialog
 */

#include "startup_readiness_checker.hpp"
#include "unified_hotpatch_manager.hpp"
#include <QDateTime>
#include <QDebug>
#include <QTimer>

// ============================================================================
// StartupReadinessDialog: Production-Ready UI with Progress Tracking
// ============================================================================

StartupReadinessDialog::StartupReadinessDialog(QWidget* parent)
    : QDialog(parent)
    , m_checker(new StartupReadinessChecker(this))
    , m_hotpatchManager(nullptr)
    , m_checksPassed(false)
{
    setWindowTitle("Startup Readiness Validation");
    setMinimumSize(700, 500);
    setModal(true);
    
    qInfo() << "[StartupReadinessDialog] Initializing production-grade UI";
    
    setupUI();
    
    // Connect signals
    connect(m_checker, &StartupReadinessChecker::readinessComplete,
            this, &StartupReadinessDialog::onReadinessComplete);
    connect(m_checker, &StartupReadinessChecker::checkProgress,
            this, &StartupReadinessDialog::onCheckProgress);
    
    qDebug() << "[StartupReadinessDialog] UI initialization complete";
}

StartupReadinessDialog::~StartupReadinessDialog() {
    qInfo() << "[StartupReadinessDialog] Dialog destroyed";
}

void StartupReadinessDialog::setupUI() {
    m_mainLayout = new QVBoxLayout(this);
    
    // Title
    QLabel* titleLabel = new QLabel("<h2>System Health Validation</h2>", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    m_mainLayout->addWidget(titleLabel);
    
    // Checks group
    m_checksGroup = new QGroupBox("Health Checks", this);
    m_checksLayout = new QVBoxLayout(m_checksGroup);
    m_mainLayout->addWidget(m_checksGroup);
    
    // Overall progress
    m_summaryLabel = new QLabel("Preparing health checks...", this);
    m_mainLayout->addWidget(m_summaryLabel);
    
    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 100);
    m_overallProgress->setValue(0);
    m_mainLayout->addWidget(m_overallProgress);
    
    // Diagnostics log
    QLabel* logLabel = new QLabel("Diagnostic Log:", this);
    m_mainLayout->addWidget(logLabel);
    
    m_diagnosticsLog = new QTextEdit(this);
    m_diagnosticsLog->setReadOnly(true);
    m_diagnosticsLog->setMaximumHeight(200);
    m_mainLayout->addWidget(m_diagnosticsLog);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_retryButton = new QPushButton("Retry Checks", this);
    m_skipButton = new QPushButton("Skip", this);
    m_configureButton = new QPushButton("Configure", this);
    m_continueButton = new QPushButton("Continue", this);
    
    m_retryButton->setEnabled(false);
    m_continueButton->setEnabled(false);
    
    buttonLayout->addWidget(m_retryButton);
    buttonLayout->addWidget(m_skipButton);
    buttonLayout->addWidget(m_configureButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_continueButton);
    
    m_mainLayout->addLayout(buttonLayout);
    
    // Connect buttons
    connect(m_retryButton, &QPushButton::clicked, this, &StartupReadinessDialog::onRetryClicked);
    connect(m_skipButton, &QPushButton::clicked, this, &StartupReadinessDialog::onSkipClicked);
    connect(m_configureButton, &QPushButton::clicked, this, &StartupReadinessDialog::onConfigureClicked);
    connect(m_continueButton, &QPushButton::clicked, this, &QDialog::accept);
}

bool StartupReadinessDialog::runChecks(UnifiedHotpatchManager* hotpatchMgr, const QString& projectRoot) {
    QElapsedTimer timer;
    timer.start();
    
    qInfo() << "[StartupReadinessDialog] Starting health checks";
    qInfo() << "[StartupReadinessDialog] Project root:" << projectRoot;
    
    m_hotpatchManager = hotpatchMgr;
    m_projectRoot = projectRoot;
    
    m_summaryLabel->setText("Running system health checks...");
    m_overallProgress->setValue(10);
    m_diagnosticsLog->append(QString("[%1] Starting validation...\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    // Run async checks
    m_checker->runChecks();
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "[StartupReadinessDialog] Check initiation took" << elapsed << "ms";
    
    return exec() == QDialog::Accepted;
}

void StartupReadinessDialog::onCheckProgress(const QString& subsystem, int progress, const QString& message) {
    m_overallProgress->setValue(progress);
    m_summaryLabel->setText(message);
    
    m_diagnosticsLog->append(QString("[%1] %2: %3\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(subsystem)
        .arg(message));
    
    qDebug() << "[StartupReadinessDialog] Progress update:" << subsystem << progress << "%";
}

void StartupReadinessDialog::onReadinessComplete(const AgentReadinessReport& report) {
    QElapsedTimer timer;
    timer.start();
    
    qInfo() << "[StartupReadinessDialog] Health checks completed";
    qInfo() << "[StartupReadinessDialog] Overall ready:" << report.overallReady;
    qInfo() << "[StartupReadinessDialog] Total latency:" << report.totalLatency << "ms";
    
    m_report = report;
    m_overallProgress->setValue(100);
    
    if (report.overallReady) {
        m_summaryLabel->setText("\u2713 All health checks passed successfully");
        m_diagnosticsLog->append(QString("[%1] SUCCESS: System ready for operation\n")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        
        m_checksPassed = true;
        m_continueButton->setEnabled(true);
        m_skipButton->setEnabled(false);
        
        // Log success metrics
        qInfo() << "[StartupReadiness] Metrics: checks_passed=1, latency_ms=" << timer.elapsed();
        
        // Auto-close after 2 seconds
        QTimer::singleShot(2000, this, &QDialog::accept);
    } else {
        m_summaryLabel->setText("\u26a0 Health checks completed with warnings");
        m_diagnosticsLog->append(QString("[%1] WARNING: Some checks failed\n")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
        
        for (const QString& failure : report.failures) {
            m_diagnosticsLog->append(QString("  - FAILED: %1\n").arg(failure));
        }
        
        m_checksPassed = false;
        m_retryButton->setEnabled(true);
        m_continueButton->setEnabled(true); // Allow continuing with warnings
        
        // Log warning metrics
        qWarning() << "[StartupReadiness] Metrics: checks_failed=1, latency_ms=" << timer.elapsed();
    }
    
    showFinalSummary();
}

void StartupReadinessDialog::showFinalSummary() {
    // Build summary text
    QString summary = QString("<h3>Validation Summary</h3>");
    summary += QString("<p>Total checks: %1</p>").arg(m_report.checks.size());
    summary += QString("<p>Failures: %1</p>").arg(m_report.failures.size());
    summary += QString("<p>Warnings: %1</p>").arg(m_report.warnings.size());
    summary += QString("<p>Total time: %1 ms</p>").arg(m_report.totalLatency);
    
    // Update status for each subsystem
    for (auto it = m_report.checks.constBegin(); it != m_report.checks.constEnd(); ++it) {
        updateSubsystemStatus(it.key(), it.value().success, it.value().message);
    }
}

void StartupReadinessDialog::updateSubsystemStatus(const QString& subsystem, bool success, const QString& message) {
    QString icon = success ? "\u2713" : "\u2717";
    QString color = success ? "green" : "red";
    
    QString statusText = QString("<font color='%1'>%2 %3</font>: %4")
        .arg(color)
        .arg(icon)
        .arg(subsystem)
        .arg(message);
    
    // Add to checks group if not already present
    if (!m_statusLabels.contains(subsystem)) {
        QLabel* label = new QLabel(statusText, this);
        m_checksLayout->addWidget(label);
        m_statusLabels[subsystem] = label;
    } else {
        m_statusLabels[subsystem]->setText(statusText);
    }
}

void StartupReadinessDialog::onRetryClicked() {
    qInfo() << "[StartupReadinessDialog] User initiated retry";
    
    m_diagnosticsLog->append(QString("[%1] Retrying health checks...\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    m_retryButton->setEnabled(false);
    m_overallProgress->setValue(0);
    m_continueButton->setEnabled(false);
    
    // Log retry metrics
    qInfo() << "[StartupReadiness] Metrics: checks_retried=1";
    
    runChecks(m_hotpatchManager, m_projectRoot);
}

void StartupReadinessDialog::onSkipClicked() {
    qWarning() << "[StartupReadinessDialog] User skipped health checks";
    
    m_diagnosticsLog->append(QString("[%1] Health checks skipped by user\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    // Log metrics
    qInfo() << "[StartupReadiness] Metrics: checks_skipped=1";
    
    QDialog::accept();
}

void StartupReadinessDialog::onConfigureClicked() {
    qInfo() << "[StartupReadinessDialog] Opening configuration dialog";
    
    m_diagnosticsLog->append(QString("[%1] Opening system configuration...\n")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    
    // In production, this would open a configuration dialog
    // For now, just close
    QDialog::accept();
}
