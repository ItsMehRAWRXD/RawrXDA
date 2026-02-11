/**
 * @file SetupWizard.hpp
 * @brief RawrXD IDE Setup Wizard - Graphical Configuration Interface
 * 
 * Provides a user-friendly wizard for first-time setup, hardware detection,
 * and thermal configuration. Guides users through all necessary steps.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include <memory>
#include <vector>
#include <functional>

namespace rawrxd::setup {

// Forward declarations
class HardwareDetector;
struct DetectedHardware;

// ═══════════════════════════════════════════════════════════════════════════════
// Hardware Detection Structures
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief CPU information structure
 */
struct CPUInfo {
    std::string name;
    std::string processorId;
    std::string manufacturer;
    int coreCount = 0;
    int threadCount = 0;
    int maxClockMHz = 0;
    int l3CacheKB = 0;
    bool supportsRDRAND = false;
    bool supportsAVX512 = false;
};

/**
 * @brief NVMe/SSD drive information
 */
struct DriveInfo {
    int index = -1;
    std::string deviceId;
    std::string model;
    std::string serialNumber;
    int64_t sizeBytes = 0;
    std::string busType;
    std::string healthStatus;
    double maxTempCelsius = 70.0;
    bool isNVMe = false;
};

/**
 * @brief GPU information
 */
struct GPUInfo {
    std::string name;
    std::string driverVersion;
    int64_t vramBytes = 0;
    int maxTempCelsius = 100;
    bool isDiscrete = false;
};

/**
 * @brief Memory information
 */
struct MemoryInfo {
    int64_t totalBytes = 0;
    int moduleCount = 0;
    int speedMHz = 0;
};

/**
 * @brief Complete hardware detection results
 */
struct DetectedHardware {
    CPUInfo cpu;
    std::vector<DriveInfo> drives;
    std::vector<GPUInfo> gpus;
    MemoryInfo memory;
    std::string fingerprint;
    bool detectionComplete = false;
    std::string errorMessage;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Thermal Configuration
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Thermal operating mode
 */
enum class ThermalMode {
    Sustainable,    // 59.5°C ceiling, infinite duration
    Hybrid,         // 65°C ceiling, 5-10 min bursts
    Burst           // 75°C ceiling, 60s max
};

/**
 * @brief Thermal configuration settings
 */
struct ThermalConfig {
    ThermalMode defaultMode = ThermalMode::Sustainable;
    double sustainableCeiling = 59.5;
    double hybridCeiling = 65.0;
    double burstCeiling = 75.0;
    int burstDurationSeconds = 60;
    int cooldownSeconds = 300;
    double ewmaAlpha = 0.3;
    int predictionHorizonMs = 5000;
    bool enablePredictive = true;
    bool enableLoadBalancing = true;
};

// ═══════════════════════════════════════════════════════════════════════════════
// Wizard Page IDs
// ═══════════════════════════════════════════════════════════════════════════════

enum WizardPageId {
    Page_Intro = 0,
    Page_Hardware,
    Page_Thermal,
    Page_Security,
    Page_Summary,
    Page_Complete
};

// ═══════════════════════════════════════════════════════════════════════════════
// Introduction Page
// ═══════════════════════════════════════════════════════════════════════════════

class IntroPage {public:
    explicit IntroPage(void* parent = nullptr);
    
    void initializePage() override;
    bool isComplete() const override;

private:
    void* m_welcomeLabel;
    void* m_descriptionLabel;
    void* m_acceptTermsCheck;
    
    void setupUI();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Hardware Detection Page
// ═══════════════════════════════════════════════════════════════════════════════

class HardwarePage {public:
    explicit HardwarePage(void* parent = nullptr);
    ~HardwarePage();
    
    void initializePage() override;
    bool isComplete() const override;
    bool validatePage() override;
    
    DetectedHardware getDetectedHardware() const { return m_hardware; }

\npublic:\n    void hardwareDetectionComplete(bool success);

\nprivate:\n    void startDetection();
    void onDetectionProgress(int percent, const std::string& status);
    void onDetectionComplete(const DetectedHardware& hardware);
    void onDetectionError(const std::string& error);

private:
    void* m_statusLabel;
    void* m_progressBar;
    void* m_hardwareList;  // HWND list control (was QListWidget*)
    void* m_detectButton;
    void* m_refreshButton;
    void* m_cpuGroup;
    void* m_storageGroup;
    void* m_gpuGroup;
    void* m_memoryGroup;
    
    DetectedHardware m_hardware;
    bool m_detectionComplete = false;
    std::unique_ptr<HardwareDetector> m_detector;
    
    void setupUI();
    void populateHardwareInfo();
    void runDetection();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Thermal Configuration Page
// ═══════════════════════════════════════════════════════════════════════════════

class ThermalPage {public:
    explicit ThermalPage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    ThermalConfig getThermalConfig() const { return m_config; }

\nprivate:\n    void onModeChanged(int index);
    void onAdvancedToggled(bool checked);
    void updatePreview();

private:
    void* m_modeCombo;
    void* m_modeDescription;
    void* m_advancedGroup;
    void* m_ceilingSpin;
    void* m_alphaSpin;
    void* m_horizonSpin;
    void* m_predictiveCheck;
    void* m_loadBalanceCheck;
    void* m_previewText;
    
    ThermalConfig m_config;
    
    void setupUI();
    void applyModeDefaults(ThermalMode mode);
};

// ═══════════════════════════════════════════════════════════════════════════════
// Security Configuration Page
// ═══════════════════════════════════════════════════════════════════════════════

class SecurityPage {public:
    explicit SecurityPage(void* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    std::string getEntropyKey() const { return m_entropyKey; }
    bool isHardwareBindingEnabled() const { return m_hardwareBinding; }

\nprivate:\n    void generateKey();
    void importKey();
    void exportKey();

private:
    void* m_keyLabel;
    voidEdit* m_keyDisplay;
    void* m_generateButton;
    void* m_importButton;
    void* m_exportButton;
    void* m_hardwareBindingCheck;
    void* m_sessionAuthCheck;
    void* m_securityLevel;
    
    std::string m_entropyKey;
    bool m_hardwareBinding = true;
    
    void setupUI();
    void updateSecurityLevel();
    std::string generateRDRANDKey();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Summary Page
// ═══════════════════════════════════════════════════════════════════════════════

class SummaryPage {public:
    explicit SummaryPage(void* parent = nullptr);
    
    void initializePage() override;

private:
    void* m_summaryText;
    void* m_configPathLabel;
    void* m_changePathButton;
    std::string m_configPath;
    
    void setupUI();
    void generateSummary();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Completion Page
// ═══════════════════════════════════════════════════════════════════════════════

class CompletePage {public:
    explicit CompletePage(void* parent = nullptr);
    
    void initializePage() override;

\nprivate:\n    void onInstallProgress(int percent, const std::string& status);
    void onInstallComplete(bool success);

private:
    void* m_statusLabel;
    void* m_progressBar;
    void* m_logText;
    void* m_resultLabel;
    void* m_launchIdeCheck;
    void* m_openDocsCheck;
    
    bool m_installComplete = false;
    
    void setupUI();
    void performInstallation();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Main Setup Wizard
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Main setup wizard class
 */
class SetupWizard {public:
    explicit SetupWizard(void* parent = nullptr);
    ~SetupWizard();
    
    // Configuration access
    DetectedHardware getHardware() const;
    ThermalConfig getThermalConfig() const;
    std::string getEntropyKey() const;
    std::string getConfigPath() const { return m_configPath; }

\npublic:\n    void setupComplete(bool success);
    void configurationSaved(const std::string& path);

\npublic:\n    void saveConfiguration();

protected:
    void done(int result) override;

\nprivate:\n    void onPageChanged(int id);
    void onHelpRequested();

private:
    IntroPage* m_introPage;
    HardwarePage* m_hardwarePage;
    ThermalPage* m_thermalPage;
    SecurityPage* m_securityPage;
    SummaryPage* m_summaryPage;
    CompletePage* m_completePage;
    
    std::string m_configPath;
    
    void setupPages();
    void setupButtons();
    void applyTheme();
    bool writeConfigFiles();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Hardware Detector (Background Worker)
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Background hardware detection worker
 */
class HardwareDetector  {public:
    explicit HardwareDetector( = nullptr);
    
\npublic:\n    void detect();
    void cancel();

\npublic:\n    void progress(int percent, const std::string& status);
    void complete(const DetectedHardware& hardware);
    void error(const std::string& message);

private:
    bool m_cancelled = false;
    
    CPUInfo detectCPU();
    std::vector<DriveInfo> detectDrives();
    std::vector<GPUInfo> detectGPUs();
    MemoryInfo detectMemory();
    std::string generateFingerprint(const DetectedHardware& hw);
};

} // namespace rawrxd::setup

