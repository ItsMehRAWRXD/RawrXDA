#pragma once

#include <QObject>
#include <QString>

/**
 * @brief Test Model Generator - Creates test models for IDE testing
 * 
 * Generates SafeTensors and PyTorch test models using pure MASM
 * Zero dependencies - all file I/O done via Windows API
 */
class TestModelGenerator : public QObject
{
    Q_OBJECT

public:
    explicit TestModelGenerator(QObject* parent = nullptr);
    
    /**
     * @brief Generate a test SafeTensors model
     * @return true if successful, false otherwise
     */
    bool generateSafeTensorsModel();
    
    /**
     * @brief Generate a test PyTorch model
     * @return true if successful, false otherwise
     */
    bool generatePyTorchModel();
    
    /**
     * @brief Generate both test models
     * @return true if both successful, false otherwise
     */
    bool generateAllTestModels();
    
    /**
     * @brief Get the path to generated SafeTensors model
     */
    QString getSafeTensorsPath() const { return m_safeTensorsPath; }
    
    /**
     * @brief Get the path to generated PyTorch model
     */
    QString getPyTorchPath() const { return m_pytorchPath; }

signals:
    /**
     * @brief Emitted when model generation completes
     * @param modelType "safetensors" or "pytorch"
     * @param success true if generation succeeded
     */
    void modelGenerated(const QString& modelType, bool success);

private:
    QString m_safeTensorsPath = "test_model.safetensors";
    QString m_pytorchPath = "test_model.pt";
};

// MASM extern declarations
extern "C" {
    /**
     * @brief Generate test SafeTensors model (MASM implementation)
     * @return 1 if success, 0 if failure
     */
    int GenerateTestSafeTensors();
    
    /**
     * @brief Generate test PyTorch model (MASM implementation)
     * @return 1 if success, 0 if failure
     */
    int GenerateTestPyTorch();
}