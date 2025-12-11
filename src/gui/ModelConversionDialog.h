#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>

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
class ModelConversionDialog : public QDialog {
    Q_OBJECT
    
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
    explicit ModelConversionDialog(const QStringList& unsupportedTypes,
                                  const QString& recommendedType,
                                  const QString& modelPath,
                                  QWidget* parent = nullptr);
    
    /**
     * @brief Get the result of the conversion attempt
     */
    ConversionResult conversionResult() const { return m_result; }
    
    /**
     * @brief Get the path to the converted model (if successful)
     */
    QString convertedModelPath() const { return m_convertedPath; }

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief User clicked "Cancel" button
     */
    void onCancel();
    
    /**
     * @brief User clicked "Yes, Convert" button
     */
    void onConvertClicked();
    
    /**
     * @brief User clicked "More Info" button
     */
    void onMoreInfoClicked();
    
    /**
     * @brief Terminal output from quantization process
     */
    void onTerminalOutput(const QString& output);
    
    /**
     * @brief Quantization process completed
     */
    void onConversionComplete(bool success);

private:
    void setupUI();
    void startConversion();
    void updateProgress(const QString& message);
    void showInfoPanel();
    void hideInfoPanel();
    
    // UI elements
    QLabel* m_titleLabel;
    QLabel* m_messageLabel;
    QTextEdit* m_detailsText;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_convertButton;
    QPushButton* m_cancelButton;
    QPushButton* m_moreInfoButton;
    QWidget* m_infoPanel;
    
    // State
    QStringList m_unsupportedTypes;
    QString m_recommendedType;
    QString m_modelPath;
    ConversionResult m_result = Cancelled;
    QString m_convertedPath;
    bool m_conversionInProgress = false;
    
    // Terminal ID for process monitoring
    QString m_terminalId;
};
