#pragma once


#include <memory>

class TerminalManager;

/**
 * @brief Dialog for prompting model quantization conversion
 * 
 * When a model with unsupported quantization types is detected (e.g., IQ4_NL),
 * this dialog appears to:
 * 1. Inform the user of the incompatibility
 * 2. Propose conversion to a supported type (e.g., Q5_K)
 * 3. Run the conversion script via IDE terminal
 * 4. Monitor progress and auto-reload the model after success
 */
class ModelConversionDialog : public void {

public:
    enum ConversionResult {
        Cancelled = 0,
        ConversionFailed = 1,
        ConversionSucceeded = 2
    };
    
    /**
     * @brief Create conversion dialog
     * @param unsupportedTypes List of unsupported quantization info strings
     * @param recommendedType Recommended conversion type (e.g., "Q5_K")
     * @param modelPath Path to the model file
     * @param parent Parent widget
     */
    explicit ModelConversionDialog(const std::vector<std::string>& unsupportedTypes,
                                  const std::string& recommendedType,
                                  const std::string& modelPath,
                                  void* parent = nullptr);
    
    ~ModelConversionDialog() override;
    
    /**
     * @brief Get the result of the conversion attempt
     */
    ConversionResult conversionResult() const { return m_result; }
    
    /**
     * @brief Get the path to the converted model (if successful)
     */
    std::string convertedModelPath() const { return m_convertedPath; }

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    /**
     * @brief User clicked "Cancel" button
     */
    void onCancel();
    
    /**
     * @brief User clicked "Cancel Conversion" button during active conversion
     */
    void onCancelConversion();
    
    /**
     * @brief User clicked "Yes, Convert" button
     */
    void onConvertClicked();
    
    /**
     * @brief User clicked "More Info" button
     */
    void onMoreInfoClicked();
    
    /**
     * @brief Terminal output from quantization process (stdout)
     */
    void onTerminalOutput(const std::vector<uint8_t>& output);
    
    /**
     * @brief Terminal error output from quantization process (stderr)
     */
    void onTerminalError(const std::vector<uint8_t>& output);
    
    /**
     * @brief Quantization process finished
     */
    void onTerminalFinished(int exitCode);
    
    /**
     * @brief Verify converted model exists and reload it
     */
    void onVerifyAndReload();

private:
    void setupUI();
    void startConversion();
    void updateProgress(const std::string& message);
    void updateProgressPercentage(int current, int total);
    void showInfoPanel();
    void hideInfoPanel();
    void parseProgressFromOutput(const std::string& output);
    bool verifyConvertedModelExists();
    void logConversionHistory(bool success, qint64 durationMs);
    
    // UI elements
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    QTextEdit* m_detailsText;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_convertButton;
    QPushButton* m_cancelButton;
    QPushButton* m_cancelConversionButton;
    QPushButton* m_moreInfoButton;
    void* m_infoPanel;
    
    // State
    std::vector<std::string> m_unsupportedTypes;
    std::string m_recommendedType;
    std::string m_modelPath;
    ConversionResult m_result = Cancelled;
    std::string m_convertedPath;
    bool m_conversionInProgress = false;
    
    // Terminal management
    std::unique_ptr<TerminalManager> m_terminalManager;
    void** m_verifyTimer;
    int m_conversionStage = 0;  // Track which stage: 0=setup, 1=clone, 2=build, 3=convert
    
    // Progress tracking
    qint64 m_conversionStartTime = 0;  // Timestamp when conversion started
    int m_chunksProcessed = 0;  // Current chunk count from quantization output
    int m_totalChunks = 0;      // Total chunks to process
};

