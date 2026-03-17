#pragma once

#include "model_loader_with_compression.hpp"
#include <QObject>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QThread>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>

/**
 * @brief ModelLoaderWidget - Qt widget for loading and managing GGUF models with compression
 * 
 * Provides a complete UI for:
 * - Model selection and loading
 * - Compression/decompression operations
 * - Tensor caching and management
 * - Performance monitoring
 * - Model validation
 */
class ModelLoaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit ModelLoaderWidget(QWidget* parent = nullptr);
    ~ModelLoaderWidget();

    // Model operations
    void loadModel(const QString& modelPath);
    void unloadModel();
    bool isModelLoaded() const { return m_modelLoader && m_modelLoader->isLoaded(); }
    
    // Compression operations
    void compressSelectedTensor();
    void decompressSelectedTensor();
    void cacheAllTensors();
    void clearCache();
    
    // UI updates
    void updateModelInfo();
    void updateTensorList();
    void updateCompressionStats();

signals:
    void modelLoaded(const QString& modelPath);
    void modelUnloaded();
    void tensorLoaded(const QString& tensorName, const QByteArray& data);
    void compressionCompleted(const QString& tensorName, double ratio);
    void errorOccurred(const QString& error);

private slots:
    void onModelSelected(const QString& modelPath);
    void onLoadButtonClicked();
    void onUnloadButtonClicked();
    void onTensorSelected(int index);
    void onCompressButtonClicked();
    void onDecompressButtonClicked();
    void onCacheAllButtonClicked();
    void onClearCacheButtonClicked();
    void onModelLoadFinished();
    void onCompressionFinished();

private:
    void setupUI();
    void setupConnections();
    void loadModelAsync(const QString& modelPath);  // async operation
    QByteArray compressTensorAsync(const QString& tensorName);  // returns compressed data
    void cacheTensorsAsync();
    
    // UI components
    QComboBox* m_modelSelector;
    QPushButton* m_loadButton;
    QPushButton* m_unloadButton;
    QComboBox* m_tensorSelector;
    QPushButton* m_compressButton;
    QPushButton* m_decompressButton;
    QPushButton* m_cacheAllButton;
    QPushButton* m_clearCacheButton;
    QProgressBar* m_progressBar;
    QTextEdit* m_infoDisplay;
    QTableWidget* m_statsTable;
    
    // Model loader
    std::unique_ptr<ModelLoaderWithCompression> m_modelLoader;
    
    // Async operations
    QFutureWatcher<bool>* m_loadWatcher;
    QFutureWatcher<QByteArray>* m_compressWatcher;
    QFutureWatcher<void>* m_cacheWatcher;
    
    // Current state
    QString m_currentModelPath;
    QString m_currentTensor;
    bool m_loading = false;
    bool m_compressing = false;
    bool m_caching = false;
};