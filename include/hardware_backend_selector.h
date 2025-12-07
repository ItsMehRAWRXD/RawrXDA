#pragma once

#include <QDialog>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>

// Forward declarations
class QComboBox;
class QLabel;
class QTextEdit;
class QPushButton;
class QGroupBox;
class QRadioButton;
class QButtonGroup;

/**
 * @class HardwareBackendSelector
 * @brief Hardware acceleration backend selection and configuration
 *
 * Features:
 * - Detect available backends (CPU, CUDA, Vulkan, ROCm, oneAPI)
 * - Display hardware capabilities (VRAM, compute capability, etc.)
 * - Select preferred backend for training
 * - Configure backend-specific options (precision, memory pool, etc.)
 * - Runtime backend switching
 */
class HardwareBackendSelector : public QDialog
{
    Q_OBJECT

public:
    /**
     * @enum Backend
     * @brief Available hardware backends
     */
    enum class Backend {
        CPU,        ///< CPU-only (fallback)
        CUDA,       ///< NVIDIA CUDA
        Vulkan,     ///< Vulkan compute
        ROCm,       ///< AMD ROCm
        OneAPI,     ///< Intel oneAPI
        Metal       ///< Apple Metal
    };

    /**
     * @struct BackendInfo
     * @brief Information about a backend
     */
    struct BackendInfo {
        Backend backend;
        QString name;
        QString version;
        bool available = false;
        QString deviceName;
        uint64_t vramBytes = 0;
        QString computeCapability;
        bool supportsFP16 = false;
        bool supportsInt8 = false;
        QString details;
    };

    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit HardwareBackendSelector(QWidget* parent = nullptr);
    ~HardwareBackendSelector() override = default;

    /**
     * @brief Get currently selected backend
     * @return Selected backend type
     */
    Backend getSelectedBackend() const;

    /**
     * @brief Get selected backend as string
     * @return Backend name as string
     */
    QString getSelectedBackendName() const;

    /**
     * @brief Get configuration for selected backend
     * @return JSON object with backend configuration
     */
    QJsonObject getBackendConfig() const;

    /**
     * @brief Set backend configuration
     * @param config Configuration JSON object
     */
    void setBackendConfig(const QJsonObject& config);

    /**
     * @brief Check if backend is available
     * @param backend Backend to check
     * @return true if backend available
     */
    bool isBackendAvailable(Backend backend) const;

    /**
     * @brief Get all available backends
     * @return Vector of available backend info
     */
    std::vector<BackendInfo> getAvailableBackends() const;

signals:
    /**
     * @brief Emitted when backend selection changes
     * @param backend Selected backend
     */
    void backendSelected(int backend);

    /**
     * @brief Emitted when backend configuration changes
     * @param config New configuration
     */
    void configurationChanged(const QJsonObject& config);

    /**
     * @brief Emitted when user confirms selection
     */
    void backendConfirmed(int backend);

private slots:
    void onBackendSelected(int index);
    void onPrecisionChanged();
    void onMemoryPoolChanged();
    void onApplyConfiguration();
    void onDetectHardware();
    void onResetToDefaults();

private:
    void setupUI();
    void setupConnections();
    void detectAvailableBackends();
    void populateBackendList();
    void updateBackendDetails(Backend backend);
    void loadBackendCapabilities();

    // ===== Backend Detection =====
    bool detectCuda();
    bool detectVulkan();
    bool detectRocm();
    bool detectOneAPI();
    bool detectMetal();

    // ===== UI Components =====
    QComboBox* m_backendCombo;
    QTextEdit* m_detailsText;
    
    // Precision selection
    QGroupBox* m_precisionGroup;
    QRadioButton* m_fp32Radio;
    QRadioButton* m_fp16Radio;
    QRadioButton* m_int8Radio;
    QButtonGroup* m_precisionGroup_impl;
    
    // Memory configuration
    QGroupBox* m_memoryGroup;
    QComboBox* m_memoryPoolCombo;
    QLabel* m_vramLabel;
    QLabel* m_vramUsageLabel;
    
    // Device selection
    QComboBox* m_deviceCombo;
    QLabel* m_deviceInfoLabel;
    
    // Optimization options
    QGroupBox* m_optimizationGroup;
    QLabel* m_enableTensorCoresLabel;
    QLabel* m_enableGraphsLabel;
    QPushButton* m_detectBtn;
    QPushButton* m_applyBtn;
    QPushButton* m_resetBtn;

    // ===== Backend State =====
    std::vector<BackendInfo> m_backends;
    Backend m_selectedBackend = Backend::CPU;
    bool m_fp16Enabled = false;
    bool m_int8Enabled = false;
    int m_memoryPoolMB = 1024;
};

