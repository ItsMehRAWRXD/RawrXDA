#include "model_loader_widget.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include "deflate_brutal_qt.hpp"
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <windows.h>
#include <psapi.h>
#include <QProcess>
#include "utils/model_metadata_utils.hpp"

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
    
    // Backend selection section
    QHBoxLayout* backendLayout = new QHBoxLayout();
    QLabel* backendLabel = new QLabel("Backend:", this);
    m_backendSelector = new QComboBox(this);
    m_backendSelector->setMinimumWidth(200);
    m_backendSelector->addItem("Standard GGUF Loader", "standard");
    m_backendSelector->addItem("MASM Dual Engine FP32", "masm_fp32");
    m_backendSelector->addItem("MASM Dual Engine Quantized", "masm_quantized");
    m_backendSelector->addItem("MASM Sliding Door", "masm_sliding");
    m_backendSelector->addItem("MASM Hotpatch", "masm_hotpatch");
    
    backendLayout->addWidget(backendLabel);
    backendLayout->addWidget(m_backendSelector, 1);
    
    // Model selection section
    QHBoxLayout* modelLayout = new QHBoxLayout();
    QLabel* modelLabel = new QLabel("Model:", this);
    m_modelSelector = new QComboBox(this);
    m_modelSelector->setMinimumWidth(300);
    m_refreshModelsButton = new QPushButton("🔄", this);
    m_refreshModelsButton->setMaximumWidth(30);
    m_refreshModelsButton->setToolTip("Refresh model list");
    m_loadButton = new QPushButton("Load", this);
    m_unloadButton = new QPushButton("Unload", this);
    m_unloadButton->setEnabled(false);
    
    modelLayout->addWidget(modelLabel);
    modelLayout->addWidget(m_modelSelector, 1);
    modelLayout->addWidget(m_refreshModelsButton);
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
    
    // RAM usage display
    QHBoxLayout* ramLayout = new QHBoxLayout();
    QLabel* ramLabel = new QLabel("RAM Usage:", this);
    m_ramUsageLabel = new QLabel("0 MB", this);
    m_ramUsageLabel->setStyleSheet("QLabel { background-color: #2d2d30; color: #4ec9b0; padding: 2px 8px; border-radius: 3px; }");
    m_ramUsageLabel->setMinimumWidth(100);
    m_ramUsageLabel->setAlignment(Qt::AlignCenter);
    
    ramLayout->addWidget(ramLabel);
    ramLayout->addWidget(m_ramUsageLabel);
    ramLayout->addStretch();
    
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
    mainLayout->addLayout(backendLayout);
    mainLayout->addLayout(modelLayout);
    mainLayout->addLayout(tensorLayout);
    mainLayout->addLayout(cacheLayout);
    mainLayout->addLayout(ramLayout);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_infoDisplay);
    mainLayout->addWidget(m_statsTable);
    
    setLayout(mainLayout);
}

void ModelLoaderWidget::setupConnections()
{
    connect(m_backendSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelLoaderWidget::onBackendSelected);
        connect(m_modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ModelLoaderWidget::onModelComboChanged);
        connect(m_refreshModelsButton, &QPushButton::clicked, this, &ModelLoaderWidget::refreshModels);
    connect(m_loadButton, &QPushButton::clicked, this, &ModelLoaderWidget::onLoadButtonClicked);
    connect(m_unloadButton, &QPushButton::clicked, this, &ModelLoaderWidget::onUnloadButtonClicked);
    connect(m_tensorSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &ModelLoaderWidget::onTensorSelected);
    connect(m_compressButton, &QPushButton::clicked, this, &ModelLoaderWidget::onCompressButtonClicked);
    connect(m_decompressButton, &QPushButton::clicked, this, &ModelLoaderWidget::onDecompressButtonClicked);
    connect(m_cacheAllButton, &QPushButton::clicked, this, &ModelLoaderWidget::onCacheAllButtonClicked);
    connect(m_clearCacheButton, &QPushButton::clicked, this, &ModelLoaderWidget::onClearCacheButtonClicked);
    
    // RAM usage timer
    m_ramTimer = new QTimer(this);
    connect(m_ramTimer, &QTimer::timeout, this, &ModelLoaderWidget::updateRamUsage);
    m_ramTimer->start(1000); // Update every second

    // Initial model list refresh
    QTimer::singleShot(0, this, &ModelLoaderWidget::refreshModels);
}

void ModelLoaderWidget::refreshModels()
{
    QString current = m_modelSelector->currentData().toString();
    m_modelSelector->clear();
    m_modelSelector->addItem("No Model Selected", "");

    QSet<QString> seen;

    // Search local GGUF directories
    QStringList searchDirs = {
        QDir::currentPath() + "/models",
        QDir::homePath() + "/models",
        QString("D:/OllamaModels")
    };

    for (const QString& dirPath : searchDirs) {
        QDir d(dirPath);
        if (!d.exists()) continue;
        QDirIterator it(dirPath, QStringList() << "*.gguf", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString file = it.next();
            QString display = QFileInfo(file).fileName();
            if (seen.contains(file)) continue;
            m_modelSelector->addItem(display, file);
            // Build initial tooltip (heuristics) and schedule enrichment
            ensureTooltipForModelData(file, file);
            seen.insert(file);
        }
    }

    // Add Ollama models via `ollama list` (non-blocking but short timeout)
    QProcess ollamaProcess;
    ollamaProcess.start("ollama", QStringList() << "list");
    if (ollamaProcess.waitForStarted(2000) && ollamaProcess.waitForFinished(4000)) {
        QString output = QString::fromUtf8(ollamaProcess.readAllStandardOutput());
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        for (int i = 1; i < lines.size(); ++i) {
            QString line = lines[i].trimmed();
            if (line.isEmpty()) continue;
            QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.isEmpty()) continue;
            QString modelName = parts[0];
            QString key = QString("ollama:%1").arg(modelName);
            if (seen.contains(key)) continue;
            m_modelSelector->addItem(QString("[Ollama] %1").arg(modelName), key);
            // If a GGUF can be resolved for this Ollama model, use it to enrich the tooltip
            QString resolved = resolveOllamaGgufPath(modelName);
            ensureTooltipForModelData(key, resolved);
            seen.insert(key);
        }
    } else {
        qDebug() << "[ModelLoaderWidget] Ollama list not available or timed out";
    }

    // Try to restore selection
    int idx = m_modelSelector->findData(current);
    if (idx >= 0) m_modelSelector->setCurrentIndex(idx);
}

void ModelLoaderWidget::ensureTooltipForModelData(const QString& dataKey, const QString& resolvedPath)
{
    // If we already have a cached tooltip, apply it to matching items
    if (m_modelTooltipCache.contains(dataKey)) {
        QString tt = m_modelTooltipCache.value(dataKey);
        for (int i = 0; i < m_modelSelector->count(); ++i) {
            if (m_modelSelector->itemData(i).toString() == dataKey) {
                m_modelSelector->setItemData(i, tt, Qt::ToolTipRole);
            }
        }
        return;
    }

    // Build heuristic tooltip synchronously: from filename or key
    QString heurHtml;
    QString name;
    QString quant;
    QString params;
    double sizeMB = 0.0;

    if (!resolvedPath.isEmpty() && QFile::exists(resolvedPath)) {
        QFileInfo fi(resolvedPath);
        name = fi.fileName();
        quant = model_metadata::parseQuantFromFilename(fi.fileName());
        auto p = model_metadata::parseParamCountFromFilename(fi.fileName());
        if (p.has_value()) params = model_metadata::formatParamCount(*p);
        sizeMB = fi.size() / (1024.0 * 1024.0);
    } else {
        // Fall back to parsing the key/display
        QString key = dataKey;
        if (key.startsWith("ollama:")) key = key.mid(7);
        name = key;
        quant = model_metadata::parseQuantFromFilename(key);
        auto p2 = model_metadata::parseParamCountFromFilename(key);
        if (p2.has_value()) params = model_metadata::formatParamCount(*p2);
    }

    heurHtml = QString("<b>%1</b><br>").arg(name.toHtmlEscaped());
    if (!quant.isEmpty()) heurHtml += QString("Quantization: <b>%1</b><br>").arg(quant.toHtmlEscaped());
    if (!params.isEmpty()) heurHtml += QString("Params: <b>%1</b><br>").arg(params.toHtmlEscaped());
    if (sizeMB > 0.0) heurHtml += QString("Size: <b>%1 MB</b><br>").arg(QString::number(sizeMB, 'f', 1));
    heurHtml += QString("<i>Tooltip will be enriched if GGUF metadata is available.</i>");

    // Cache and apply the heuristic tooltip
    m_modelTooltipCache.insert(dataKey, heurHtml);
    for (int i = 0; i < m_modelSelector->count(); ++i) {
        if (m_modelSelector->itemData(i).toString() == dataKey) {
            m_modelSelector->setItemData(i, heurHtml, Qt::ToolTipRole);
            m_modelSelector->setItemData(i, heurHtml, Qt::UserRole + 1); // accessible text fallback
        }
    }

    // If we have a resolved GGUF path, run metadata extraction asynchronously
    if (!resolvedPath.isEmpty() && QFile::exists(resolvedPath)) {
        QString pathCopy = resolvedPath;
        QString keyCopy = dataKey;
        QtConcurrent::run([this, pathCopy, keyCopy]() {
            auto metaOpt = model_metadata::extractGgufMetadata(pathCopy);
            QString enriched;
            if (metaOpt.has_value()) {
                auto m = *metaOpt;
                enriched = QString("<b>%1</b><br>")
                    .arg(m.name.toHtmlEscaped());
                if (!m.quantization.isEmpty()) enriched += QString("Quantization: <b>%1</b><br>").arg(m.quantization.toHtmlEscaped());
                if (m.parameterCount.has_value()) enriched += QString("Params: <b>%1</b><br>").arg(model_metadata::formatParamCount(*m.parameterCount));
                if (!m.architecture.isEmpty()) enriched += QString("Arch: <b>%1</b><br>").arg(m.architecture.toHtmlEscaped());
                enriched += QString("Size: <b>%1 MB</b><br>").arg(QString::number(m.sizeMB, 'f', 1));
            } else {
                enriched = QString();
            }

            QMetaObject::invokeMethod(this, [this, keyCopy, enriched]() {
                if (!enriched.isEmpty()) {
                    m_modelTooltipCache.insert(keyCopy, enriched);
                    for (int j = 0; j < m_modelSelector->count(); ++j) {
                        if (m_modelSelector->itemData(j).toString() == keyCopy) {
                            m_modelSelector->setItemData(j, enriched, Qt::ToolTipRole);
                            m_modelSelector->setItemData(j, enriched, Qt::UserRole + 1);
                        }
                    }
                }
            }, Qt::QueuedConnection);
        });
    }
}

QString ModelLoaderWidget::resolveOllamaGgufPath(const QString& modelName)
{
    // Look for GGUF files that match the model name in common directories
    QStringList searchDirs = {QString("D:/OllamaModels"), QDir::homePath() + "/models", QDir::currentPath() + "/models"};
    for (const QString& dirPath : searchDirs) {
        QDir d(dirPath);
        if (!d.exists()) continue;
        QStringList matches = d.entryList(QStringList() << QString("*%1*.gguf").arg(modelName), QDir::Files, QDir::Name);
        if (!matches.isEmpty()) {
            return d.filePath(matches.first());
        }
    }
    return QString();
}

void ModelLoaderWidget::onModelComboChanged(int index)
{
    if (index <= 0) return;
    QString data = m_modelSelector->itemData(index).toString();
    if (data.startsWith("ollama:")) {
        QString modelName = data.mid(7);
        QString gguf = resolveOllamaGgufPath(modelName);
        if (!gguf.isEmpty()) {
            m_infoDisplay->append("[ModelLoader] Resolved Ollama model " + modelName + " to GGUF: " + gguf);
            loadModel(gguf);
        } else {
            m_infoDisplay->append("[ModelLoader] No GGUF found for Ollama model " + modelName + " in D:/OllamaModels");
        }
    } else if (QFile::exists(data)) {
        loadModel(data);
    } else {
        m_infoDisplay->append("[ModelLoader] Invalid model selection: " + data);
    }
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
    
    // Check if model loader is allocated AND successfully loaded
    bool success = (m_modelLoader != nullptr && m_modelLoader->isLoaded());
    
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

void ModelLoaderWidget::onBackendSelected(int index)
{
    QString backend = m_backendSelector->itemData(index).toString();
    m_currentBackend = backend;
    
    QString backendName = m_backendSelector->itemText(index);
    m_infoDisplay->append(QString("Backend selected: %1").arg(backendName));
    
    qDebug() << "[ModelLoaderWidget] Backend selected:" << backend;
    
    // Update UI based on backend capabilities
    if (backend == "masm_fp32" || backend == "masm_quantized" || backend == "masm_sliding" || backend == "masm_hotpatch") {
        m_infoDisplay->append("MASM backend enabled - hotpatch capabilities available");
    }

    emit backendChanged(backendName);
}

void ModelLoaderWidget::updateRamUsage()
{
    // Get current process memory usage
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc))) {
        qint64 memoryMB = pmc.WorkingSetSize / (1024 * 1024);
        m_ramUsageLabel->setText(QString("%1 MB").arg(memoryMB));
        
        // Color coding based on memory usage
        if (memoryMB > 8000) {
            m_ramUsageLabel->setStyleSheet("QLabel { background-color: #f44336; color: white; padding: 2px 8px; border-radius: 3px; }");
        } else if (memoryMB > 4000) {
            m_ramUsageLabel->setStyleSheet("QLabel { background-color: #ff9800; color: black; padding: 2px 8px; border-radius: 3px; }");
        } else {
            m_ramUsageLabel->setStyleSheet("QLabel { background-color: #4caf50; color: white; padding: 2px 8px; border-radius: 3px; }");
        }
    }
}

void ModelLoaderWidget::updateCompressionStats()
{
    if (!m_modelLoader) {
        return;
    }
    
    // This would update the stats table with compression results
    // Implementation depends on specific stats tracking
}
