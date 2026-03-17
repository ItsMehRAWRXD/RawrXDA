#include "CloudRunnerWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLabel>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QDir>
#include <QProcess>
#include <QElapsedTimer>
#include <QTimer>
#include <QSettings>

CloudRunnerWidget::CloudRunnerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    
    network_ = new QNetworkAccessManager(this);
    logSocket_ = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    
    connect(logSocket_, &QWebSocket::textMessageReceived, this, &CloudRunnerWidget::onWebSocketMessage);
    
    // Load credentials from settings
    QSettings settings("RawrXD", "QtShell");
    githubToken_ = settings.value("cloud/github_token").toString();
    awsAccessKey_ = settings.value("cloud/aws_key").toString();
}

CloudRunnerWidget::~CloudRunnerWidget() = default;

void CloudRunnerWidget::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Provider & instance selection
    QFormLayout* configForm = new QFormLayout();
    
    providerCombo_ = new QComboBox(this);
    providerCombo_->addItems({"GitHub Actions", "AWS EC2 Spot", "GCP Preemptible"});
    
    instanceCombo_ = new QComboBox(this);
    instanceCombo_->addItems({
        "ubuntu-4-core (2¢/min)",
        "ubuntu-8-core (3¢/min)",
        "ubuntu-16-core (6¢/min)",
        "ubuntu-32-core (12¢/min)"
    });
    
    configForm->addRow(tr("Provider:"), providerCombo_);
    configForm->addRow(tr("Instance:"), instanceCombo_);
    
    // Control buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    startBtn_ = new QPushButton(tr("⚡ Run on Cloud"), this);
    cancelBtn_ = new QPushButton(tr("Cancel"), this);
    cancelBtn_->setEnabled(false);
    
    connect(startBtn_, &QPushButton::clicked, this, [this]() {
        startJob(providerCombo_->currentText(), instanceCombo_->currentText().split(' ').first());
    });
    connect(cancelBtn_, &QPushButton::clicked, this, &CloudRunnerWidget::cancelJob);
    
    btnLayout->addWidget(startBtn_);
    btnLayout->addWidget(cancelBtn_);
    btnLayout->addStretch();
    
    // Progress bar
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    
    // Status labels
    statusLabel_ = new QLabel(tr("Ready"), this);
    statusLabel_->setStyleSheet("font-weight: bold; color: #00aa00;");
    
    costLabel_ = new QLabel(tr("Cost: $0.000"), this);
    costLabel_->setStyleSheet("color: #0088ff;");
    
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLayout->addWidget(statusLabel_);
    statusLayout->addStretch();
    statusLayout->addWidget(costLabel_);
    
    // Log output
    logOutput_ = new QPlainTextEdit(this);
    logOutput_->setReadOnly(true);
    logOutput_->setStyleSheet("background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', monospace;");
    logOutput_->setPlaceholderText(tr("Cloud job logs will appear here..."));
    
    // Assemble layout
    mainLayout->addLayout(configForm);
    mainLayout->addLayout(btnLayout);
    mainLayout->addWidget(progressBar_);
    mainLayout->addLayout(statusLayout);
    mainLayout->addWidget(logOutput_, 1);
}

void CloudRunnerWidget::startJob(const QString& provider, const QString& instanceType) {
    if (jobRunning_) {
        logOutput_->appendPlainText("[WARN] Job already running");
        return;
    }
    
    if (githubToken_.isEmpty() && provider.contains("GitHub")) {
        logOutput_->appendPlainText("[ERROR] GitHub token not configured. Set via Settings → Cloud Credentials");
        return;
    }
    
    jobRunning_ = true;
    startBtn_->setEnabled(false);
    cancelBtn_->setEnabled(true);
    statusLabel_->setText(tr("Preparing..."));
    progressBar_->setValue(10);
    logOutput_->clear();
    jobTimer_.start();
    
    logOutput_->appendPlainText(QString("[INFO] Starting cloud job on %1 (%2)").arg(provider, instanceType));
    
    // Step 1: Zip workspace
    QString projectRoot = QDir::currentPath();
    logOutput_->appendPlainText(QString("[INFO] Zipping workspace: %1").arg(projectRoot));
    QString zipPath = zipWorkspace(projectRoot);
    
    if (!zipPath.isEmpty()) {
        onZipReady(zipPath);
    } else {
        jobRunning_ = false;
        startBtn_->setEnabled(true);
        cancelBtn_->setEnabled(false);
        statusLabel_->setText(tr("Failed"));
        statusLabel_->setStyleSheet("font-weight: bold; color: #aa0000;");
    }
}

void CloudRunnerWidget::cancelJob() {
    if (!jobRunning_) return;
    
    logOutput_->appendPlainText("[INFO] Cancelling job...");
    // TODO: POST /repos/{owner}/{repo}/actions/runs/{run_id}/cancel
    
    if (logSocket_->isValid()) {
        logSocket_->close();
    }
    
    jobRunning_ = false;
    startBtn_->setEnabled(true);
    cancelBtn_->setEnabled(false);
    statusLabel_->setText(tr("Cancelled"));
    statusLabel_->setStyleSheet("font-weight: bold; color: #aaaa00;");
}

QString CloudRunnerWidget::zipWorkspace(const QString& projectRoot) {
    QTemporaryFile* zipFile = new QTemporaryFile(this);
    zipFile->setFileTemplate(QDir::temp().filePath("rawrxd-XXXXXX.zip"));
    if (!zipFile->open()) {
        logOutput_->appendPlainText("[ERROR] Failed to create temp zip file");
        return QString();
    }
    
    QString zipPath = zipFile->fileName();
    zipFile->close();
    
    // Use 7z or system zip (cross-platform fallback)
    QProcess zipper;
    QStringList args;
    
#ifdef Q_OS_WIN
    args << "a" << "-tzip" << zipPath << projectRoot + "\\*";
    zipper.start("7z", args);
#else
    args << "-r" << zipPath << ".";
    zipper.setWorkingDirectory(projectRoot);
    zipper.start("zip", args);
#endif
    
    if (!zipper.waitForFinished(30000)) {
        logOutput_->appendPlainText("[ERROR] Zip operation timed out");
        return QString();
    }
    
    if (zipper.exitCode() != 0) {
        logOutput_->appendPlainText(QString("[ERROR] Zip failed: %1").arg(QString::fromLocal8Bit(zipper.readAllStandardError())));
        return QString();
    }
    
    logOutput_->appendPlainText(QString("[INFO] Workspace zipped: %1").arg(zipPath));
    return zipPath;
}

void CloudRunnerWidget::onZipReady(const QString& zipPath) {
    progressBar_->setValue(20);
    statusLabel_->setText(tr("Uploading..."));
    uploadZip(zipPath);
}

void CloudRunnerWidget::uploadZip(const QString& zipPath) {
    // TODO: Upload to temporary storage (GitHub Gist, AWS S3, or direct artifact API)
    // For now, mock upload
    logOutput_->appendPlainText("[INFO] Uploading zip to cloud storage...");
    
    // Simulate upload delay
    QTimer::singleShot(2000, this, [this]() {
        QString mockUploadUrl = "https://storage.rawrxd.dev/projects/mock-upload-123.zip";
        logOutput_->appendPlainText(QString("[INFO] Upload complete: %1").arg(mockUploadUrl));
        progressBar_->setValue(40);
        dispatchWorkflow(instanceCombo_->currentText().split(' ').first(), mockUploadUrl);
    });
}

void CloudRunnerWidget::dispatchWorkflow(const QString& instanceType, const QString& zipUrl) {
    statusLabel_->setText(tr("Starting build..."));
    logOutput_->appendPlainText(QString("[INFO] Dispatching workflow (instance: %1)").arg(instanceType));
    
    QNetworkRequest req(QUrl("https://api.github.com/repos/ItsMehRAWRXD/RawrXD/actions/workflows/cloud.yml/dispatches"));
    req.setRawHeader("Authorization", QString("Bearer %1").arg(githubToken_).toUtf8());
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject payload;
    payload["ref"] = "main";
    QJsonObject inputs;
    inputs["instance"] = instanceType;
    inputs["project_zip"] = zipUrl;
    payload["inputs"] = inputs;
    
    QNetworkReply* reply = network_->post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* r = qobject_cast<QNetworkReply*>(sender());
        if (r) onDispatchFinished(r);
    });
}

void CloudRunnerWidget::onDispatchFinished(QNetworkReply* reply) {
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        logOutput_->appendPlainText(QString("[ERROR] Dispatch failed: %1").arg(reply->errorString()));
        jobRunning_ = false;
        startBtn_->setEnabled(true);
        cancelBtn_->setEnabled(false);
        statusLabel_->setText(tr("Failed"));
        statusLabel_->setStyleSheet("font-weight: bold; color: #aa0000;");
        return;
    }
    
    logOutput_->appendPlainText("[INFO] Workflow dispatched successfully");
    progressBar_->setValue(50);
    
    // Start polling for run ID
    QTimer::singleShot(5000, this, [this]() {
        // TODO: GET /repos/{owner}/{repo}/actions/runs to find the latest run
        QString mockRunId = "1234567890";
        currentRunId_ = mockRunId;
        logOutput_->appendPlainText(QString("[INFO] Run ID: %1").arg(mockRunId));
        startPolling(mockRunId);
        connectWebSocket(mockRunId);
    });
}

void CloudRunnerWidget::startPolling(const QString& runId) {
    QTimer* pollTimer = new QTimer(this);
    connect(pollTimer, &QTimer::timeout, this, [this, runId]() {
        downloadArtifacts(runId);
    });
    pollTimer->start(pollIntervalMs_);
}

void CloudRunnerWidget::downloadArtifacts(const QString& runId) {
    QNetworkRequest req(QUrl(QString("https://api.github.com/repos/ItsMehRAWRXD/RawrXD/actions/runs/%1/artifacts").arg(runId)));
    req.setRawHeader("Authorization", QString("Bearer %1").arg(githubToken_).toUtf8());
    req.setRawHeader("Accept", "application/vnd.github+json");
    
    QNetworkReply* reply = network_->get(req);
    connect(reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply* r = qobject_cast<QNetworkReply*>(sender());
        if (r) onPollFinished(r);
    });
}

void CloudRunnerWidget::onPollFinished(QNetworkReply* reply) {
    reply->deleteLater();
    
    if (reply->error() != QNetworkReply::NoError) {
        return; // Continue polling
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray artifacts = doc.object()["artifacts"].toArray();
    
    if (!artifacts.isEmpty()) {
        logOutput_->appendPlainText("[INFO] Artifacts ready, downloading...");
        progressBar_->setValue(90);
        
        for (const QJsonValue& val : artifacts) {
            QString url = val.toObject()["archive_download_url"].toString();
            // TODO: Download and extract artifacts
        }
        
        jobRunning_ = false;
        startBtn_->setEnabled(true);
        cancelBtn_->setEnabled(false);
        statusLabel_->setText(tr("Completed ✓"));
        statusLabel_->setStyleSheet("font-weight: bold; color: #00aa00;");
        progressBar_->setValue(100);
        
        emit jobCompleted(true, artifacts.first().toObject()["archive_download_url"].toString());
    }
}

void CloudRunnerWidget::connectWebSocket(const QString& jobId) {
    QString wsUrl = QString("wss://jobs.stream.rawrxd.dev/%1").arg(jobId);
    logOutput_->appendPlainText(QString("[INFO] Connecting to log stream: %1").arg(wsUrl));
    logSocket_->open(QUrl(wsUrl));
}

void CloudRunnerWidget::onWebSocketMessage(const QString& msg) {
    logOutput_->appendPlainText(msg);
    emit logChunk(msg);
    
    // Update progress based on log keywords
    if (msg.contains("cmake", Qt::CaseInsensitive)) {
        progressBar_->setValue(60);
        statusLabel_->setText(tr("Building..."));
    } else if (msg.contains("ctest", Qt::CaseInsensitive)) {
        progressBar_->setValue(75);
        statusLabel_->setText(tr("Testing..."));
    } else if (msg.contains("perf", Qt::CaseInsensitive)) {
        progressBar_->setValue(85);
        statusLabel_->setText(tr("Profiling..."));
    }
    
    updateCostEstimate();
}

void CloudRunnerWidget::updateCostEstimate() {
    double elapsedMin = jobTimer_.elapsed() / 60000.0;
    double spotPrice = getSpotPrice(instanceCombo_->currentText().split(' ').first());
    double cost = elapsedMin * spotPrice;
    
    costLabel_->setText(QString("Cost: $%1").arg(cost, 0, 'f', 3));
    emit costUpdated(cost);
}

double CloudRunnerWidget::getSpotPrice(const QString& instanceType) const {
    // Mock spot pricing
    if (instanceType.contains("4-core")) return 0.02;
    if (instanceType.contains("8-core")) return 0.03;
    if (instanceType.contains("16-core")) return 0.06;
    if (instanceType.contains("32-core")) return 0.12;
    return 0.01;
}
