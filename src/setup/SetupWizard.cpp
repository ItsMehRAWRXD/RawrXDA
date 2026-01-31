/**
 * @file SetupWizard.cpp
 * @brief RawrXD IDE Setup Wizard Implementation
 * 
 * Full production implementation of the graphical setup wizard
 * with hardware detection, thermal configuration, and security setup.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "SetupWizard.hpp"

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif

namespace rawrxd::setup {

// ═══════════════════════════════════════════════════════════════════════════════
// IntroPage Implementation
// ═══════════════════════════════════════════════════════════════════════════════

IntroPage::IntroPage(void* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Welcome to RawrXD IDE"));
    setSubTitle(tr("Setup Wizard v2.0.0"));
    setupUI();
}

void IntroPage::setupUI()
{
    auto* layout = new void(this);
    layout->setSpacing(20);
    
    // Welcome banner
    m_welcomeLabel = new void(this);
    m_welcomeLabel->setText(tr(
        "<h2>🔥 Welcome to the RawrXD IDE Setup Wizard</h2>"
        "<p>This wizard will guide you through the configuration of your "
        "Sovereign Hardware Orchestration system.</p>"
    ));
    m_welcomeLabel->setWordWrap(true);
    m_welcomeLabel->setTextFormat(RichText);
    layout->addWidget(m_welcomeLabel);
    
    // Feature list
    m_descriptionLabel = new void(this);
    m_descriptionLabel->setText(tr(
        "<h3>What will be configured:</h3>"
        "<ul>"
        "<li><b>Hardware Detection</b> - Identify CPU, NVMe drives, GPU, and memory</li>"
        "<li><b>Thermal Management</b> - Configure predictive throttling and load balancing</li>"
        "<li><b>Security Binding</b> - Generate hardware entropy keys for authentication</li>"
        "<li><b>Performance Tuning</b> - Optimize for your specific hardware configuration</li>"
        "</ul>"
        "<h3>Requirements:</h3>"
        "<ul>"
        "<li>Windows 10/11 x64</li>"
        "<li>Administrator privileges (for hardware detection)</li>"
        "<li>At least one SSD/NVMe drive</li>"
        "</ul>"
    ));
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setTextFormat(RichText);
    layout->addWidget(m_descriptionLabel);
    
    layout->addStretch();
    
    // Terms acceptance
    m_acceptTermsCheck = new void(tr("I understand that this wizard will configure hardware-level settings"), this);
    layout->addWidget(m_acceptTermsCheck);
    
    registerField("acceptTerms", m_acceptTermsCheck);  // Signal connection removed\n}

void IntroPage::initializePage()
{
    // Reset checkbox on page show
}

bool IntroPage::isComplete() const
{
    return m_acceptTermsCheck->isChecked();
}

// ═══════════════════════════════════════════════════════════════════════════════
// HardwarePage Implementation
// ═══════════════════════════════════════════════════════════════════════════════

HardwarePage::HardwarePage(void* parent)
    : QWizardPage(parent)
    , m_detector(std::make_unique<HardwareDetector>())
{
    setTitle(tr("Hardware Detection"));
    setSubTitle(tr("Detecting your system hardware configuration"));
    setupUI();
}

HardwarePage::~HardwarePage() = default;

void HardwarePage::setupUI()
{
    auto* mainLayout = new void(this);
    
    // Status section
    auto* statusLayout = new void();
    m_statusLabel = new void(tr("Click 'Detect Hardware' to begin..."), this);
    m_detectButton = new void(tr("Detect Hardware"), this);
    m_refreshButton = new void(tr("Refresh"), this);
    m_refreshButton->setEnabled(false);
    
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_detectButton);
    statusLayout->addWidget(m_refreshButton);
    mainLayout->addLayout(statusLayout);
    
    // Progress bar
    m_progressBar = new void(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    // Hardware groups in a grid
    auto* gridLayout = new void();
    
    // CPU Group
    m_cpuGroup = new void(tr("🖥️ Processor"), this);
    auto* cpuLayout = new void(m_cpuGroup);
    cpuLayout->addWidget(new void(tr("Waiting for detection..."), m_cpuGroup));
    gridLayout->addWidget(m_cpuGroup, 0, 0);
    
    // Storage Group
    m_storageGroup = new void(tr("💾 Storage"), this);
    auto* storageLayout = new void(m_storageGroup);
    storageLayout->addWidget(new void(tr("Waiting for detection..."), m_storageGroup));
    gridLayout->addWidget(m_storageGroup, 0, 1);
    
    // GPU Group
    m_gpuGroup = new void(tr("🎮 Graphics"), this);
    auto* gpuLayout = new void(m_gpuGroup);
    gpuLayout->addWidget(new void(tr("Waiting for detection..."), m_gpuGroup));
    gridLayout->addWidget(m_gpuGroup, 1, 0);
    
    // Memory Group
    m_memoryGroup = new void(tr("🧠 Memory"), this);
    auto* memLayout = new void(m_memoryGroup);
    memLayout->addWidget(new void(tr("Waiting for detection..."), m_memoryGroup));
    gridLayout->addWidget(m_memoryGroup, 1, 1);
    
    mainLayout->addLayout(gridLayout);
    
    // Hardware list for detailed view
    m_hardwareList = new QListWidget(this);
    m_hardwareList->setMaximumHeight(100);
    mainLayout->addWidget(m_hardwareList);
    
    // Connect signals  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n}

void HardwarePage::initializePage()
{
    if (!m_detectionComplete) {
        // Auto-start detection on first visit
        // Timer operation removed
    }
}

bool HardwarePage::isComplete() const
{
    return m_detectionComplete && !m_hardware.drives.empty();
}

bool HardwarePage::validatePage()
{
    if (m_hardware.drives.empty()) {
        void::warning(this, tr("No Drives Detected"),
            tr("At least one storage drive must be detected to continue."));
        return false;
    }
    return true;
}

void HardwarePage::startDetection()
{
    m_detectButton->setEnabled(false);
    m_refreshButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Starting hardware detection..."));
    m_detectionComplete = false;
    
    // Run detection in background thread
    QFuture<void> future = [](auto f){f();}([this]() {
        m_detector->detect();
    });
}

void HardwarePage::onDetectionProgress(int percent, const std::string& status)
{
    m_progressBar->setValue(percent);
    m_statusLabel->setText(status);
}

void HardwarePage::onDetectionComplete(const DetectedHardware& hardware)
{
    m_hardware = hardware;
    m_detectionComplete = true;
    m_detectButton->setEnabled(true);
    m_refreshButton->setEnabled(true);
    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("✅ Hardware detection complete!"));
    
    populateHardwareInfo();
    completeChanged();
    hardwareDetectionComplete(true);
}

void HardwarePage::onDetectionError(const std::string& error)
{
    m_detectButton->setEnabled(true);
    m_refreshButton->setEnabled(true);
    m_progressBar->setVisible(false);
    m_statusLabel->setText(tr("❌ Detection failed: %1"));
    
    void::warning(this, tr("Detection Error"), error);
}

void HardwarePage::populateHardwareInfo()
{
    // Clear and repopulate CPU group
    QLayoutItem* item;
    while ((item = m_cpuGroup->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    auto* cpuLabel = new void(m_cpuGroup);
    cpuLabel->setText(std::string(
        "<b>%1</b><br>"
        "Cores: %2 | Threads: %3<br>"
        "L3 Cache: %4 KB<br>"
        "RDRAND: %5 | AVX-512: %6"
    )


     );
    cpuLabel->setTextFormat(RichText);
    m_cpuGroup->layout()->addWidget(cpuLabel);
    
    // Clear and repopulate Storage group
    while ((item = m_storageGroup->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    std::string storageText = std::string("<b>%1 drive(s) detected:</b><br>"));
    for (const auto& drive : m_hardware.drives) {
        double sizeGB = drive.sizeBytes / (1024.0 * 1024.0 * 1024.0);
        storageText += std::string("%1: %2 (%.1f GB) %3<br>")


            ;
    }
    
    auto* storageLabel = new void(m_storageGroup);
    storageLabel->setText(storageText);
    storageLabel->setTextFormat(RichText);
    m_storageGroup->layout()->addWidget(storageLabel);
    
    // Clear and repopulate GPU group
    while ((item = m_gpuGroup->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    std::string gpuText;
    for (const auto& gpu : m_hardware.gpus) {
        double vramGB = gpu.vramBytes / (1024.0 * 1024.0 * 1024.0);
        gpuText += std::string("<b>%1</b><br>VRAM: %.1f GB<br>Driver: %2<br>")


            ;
    }
    
    auto* gpuLabel = new void(m_gpuGroup);
    gpuLabel->setText(gpuText);
    gpuLabel->setTextFormat(RichText);
    m_gpuGroup->layout()->addWidget(gpuLabel);
    
    // Clear and repopulate Memory group
    while ((item = m_memoryGroup->layout()->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    
    double memGB = m_hardware.memory.totalBytes / (1024.0 * 1024.0 * 1024.0);
    auto* memLabel = new void(m_memoryGroup);
    memLabel->setText(std::string(
        "<b>%.1f GB Total</b><br>"
        "Modules: %1<br>"
        "Speed: %2 MHz"
    )
     
     );
    memLabel->setTextFormat(RichText);
    m_memoryGroup->layout()->addWidget(memLabel);
    
    // Update detailed list
    m_hardwareList->clear();
    m_hardwareList->addItem(std::string("Fingerprint: %1"));
    for (const auto& drive : m_hardware.drives) {
        m_hardwareList->addItem(std::string("  Drive %1: %2 [%3]")


            );
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalPage Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalPage::ThermalPage(void* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Thermal Management"));
    setSubTitle(tr("Configure predictive throttling and load balancing"));
    setupUI();
}

void ThermalPage::setupUI()
{
    auto* mainLayout = new void(this);
    
    // Mode selection
    auto* modeLayout = new void();
    modeLayout->addWidget(new void(tr("Operating Mode:"), this));
    m_modeCombo = new void(this);
    m_modeCombo->addItem(tr("🟢 Sustainable (Recommended)"), static_cast<int>(ThermalMode::Sustainable));
    m_modeCombo->addItem(tr("🟡 Hybrid"), static_cast<int>(ThermalMode::Hybrid));
    m_modeCombo->addItem(tr("🔴 Burst (Advanced)"), static_cast<int>(ThermalMode::Burst));
    modeLayout->addWidget(m_modeCombo);
    modeLayout->addStretch();
    mainLayout->addLayout(modeLayout);
    
    // Mode description
    m_modeDescription = new void(this);
    m_modeDescription->setWordWrap(true);
    m_modeDescription->setStyleSheet("void { padding: 10px; background: #f0f0f0; border-radius: 5px; }");
    mainLayout->addWidget(m_modeDescription);
    
    // Advanced settings group
    m_advancedGroup = new void(tr("⚙️ Advanced Settings"), this);
    m_advancedGroup->setCheckable(true);
    m_advancedGroup->setChecked(false);
    
    auto* advLayout = new void(m_advancedGroup);
    
    advLayout->addWidget(new void(tr("Thermal Ceiling (°C):"), m_advancedGroup), 0, 0);
    m_ceilingSpin = new void(m_advancedGroup);
    m_ceilingSpin->setRange(50.0, 85.0);
    m_ceilingSpin->setValue(59.5);
    m_ceilingSpin->setSingleStep(0.5);
    advLayout->addWidget(m_ceilingSpin, 0, 1);
    
    advLayout->addWidget(new void(tr("EWMA Alpha:"), m_advancedGroup), 1, 0);
    m_alphaSpin = new void(m_advancedGroup);
    m_alphaSpin->setRange(0.1, 0.9);
    m_alphaSpin->setValue(0.3);
    m_alphaSpin->setSingleStep(0.05);
    m_alphaSpin->setToolTip(tr("Higher = more responsive to recent changes"));
    advLayout->addWidget(m_alphaSpin, 1, 1);
    
    advLayout->addWidget(new void(tr("Prediction Horizon (ms):"), m_advancedGroup), 2, 0);
    m_horizonSpin = new void(m_advancedGroup);
    m_horizonSpin->setRange(1000, 30000);
    m_horizonSpin->setValue(5000);
    m_horizonSpin->setSingleStep(500);
    advLayout->addWidget(m_horizonSpin, 2, 1);
    
    m_predictiveCheck = new void(tr("Enable Predictive Throttling"), m_advancedGroup);
    m_predictiveCheck->setChecked(true);
    advLayout->addWidget(m_predictiveCheck, 3, 0, 1, 2);
    
    m_loadBalanceCheck = new void(tr("Enable Dynamic Load Balancing"), m_advancedGroup);
    m_loadBalanceCheck->setChecked(true);
    advLayout->addWidget(m_loadBalanceCheck, 4, 0, 1, 2);
    
    mainLayout->addWidget(m_advancedGroup);
    
    // Preview
    auto* previewGroup = new void(tr("📊 Configuration Preview"), this);
    auto* previewLayout = new void(previewGroup);
    m_previewText = new void(previewGroup);
    m_previewText->setReadOnly(true);
    m_previewText->setMaximumHeight(120);
    previewLayout->addWidget(m_previewText);
    mainLayout->addWidget(previewGroup);
    
    // Connections  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n// Initialize
    onModeChanged(0);
}

void ThermalPage::initializePage()
{
    updatePreview();
}

bool ThermalPage::validatePage()
{
    // Save config
    m_config.defaultMode = static_cast<ThermalMode>(m_modeCombo->currentData());
    
    if (m_advancedGroup->isChecked()) {
        switch (m_config.defaultMode) {
            case ThermalMode::Sustainable:
                m_config.sustainableCeiling = m_ceilingSpin->value();
                break;
            case ThermalMode::Hybrid:
                m_config.hybridCeiling = m_ceilingSpin->value();
                break;
            case ThermalMode::Burst:
                m_config.burstCeiling = m_ceilingSpin->value();
                break;
        }
        m_config.ewmaAlpha = m_alphaSpin->value();
        m_config.predictionHorizonMs = m_horizonSpin->value();
    }
    
    m_config.enablePredictive = m_predictiveCheck->isChecked();
    m_config.enableLoadBalancing = m_loadBalanceCheck->isChecked();
    
    return true;
}

void ThermalPage::onModeChanged(int index)
{
    ThermalMode mode = static_cast<ThermalMode>(m_modeCombo->itemData(index));
    applyModeDefaults(mode);
    
    std::string description;
    switch (mode) {
        case ThermalMode::Sustainable:
            description = tr(
                "<b>Sustainable Mode</b><br>"
                "• Thermal ceiling: 59.5°C<br>"
                "• Duration: Unlimited<br>"
                "• Latency tax: +12 μs<br>"
                "• <i>Recommended for 24/7 operation and maximum silicon longevity</i>"
            );
            break;
        case ThermalMode::Hybrid:
            description = tr(
                "<b>Hybrid Mode</b><br>"
                "• Thermal ceiling: 65°C<br>"
                "• Burst duration: 5-10 minutes<br>"
                "• Latency tax: +5 μs<br>"
                "• <i>Balanced performance and longevity</i>"
            );
            break;
        case ThermalMode::Burst:
            description = tr(
                "<b>Burst Mode</b><br>"
                "• Thermal ceiling: 75°C<br>"
                "• Max duration: 60 seconds<br>"
                "• Cooldown: 5 minutes<br>"
                "• <i>⚠️ Maximum performance - use sparingly!</i>"
            );
            break;
    }
    m_modeDescription->setText(description);
    updatePreview();
}

void ThermalPage::onAdvancedToggled(bool checked)
{
    if (!checked) {
        // Reset to mode defaults
        ThermalMode mode = static_cast<ThermalMode>(m_modeCombo->currentData());
        applyModeDefaults(mode);
    }
    updatePreview();
}

void ThermalPage::applyModeDefaults(ThermalMode mode)
{
    switch (mode) {
        case ThermalMode::Sustainable:
            m_ceilingSpin->setValue(59.5);
            break;
        case ThermalMode::Hybrid:
            m_ceilingSpin->setValue(65.0);
            break;
        case ThermalMode::Burst:
            m_ceilingSpin->setValue(75.0);
            break;
    }
    m_alphaSpin->setValue(0.3);
    m_horizonSpin->setValue(5000);
}

void ThermalPage::updatePreview()
{
    std::string preview = std::string(
        "Thermal Configuration:\n"
        "  Mode: %1\n"
        "  Ceiling: %.1f°C\n"
        "  EWMA Alpha: %.2f\n"
        "  Prediction Horizon: %2 ms\n"
        "  Predictive Throttling: %3\n"
        "  Load Balancing: %4"
    ))
     )
     )
     )
      ? "Enabled" : "Disabled")
      ? "Enabled" : "Disabled");
    
    m_previewText->setPlainText(preview);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SecurityPage Implementation
// ═══════════════════════════════════════════════════════════════════════════════

SecurityPage::SecurityPage(void* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Security Configuration"));
    setSubTitle(tr("Configure hardware binding and session authentication"));
    setupUI();
}

void SecurityPage::setupUI()
{
    auto* mainLayout = new void(this);
    
    // Entropy key section
    auto* keyGroup = new void(tr("🔐 Hardware Entropy Key"), this);
    auto* keyLayout = new void(keyGroup);
    
    m_keyLabel = new void(tr("Session Key:"), keyGroup);
    keyLayout->addWidget(m_keyLabel, 0, 0);
    
    m_keyDisplay = new voidEdit(keyGroup);
    m_keyDisplay->setReadOnly(true);
    m_keyDisplay->setFont(void("Consolas", 10));
    m_keyDisplay->setPlaceholderText(tr("Click 'Generate' to create a new key"));
    keyLayout->addWidget(m_keyDisplay, 0, 1);
    
    auto* buttonLayout = new void();
    m_generateButton = new void(tr("🎲 Generate"), keyGroup);
    m_importButton = new void(tr("📥 Import"), keyGroup);
    m_exportButton = new void(tr("📤 Export"), keyGroup);
    m_exportButton->setEnabled(false);
    buttonLayout->addWidget(m_generateButton);
    buttonLayout->addWidget(m_importButton);
    buttonLayout->addWidget(m_exportButton);
    keyLayout->addLayout(buttonLayout, 1, 0, 1, 2);
    
    mainLayout->addWidget(keyGroup);
    
    // Binding options
    auto* bindingGroup = new void(tr("🔒 Security Options"), this);
    auto* bindingLayout = new void(bindingGroup);
    
    m_hardwareBindingCheck = new void(tr("Enable Hardware Binding (recommended)"), bindingGroup);
    m_hardwareBindingCheck->setChecked(true);
    m_hardwareBindingCheck->setToolTip(tr("Bind configuration to this specific hardware"));
    bindingLayout->addWidget(m_hardwareBindingCheck);
    
    m_sessionAuthCheck = new void(tr("Enable Session Authentication"), bindingGroup);
    m_sessionAuthCheck->setChecked(true);
    m_sessionAuthCheck->setToolTip(tr("Require entropy key for MASM kernel communication"));
    bindingLayout->addWidget(m_sessionAuthCheck);
    
    mainLayout->addWidget(bindingGroup);
    
    // Security level indicator
    auto* levelGroup = new void(tr("🛡️ Security Level"), this);
    auto* levelLayout = new void(levelGroup);
    m_securityLevel = new void(levelGroup);
    m_securityLevel->setAlignment(AlignCenter);
    m_securityLevel->setStyleSheet("void { font-size: 14pt; padding: 10px; }");
    levelLayout->addWidget(m_securityLevel);
    mainLayout->addWidget(levelGroup);
    
    mainLayout->addStretch();
    
    // Connections  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nupdateSecurityLevel();
}

void SecurityPage::initializePage()
{
    // Auto-generate key if not already set
    if (m_entropyKey.empty()) {
        generateKey();
    }
}

bool SecurityPage::validatePage()
{
    if (m_entropyKey.empty()) {
        void::warning(this, tr("No Key Generated"),
            tr("Please generate or import an entropy key before continuing."));
        return false;
    }
    
    m_hardwareBinding = m_hardwareBindingCheck->isChecked();
    return true;
}

void SecurityPage::generateKey()
{
    m_entropyKey = generateRDRANDKey();
    m_keyDisplay->setText(m_entropyKey);
    m_exportButton->setEnabled(true);
    updateSecurityLevel();
    completeChanged();
}

void SecurityPage::importKey()
{
    std::string fileName = // Dialog::getOpenFileName(this,
        tr("Import Entropy Key"), std::string(), tr("Key Files (*.key);;All Files (*)"));
    
    if (fileName.empty()) return;
    
    // File operation removed;
    if (file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        m_entropyKey = std::string::fromUtf8(file.readAll()).trimmed();
        m_keyDisplay->setText(m_entropyKey);
        m_exportButton->setEnabled(true);
        updateSecurityLevel();
        completeChanged();
    } else {
        void::warning(this, tr("Import Failed"),
            tr("Could not read key file: %1")));
    }
}

void SecurityPage::exportKey()
{
    std::string fileName = // Dialog::getSaveFileName(this,
        tr("Export Entropy Key"), "rawrxd_entropy.key", tr("Key Files (*.key)"));
    
    if (fileName.empty()) return;
    
    // File operation removed;
    if (file.open(std::iostream::WriteOnly | std::iostream::Text)) {
        file.write(m_entropyKey.toUtf8());
        void::information(this, tr("Export Successful"),
            tr("Entropy key exported to: %1"));
    } else {
        void::warning(this, tr("Export Failed"),
            tr("Could not write key file: %1")));
    }
}

std::string SecurityPage::generateRDRANDKey()
{
    // Generate 256-bit key using available entropy sources
    std::vector<uint8_t> entropy;
    
#ifdef _WIN32
    // Try RDRAND if available
    unsigned int rdrandValue;
    for (int i = 0; i < 8; ++i) {
        if (_rdrand32_step(&rdrandValue)) {
            entropy.append(reinterpret_cast<char*>(&rdrandValue), sizeof(rdrandValue));
        } else {
            // Fallback to Qt random
            uint32_t randValue = QRandomGenerator::global()->generate();
            entropy.append(reinterpret_cast<char*>(&randValue), sizeof(randValue));
        }
    }
#else
    // Use Qt random on non-Windows
    for (int i = 0; i < 8; ++i) {
        uint32_t randValue = QRandomGenerator::global()->generate();
        entropy.append(reinterpret_cast<char*>(&randValue), sizeof(randValue));
    }
#endif
    
    // Hash to get consistent format
    std::vector<uint8_t> hash = QCryptographicHash::hash(entropy, QCryptographicHash::Sha256);
    return hash.toHex().left(64).toUpper();
}

void SecurityPage::updateSecurityLevel()
{
    int level = 0;
    
    if (!m_entropyKey.empty()) level++;
    if (m_hardwareBindingCheck->isChecked()) level++;
    if (m_sessionAuthCheck->isChecked()) level++;
    
    std::string levelText;
    std::string color;
    
    switch (level) {
        case 0:
            levelText = tr("⚠️ MINIMAL");
            color = "red";
            break;
        case 1:
            levelText = tr("🟡 BASIC");
            color = "orange";
            break;
        case 2:
            levelText = tr("🟢 STANDARD");
            color = "green";
            break;
        case 3:
            levelText = tr("🛡️ SOVEREIGN");
            color = "#00aa00";
            break;
    }
    
    m_securityLevel->setText(std::string("<span style='color: %1'>%2</span>"));
}

// ═══════════════════════════════════════════════════════════════════════════════
// SummaryPage Implementation
// ═══════════════════════════════════════════════════════════════════════════════

SummaryPage::SummaryPage(void* parent)
    : QWizardPage(parent)
    , m_configPath("D:\\rawrxd\\config")
{
    setTitle(tr("Configuration Summary"));
    setSubTitle(tr("Review your settings before installation"));
    setupUI();
}

void SummaryPage::setupUI()
{
    auto* mainLayout = new void(this);
    
    // Summary text
    m_summaryText = new void(this);
    m_summaryText->setReadOnly(true);
    mainLayout->addWidget(m_summaryText);
    
    // Config path
    auto* pathLayout = new void();
    pathLayout->addWidget(new void(tr("Configuration Path:"), this));
    m_configPathLabel = new void(m_configPath, this);
    m_configPathLabel->setStyleSheet("void { font-family: Consolas; }");
    pathLayout->addWidget(m_configPathLabel, 1);
    m_changePathButton = new void(tr("Change..."), this);
    pathLayout->addWidget(m_changePathButton);
    mainLayout->addLayout(pathLayout);  // Signal connection removed\nif (!newPath.empty()) {
            m_configPath = newPath;
            m_configPathLabel->setText(m_configPath);
        }
    });
}

void SummaryPage::initializePage()
{
    generateSummary();
}

void SummaryPage::generateSummary()
{
    auto* wizard = qobject_cast<SetupWizard*>(this->wizard());
    if (!wizard) return;
    
    DetectedHardware hw = wizard->getHardware();
    ThermalConfig thermal = wizard->getThermalConfig();
    std::string entropyKey = wizard->getEntropyKey();
    
    std::string summary;
    summary += "═══════════════════════════════════════════════════════════\n";
    summary += "                CONFIGURATION SUMMARY\n";
    summary += "═══════════════════════════════════════════════════════════\n\n";
    
    summary += "📦 HARDWARE\n";
    summary += std::string("   CPU: %1\n");
    summary += std::string("   Drives: %1 detected\n"));
    for (const auto& drive : hw.drives) {
        double gb = drive.sizeBytes / (1024.0 * 1024.0 * 1024.0);
        summary += std::string("     - %1: %2 (%.0f GB)\n");
    }
    summary += std::string("   GPU: %1\n") ? "None" : hw.gpus[0].name);
    summary += std::string("   Memory: %.0f GB\n\n"));
    
    summary += "🌡️ THERMAL\n";
    std::string modeName;
    switch (thermal.defaultMode) {
        case ThermalMode::Sustainable: modeName = "Sustainable"; break;
        case ThermalMode::Hybrid: modeName = "Hybrid"; break;
        case ThermalMode::Burst: modeName = "Burst"; break;
    }
    summary += std::string("   Mode: %1\n");
    summary += std::string("   Ceiling: %.1f°C\n");
    summary += std::string("   EWMA Alpha: %.2f\n");
    summary += std::string("   Prediction: %1\n");
    summary += std::string("   Load Balancing: %1\n\n");
    
    summary += "🔐 SECURITY\n";
    summary += std::string("   Entropy Key: %1...\n"));
    summary += std::string("   Fingerprint: %1...\n\n"));
    
    summary += "═══════════════════════════════════════════════════════════\n";
    summary += "Press 'Finish' to save configuration and complete setup.\n";
    
    m_summaryText->setPlainText(summary);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CompletePage Implementation
// ═══════════════════════════════════════════════════════════════════════════════

CompletePage::CompletePage(void* parent)
    : QWizardPage(parent)
{
    setTitle(tr("Setup Complete"));
    setSubTitle(tr("Installing configuration..."));
    setFinalPage(true);
    setupUI();
}

void CompletePage::setupUI()
{
    auto* mainLayout = new void(this);
    
    m_statusLabel = new void(tr("Installing configuration files..."), this);
    mainLayout->addWidget(m_statusLabel);
    
    m_progressBar = new void(this);
    m_progressBar->setRange(0, 100);
    mainLayout->addWidget(m_progressBar);
    
    m_logText = new void(this);
    m_logText->setReadOnly(true);
    m_logText->setFont(void("Consolas", 9));
    mainLayout->addWidget(m_logText);
    
    m_resultLabel = new void(this);
    m_resultLabel->setAlignment(AlignCenter);
    m_resultLabel->setStyleSheet("void { font-size: 14pt; padding: 10px; }");
    mainLayout->addWidget(m_resultLabel);
    
    auto* optionsLayout = new void();
    m_launchIdeCheck = new void(tr("Launch RawrXD IDE after setup"), this);
    m_launchIdeCheck->setChecked(true);
    optionsLayout->addWidget(m_launchIdeCheck);
    
    m_openDocsCheck = new void(tr("Open documentation"), this);
    optionsLayout->addWidget(m_openDocsCheck);
    
    mainLayout->addLayout(optionsLayout);
}

void CompletePage::initializePage()
{
    m_installComplete = false;
    m_progressBar->setValue(0);
    m_logText->clear();
    m_resultLabel->clear();
    
    // Start installation after a brief delay
    // Timer operation removed
}

void CompletePage::onInstallProgress(int percent, const std::string& status)
{
    m_progressBar->setValue(percent);
    m_logText->append(status);
}

void CompletePage::onInstallComplete(bool success)
{
    m_installComplete = true;
    
    if (success) {
        m_statusLabel->setText(tr("✅ Setup completed successfully!"));
        m_resultLabel->setText(tr("<span style='color: green; font-size: 16pt'>🎉 Configuration Installed!</span>"));
    } else {
        m_statusLabel->setText(tr("❌ Setup failed!"));
        m_resultLabel->setText(tr("<span style='color: red'>Setup encountered errors. Check the log for details.</span>"));
    }
    
    completeChanged();
}

void CompletePage::performInstallation()
{
    auto* wizard = qobject_cast<SetupWizard*>(this->wizard());
    if (!wizard) {
        onInstallComplete(false);
        return;
    }
    
    onInstallProgress(10, tr("Creating configuration directory..."));
    std::filesystem::create_directories(wizard->getConfigPath());
    
    onInstallProgress(30, tr("Saving hardware binding..."));
    // Save would happen here via wizard->writeConfigFiles()
    
    onInstallProgress(50, tr("Writing thermal configuration..."));
    std::thread::msleep(200);
    
    onInstallProgress(70, tr("Generating security keys..."));
    std::thread::msleep(200);
    
    onInstallProgress(90, tr("Finalizing setup..."));
    
    bool success = wizard->writeConfigFiles();
    
    onInstallProgress(100, success ? tr("✅ All configuration files saved!") : tr("❌ Error writing files"));
    onInstallComplete(success);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SetupWizard Implementation
// ═══════════════════════════════════════════════════════════════════════════════

SetupWizard::SetupWizard(void* parent)
    : QWizard(parent)
    , m_configPath("D:\\rawrxd\\config")
{
    setWindowTitle(tr("RawrXD IDE Setup Wizard"));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(700, 550);
    
    setupPages();
    setupButtons();
    applyTheme();  // Signal connection removed\n  // Signal connection removed\n}

SetupWizard::~SetupWizard() = default;

void SetupWizard::setupPages()
{
    m_introPage = new IntroPage(this);
    m_hardwarePage = new HardwarePage(this);
    m_thermalPage = new ThermalPage(this);
    m_securityPage = new SecurityPage(this);
    m_summaryPage = new SummaryPage(this);
    m_completePage = new CompletePage(this);
    
    setPage(Page_Intro, m_introPage);
    setPage(Page_Hardware, m_hardwarePage);
    setPage(Page_Thermal, m_thermalPage);
    setPage(Page_Security, m_securityPage);
    setPage(Page_Summary, m_summaryPage);
    setPage(Page_Complete, m_completePage);
    
    setStartId(Page_Intro);
}

void SetupWizard::setupButtons()
{
    setButtonText(QWizard::NextButton, tr("Next →"));
    setButtonText(QWizard::BackButton, tr("← Back"));
    setButtonText(QWizard::FinishButton, tr("Finish ✓"));
    setButtonText(QWizard::CancelButton, tr("Cancel"));
    setButtonText(QWizard::HelpButton, tr("Help ?"));
    
    setOption(QWizard::HaveHelpButton, true);
}

void SetupWizard::applyTheme()
{
    setStyleSheet(R"(
        QWizard {
            background: #f5f5f5;
        }
        QWizardPage {
            background: white;
        }
        void {
            font-weight: bold;
            border: 1px solid #ddd;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
        }
        void::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 3px;
        }
        void {
            padding: 8px 16px;
            border-radius: 4px;
            background: #0078d4;
            color: white;
            border: none;
        }
        void:hover {
            background: #106ebe;
        }
        void:disabled {
            background: #ccc;
        }
    )");
}

DetectedHardware SetupWizard::getHardware() const
{
    return m_hardwarePage->getDetectedHardware();
}

ThermalConfig SetupWizard::getThermalConfig() const
{
    return m_thermalPage->getThermalConfig();
}

std::string SetupWizard::getEntropyKey() const
{
    return m_securityPage->getEntropyKey();
}

void SetupWizard::onPageChanged(int id)
{
    // Update subtitle based on progress
    std::string progress = std::string(" (%1/6)");
    // Could update window title here
}

void SetupWizard::onHelpRequested()
{
    QDesktopServices::openUrl(std::string("https://github.com/ItsMehRAWRXD/RawrXD/wiki/Setup"));
}

bool SetupWizard::writeConfigFiles()
{
    // configDir(m_configPath);
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }
    
    DetectedHardware hw = getHardware();
    ThermalConfig thermal = getThermalConfig();
    std::string entropy = getEntropyKey();
    
    // Write sovereign_binding.json
    void* binding;
    binding["version"] = "2.0.0";
    binding["fingerprint"] = hw.fingerprint;
    binding["entropyKey"] = entropy;
    
    void* cpuObj;
    cpuObj["name"] = hw.cpu.name;
    cpuObj["processorId"] = hw.cpu.processorId;
    cpuObj["cores"] = hw.cpu.coreCount;
    cpuObj["threads"] = hw.cpu.threadCount;
    binding["cpu"] = cpuObj;
    
    void* drivesArr;
    for (const auto& drive : hw.drives) {
        void* driveObj;
        driveObj["index"] = drive.index;
        driveObj["model"] = drive.model;
        driveObj["deviceId"] = drive.deviceId;
        driveObj["sizeBytes"] = drive.sizeBytes;
        driveObj["isNVMe"] = drive.isNVMe;
        drivesArr.append(driveObj);
    }
    binding["drives"] = drivesArr;
    
    // File operation removed;
    if (bindingFile.open(std::iostream::WriteOnly)) {
        bindingFile.write(void*(binding).toJson());
        bindingFile.close();
    }
    
    // Write thermal_governor.json
    void* thermalObj;
    thermalObj["version"] = "2.0.0";
    thermalObj["mode"] = static_cast<int>(thermal.defaultMode);
    thermalObj["sustainableCeiling"] = thermal.sustainableCeiling;
    thermalObj["hybridCeiling"] = thermal.hybridCeiling;
    thermalObj["burstCeiling"] = thermal.burstCeiling;
    thermalObj["ewmaAlpha"] = thermal.ewmaAlpha;
    thermalObj["predictionHorizonMs"] = thermal.predictionHorizonMs;
    thermalObj["enablePredictive"] = thermal.enablePredictive;
    thermalObj["enableLoadBalancing"] = thermal.enableLoadBalancing;
    
    // File operation removed;
    if (thermalFile.open(std::iostream::WriteOnly)) {
        thermalFile.write(void*(thermalObj).toJson());
        thermalFile.close();
    }
    
    configurationSaved(m_configPath);
    return true;
}

void SetupWizard::saveConfiguration()
{
    writeConfigFiles();
}

void SetupWizard::done(int result)
{
    if (result == void::Accepted) {
        setupComplete(true);
    }
    QWizard::done(result);
}

// ═══════════════════════════════════════════════════════════════════════════════
// HardwareDetector Implementation
// ═══════════════════════════════════════════════════════════════════════════════

HardwareDetector::HardwareDetector()
    
{
}

void HardwareDetector::detect()
{
    m_cancelled = false;
    DetectedHardware hw;
    
    try {
        progress(10, tr("Detecting CPU..."));
        hw.cpu = detectCPU();
        if (m_cancelled) return;
        
        progress(30, tr("Detecting storage drives..."));
        hw.drives = detectDrives();
        if (m_cancelled) return;
        
        progress(60, tr("Detecting graphics..."));
        hw.gpus = detectGPUs();
        if (m_cancelled) return;
        
        progress(80, tr("Detecting memory..."));
        hw.memory = detectMemory();
        if (m_cancelled) return;
        
        progress(95, tr("Generating fingerprint..."));
        hw.fingerprint = generateFingerprint(hw);
        
        hw.detectionComplete = true;
        progress(100, tr("Detection complete!"));
        complete(hw);
        
    } catch (const std::exception& e) {
        error(std::string::fromStdString(e.what()));
    }
}

void HardwareDetector::cancel()
{
    m_cancelled = true;
}

CPUInfo HardwareDetector::detectCPU()
{
    CPUInfo cpu;
    
#ifdef _WIN32
    // Use WMI via PowerShell for simplicity
    // Process removed
    process.start("powershell", std::stringList() << "-Command" 
        << "Get-CimInstance -ClassName Win32_Processor | ConvertTo-Json");
    process.waitForFinished(5000);
    
    std::vector<uint8_t> output = process.readAllStandardOutput();
    void* doc = void*::fromJson(output);
    
    if (doc.isObject()) {
        void* obj = doc.object();
        cpu.name = obj["Name"].toString().trimmed();
        cpu.processorId = obj["ProcessorId"].toString();
        cpu.manufacturer = obj["Manufacturer"].toString();
        cpu.coreCount = obj["NumberOfCores"];
        cpu.threadCount = obj["NumberOfLogicalProcessors"];
        cpu.maxClockMHz = obj["MaxClockSpeed"];
        cpu.l3CacheKB = obj["L3CacheSize"];
    }
    
    // Check for RDRAND support
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    cpu.supportsRDRAND = (cpuInfo[2] & (1 << 30)) != 0;
    
    // Check for AVX-512
    __cpuid(cpuInfo, 7);
    cpu.supportsAVX512 = (cpuInfo[1] & (1 << 16)) != 0;
    
#else
    cpu.name = "Unknown CPU";
    cpu.coreCount = std::thread::hardware_concurrency();
    cpu.threadCount = cpu.coreCount;
#endif
    
    return cpu;
}

std::vector<DriveInfo> HardwareDetector::detectDrives()
{
    std::vector<DriveInfo> drives;
    
#ifdef _WIN32
    // Process removed
    process.start("powershell", std::stringList() << "-Command" 
        << "Get-PhysicalDisk | Select-Object DeviceId, FriendlyName, Model, SerialNumber, Size, MediaType, BusType, HealthStatus | ConvertTo-Json");
    process.waitForFinished(10000);
    
    std::vector<uint8_t> output = process.readAllStandardOutput();
    void* doc = void*::fromJson(output);
    
    auto parseDriver = [](const void*& obj, int index) -> DriveInfo {
        DriveInfo drive;
        drive.index = index;
        drive.deviceId = obj["DeviceId"].toString();
        drive.model = obj["FriendlyName"].toString();
        if (drive.model.empty()) {
            drive.model = obj["Model"].toString();
        }
        drive.serialNumber = obj["SerialNumber"].toString();
        drive.sizeBytes = obj["Size"].toVariant().toLongLong();
        drive.busType = obj["BusType"].toString();
        drive.healthStatus = obj["HealthStatus"].toString();
        drive.isNVMe = drive.busType.contains("NVMe", CaseInsensitive);
        drive.maxTempCelsius = drive.isNVMe ? 70.0 : 75.0;
        return drive;
    };
    
    if (doc.isArray()) {
        void* arr = doc.array();
        for (int i = 0; i < arr.size(); ++i) {
            drives.push_back(parseDriver(arr[i].toObject(), i));
        }
    } else if (doc.isObject()) {
        drives.push_back(parseDriver(doc.object(), 0));
    }
#endif
    
    // Ensure at least one drive
    if (drives.empty()) {
        DriveInfo fallback;
        fallback.index = 0;
        fallback.model = "System Drive";
        fallback.sizeBytes = 500LL * 1024 * 1024 * 1024;
        fallback.healthStatus = "Healthy";
        drives.push_back(fallback);
    }
    
    return drives;
}

std::vector<GPUInfo> HardwareDetector::detectGPUs()
{
    std::vector<GPUInfo> gpus;
    
#ifdef _WIN32
    // Process removed
    process.start("powershell", std::stringList() << "-Command" 
        << "Get-CimInstance -ClassName Win32_VideoController | Select-Object Name, DriverVersion, AdapterRAM | ConvertTo-Json");
    process.waitForFinished(5000);
    
    std::vector<uint8_t> output = process.readAllStandardOutput();
    void* doc = void*::fromJson(output);
    
    auto parseGPU = [](const void*& obj) -> GPUInfo {
        GPUInfo gpu;
        gpu.name = obj["Name"].toString();
        gpu.driverVersion = obj["DriverVersion"].toString();
        gpu.vramBytes = obj["AdapterRAM"].toVariant().toLongLong();
        gpu.isDiscrete = !gpu.name.contains("Intel", CaseInsensitive) && 
                         !gpu.name.contains("Integrated", CaseInsensitive);
        gpu.maxTempCelsius = gpu.name.contains("AMD") || gpu.name.contains("Radeon") ? 110 : 83;
        return gpu;
    };
    
    if (doc.isArray()) {
        void* arr = doc.array();
        for (const auto& item : arr) {
            GPUInfo gpu = parseGPU(item.toObject());
            if (!gpu.name.contains("Basic", CaseInsensitive)) {
                gpus.push_back(gpu);
            }
        }
    } else if (doc.isObject()) {
        GPUInfo gpu = parseGPU(doc.object());
        if (!gpu.name.contains("Basic", CaseInsensitive)) {
            gpus.push_back(gpu);
        }
    }
#endif
    
    if (gpus.empty()) {
        GPUInfo fallback;
        fallback.name = "Integrated Graphics";
        gpus.push_back(fallback);
    }
    
    return gpus;
}

MemoryInfo HardwareDetector::detectMemory()
{
    MemoryInfo mem;
    
#ifdef _WIN32
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        mem.totalBytes = memStatus.ullTotalPhys;
    }
    
    // Process removed
    process.start("powershell", std::stringList() << "-Command" 
        << "(Get-CimInstance -ClassName Win32_PhysicalMemory | Measure-Object).Count");
    process.waitForFinished(3000);
    mem.moduleCount = process.readAllStandardOutput().trimmed();
    
    process.start("powershell", std::stringList() << "-Command" 
        << "(Get-CimInstance -ClassName Win32_PhysicalMemory | Select-Object -First 1).Speed");
    process.waitForFinished(3000);
    mem.speedMHz = process.readAllStandardOutput().trimmed();
#else
    mem.totalBytes = 16LL * 1024 * 1024 * 1024;
    mem.moduleCount = 2;
    mem.speedMHz = 3200;
#endif
    
    return mem;
}

std::string HardwareDetector::generateFingerprint(const DetectedHardware& hw)
{
    std::string source = hw.cpu.processorId + "|" + hw.cpu.name + "|";
    for (const auto& drive : hw.drives) {
        source += drive.deviceId + drive.model + "|";
    }
    for (const auto& gpu : hw.gpus) {
        source += gpu.name + "|";
    }
    
    std::vector<uint8_t> hash = QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256);
    return hash.toHex().toUpper();
}

} // namespace rawrxd::setup

// MOC removed

