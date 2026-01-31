#pragma once


#include <vector>

// Forward declarations


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
class HardwareBackendSelector : public void
{

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
        std::string name;
        std::string version;
        bool available = false;
        std::string deviceName;
        uint64_t vramBytes = 0;
        std::string computeCapability;
        bool supportsFP16 = false;
        bool supportsInt8 = false;
        std::string details;
    };

    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit HardwareBackendSelector(void* parent = nullptr);
    ~HardwareBackendSelector() override = default;
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets, sets up connections, and detects backends
     */
    void initialize();

    /**
     * @brief Get currently selected backend
     * @return Selected backend type
     */
    Backend getSelectedBackend() const;

    /**
     * @brief Get selected backend as string
     * @return Backend name as string
     */
    std::string getSelectedBackendName() const;

    /**
     * @brief Get configuration for selected backend
     * @return JSON object with backend configuration
     */
    void* getBackendConfig() const;

    /**
     * @brief Set backend configuration
     * @param config Configuration JSON object
     */
    void setBackendConfig(const void*& config);

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


    /**
     * @brief Emitted when backend selection changes
     * @param backend Selected backend
     */
    void backendSelected(int backend);

    /**
     * @brief Emitted when backend configuration changes
     * @param config New configuration
     */
    void configurationChanged(const void*& config);

    /**
     * @brief Emitted when user confirms selection
     */
    void backendConfirmed(int backend);

private:
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

