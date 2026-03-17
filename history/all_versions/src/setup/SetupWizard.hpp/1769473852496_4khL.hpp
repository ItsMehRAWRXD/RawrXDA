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

#include <QWizard>
#include <QWizardPage>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QProgressBar>
#include <QListWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QThread>
#include <QFuture>
#include <QtConcurrent>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>

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
    QString name;
    QString processorId;
    QString manufacturer;
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
    QString deviceId;
    QString model;
    QString serialNumber;
    qint64 sizeBytes = 0;
    QString busType;
    QString healthStatus;
    double maxTempCelsius = 70.0;
    bool isNVMe = false;
};

/**
 * @brief GPU information
 */
struct GPUInfo {
    QString name;
    QString driverVersion;
    qint64 vramBytes = 0;
    int maxTempCelsius = 100;
    bool isDiscrete = false;
};

/**
 * @brief Memory information
 */
struct MemoryInfo {
    qint64 totalBytes = 0;
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
    QString fingerprint;
    bool detectionComplete = false;
    QString errorMessage;
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

class IntroPage : public QWizardPage {
    Q_OBJECT

public:
    explicit IntroPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool isComplete() const override;

private:
    QLabel* m_welcomeLabel;
    QLabel* m_descriptionLabel;
    QCheckBox* m_acceptTermsCheck;
    
    void setupUI();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Hardware Detection Page
// ═══════════════════════════════════════════════════════════════════════════════

class HardwarePage : public QWizardPage {
    Q_OBJECT

public:
    explicit HardwarePage(QWidget* parent = nullptr);
    ~HardwarePage();
    
    void initializePage() override;
    bool isComplete() const override;
    bool validatePage() override;
    
    DetectedHardware getDetectedHardware() const { return m_hardware; }

signals:
    void hardwareDetectionComplete(bool success);

private slots:
    void startDetection();
    void onDetectionProgress(int percent, const QString& status);
    void onDetectionComplete(const DetectedHardware& hardware);
    void onDetectionError(const QString& error);

private:
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QListWidget* m_hardwareList;
    QPushButton* m_detectButton;
    QPushButton* m_refreshButton;
    QGroupBox* m_cpuGroup;
    QGroupBox* m_storageGroup;
    QGroupBox* m_gpuGroup;
    QGroupBox* m_memoryGroup;
    
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

class ThermalPage : public QWizardPage {
    Q_OBJECT

public:
    explicit ThermalPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    ThermalConfig getThermalConfig() const { return m_config; }

private slots:
    void onModeChanged(int index);
    void onAdvancedToggled(bool checked);
    void updatePreview();

private:
    QComboBox* m_modeCombo;
    QLabel* m_modeDescription;
    QGroupBox* m_advancedGroup;
    QDoubleSpinBox* m_ceilingSpin;
    QDoubleSpinBox* m_alphaSpin;
    QSpinBox* m_horizonSpin;
    QCheckBox* m_predictiveCheck;
    QCheckBox* m_loadBalanceCheck;
    QTextEdit* m_previewText;
    
    ThermalConfig m_config;
    
    void setupUI();
    void applyModeDefaults(ThermalMode mode);
};

// ═══════════════════════════════════════════════════════════════════════════════
// Security Configuration Page
// ═══════════════════════════════════════════════════════════════════════════════

class SecurityPage : public QWizardPage {
    Q_OBJECT

public:
    explicit SecurityPage(QWidget* parent = nullptr);
    
    void initializePage() override;
    bool validatePage() override;
    
    QString getEntropyKey() const { return m_entropyKey; }
    bool isHardwareBindingEnabled() const { return m_hardwareBinding; }

private slots:
    void generateKey();
    void importKey();
    void exportKey();

private:
    QLabel* m_keyLabel;
    QLineEdit* m_keyDisplay;
    QPushButton* m_generateButton;
    QPushButton* m_importButton;
    QPushButton* m_exportButton;
    QCheckBox* m_hardwareBindingCheck;
    QCheckBox* m_sessionAuthCheck;
    QLabel* m_securityLevel;
    
    QString m_entropyKey;
    bool m_hardwareBinding = true;
    
    void setupUI();
    void updateSecurityLevel();
    QString generateRDRANDKey();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Summary Page
// ═══════════════════════════════════════════════════════════════════════════════

class SummaryPage : public QWizardPage {
    Q_OBJECT

public:
    explicit SummaryPage(QWidget* parent = nullptr);
    
    void initializePage() override;

private:
    QTextEdit* m_summaryText;
    QLabel* m_configPathLabel;
    QPushButton* m_changePathButton;
    QString m_configPath;
    
    void setupUI();
    void generateSummary();
};

// ═══════════════════════════════════════════════════════════════════════════════
// Completion Page
// ═══════════════════════════════════════════════════════════════════════════════

class CompletePage : public QWizardPage {
    Q_OBJECT

public:
    explicit CompletePage(QWidget* parent = nullptr);
    
    void initializePage() override;

private slots:
    void onInstallProgress(int percent, const QString& status);
    void onInstallComplete(bool success);

private:
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTextEdit* m_logText;
    QLabel* m_resultLabel;
    QCheckBox* m_launchIdeCheck;
    QCheckBox* m_openDocsCheck;
    
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
class SetupWizard : public QWizard {
    Q_OBJECT

public:
    explicit SetupWizard(QWidget* parent = nullptr);
    ~SetupWizard();
    
    // Configuration access
    DetectedHardware getHardware() const;
    ThermalConfig getThermalConfig() const;
    QString getEntropyKey() const;
    QString getConfigPath() const { return m_configPath; }

signals:
    void setupComplete(bool success);
    void configurationSaved(const QString& path);

public slots:
    void saveConfiguration();

protected:
    void done(int result) override;

private slots:
    void onPageChanged(int id);
    void onHelpRequested();

private:
    IntroPage* m_introPage;
    HardwarePage* m_hardwarePage;
    ThermalPage* m_thermalPage;
    SecurityPage* m_securityPage;
    SummaryPage* m_summaryPage;
    CompletePage* m_completePage;
    
    QString m_configPath;
    
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
class HardwareDetector : public QObject {
    Q_OBJECT

public:
    explicit HardwareDetector(QObject* parent = nullptr);
    
public slots:
    void detect();
    void cancel();

signals:
    void progress(int percent, const QString& status);
    void complete(const DetectedHardware& hardware);
    void error(const QString& message);

private:
    bool m_cancelled = false;
    
    CPUInfo detectCPU();
    std::vector<DriveInfo> detectDrives();
    std::vector<GPUInfo> detectGPUs();
    MemoryInfo detectMemory();
    QString generateFingerprint(const DetectedHardware& hw);
};

} // namespace rawrxd::setup
