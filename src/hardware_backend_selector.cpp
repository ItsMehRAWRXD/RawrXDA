#include "hardware_backend_selector.h"


#include <windows.h>
#include <dxgi.h>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

HardwareBackendSelector::HardwareBackendSelector(void* parent)
    : void(parent)
    , m_selectedBackend(Backend::CPU)
    , m_fp16Enabled(false)
    , m_int8Enabled(false)
    , m_memoryPoolMB(1024)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
    setWindowTitle("Hardware Backend Configuration");
    setMinimumSize(700, 600);
    setModal(true);
}

void HardwareBackendSelector::initialize() {
    if (m_backendCombo) return;  // Already initialized
    setupUI();
    setupConnections();
    detectAvailableBackends();
    populateBackendList();
}

void HardwareBackendSelector::setupUI()
{
    void* mainLayout = new void(this);

    // ===== Backend Selection Section =====
    void* backendGroup = new void("Available Backends", this);
    void* backendLayout = new void(backendGroup);

    void* selectorLayout = new void();
    void* backendLabel = new void("Select Backend:", this);
    m_backendCombo = new void(this);
    selectorLayout->addWidget(backendLabel);
    selectorLayout->addWidget(m_backendCombo);
    selectorLayout->addStretch();

    backendLayout->addLayout(selectorLayout);
    
    m_detailsText = new void(this);
    m_detailsText->setReadOnly(true);
    m_detailsText->setMinimumHeight(150);
    backendLayout->addWidget(m_detailsText);

    mainLayout->addWidget(backendGroup);

    // ===== Device Selection =====
    void* deviceGroup = new void("Device Configuration", this);
    void* deviceLayout = new void(deviceGroup);

    void* deviceComboLayout = new void();
    void* deviceLabel = new void("Device:", this);
    m_deviceCombo = new void(this);
    m_deviceCombo->addItem("Default Device");
    deviceComboLayout->addWidget(deviceLabel);
    deviceComboLayout->addWidget(m_deviceCombo);
    deviceComboLayout->addStretch();
    deviceLayout->addLayout(deviceComboLayout);

    m_deviceInfoLabel = new void("", this);
    m_deviceInfoLabel->setStyleSheet("color: gray; font-size: 10px;");
    deviceLayout->addWidget(m_deviceInfoLabel);

    mainLayout->addWidget(deviceGroup);

    // ===== Precision Selection =====
    m_precisionGroup = new void("Precision/Quantization", this);
    void* precisionLayout = new void(m_precisionGroup);
    m_precisionGroup_impl = nullptr;

    m_fp32Radio = nullptr", this);
    m_fp32Radio->setChecked(true);
    m_fp16Radio = nullptr", this);
    m_int8Radio = nullptr", this);

    m_precisionGroup_impl->addButton(m_fp32Radio, 0);
    m_precisionGroup_impl->addButton(m_fp16Radio, 1);
    m_precisionGroup_impl->addButton(m_int8Radio, 2);

    precisionLayout->addWidget(m_fp32Radio);
    precisionLayout->addWidget(m_fp16Radio);
    precisionLayout->addWidget(m_int8Radio);

    mainLayout->addWidget(m_precisionGroup);

    // ===== Memory Configuration =====
    m_memoryGroup = new void("Memory Configuration", this);
    void* memoryLayout = new void(m_memoryGroup);

    void* vramLabel = new void("Available VRAM:", this);
    m_vramLabel = new void("N/A", this);
    m_vramLabel->setStyleSheet("font-weight: bold;");
    memoryLayout->addWidget(vramLabel, 0, 0);
    memoryLayout->addWidget(m_vramLabel, 0, 1);

    void* poolLabel = new void("Memory Pool Type:", this);
    m_memoryPoolCombo = new void(this);
    m_memoryPoolCombo->addItem("Unified Memory (Default)", 0);
    m_memoryPoolCombo->addItem("Device Memory Only", 1);
    m_memoryPoolCombo->addItem("Host Pinned Memory", 2);
    memoryLayout->addWidget(poolLabel, 1, 0);
    memoryLayout->addWidget(m_memoryPoolCombo, 1, 1);

    void* usageLabel = new void("Estimated VRAM Usage:", this);
    m_vramUsageLabel = new void("0 MB", this);
    m_vramUsageLabel->setStyleSheet("color: blue;");
    memoryLayout->addWidget(usageLabel, 2, 0);
    memoryLayout->addWidget(m_vramUsageLabel, 2, 1);

    mainLayout->addWidget(m_memoryGroup);

    // ===== Optimization Options =====
    m_optimizationGroup = new void("Optimization Options", this);
    void* optimLayout = new void(m_optimizationGroup);

    m_enableTensorCoresLabel = new void("Tensor Cores: Disabled", this);
    m_enableGraphsLabel = new void("Graph Optimization: Disabled", this);

    optimLayout->addWidget(m_enableTensorCoresLabel);
    optimLayout->addWidget(m_enableGraphsLabel);

    mainLayout->addWidget(m_optimizationGroup);

    // ===== Action Buttons =====
    void* buttonLayout = new void();
    
    m_detectBtn = new void("Detect Hardware", this);
    m_resetBtn = new void("Reset to Defaults", this);
    m_applyBtn = new void("Apply Configuration", this);
    m_applyBtn->setStyleSheet("background-color: green; color: white; font-weight: bold;");

    buttonLayout->addWidget(m_detectBtn);
    buttonLayout->addWidget(m_resetBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void HardwareBackendSelector::setupConnections()
{
// Qt connect removed
// Qt connect removed
            this, [this](QAbstractButton*) { onPrecisionChanged(); });
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

void HardwareBackendSelector::detectAvailableBackends()
{
    m_backends.clear();

    // ===== CPU Backend (Always Available) =====
    BackendInfo cpuInfo;
    cpuInfo.backend = Backend::CPU;
    cpuInfo.name = "CPU (Fallback)";
    cpuInfo.version = "1.0";
    cpuInfo.available = true;
    cpuInfo.deviceName = "All CPU Cores";
    cpuInfo.computeCapability = "Native";
    cpuInfo.supportsFP16 = true;
    cpuInfo.supportsInt8 = true;
    cpuInfo.details = "CPU-only training. Slower but always available.\n"
                      "Best for development and debugging.\n"
                      "Supports all precision formats.";
    m_backends.push_back(cpuInfo);

    // ===== CUDA Backend Detection =====
    if (detectCuda()) {
        BackendInfo cudaInfo;
        cudaInfo.backend = Backend::CUDA;
        cudaInfo.name = "NVIDIA CUDA";
        cudaInfo.version = "12.0+";
        cudaInfo.available = true;
        cudaInfo.deviceName = "NVIDIA GPU";
        cudaInfo.computeCapability = "8.0+";
        cudaInfo.supportsFP16 = true;
        cudaInfo.supportsInt8 = true;
        cudaInfo.details = "NVIDIA CUDA GPU acceleration.\n"
                          "Requires NVIDIA GPU with CUDA Compute Capability 3.0+\n"
                          "Supports FP32, FP16, and INT8 precision.\n"
                          "Fastest for NVIDIA GPUs.";
        m_backends.push_back(cudaInfo);
    }

    // ===== Vulkan Backend Detection =====
    if (detectVulkan()) {
        BackendInfo vulkanInfo;
        vulkanInfo.backend = Backend::Vulkan;
        vulkanInfo.name = "Vulkan Compute";
        vulkanInfo.version = "1.3+";
        vulkanInfo.available = true;
        vulkanInfo.deviceName = "Compatible GPU";
        vulkanInfo.computeCapability = "Universal";
        vulkanInfo.supportsFP16 = true;
        vulkanInfo.supportsInt8 = false;
        vulkanInfo.details = "Cross-platform Vulkan compute.\n"
                            "Works on NVIDIA, AMD, Intel GPUs.\n"
                            "Good for compatibility and portability.\n"
                            "Supports FP32 and FP16 precision.";
        m_backends.push_back(vulkanInfo);
    }

    // ===== ROCm Backend Detection =====
    if (detectRocm()) {
        BackendInfo rocmInfo;
        rocmInfo.backend = Backend::ROCm;
        rocmInfo.name = "AMD ROCm";
        rocmInfo.version = "5.0+";
        rocmInfo.available = true;
        rocmInfo.deviceName = "AMD GPU";
        rocmInfo.computeCapability = "RDNA/CDNA";
        rocmInfo.supportsFP16 = true;
        rocmInfo.supportsInt8 = true;
        rocmInfo.details = "AMD GPU acceleration via ROCm.\n"
                          "Requires AMD GPU (RDNA or CDNA architecture).\n"
                          "Comparable performance to CUDA on AMD hardware.\n"
                          "Supports FP32, FP16, and INT8 precision.";
        m_backends.push_back(rocmInfo);
    }

    // ===== oneAPI Backend Detection =====
    if (detectOneAPI()) {
        BackendInfo oneapiInfo;
        oneapiInfo.backend = Backend::OneAPI;
        oneapiInfo.name = "Intel oneAPI";
        oneapiInfo.version = "2022.0+";
        oneapiInfo.available = true;
        oneapiInfo.deviceName = "Intel GPU/Accelerator";
        oneapiInfo.computeCapability = "Gen9+";
        oneapiInfo.supportsFP16 = true;
        oneapiInfo.supportsInt8 = true;
        oneapiInfo.details = "Intel GPU acceleration via oneAPI.\n"
                            "Requires Intel Arc or Data Center GPU.\n"
                            "Good for Intel-based systems.\n"
                            "Supports FP32, FP16, and INT8 precision.";
        m_backends.push_back(oneapiInfo);
    }

    // ===== Metal Backend Detection =====
    if (detectMetal()) {
        BackendInfo metalInfo;
        metalInfo.backend = Backend::Metal;
        metalInfo.name = "Apple Metal";
        metalInfo.version = "2.3+";
        metalInfo.available = true;
        metalInfo.deviceName = "Apple Silicon/GPU";
        metalInfo.computeCapability = "M1+";
        metalInfo.supportsFP16 = true;
        metalInfo.supportsInt8 = true;
        metalInfo.details = "Apple Metal GPU acceleration.\n"
                           "Works on Apple Silicon (M1/M2/M3) and discrete GPUs.\n"
                           "Optimized for macOS and iOS.\n"
                           "Supports FP32, FP16, and INT8 precision.";
        m_backends.push_back(metalInfo);
    }

    loadBackendCapabilities();
}

bool HardwareBackendSelector::detectCuda()
{
    // Probe for NVIDIA CUDA driver DLL
    HMODULE hLib = LoadLibraryA("nvcuda.dll");
    if (hLib) {
        FreeLibrary(hLib);
        return true;
    }
    return false;
}

bool HardwareBackendSelector::detectVulkan()
{
    // Probe for Vulkan loader DLL
    HMODULE hLib = LoadLibraryA("vulkan-1.dll");
    if (hLib) {
        FreeLibrary(hLib);
        return true;
    }
    return false;
}

bool HardwareBackendSelector::detectRocm()
{
    // Probe for AMD HIP runtime DLL
    HMODULE hLib = LoadLibraryA("amdhip64.dll");
    if (hLib) {
        FreeLibrary(hLib);
        return true;
    }
    // Also check hiprt64.dll (older naming)
    hLib = LoadLibraryA("hiprt64.dll");
    if (hLib) {
        FreeLibrary(hLib);
        return true;
    }
    return false;
}

bool HardwareBackendSelector::detectOneAPI()
{
    // Probe for Intel oneAPI Level Zero loader
    HMODULE hLib = LoadLibraryA("ze_loader.dll");
    if (hLib) {
        FreeLibrary(hLib);
        return true;
    }
    return false;
}

bool HardwareBackendSelector::detectMetal()
{
    // Metal is only available on macOS
    return false;
}

void HardwareBackendSelector::populateBackendList()
{
    m_backendCombo->clear();
    
    for (const auto& backend : m_backends) {
        std::string displayName = backend.name;
        if (backend.available) {
            displayName += " [Available]";
        } else {
            displayName += " [Unavailable]";
        }
        
        m_backendCombo->addItem(displayName, static_cast<int>(backend.backend));
    }

    if (!m_backendCombo->count()) {
        m_backendCombo->addItem("CPU (Fallback)", static_cast<int>(Backend::CPU));
    }
}

void HardwareBackendSelector::onBackendSelected(int index)
{
    if (index < 0 || index >= static_cast<int>(m_backends.size())) {
        return;
    }

    const auto& info = m_backends[index];
    m_selectedBackend = info.backend;

    m_detailsText->setText(info.details);
    m_vramLabel->setText(info.vramBytes > 0 
        ? std::string::number(info.vramBytes / (1024 * 1024 * 1024)) + " GB"
        : "Unknown");
    
    m_deviceInfoLabel->setText(std::string("Device: %1 | Compute: %2")
        
        );

    // Update supported precision options
    m_fp16Radio->setEnabled(info.supportsFP16);
    m_int8Radio->setEnabled(info.supportsInt8);

    if (!info.supportsFP16 && m_fp16Radio->isChecked()) {
        m_fp32Radio->setChecked(true);
    }
    if (!info.supportsInt8 && m_int8Radio->isChecked()) {
        m_fp32Radio->setChecked(true);
    }

    backendSelected(static_cast<int>(m_selectedBackend));
}

void HardwareBackendSelector::onPrecisionChanged()
{
    if (m_fp16Radio->isChecked()) {
        m_fp16Enabled = true;
        m_int8Enabled = false;
        m_vramUsageLabel->setText("~50% of FP32");
    } else if (m_int8Radio->isChecked()) {
        m_fp16Enabled = false;
        m_int8Enabled = true;
        m_vramUsageLabel->setText("~25% of FP32");
    } else {
        m_fp16Enabled = false;
        m_int8Enabled = false;
        m_vramUsageLabel->setText("100% baseline");
    }
}

void HardwareBackendSelector::onDetectHardware()
{
    m_backends.clear();
    detectAvailableBackends();
    populateBackendList();
    
    QMessageBox::information(this, "Hardware Detection",
        std::string("Detected %1 available backend(s)")));
}

void HardwareBackendSelector::onResetToDefaults()
{
    m_backendCombo->setCurrentIndex(0);
    m_fp32Radio->setChecked(true);
    m_memoryPoolCombo->setCurrentIndex(0);
    m_fp16Enabled = false;
    m_int8Enabled = false;
    m_memoryPoolMB = 1024;
}

void HardwareBackendSelector::onApplyConfiguration()
{
    void* config = getBackendConfig();
    configurationChanged(config);
    backendConfirmed(static_cast<int>(m_selectedBackend));
    
    accept();
}

void HardwareBackendSelector::loadBackendCapabilities()
{
    // This would load actual GPU capabilities from the system
    // For now, we use placeholder values
}

HardwareBackendSelector::Backend HardwareBackendSelector::getSelectedBackend() const
{
    return m_selectedBackend;
}

std::string HardwareBackendSelector::getSelectedBackendName() const
{
    for (const auto& backend : m_backends) {
        if (backend.backend == m_selectedBackend) {
            return backend.name;
        }
    }
    return "Unknown";
}

void* HardwareBackendSelector::getBackendConfig() const
{
    void* config;
    config["backend"] = static_cast<int>(m_selectedBackend);
    config["backendName"] = getSelectedBackendName();
    config["device"] = m_deviceCombo->currentText();
    config["precision"] = m_fp16Enabled ? "fp16" : (m_int8Enabled ? "int8" : "fp32");
    config["memoryPool"] = m_memoryPoolCombo->currentData().toInt();
    config["memoryPoolMB"] = m_memoryPoolMB;
    
    return config;
}

void HardwareBackendSelector::setBackendConfig(const void*& config)
{
    int backend = config["backend"].toInt(0);
    for (int i = 0; i < m_backendCombo->count(); ++i) {
        if (m_backendCombo->itemData(i).toInt() == backend) {
            m_backendCombo->setCurrentIndex(i);
            break;
        }
    }

    std::string precision = config["precision"].toString("fp32");
    if (precision == "fp16") {
        m_fp16Radio->setChecked(true);
    } else if (precision == "int8") {
        m_int8Radio->setChecked(true);
    } else {
        m_fp32Radio->setChecked(true);
    }
}

bool HardwareBackendSelector::isBackendAvailable(Backend backend) const
{
    for (const auto& b : m_backends) {
        if (b.backend == backend && b.available) {
            return true;
        }
    }
    return false;
}

std::vector<HardwareBackendSelector::BackendInfo> HardwareBackendSelector::getAvailableBackends() const
{
    return m_backends;
}

void HardwareBackendSelector::onMemoryPoolChanged()
{
    // Update memory pool selection and signal
    configurationChanged(getBackendConfig());
}

