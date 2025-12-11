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
    m_progressBar->setRange(0, 0);  // Indeterminate progress
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
            "Please wait for the conversion to complete before cancelling.");
        return;
    }
    m_result = Cancelled;
    reject();
}

void ModelConversionDialog::onConvertClicked()
{
    m_convertButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
    m_moreInfoButton->setEnabled(false);
    
    m_progressBar->setVisible(true);
    m_statusLabel->setVisible(true);
    m_conversionInProgress = true;
    
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
    // Look for key stage transitions in the script output
    if (output.contains("Cloning", Qt::CaseInsensitive)) {
        m_conversionStage = 1;
        updateProgress("Cloning llama.cpp repository...");
    } else if (output.contains("Building", Qt::CaseInsensitive) || output.contains("cmake", Qt::CaseInsensitive)) {
        m_conversionStage = 2;
        updateProgress("Building quantization tool...");
    } else if (output.contains("Converting", Qt::CaseInsensitive) || output.contains("quantize", Qt::CaseInsensitive)) {
        m_conversionStage = 3;
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
        // Conversion failed
        updateProgress(QString("✗ Conversion process exited with code %1").arg(exitCode));
        m_statusLabel->setStyleSheet("color: #d9534f; font-weight: bold;");
        
        m_convertButton->setEnabled(true);
        m_cancelButton->setEnabled(true);
        m_moreInfoButton->setEnabled(true);
        m_progressBar->setVisible(false);
        m_conversionInProgress = false;
    }
}

void ModelConversionDialog::onVerifyAndReload()
{
    if (verifyConvertedModelExists()) {
        m_verifyTimer->stop();
        
        m_conversionInProgress = false;
        m_result = ConversionSucceeded;
        m_statusLabel->setText("✓ Model converted successfully! Reloading...");
        m_statusLabel->setStyleSheet("color: #5cb85c; font-weight: bold;");
        m_progressBar->setValue(100);
        m_progressBar->setVisible(false);
        
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

void ModelConversionDialog::onTerminalOutput(const QString& output)
{
    // Legacy overload for string output - deprecated in favor of QByteArray version
    m_detailsText->append(output);
}

void ModelConversionDialog::closeEvent(QCloseEvent* event)
{
    if (m_conversionInProgress) {
        event->ignore();  // Don't close while conversion is running
        return;
    }
    event->accept();
}
