#include "ModelConversionDialog.h"
#include "TerminalManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTextBrowser>
#include <QScrollArea>
#include <QStyle>
#include <QStyleFactory>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

ModelConversionDialog::ModelConversionDialog(const QStringList& unsupportedTypes,
                                           const QString& recommendedType,
                                           const QString& modelPath,
                                           QWidget* parent)
    : QDialog(parent),
      m_unsupportedTypes(unsupportedTypes),
      m_recommendedType(recommendedType),
      m_modelPath(modelPath),
      m_result(Cancelled),
      m_terminalManager(std::make_unique<TerminalManager>(this)),
      m_verifyTimer(new QTimer(this)),
      m_conversionStage(0)
{
    setWindowTitle("Model Quantization Conversion Required");
    setModal(true);
    setMinimumWidth(600);
    setMinimumHeight(300);
    
    setupUI();
    
    // Connect terminal signals for real-time output monitoring
    connect(m_terminalManager.get(), &TerminalManager::outputReady, this, &ModelConversionDialog::onTerminalOutput);
    connect(m_terminalManager.get(), &TerminalManager::errorReady, this, &ModelConversionDialog::onTerminalError);
    connect(m_terminalManager.get(), QOverload<int, QProcess::ExitStatus>::of(&TerminalManager::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
                onTerminalFinished(exitCode);
            });
    
    // Set up verify timer for polling converted model existence
    connect(m_verifyTimer, &QTimer::timeout, this, &ModelConversionDialog::onVerifyAndReload);
    
    // Initially hide the info panel
    hideInfoPanel();
}

ModelConversionDialog::~ModelConversionDialog()
{
    m_verifyTimer->stop();
    if (m_terminalManager && m_terminalManager->isRunning()) {
        m_terminalManager->stop();
    }
}

void ModelConversionDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    
    // Title
    m_titleLabel = new QLabel(
        "<b>Model Quantization Incompatibility Detected</b>",
        this
    );
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #d9534f;");
    mainLayout->addWidget(m_titleLabel);
    
    // Main message
    m_messageLabel = new QLabel(this);
    m_messageLabel->setWordWrap(true);
    QString message = QString(
        "Your model uses unsupported quantization type(s) that your GGML library doesn't support yet:\n\n"
    );
    for (const auto& type : m_unsupportedTypes) {
        message += "  • " + type + "\n";
    }
    message += QString(
        "\nWould you like to convert the model to <b>%1</b> quantization?\n"
        "This is a one-time operation that will take 15-30 minutes.\n"
        "The converted model will be saved alongside the original."
    ).arg(m_recommendedType);
    m_messageLabel->setText(message);
    mainLayout->addWidget(m_messageLabel);
    
    // Details text (initially hidden, shown on "More Info")
    m_detailsText = new QTextEdit(this);
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumHeight(150);
    m_detailsText->setVisible(false);
    QString details = QString(
        "<h3>Quantization Conversion Details</h3>\n"
        "<p>Quantization reduces model size while maintaining quality:\n"
        "<ul>\n"
        "<li><b>IQ4_NL (type 39)</b> - Latest Ollama quantization, not yet in GGML</li>\n"
        "<li><b>%1</b> - Excellent 5-bit compression, widely supported</li>\n"
        "</ul>\n"
        "<p><b>Process:</b></p>\n"
        "<ol>\n"
        "<li>Clone llama.cpp repository (first time only, ~1 GB)</li>\n"
        "<li>Build quantize tool (~5 minutes)</li>\n"
        "<li>Convert model to %1 (~20 minutes)</li>\n"
        "<li>Model will be reloaded automatically</li>\n"
        "</ol>\n"
        "<p><b>Model path:</b> %2</p>\n"
    ).arg(m_recommendedType, m_modelPath);
    m_detailsText->setHtml(details);
    mainLayout->addWidget(m_detailsText);
    
    // Progress bar (hidden initially)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);  // Percentage-based progress (0-100%)
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    // Status label (for progress messages)
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #5cb85c; font-style: italic;");
    m_statusLabel->setVisible(false);
    mainLayout->addWidget(m_statusLabel);
    
    // Stretch to take remaining space
    mainLayout->addStretch();
    
    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    
    m_moreInfoButton = new QPushButton("More Info", this);
    m_moreInfoButton->setFlat(true);
    connect(m_moreInfoButton, &QPushButton::clicked, this, &ModelConversionDialog::onMoreInfoClicked);
    buttonLayout->addWidget(m_moreInfoButton);
    
    buttonLayout->addStretch();
    
    // Cancel Conversion button (shown only during active conversion)
    m_cancelConversionButton = new QPushButton("Cancel Conversion", this);
    m_cancelConversionButton->setMinimumWidth(140);
    m_cancelConversionButton->setStyleSheet(
        "QPushButton { background-color: #d9534f; color: white; font-weight: bold; padding: 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #c9302c; }"
        "QPushButton:pressed { background-color: #ac2925; }"
    );
    m_cancelConversionButton->setVisible(false);
    connect(m_cancelConversionButton, &QPushButton::clicked, this, &ModelConversionDialog::onCancelConversion);
    buttonLayout->addWidget(m_cancelConversionButton);
    
    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setMinimumWidth(100);
    connect(m_cancelButton, &QPushButton::clicked, this, &ModelConversionDialog::onCancel);
    buttonLayout->addWidget(m_cancelButton);
    
    m_convertButton = new QPushButton("Yes, Convert", this);
    m_convertButton->setMinimumWidth(120);
    m_convertButton->setStyleSheet(
        "QPushButton { background-color: #5cb85c; color: white; font-weight: bold; padding: 6px; border-radius: 3px; }"
        "QPushButton:hover { background-color: #4cae4c; }"
        "QPushButton:pressed { background-color: #398439; }"
    );
    connect(m_convertButton, &QPushButton::clicked, this, &ModelConversionDialog::onConvertClicked);
    buttonLayout->addWidget(m_convertButton);
    
    mainLayout->addLayout(buttonLayout);
}

void ModelConversionDialog::showInfoPanel()
{
    m_detailsText->setVisible(true);
    m_moreInfoButton->setText("Hide Info");
    resize(width(), height() + 150);
}

void ModelConversionDialog::hideInfoPanel()
{
    m_detailsText->setVisible(false);
    m_moreInfoButton->setText("More Info");
}

void ModelConversionDialog::onMoreInfoClicked()
{
    if (m_detailsText->isVisible()) {
        hideInfoPanel();
    } else {
        showInfoPanel();
    }
}

void ModelConversionDialog::onCancel()
{
    if (m_conversionInProgress) {
        QMessageBox::warning(this, "Conversion In Progress",
            "Use the 'Cancel Conversion' button to stop the conversion.");
        return;
    }
    m_result = Cancelled;
    reject();
}

void ModelConversionDialog::onCancelConversion()
{
    if (!m_conversionInProgress) {
        return;
    }
    
    auto reply = QMessageBox::question(this, "Cancel Conversion",
        "Are you sure you want to cancel the conversion?\nThis will terminate the process and may leave partial files.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        updateProgress("✗ Conversion cancelled by user");
        m_statusLabel->setStyleSheet("color: #d9534f; font-weight: bold;");
        
        // Terminate terminal process
        if (m_terminalManager && m_terminalManager->isRunning()) {
            m_terminalManager->stop();
        }
        
        // Log cancellation
        qint64 duration = QDateTime::currentMSecsSinceEpoch() - m_conversionStartTime;
        logConversionHistory(false, duration);
        
        m_conversionInProgress = false;
        m_result = Cancelled;
        m_progressBar->setVisible(false);
        m_cancelConversionButton->setVisible(false);
        m_convertButton->setEnabled(true);
        m_cancelButton->setEnabled(true);
        m_moreInfoButton->setEnabled(true);
    }
}

void ModelConversionDialog::onConvertClicked()
{
    m_convertButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
    m_moreInfoButton->setEnabled(false);
    m_convertButton->setVisible(false);
    m_cancelButton->setVisible(false);
    
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setVisible(true);
    m_cancelConversionButton->setVisible(true);
    m_conversionInProgress = true;
    m_conversionStartTime = QDateTime::currentMSecsSinceEpoch();
    m_chunksProcessed = 0;
    m_totalChunks = 0;
    
    startConversion();
}

void ModelConversionDialog::startConversion()
{
    m_conversionStage = 0;
    updateProgress("Initializing quantization conversion...");
    
    // Extract output directory from model path
    QString outputDir = m_modelPath.left(m_modelPath.lastIndexOf('/'));
    if (outputDir.isEmpty()) {
        outputDir = ".";
    }
    
    // Build PowerShell command to invoke the conversion script
    // The script D:\setup-quantized-model.ps1 handles all the heavy lifting
    QString command = QString(
        "& 'D:\\setup-quantized-model.ps1' -BlobPath '%1' -OutputDir '%2' -TargetQuantization '%3'"
    ).arg(m_modelPath, outputDir, m_recommendedType);
    
    // Start terminal with PowerShell
    if (!m_terminalManager->start(TerminalManager::PowerShell)) {
        updateProgress("✗ Failed to start PowerShell terminal");
        m_statusLabel->setStyleSheet("color: #d9534f; font-weight: bold;");
        m_convertButton->setEnabled(true);
        m_cancelButton->setEnabled(true);
        m_moreInfoButton->setEnabled(true);
        return;
    }
    
    // Wait for shell to initialize, then send command
    QTimer::singleShot(500, this, [this, command]() {
        if (m_terminalManager->isRunning()) {
            m_terminalManager->writeInput(command.toUtf8());
            updateProgress("Conversion script started...");
        }
    });
}

void ModelConversionDialog::updateProgress(const QString& message)
{
    m_statusLabel->setText(message);
    m_detailsText->append(message);
    m_statusLabel->repaint();
}

void ModelConversionDialog::onTerminalOutput(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    m_detailsText->append(text);
    
    // Parse progress from output
    parseProgressFromOutput(text);
}

void ModelConversionDialog::onTerminalError(const QByteArray& output)
{
    QString text = QString::fromUtf8(output);
    m_detailsText->append(QString("<span style='color: red;'>%1</span>").arg(text));
}

void ModelConversionDialog::parseProgressFromOutput(const QString& output)
{
    // Parse chunk progress (e.g., "21/4567" or "[21/4567]" or "Processing chunk 21 of 4567")
    static QRegularExpression chunkRegex(R"((\d+)\s*/\s*(\d+))");
    QRegularExpressionMatch chunkMatch = chunkRegex.match(output);
    if (chunkMatch.hasMatch()) {
        bool okCurrent, okTotal;
        int current = chunkMatch.captured(1).toInt(&okCurrent);
        int total = chunkMatch.captured(2).toInt(&okTotal);
        
        if (okCurrent && okTotal && total > 0) {
            m_chunksProcessed = current;
            m_totalChunks = total;
            updateProgressPercentage(current, total);
            return;  // Don't process other patterns if we found chunks
        }
    }
    
    // Look for key stage transitions in the script output
    if (output.contains("Cloning", Qt::CaseInsensitive)) {
        m_conversionStage = 1;
        m_progressBar->setValue(5);
        updateProgress("Cloning llama.cpp repository...");
    } else if (output.contains("Building", Qt::CaseInsensitive) || output.contains("cmake", Qt::CaseInsensitive)) {
        m_conversionStage = 2;
        m_progressBar->setValue(15);
        updateProgress("Building quantization tool...");
    } else if (output.contains("Converting", Qt::CaseInsensitive) || output.contains("quantize", Qt::CaseInsensitive)) {
        m_conversionStage = 3;
        m_progressBar->setValue(25);
        updateProgress("Converting model to " + m_recommendedType + "...");
    } else if (output.contains("Successfully", Qt::CaseInsensitive) || output.contains("Complete", Qt::CaseInsensitive)) {
        updateProgress("✓ Conversion completed! Verifying model...");
        m_progressBar->setValue(90);
    } else if (output.contains("Error", Qt::CaseInsensitive) || output.contains("Failed", Qt::CaseInsensitive)) {
        updateProgress("✗ Conversion error detected");
    } else if (output.contains("100%", Qt::CaseInsensitive) || output.contains("done", Qt::CaseInsensitive)) {
        m_progressBar->setValue(95);
    }
}

void ModelConversionDialog::updateProgressPercentage(int current, int total)
{
    if (total <= 0) return;
    
    // Calculate percentage for conversion stage (25% to 85% of total progress)
    // Stages before conversion: 0-25%, conversion: 25-85%, verification: 85-100%
    int conversionPercentage = (current * 100) / total;
    int overallPercentage = 25 + (conversionPercentage * 60 / 100);  // Map to 25-85% range
    
    m_progressBar->setValue(overallPercentage);
    
    // Update status with chunk info and ETA if available
    QString statusText = QString("Converting: %1/%2 chunks (%3%)")
        .arg(current)
        .arg(total)
        .arg(conversionPercentage);
    
    // Calculate ETA if we have timing data
    if (m_conversionStartTime > 0 && current > 0) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_conversionStartTime;
        qint64 estimatedTotal = (elapsed * total) / current;
        qint64 remaining = estimatedTotal - elapsed;
        
        if (remaining > 0) {
            int remainingMinutes = remaining / 60000;
            int remainingSeconds = (remaining % 60000) / 1000;
            statusText += QString(" - ETA: %1m %2s").arg(remainingMinutes).arg(remainingSeconds);
        }
    }
    
    m_statusLabel->setText(statusText);
    m_statusLabel->repaint();
}

void ModelConversionDialog::onTerminalFinished(int exitCode)
{
    if (exitCode == 0) {
        // Conversion likely succeeded, start verification timer
        updateProgress("Verifying converted model exists...");
        m_progressBar->setValue(85);
        
        // Check if model exists every 500ms for up to 10 seconds
        m_verifyTimer->start(500);
        QTimer::singleShot(10000, m_verifyTimer, &QTimer::stop);
    } else {
        // Conversion failed - log failure
        qint64 duration = QDateTime::currentMSecsSinceEpoch() - m_conversionStartTime;
        logConversionHistory(false, duration);
        
        updateProgress(QString("✗ Conversion process exited with code %1").arg(exitCode));
        m_statusLabel->setStyleSheet("color: #d9534f; font-weight: bold;");
        
        m_convertButton->setEnabled(true);
        m_convertButton->setVisible(true);
        m_cancelButton->setEnabled(true);
        m_cancelButton->setVisible(true);
        m_moreInfoButton->setEnabled(true);
        m_cancelConversionButton->setVisible(false);
        m_progressBar->setVisible(false);
        m_conversionInProgress = false;
    }
}

void ModelConversionDialog::onVerifyAndReload()
{
    if (verifyConvertedModelExists()) {
        m_verifyTimer->stop();
        
        // Log successful conversion
        qint64 duration = QDateTime::currentMSecsSinceEpoch() - m_conversionStartTime;
        logConversionHistory(true, duration);
        
        m_conversionInProgress = false;
        m_result = ConversionSucceeded;
        m_statusLabel->setText("✓ Model converted successfully! Reloading...");
        m_statusLabel->setStyleSheet("color: #5cb85c; font-weight: bold;");
        m_progressBar->setValue(100);
        m_progressBar->setVisible(false);
        m_cancelConversionButton->setVisible(false);
        
        // Close dialog after 2 seconds to allow model to reload
        QTimer::singleShot(2000, this, &QDialog::accept);
    }
}

bool ModelConversionDialog::verifyConvertedModelExists()
{
    // Construct expected converted model path
    QString basePath = m_modelPath;
    if (basePath.endsWith(".gguf", Qt::CaseInsensitive)) {
        basePath = basePath.left(basePath.length() - 5);
    }
    m_convertedPath = basePath + "_" + m_recommendedType + ".gguf";
    
    QFileInfo fileInfo(m_convertedPath);
    bool exists = fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0;
    
    if (exists) {
        updateProgress(QString("✓ Found converted model: %1 (%2 MB)").arg(
            m_convertedPath,
            QString::number(fileInfo.size() / 1024.0 / 1024.0, 'f', 1)
        ));
    }
    
    return exists;
}

void ModelConversionDialog::logConversionHistory(bool success, qint64 durationMs)
{
    // Get application data directory for log file
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appDataDir(appDataPath);
    if (!appDataDir.exists()) {
        appDataDir.mkpath(".");
    }
    
    QString logPath = appDataDir.filePath("model_conversion_history.log");
    QFile logFile(logPath);
    
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&logFile);
        
        // Format: [Timestamp] Status | Source: path | Target: type | Duration: Xm Ys | Chunks: X/Y
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        QString status = success ? "SUCCESS" : "FAILED";
        QString duration = QString("%1m %2s").arg(durationMs / 60000).arg((durationMs % 60000) / 1000);
        QString chunks = (m_totalChunks > 0) ? QString("%1/%2").arg(m_chunksProcessed).arg(m_totalChunks) : "N/A";
        
        stream << QString("[%1] %2 | Source: %3 | Target: %4 | Duration: %5 | Chunks: %6\n")
            .arg(timestamp)
            .arg(status)
            .arg(m_modelPath)
            .arg(m_recommendedType)
            .arg(duration)
            .arg(chunks);
        
        logFile.close();
    }
}

void ModelConversionDialog::closeEvent(QCloseEvent* event)
{
    if (m_conversionInProgress) {
        event->ignore();  // Don't close while conversion is running
        return;
    }
    event->accept();
}
