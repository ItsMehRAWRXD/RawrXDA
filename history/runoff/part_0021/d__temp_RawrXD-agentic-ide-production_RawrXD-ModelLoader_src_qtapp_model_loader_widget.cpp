#include "model_loader_widget.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include "deflate_brutal_qt.hpp"
#include <QDebug>
#include <QFile>
#include <QTextStream>

ModelLoaderWidget::ModelLoaderWidget(QWidget* parent)
    : QWidget(parent), m_loading(false), m_compressing(false), m_caching(false)
{
    setupUI();
    setupConnections();
    
    // Initialize async watchers
    m_loadWatcher = new QFutureWatcher<bool>(this);
    m_compressWatcher = new QFutureWatcher<QByteArray>(this);
    m_cacheWatcher = new QFutureWatcher<void>(this);
    
    connect(m_loadWatcher, &QFutureWatcher<bool>::finished, this, &ModelLoaderWidget::onModelLoadFinished);
    connect(m_compressWatcher, &QFutureWatcher<QByteArray>::finished, this, &ModelLoaderWidget::onCompressionFinished);
    
    qDebug() << "[ModelLoaderWidget] Created";

#ifdef TEST_BRUTAL
    // 4-KB dummy tensor round-trip using MASM brutal codec
    std::vector<uint8_t> original(4096, 0x42);
    for (size_t i = 0; i < original.size(); ++i) original[i] = static_cast<uint8_t>(i & 0xFF);

    size_t compBound = brutal::compressBound(original.size());
    std::vector<uint8_t> compressed(compBound);
    size_t compLen = brutal::compress(compressed.data(), compBound,
                                      original.data(), original.size());

    std::vector<uint8_t> decompressed(original.size());
    size_t decompLen = brutal::decompress(decompressed.data(), original.size(),
                                          compressed.data(), compLen);

    const double ratio = (100.0 * static_cast<double>(compLen) / static_cast<double>(original.size()));
    const bool match = (original.size() == decompLen && original == decompressed);
    qDebug() << "[Brutal] ratio" << ratio << "%  match" << match;
    QFile f("brutal_poc.log");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts.setRealNumberNotation(QTextStream::FixedNotation);
        ts.setRealNumberPrecision(2);
        ts << "[Brutal] ratio " << ratio << " %  match " << (match ? "true" : "false") << "\n";
        f.close();
    }
#endif
}

ModelLoaderWidget::~ModelLoaderWidget()
{
    qDebug() << "[ModelLoaderWidget] Destroyed";
}

void ModelLoaderWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Model selection section
    QHBoxLayout* modelLayout = new QHBoxLayout();
    QLabel* modelLabel = new QLabel("Model:", this);
    m_modelSelector = new QComboBox(this);
    m_modelSelector->setMinimumWidth(300);
    m_loadButton = new QPushButton("Load", this);
    m_unloadButton = new QPushButton("Unload", this);
    m_unloadButton->setEnabled(false);
    
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(m_modelSelector, 1);
    modelLayout->addWidget(m_loadButton);
    modelLayout->addWidget(m_unloadButton);
    
    // Tensor selection section
    QHBoxLayout* tensorLayout = new QHBoxLayout();
    QLabel* tensorLabel = new QLabel("Tensor:", this);
    m_tensorSelector = new QComboBox(this);
    m_tensorSelector->setMinimumWidth(300);
    m_compressButton = new QPushButton("Compress", this);
    m_decompressButton = new QPushButton("Decompress", this);
    m_compressButton->setEnabled(false);
    m_decompressButton->setEnabled(false);
    
    tensorLayout->addWidget(tensorLabel);
    tensorLayout->addWidget(m_tensorSelector, 1);
    tensorLayout->addWidget(m_compressButton);
    tensorLayout->addWidget(m_decompressButton);
    
    // Cache operations section
    QHBoxLayout* cacheLayout = new QHBoxLayout();
    m_cacheAllButton = new QPushButton("Cache All Tensors", this);
    m_clearCacheButton = new QPushButton("Clear Cache", this);
    m_cacheAllButton->setEnabled(false);
    m_clearCacheButton->setEnabled(false);
    
    cacheLayout->addWidget(m_cacheAllButton);
    cacheLayout->addWidget(m_clearCacheButton);
    cacheLayout->addStretch();
    
    // Progress bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    
    // Info display
    m_infoDisplay = new QTextEdit(this);
    m_infoDisplay->setReadOnly(true);
    m_infoDisplay->setMaximumHeight(150);
    
    // Stats table
    m_statsTable = new QTableWidget(this);
    m_statsTable->setColumnCount(5);
    m_statsTable->setHorizontalHeaderLabels({"Tensor", "Original Size", "Compressed Size", "Ratio", "Time (ms)"});
    m_statsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_statsTable->setMaximumHeight(200);
    
    // Assembly
    mainLayout->addLayout(modelLayout);
    mainLayout->addLayout(tensorLayout);
    mainLayout->addLayout(cacheLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_infoDisplay);
    mainLayout->addWidget(m_statsTable);
    
    setLayout(mainLayout);
}

void ModelLoaderWidget::setupConnections()
{
    connect(m_loadButton, &QPushButton::clicked, this, &ModelLoaderWidget::onLoadButtonClicked);
    connect(m_unloadButton, &QPushButton::clicked, this, &ModelLoaderWidget::onUnloadButtonClicked);
    connect(m_tensorSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ModelLoaderWidget::onTensorSelected);
    connect(m_compressButton, &QPushButton::clicked, this, &ModelLoaderWidget::onCompressButtonClicked);
    connect(m_decompressButton, &QPushButton::clicked, this, &ModelLoaderWidget::onDecompressButtonClicked);
    connect(m_cacheAllButton, &QPushButton::clicked, this, &ModelLoaderWidget::onCacheAllButtonClicked);
    connect(m_clearCacheButton, &QPushButton::clicked, this, &ModelLoaderWidget::onClearCacheButtonClicked);
}

void ModelLoaderWidget::onModelSelected(const QString& modelPath)
{
    if (modelPath.isEmpty()) {
        return;
    }
    m_currentModelPath = modelPath;
    m_infoDisplay->append("Selected model: " + QFileInfo(modelPath).fileName());
    loadModel(modelPath);
}

void ModelLoaderWidget::loadModel(const QString& modelPath)
{
    if (m_loading) {
        qWarning() << "[ModelLoaderWidget] Already loading a model";
        return;
    }
    
    m_loading = true;
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    // Load model asynchronously
    QtConcurrent::run([this, modelPath]() {
        loadModelAsync(modelPath);
    });
}

void ModelLoaderWidget::loadModelAsync(const QString& modelPath)
{
    try {
        m_modelLoader = std::make_unique<ModelLoaderWithCompression>(modelPath);
        m_modelLoader->loadModel();
        QMetaObject::invokeMethod(this, "onModelLoadFinished", Qt::QueuedConnection);
    } catch (const std::exception& e) {
        qCritical() << "[ModelLoaderWidget] Async load error:" << e.what();
        QMetaObject::invokeMethod(this, "onModelLoadFinished", Qt::QueuedConnection);
    }
}

void ModelLoaderWidget::onModelLoadFinished()
{
    m_loading = false;
    m_progressBar->setVisible(false);
    
    bool success = m_modelLoader != nullptr;
    
    if (success) {
        m_loadButton->setEnabled(false);
        m_unloadButton->setEnabled(true);
        m_cacheAllButton->setEnabled(true);
        m_clearCacheButton->setEnabled(true);
        
        updateModelInfo();
        updateTensorList();
        
        emit modelLoaded(m_currentModelPath);
        
        m_infoDisplay->setPlainText("Model loaded successfully!");
    } else {
        m_modelLoader.reset();
        emit errorOccurred("Failed to load model");
        m_infoDisplay->setPlainText("Failed to load model");
    }
}

void ModelLoaderWidget::unloadModel()
{
    m_modelLoader.reset();
    m_loadButton->setEnabled(true);
    m_unloadButton->setEnabled(false);
    m_compressButton->setEnabled(false);
    m_decompressButton->setEnabled(false);
    m_cacheAllButton->setEnabled(false);
    m_clearCacheButton->setEnabled(false);
    
    m_tensorSelector->clear();
    m_infoDisplay->clear();
    m_statsTable->setRowCount(0);
    
    emit modelUnloaded();
}

void ModelLoaderWidget::onLoadButtonClicked()
{
    QString modelPath = QFileDialog::getOpenFileName(this, 
        "Select GGUF Model", 
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation), 
        "GGUF Files (*.gguf)");
    
    if (!modelPath.isEmpty()) {
        m_currentModelPath = modelPath;
        loadModel(modelPath);
    }
}

void ModelLoaderWidget::onUnloadButtonClicked()
{
    unloadModel();
}

void ModelLoaderWidget::onTensorSelected(int index)
{
    if (index >= 0 && m_modelLoader) {
        m_currentTensor = m_tensorSelector->itemText(index);
        m_compressButton->setEnabled(true);
        m_decompressButton->setEnabled(true);
    } else {
        m_compressButton->setEnabled(false);
        m_decompressButton->setEnabled(false);
    }
}

void ModelLoaderWidget::compressSelectedTensor()
{
    if (m_currentTensor.isEmpty() || !m_modelLoader) {
        return;
    }
    
    m_compressing = true;
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    QtConcurrent::run([this]() {
        compressTensorAsync(m_currentTensor);
    });
}

QByteArray ModelLoaderWidget::compressTensorAsync(const QString& tensorName)
{
    QByteArray result = m_modelLoader->compressTensor(tensorName);
    QMetaObject::invokeMethod(this, "onCompressionFinished", Qt::QueuedConnection);
    return result;
}

void ModelLoaderWidget::onCompressionFinished()
{
    m_compressing = false;
    m_progressBar->setVisible(false);
    
    if (m_modelLoader) {
        updateCompressionStats();
        
        auto stats = m_modelLoader->getCompressionStats(m_currentTensor);
        emit compressionCompleted(m_currentTensor, stats.compressionRatio);
        
        m_infoDisplay->append(QString("Compressed %1: %2% reduction")
            .arg(m_currentTensor)
            .arg(stats.compressionRatio * 100, 0, 'f', 1));
    } else {
        emit errorOccurred("Compression failed");
        m_infoDisplay->append("Compression failed for " + m_currentTensor);
    }
}

void ModelLoaderWidget::onCompressButtonClicked()
{
    compressSelectedTensor();
}

void ModelLoaderWidget::onDecompressButtonClicked()
{
    if (m_currentTensor.isEmpty() || !m_modelLoader) {
        return;
    }
    
    // For demonstration - load tensor with decompression
    QByteArray tensorData = m_modelLoader->loadTensor(m_currentTensor, true);
    
    if (!tensorData.isEmpty()) {
        emit tensorLoaded(m_currentTensor, tensorData);
        m_infoDisplay->append("Loaded tensor: " + m_currentTensor);
    } else {
        emit errorOccurred("Failed to load tensor");
        m_infoDisplay->append("Failed to load tensor: " + m_currentTensor);
    }
}

void ModelLoaderWidget::onCacheAllButtonClicked()
{
    if (!m_modelLoader) {
        return;
    }
    
    m_caching = true;
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, m_modelLoader->getTensorNames().size());
    m_progressBar->setValue(0);
    
    QFuture<void> future = QtConcurrent::run([this]() {
        cacheTensorsAsync();
    });
    
    m_cacheWatcher->setFuture(future);
}

void ModelLoaderWidget::cacheTensorsAsync()
{
    if (!m_modelLoader) {
        return;
    }
    
    QStringList tensors = m_modelLoader->getTensorNames();
    int progress = 0;
    
    for (const QString& tensor : tensors) {
        m_modelLoader->cacheTensor(tensor, m_modelLoader->loadTensor(tensor, false));
        progress++;
        
        // Update progress (thread-safe)
        QMetaObject::invokeMethod(this, [this, progress]() {
            m_progressBar->setValue(progress);
        });
    }
}

void ModelLoaderWidget::onClearCacheButtonClicked()
{
    if (!m_modelLoader) {
        return;
    }
    
    // Clear compression stats
    m_statsTable->setRowCount(0);
    m_infoDisplay->append("Cache cleared");
}

void ModelLoaderWidget::updateModelInfo()
{
    if (!m_modelLoader) {
        return;
    }
    
    auto metadata = m_modelLoader->getModelMetadata();
    QString info = QString("Model: %1\nSize: %2 MB\nTensors: %3")
        .arg(QFileInfo(m_currentModelPath).fileName())
        .arg(m_modelLoader->getModelSize() / (1024.0 * 1024.0), 0, 'f', 1)
        .arg(m_modelLoader->getTensorNames().size());
    
    m_infoDisplay->setPlainText(info);
}

void ModelLoaderWidget::updateTensorList()
{
    if (!m_modelLoader) {
        return;
    }
    
    m_tensorSelector->clear();
    m_tensorSelector->addItems(m_modelLoader->getTensorNames());
}

void ModelLoaderWidget::updateCompressionStats()
{
    if (!m_modelLoader) {
        return;
    }
    
    // This would update the stats table with compression results
    // Implementation depends on specific stats tracking
}



// MOC is handled by CMake AUTOMOC for model_loader_widget.hpp



