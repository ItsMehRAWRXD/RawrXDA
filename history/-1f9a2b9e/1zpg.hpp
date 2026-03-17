#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QLabel>
#include <memory>

class BlobToGGUFConverter;
struct ConversionProgress;
struct GGUFMetadata;

/**
 * @class BlobConverterPanel
 * @brief UI for blob-to-GGUF model conversion
 * 
 * Provides:
 * - Blob file selection and preview
 * - Model metadata editor (name, layers, embedding dim, etc.)
 * - Quantization preset selection
 * - Real-time conversion progress
 * - Conversion log viewer
 * - Output file management
 */
class BlobConverterPanel : public QWidget {
    Q_OBJECT

public:
    explicit BlobConverterPanel(QWidget* parent = nullptr);
    ~BlobConverterPanel() override;

    /**
     * @brief Initialize the converter panel and wiring
     */
    void initialize();

    /**
     * @brief Get the underlying converter engine
     */
    BlobToGGUFConverter* getConverter() const { return m_converter.get(); }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // File operations
    void onSelectBlobFile();
    void onSelectOutputPath();
    
    // Conversion control
    void onStartConversion();
    void onCancelConversion();
    
    // Converter signals
    void onProgressUpdated(const ConversionProgress& progress);
    void onConversionComplete(const QString& outputPath);
    void onConversionError(const QString& errorMessage);
    void onConversionCancelled();
    
    // UI updates
    void onPresetChanged(int index);
    void onBlobFileChanged(const QString& path);
    void onEstimateSize();

private:
    /**
     * @brief Create UI layout
     */
    void createLayout();

    /**
     * @brief Populate quantization presets
     */
    void populatePresets();

    /**
     * @brief Load metadata from GGUF presets
     */
    void loadPresetMetadata(const QString& presetName);

    /**
     * @brief Get metadata from UI controls
     */
    GGUFMetadata getMetadataFromUI() const;

    /**
     * @brief Add log message with timestamp
     */
    void addLog(const QString& message, bool isError = false);

    /**
     * @brief Enable/disable conversion controls
     */
    void setControlsEnabled(bool enabled);

    // UI Components
    // File selection
    QLineEdit* m_blobPathEdit;
    QPushButton* m_selectBlobBtn;
    QLineEdit* m_outputPathEdit;
    QPushButton* m_selectOutputBtn;
    QLabel* m_blobInfoLabel;
    QLabel* m_estimatedSizeLabel;

    // Metadata editor
    QLineEdit* m_modelNameEdit;
    QComboBox* m_architectureCombo;
    QSpinBox* m_nEmbedSpinBox;
    QSpinBox* m_nLayerSpinBox;
    QSpinBox* m_nVocabSpinBox;
    QSpinBox* m_nCtxSpinBox;
    QSpinBox* m_nHeadSpinBox;
    QSpinBox* m_nHeadKvSpinBox;
    QDoubleSpinBox* m_ropeFreqBaseSpinBox;
    QDoubleSpinBox* m_ropeFreqScaleSpinBox;

    // Preset and quantization
    QComboBox* m_presetCombo;
    QComboBox* m_quantTypeCombo;

    // Conversion controls
    QPushButton* m_startBtn;
    QPushButton* m_cancelBtn;
    QPushButton* m_estimateSizeBtn;

    // Progress tracking
    QProgressBar* m_progressBar;
    QLabel* m_progressLabel;

    // Logging
    QTextEdit* m_logOutput;

    // Backend
    std::unique_ptr<BlobToGGUFConverter> m_converter;

    // State
    bool m_isConverting = false;
    QString m_lastBlobPath;
};
