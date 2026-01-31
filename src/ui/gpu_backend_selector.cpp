// GPU Backend Quick Selector - Implementation
#include "gpu_backend_selector.h"


#ifdef _WIN32
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace RawrXD {

GPUBackendSelector::GPUBackendSelector(void* parent)
    : void(parent)
{
    setupUI();
    detectBackends();
    
}

void GPUBackendSelector::setupUI() {
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2);
    layout->setSpacing(8);
    
    // Icon label
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(16, 16);
    m_iconLabel->setStyleSheet("color: #569cd6;");
    m_iconLabel->setText("🖥");
    layout->addWidget(m_iconLabel);
    
    // Backend combo
    m_backendCombo = new QComboBox(this);
    m_backendCombo->setMinimumWidth(120);
    m_backendCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: #2d2d30;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 3px 8px;"
        "  border-radius: 3px;"
        "}"
        "QComboBox:hover {"
        "  border: 1px solid #569cd6;"
        "}"
        "QComboBox::drop-down {"
        "  border: none;"
        "  width: 20px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #2d2d30;"
        "  color: #d4d4d4;"
        "  selection-background-color: #0e639c;"
        "}"
    );
// Qt connect removed
    layout->addWidget(m_backendCombo);
    
    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #888888; font-size: 10px;");
    m_statusLabel->setText("Detecting...");
    layout->addWidget(m_statusLabel);
    
    layout->addStretch();
    
    setStyleSheet("void { background-color: transparent; }");
}

void GPUBackendSelector::detectBackends() {
    
    m_availableBackends.clear();
    
    // CPU is always available
    BackendInfo cpuInfo;
    cpuInfo.backend = ComputeBackend::CPU;
    cpuInfo.displayName = "CPU";
    cpuInfo.icon = "💻";
    cpuInfo.available = true;
    cpuInfo.deviceName = "x86_64";
    m_availableBackends.append(cpuInfo);
    
#ifdef _WIN32
    // Detect CUDA
    QProcess nvidiaSmi;
    nvidiaSmi.start("nvidia-smi", std::vector<std::string>() << "--query-gpu=name,memory.total" << "--format=csv,noheader");
    if (nvidiaSmi.waitForFinished(2000)) {
        std::string output = nvidiaSmi.readAllStandardOutput().trimmed();
        if (!output.isEmpty()) {
            std::vector<std::string> parts = output.split(',');
            
            BackendInfo cudaInfo;
            cudaInfo.backend = ComputeBackend::CUDA;
            cudaInfo.displayName = "CUDA";
            cudaInfo.icon = "🎮";
            cudaInfo.available = true;
            cudaInfo.deviceName = parts.value(0).trimmed();
            
            if (parts.size() > 1) {
                std::string memStr = parts[1].trimmed();
                std::regex memRegex(R"((\d+)\s*MiB)");
                auto match = memRegex.match(memStr);
                if (match.hasMatch()) {
                    cudaInfo.vramMB = match"".toInt();
                }
            }
            
            m_availableBackends.append(cudaInfo);
                     << "VRAM:" << cudaInfo.vramMB << "MB";
        }
    }
    
    // Detect Vulkan
    QProcess vulkanInfo;
    vulkanInfo.start("vulkaninfo", std::vector<std::string>() << "--summary");
    if (vulkanInfo.waitForFinished(2000)) {
        std::string output = vulkanInfo.readAllStandardOutput();
        if (output.contains("Vulkan Instance Version")) {
            BackendInfo vulkanInfo;
            vulkanInfo.backend = ComputeBackend::Vulkan;
            vulkanInfo.displayName = "Vulkan";
            vulkanInfo.icon = "⚡";
            vulkanInfo.available = true;
            
            // Try to extract GPU name from DXGI
            IDXGIFactory* pFactory = nullptr;
            if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory))) {
                IDXGIAdapter* pAdapter = nullptr;
                if (SUCCEEDED(pFactory->EnumAdapters(0, &pAdapter))) {
                    DXGI_ADAPTER_DESC desc;
                    pAdapter->GetDesc(&desc);
                    vulkanInfo.deviceName = std::string::fromWCharArray(desc.Description);
                    vulkanInfo.vramMB = static_cast<int>(desc.DedicatedVideoMemory / (1024 * 1024));
                    pAdapter->Release();
                }
                pFactory->Release();
            }
            
            m_availableBackends.append(vulkanInfo);
        }
    }
    
    // Detect DirectML
    BackendInfo dmlInfo;
    dmlInfo.backend = ComputeBackend::DirectML;
    dmlInfo.displayName = "DirectML";
    dmlInfo.icon = "🪟";
    dmlInfo.available = true;  // Available on Windows 10+
    dmlInfo.deviceName = "Windows ML";
    m_availableBackends.append(dmlInfo);
#endif
    
    // Auto mode
    BackendInfo autoInfo;
    autoInfo.backend = ComputeBackend::Auto;
    autoInfo.displayName = "Auto";
    autoInfo.icon = "🔄";
    autoInfo.available = true;
    autoInfo.deviceName = "Best Available";
    m_availableBackends.append(autoInfo);
    
    // Populate combo box
    m_backendCombo->clear();
    for (const auto& backend : m_availableBackends) {
        if (backend.available) {
            std::string itemText = std::string("%1 %2");
            if (!backend.deviceName.isEmpty() && backend.backend != ComputeBackend::Auto) {
                itemText += std::string(" (%1)");
            }
            m_backendCombo->addItem(itemText, std::any::fromValue(static_cast<int>(backend.backend)));
        }
    }
    
    // Select Auto by default
    for (int i = 0; i < m_backendCombo->count(); ++i) {
        if (static_cast<ComputeBackend>(m_backendCombo->itemData(i).toInt()) == ComputeBackend::Auto) {
            m_backendCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_statusLabel->setText(std::string("✓ %1 backends")));
    
}

void GPUBackendSelector::refreshBackends() {
    detectBackends();
}

ComputeBackend GPUBackendSelector::selectedBackend() const {
    return m_currentBackend;
}

void GPUBackendSelector::setBackend(ComputeBackend backend) {
    for (int i = 0; i < m_backendCombo->count(); ++i) {
        if (static_cast<ComputeBackend>(m_backendCombo->itemData(i).toInt()) == backend) {
            m_backendCombo->setCurrentIndex(i);
            return;
        }
    }
    
}

bool GPUBackendSelector::isBackendAvailable(ComputeBackend backend) const {
    for (const auto& info : m_availableBackends) {
        if (info.backend == backend && info.available) {
            return true;
        }
    }
    return false;
}

void GPUBackendSelector::onBackendChanged(int index) {
    if (index < 0 || index >= m_backendCombo->count()) return;
    
    ComputeBackend newBackend = static_cast<ComputeBackend>(m_backendCombo->itemData(index).toInt());
    
    if (newBackend != m_currentBackend) {
                 << backendToString(m_currentBackend) << "->" << backendToString(newBackend);
        
        m_currentBackend = newBackend;
        
        // Update icon
        for (const auto& info : m_availableBackends) {
            if (info.backend == newBackend) {
                m_iconLabel->setText(info.icon);
                
                std::string statusMsg = std::string("Active: %1");
                if (info.vramMB > 0) {
                    statusMsg += std::string(" (%1 GB)");
                }
                m_statusLabel->setText(statusMsg);
                break;
            }
        }
        
        backendChanged(newBackend);
    }
}

std::string GPUBackendSelector::backendToString(ComputeBackend backend) const {
    switch (backend) {
        case ComputeBackend::CPU: return "CPU";
        case ComputeBackend::CUDA: return "CUDA";
        case ComputeBackend::Vulkan: return "Vulkan";
        case ComputeBackend::Metal: return "Metal";
        case ComputeBackend::DirectML: return "DirectML";
        case ComputeBackend::Auto: return "Auto";
        default: return "Unknown";
    }
}

std::string GPUBackendSelector::backendToIcon(ComputeBackend backend) const {
    switch (backend) {
        case ComputeBackend::CPU: return "💻";
        case ComputeBackend::CUDA: return "🎮";
        case ComputeBackend::Vulkan: return "⚡";
        case ComputeBackend::Metal: return "🍎";
        case ComputeBackend::DirectML: return "🪟";
        case ComputeBackend::Auto: return "🔄";
        default: return "❓";
    }
}

} // namespace RawrXD

