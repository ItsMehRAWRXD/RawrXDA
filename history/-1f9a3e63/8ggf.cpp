#include "blob_converter_panel.hpp"
#include "blob_to_gguf_converter.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QLabel>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDateTime>
#include <QProgressBar>
#include <QFileInfo>
#include <QDebug>

BlobConverterPanel::BlobConverterPanel(QWidget* parent)
    : QWidget(parent), m_converter(nullptr), m_isConverting(false)
{
    setWindowTitle("Blob to GGUF Converter");
    setAcceptDrops(true);
    
    m_converter = std::make_unique<BlobToGGUFConverter>(this);
    
    createLayout();
    populatePresets();
    
    // Connect converter signals
    connect(m_converter.get(), &BlobToGGUFConverter::progressUpdated,
            this, &BlobConverterPanel::onProgressUpdated);
    connect(m_converter.get(), &BlobToGGUFConverter::conversionComplete,
            this, &BlobConverterPanel::onConversionComplete);
    connect(m_converter.get(), &BlobToGGUFConverter::conversionError,
            this, &BlobConverterPanel::onConversionError);
    connect(m_converter.get(), &BlobToGGUFConverter::conversionCancelled,
            this, &BlobConverterPanel::onConversionCancelled);

    addLog("Blob to GGUF Converter initialized. Ready for conversion.");
}

BlobConverterPanel::~BlobConverterPanel()
{
}

void BlobConverterPanel::initialize()
{
    // Additional initialization if needed
}

void BlobConverterPanel::createLayout()
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ===== File Selection =====
    auto fileGroup = new QGroupBox("File Selection", this);
    auto fileLayout = new QVBoxLayout(fileGroup);

    // Blob input
    {
        auto row = new QHBoxLayout();
        row->addWidget(new QLabel("Blob File:"));
        m_blobPathEdit = new QLineEdit();
        m_blobPathEdit->setReadOnly(true);
        m_blobPathEdit->setPlaceholderText("Drag & drop blob file or click Browse");
        row->addWidget(m_blobPathEdit);
        m_selectBlobBtn = new QPushButton("Browse...");
        m_selectBlobBtn->setMaximumWidth(100);
        row->addWidget(m_selectBlobBtn);
        fileLayout->addLayout(row);

        connect(m_selectBlobBtn, &QPushButton::clicked, this, &BlobConverterPanel::onSelectBlobFile);
        connect(m_blobPathEdit, &QLineEdit::textChanged, this, &BlobConverterPanel::onBlobFileChanged);
    }

    // Blob info
    m_blobInfoLabel = new QLabel("No file selected");
    m_blobInfoLabel->setStyleSheet("color: gray; font-size: 10px;");
    fileLayout->addWidget(m_blobInfoLabel);

    // Output path
    {
        auto row = new QHBoxLayout();
        row->addWidget(new QLabel("Output GGUF:"));
        m_outputPathEdit = new QLineEdit();
        m_outputPathEdit->setPlaceholderText("Where to save the converted GGUF file");
        row->addWidget(m_outputPathEdit);
        m_selectOutputBtn = new QPushButton("Browse...");
        m_selectOutputBtn->setMaximumWidth(100);
        row->addWidget(m_selectOutputBtn);
        fileLayout->addLayout(row);

        connect(m_selectOutputBtn, &QPushButton::clicked, this, &BlobConverterPanel::onSelectOutputPath);
    }

    mainLayout->addWidget(fileGroup);

    // ===== Metadata Editor =====
    auto metaGroup = new QGroupBox("Model Metadata", this);
    auto metaLayout = new QVBoxLayout(metaGroup);

    // Preset selector
    {
        auto row = new QHBoxLayout();
        row->addWidget(new QLabel("Preset:"));
        m_presetCombo = new QComboBox();
        m_presetCombo->addItem("Custom");
        m_presetCombo->addItem("LLaMA 7B");
        m_presetCombo->addItem("LLaMA 13B");
        m_presetCombo->addItem("Mistral 7B");
        m_presetCombo->addItem("Phi-3");
        row->addWidget(m_presetCombo);
        row->addStretch();
        metaLayout->addLayout(row);

        connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &BlobConverterPanel::onPresetChanged);
    }

    // Model name
    {
        auto row = new QHBoxLayout();
        row->addWidget(new QLabel("Model Name:"));
        m_modelNameEdit = new QLineEdit();
        m_modelNameEdit->setPlaceholderText("e.g., llama-7b-converted");
        row->addWidget(m_modelNameEdit);
        metaLayout->addLayout(row);
    }

    // Architecture
    {
        auto row = new QHBoxLayout();
        row->addWidget(new QLabel("Architecture:"));
        m_architectureCombo = new QComboBox();
        m_architectureCombo->addItem("llama");
        m_architectureCombo->addItem("mistral");
        m_architectureCombo->addItem("mixtral");
        m_architectureCombo->addItem("qwen");
        m_architectureCombo->addItem("phi");
        row->addWidget(m_architectureCombo);
        row->addStretch();
        metaLayout->addLayout(row);
    }

    // Model dimensions grid
    {
        auto dimLayout = new QHBoxLayout();

        // Embedding dimension
        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("n_embed:"));
            m_nEmbedSpinBox = new QSpinBox();
            m_nEmbedSpinBox->setRange(128, 32768);
            m_nEmbedSpinBox->setValue(4096);
            col->addWidget(m_nEmbedSpinBox);
            dimLayout->addLayout(col);
        }

        // Layer count
        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("n_layer:"));
            m_nLayerSpinBox = new QSpinBox();
            m_nLayerSpinBox->setRange(1, 512);
            m_nLayerSpinBox->setValue(32);
            col->addWidget(m_nLayerSpinBox);
            dimLayout->addLayout(col);
        }

        // Vocab size
        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("n_vocab:"));
            m_nVocabSpinBox = new QSpinBox();
            m_nVocabSpinBox->setRange(1000, 200000);
            m_nVocabSpinBox->setValue(32000);
            col->addWidget(m_nVocabSpinBox);
            dimLayout->addLayout(col);
        }

        // Context length
        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("n_ctx:"));
            m_nCtxSpinBox = new QSpinBox();
            m_nCtxSpinBox->setRange(512, 131072);
            m_nCtxSpinBox->setValue(4096);
            col->addWidget(m_nCtxSpinBox);
            dimLayout->addLayout(col);
        }

        metaLayout->addLayout(dimLayout);
    }

    // Attention heads
    {
        auto row = new QHBoxLayout();

        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("n_head:"));
            m_nHeadSpinBox = new QSpinBox();
            m_nHeadSpinBox->setRange(1, 128);
            m_nHeadSpinBox->setValue(32);
            col->addWidget(m_nHeadSpinBox);
            row->addLayout(col);
        }

        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("n_head_kv:"));
            m_nHeadKvSpinBox = new QSpinBox();
            m_nHeadKvSpinBox->setRange(1, 128);
            m_nHeadKvSpinBox->setValue(8);
            col->addWidget(m_nHeadKvSpinBox);
            row->addLayout(col);
        }

        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("rope_freq_base:"));
            m_ropeFreqBaseSpinBox = new QDoubleSpinBox();
            m_ropeFreqBaseSpinBox->setRange(1.0, 1000000.0);
            m_ropeFreqBaseSpinBox->setValue(10000.0);
            col->addWidget(m_ropeFreqBaseSpinBox);
            row->addLayout(col);
        }

        {
            auto col = new QVBoxLayout();
            col->addWidget(new QLabel("rope_freq_scale:"));
            m_ropeFreqScaleSpinBox = new QDoubleSpinBox();
            m_ropeFreqScaleSpinBox->setRange(0.1, 10.0);
            m_ropeFreqScaleSpinBox->setValue(1.0);
            col->addWidget(m_ropeFreqScaleSpinBox);
            row->addLayout(col);
        }

        row->addStretch();
        metaLayout->addLayout(row);
    }

    mainLayout->addWidget(metaGroup);

    // ===== Conversion Options =====
    auto convGroup = new QGroupBox("Conversion Options", this);
    auto convLayout = new QVBoxLayout(convGroup);

    {
        auto row = new QHBoxLayout();
        row->addWidget(new QLabel("Quantization Type:"));
        m_quantTypeCombo = new QComboBox();
        m_quantTypeCombo->addItem("F32 (No compression)");
        m_quantTypeCombo->addItem("Q4_0 (Medium compression)");
        m_quantTypeCombo->addItem("Q5_K (High quality)");
        m_quantTypeCombo->addItem("Q8_0 (Lower compression)");
        row->addWidget(m_quantTypeCombo);
        row->addStretch();
        convLayout->addLayout(row);
    }

    mainLayout->addWidget(convGroup);

    // ===== Progress and Controls =====
    auto ctrlGroup = new QGroupBox("Conversion Control", this);
    auto ctrlLayout = new QVBoxLayout(ctrlGroup);

    // Estimated size
    {
        auto row = new QHBoxLayout();
        m_estimateSizeBtn = new QPushButton("Estimate Output Size");
        row->addWidget(m_estimateSizeBtn);
        m_estimatedSizeLabel = new QLabel("—");
        m_estimatedSizeLabel->setStyleSheet("color: blue;");
        row->addWidget(m_estimatedSizeLabel);
        row->addStretch();
        ctrlLayout->addLayout(row);

        connect(m_estimateSizeBtn, &QPushButton::clicked, this, &BlobConverterPanel::onEstimateSize);
    }

    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    ctrlLayout->addWidget(m_progressBar);

    m_progressLabel = new QLabel("Ready");
    m_progressLabel->setStyleSheet("font-size: 10px; color: gray;");
    ctrlLayout->addWidget(m_progressLabel);

    // Conversion buttons
    {
        auto row = new QHBoxLayout();
        m_startBtn = new QPushButton("Start Conversion");
        m_startBtn->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 5px;");
        m_startBtn->setMinimumHeight(40);
        row->addWidget(m_startBtn);

        m_cancelBtn = new QPushButton("Cancel");
        m_cancelBtn->setEnabled(false);
        m_cancelBtn->setMinimumHeight(40);
        row->addWidget(m_cancelBtn);

        ctrlLayout->addLayout(row);

        connect(m_startBtn, &QPushButton::clicked, this, &BlobConverterPanel::onStartConversion);
        connect(m_cancelBtn, &QPushButton::clicked, this, &BlobConverterPanel::onCancelConversion);
    }

    mainLayout->addWidget(ctrlGroup);

    // ===== Log Output =====
    auto logGroup = new QGroupBox("Conversion Log", this);
    auto logLayout = new QVBoxLayout(logGroup);
    m_logOutput = new QTextEdit();
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumHeight(150);
    m_logOutput->setStyleSheet("background-color: #f5f5f5; font-family: monospace; font-size: 9px;");
    logLayout->addWidget(m_logOutput);
    mainLayout->addWidget(logGroup);

    mainLayout->addStretch();
    setLayout(mainLayout);
}

void BlobConverterPanel::populatePresets()
{
    // Presets will be populated in createLayout
}

void BlobConverterPanel::onSelectBlobFile()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        "Select Blob File", "",
        "Blob Files (*.bin *.blob);;All Files (*)");

    if (!filePath.isEmpty()) {
        m_blobPathEdit->setText(filePath);
        m_lastBlobPath = filePath;
        
        // Auto-generate output name
        QFileInfo fileInfo(filePath);
        QString outputName = fileInfo.baseName() + ".gguf";
        m_outputPathEdit->setText(fileInfo.absolutePath() + "/" + outputName);
    }
}

void BlobConverterPanel::onSelectOutputPath()
{
    QString filePath = QFileDialog::getSaveFileName(this,
        "Save GGUF File", "",
        "GGUF Files (*.gguf);;All Files (*)");

    if (!filePath.isEmpty()) {
        m_outputPathEdit->setText(filePath);
    }
}

void BlobConverterPanel::onBlobFileChanged(const QString& path)
{
    if (path.isEmpty()) {
        m_blobInfoLabel->setText("No file selected");
        return;
    }

    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        m_blobInfoLabel->setText(QString("File not found: %1").arg(path));
        m_blobInfoLabel->setStyleSheet("color: red;");
        return;
    }

    qint64 sizeKB = fileInfo.size() / 1024;
    m_blobInfoLabel->setText(QString("Size: %1 KB | Modified: %2")
        .arg(sizeKB)
        .arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss")));
    m_blobInfoLabel->setStyleSheet("color: gray; font-size: 10px;");
}

void BlobConverterPanel::onPresetChanged(int index)
{
    loadPresetMetadata(m_presetCombo->currentText());
}

void BlobConverterPanel::loadPresetMetadata(const QString& presetName)
{
    if (presetName == "Custom") {
        return;
    }

    // LLaMA 7B
    if (presetName == "LLaMA 7B") {
        m_modelNameEdit->setText("llama-7b");
        m_architectureCombo->setCurrentText("llama");
        m_nEmbedSpinBox->setValue(4096);
        m_nLayerSpinBox->setValue(32);
        m_nVocabSpinBox->setValue(32000);
        m_nCtxSpinBox->setValue(2048);
        m_nHeadSpinBox->setValue(32);
        m_nHeadKvSpinBox->setValue(8);
    }
    // LLaMA 13B
    else if (presetName == "LLaMA 13B") {
        m_modelNameEdit->setText("llama-13b");
        m_architectureCombo->setCurrentText("llama");
        m_nEmbedSpinBox->setValue(5120);
        m_nLayerSpinBox->setValue(40);
        m_nVocabSpinBox->setValue(32000);
        m_nCtxSpinBox->setValue(2048);
        m_nHeadSpinBox->setValue(40);
        m_nHeadKvSpinBox->setValue(40);
    }
    // Mistral 7B
    else if (presetName == "Mistral 7B") {
        m_modelNameEdit->setText("mistral-7b");
        m_architectureCombo->setCurrentText("mistral");
        m_nEmbedSpinBox->setValue(4096);
        m_nLayerSpinBox->setValue(32);
        m_nVocabSpinBox->setValue(32768);
        m_nCtxSpinBox->setValue(8192);
        m_nHeadSpinBox->setValue(32);
        m_nHeadKvSpinBox->setValue(8);
    }
    // Phi-3
    else if (presetName == "Phi-3") {
        m_modelNameEdit->setText("phi-3");
        m_architectureCombo->setCurrentText("phi");
        m_nEmbedSpinBox->setValue(3072);
        m_nLayerSpinBox->setValue(32);
        m_nVocabSpinBox->setValue(32064);
        m_nCtxSpinBox->setValue(4096);
        m_nHeadSpinBox->setValue(32);
        m_nHeadKvSpinBox->setValue(32);
    }
}

void BlobConverterPanel::onStartConversion()
{
    if (m_blobPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a blob file first.");
        return;
    }

    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please specify an output file path.");
        return;
    }

    m_isConverting = true;
    setControlsEnabled(false);
    m_cancelBtn->setEnabled(true);

    // Load blob file
    if (!m_converter->loadBlobFile(m_blobPathEdit->text())) {
        m_isConverting = false;
        setControlsEnabled(true);
        return;
    }

    // Set metadata
    m_converter->setMetadata(getMetadataFromUI());

    // Parse blob
    if (!m_converter->parseBlob(256)) {
        addLog("Failed to parse blob file", true);
        m_isConverting = false;
        setControlsEnabled(true);
        return;
    }

    addLog(QString("Parsed blob: %1 tensors detected").arg(m_converter->getProgress().totalTensors));

    // Start conversion
    if (m_converter->convertToGGUF(m_outputPathEdit->text())) {
        addLog("Conversion queued");
    } else {
        m_isConverting = false;
        setControlsEnabled(true);
    }
}

void BlobConverterPanel::onCancelConversion()
{
    m_converter->cancelConversion();
    addLog("Cancellation requested...", true);
}

void BlobConverterPanel::onProgressUpdated(const ConversionProgress& progress)
{
    m_progressBar->setValue(static_cast<int>(progress.percentComplete));
    m_progressLabel->setText(QString("Converting: %1/%2 tensors | %3%")
        .arg(progress.processedTensors)
        .arg(progress.totalTensors)
        .arg(static_cast<int>(progress.percentComplete)));
}

void BlobConverterPanel::onConversionComplete(const QString& outputPath)
{
    m_isConverting = false;
    setControlsEnabled(true);
    m_cancelBtn->setEnabled(false);

    QFileInfo fileInfo(outputPath);
    addLog(QString("✓ Conversion complete! Output: %1 (%2 MB)")
        .arg(fileInfo.fileName())
        .arg(fileInfo.size() / (1024.0 * 1024.0), 0, 'f', 2));

    QMessageBox::information(this, "Success",
        QString("Conversion completed successfully!\n\nOutput: %1").arg(outputPath));
}

void BlobConverterPanel::onConversionError(const QString& errorMessage)
{
    m_isConverting = false;
    setControlsEnabled(true);
    m_cancelBtn->setEnabled(false);

    addLog(QString("✗ Error: %1").arg(errorMessage), true);
    QMessageBox::critical(this, "Conversion Error", errorMessage);
}

void BlobConverterPanel::onConversionCancelled()
{
    m_isConverting = false;
    setControlsEnabled(true);
    m_cancelBtn->setEnabled(false);

    addLog("Conversion cancelled by user", true);
}

void BlobConverterPanel::onEstimateSize()
{
    if (m_blobPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a blob file first.");
        return;
    }

    if (!m_converter->loadBlobFile(m_blobPathEdit->text())) {
        return;
    }

    m_converter->setMetadata(getMetadataFromUI());

    if (!m_converter->parseBlob(256)) {
        QMessageBox::warning(this, "Error", "Failed to analyze blob file.");
        return;
    }

    qint64 estimatedSize = m_converter->getEstimatedGGUFSize();
    m_estimatedSizeLabel->setText(QString("~%1 MB").arg(estimatedSize / (1024.0 * 1024.0), 0, 'f', 2));
    addLog(QString("Estimated output size: %1 MB").arg(estimatedSize / (1024.0 * 1024.0), 0, 'f', 2));
}

GGUFMetadata BlobConverterPanel::getMetadataFromUI() const
{
    GGUFMetadata metadata;
    metadata.modelName = m_modelNameEdit->text();
    if (metadata.modelName.isEmpty()) {
        metadata.modelName = "converted-model";
    }
    metadata.modelArchitecture = m_architectureCombo->currentText();
    metadata.nEmbed = m_nEmbedSpinBox->value();
    metadata.nLayer = m_nLayerSpinBox->value();
    metadata.nVocab = m_nVocabSpinBox->value();
    metadata.nCtx = m_nCtxSpinBox->value();
    metadata.nHead = m_nHeadSpinBox->value();
    metadata.nHeadKv = m_nHeadKvSpinBox->value();
    metadata.ropeFreqBase = m_ropeFreqBaseSpinBox->value();
    metadata.ropeFreqScale = m_ropeFreqScaleSpinBox->value();
    return metadata;
}

void BlobConverterPanel::addLog(const QString& message, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = isError ? "red" : "black";
    QString logLine = QString("<span style='color: %1;'>[%2] %3</span>")
        .arg(color)
        .arg(timestamp)
        .arg(message.toHtmlEscaped());

    m_logOutput->append(logLine);

    // Auto-scroll to bottom
    QTextCursor cursor(m_logOutput->document());
    cursor.movePosition(QTextCursor::End);
    m_logOutput->setTextCursor(cursor);
}

void BlobConverterPanel::setControlsEnabled(bool enabled)
{
    m_selectBlobBtn->setEnabled(enabled);
    m_selectOutputBtn->setEnabled(enabled);
    m_presetCombo->setEnabled(enabled);
    m_modelNameEdit->setEnabled(enabled);
    m_architectureCombo->setEnabled(enabled);
    m_nEmbedSpinBox->setEnabled(enabled);
    m_nLayerSpinBox->setEnabled(enabled);
    m_nVocabSpinBox->setEnabled(enabled);
    m_nCtxSpinBox->setEnabled(enabled);
    m_nHeadSpinBox->setEnabled(enabled);
    m_nHeadKvSpinBox->setEnabled(enabled);
    m_ropeFreqBaseSpinBox->setEnabled(enabled);
    m_ropeFreqScaleSpinBox->setEnabled(enabled);
    m_quantTypeCombo->setEnabled(enabled);
    m_estimateSizeBtn->setEnabled(enabled);
    m_startBtn->setEnabled(enabled);
}

void BlobConverterPanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BlobConverterPanel::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QString filePath = mimeData->urls().first().toLocalFile();
        m_blobPathEdit->setText(filePath);
        onSelectBlobFile();  // Trigger auto-output path generation
        event->acceptProposedAction();
    }
}
