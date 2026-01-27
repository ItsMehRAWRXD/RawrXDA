// GPU Backend Quick Selector - Implementation
#include "gpu_backend_selector.h"
#include <QHBoxLayout>
#include <QDebug>
#include <QDateTime>
#include <QProcess>
#include <QRegularExpression>

#ifdef _WIN32
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib, "dxgi.lib")
#endif

namespace RawrXD {

GPUBackendSelector::GPUBackendSelector(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    detectBackends();
    
    qDebug() << "[GPUBackendSelector] Initialized at" << QDateTime::currentDateTime().toString(Qt::ISODate);
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
    connect(m_backendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GPUBackendSelector::onBackendChanged);
    layout->addWidget(m_backendCombo);
    
    // Status label
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("color: #888888; font-size: 10px;");
    m_statusLabel->setText("Detecting...");
    layout->addWidget(m_statusLabel);
    
    layout->addStretch();
    
    setStyleSheet("QWidget { background-color: transparent; }");
}

void GPUBackendSelector::detectBackends() {
    qDebug() << "[GPUBackendSelector] Detecting available compute backends...";
    
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
    nvidiaSmi.start("nvidia-smi", QStringList() << "--query-gpu=name,memory.total" << "--format=csv,noheader");
    if (nvidiaSmi.waitForFinished(2000)) {
        QString output = nvidiaSmi.readAllStandardOutput().trimmed();
        if (!output.isEmpty()) {
            QStringList parts = output.split(',');
            
            BackendInfo cudaInfo;
            cudaInfo.backend = ComputeBackend::CUDA;
            cudaInfo.displayName = "CUDA";
            cudaInfo.icon = "🎮";
            cudaInfo.available = true;
            cudaInfo.deviceName = parts.value(0).trimmed();
            
            if (parts.size() > 1) {
                QString memStr = parts[1].trimmed();
                QRegularExpression memRegex(R"((\d+)\s*MiB)");
                auto match = memRegex.match(memStr);
                if (match.hasMatch()) {
                    cudaInfo.vramMB = match.captured(1).toInt();
                }
            }
            
            m_availableBackends.append(cudaInfo);
            qDebug() << "  [GPUBackendSelector] CUDA detected:" << cudaInfo.deviceName 
                     << "VRAM:" << cudaInfo.vramMB << "MB";
        }
    }
    
    // Detect Vulkan
    QProcess vulkanInfo;
    vulkanInfo.start("vulkaninfo", QStringList() << "--summary");
    if (vulkanInfo.waitForFinished(2000)) {
        QString output = vulkanInfo.readAllStandardOutput();
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
                    vulkanInfo.deviceName = QString::fromWCharArray(desc.Description);
                    vulkanInfo.vramMB = static_cast<int>(desc.DedicatedVideoMemory / (1024 * 1024));
                    pAdapter->Release();
                }
                pFactory->Release();
            }
            
            m_availableBackends.append(vulkanInfo);
            qDebug() << "  [GPUBackendSelector] Vulkan detected:" << vulkanInfo.deviceName;
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
    qDebug() << "  [GPUBackendSelector] DirectML available (Windows)";
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
            QString itemText = QString("%1 %2").arg(backend.icon, backend.displayName);
            if (!backend.deviceName.isEmpty() && backend.backend != ComputeBackend::Auto) {
                itemText += QString(" (%1)").arg(backend.deviceName);
            }
            m_backendCombo->addItem(itemText, QVariant::fromValue(static_cast<int>(backend.backend)));
        }
    }
    
    // Select Auto by default
    for (int i = 0; i < m_backendCombo->count(); ++i) {
        if (static_cast<ComputeBackend>(m_backendCombo->itemData(i).toInt()) == ComputeBackend::Auto) {
            m_backendCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_statusLabel->setText(QString("✓ %1 backends").arg(m_availableBackends.size()));
    
    qDebug() << "[GPUBackendSelector] Detection complete, found" << m_availableBackends.size() << "backends";
}

void GPUBackendSelector::refreshBackends() {
    qDebug() << "[GPUBackendSelector] Refreshing backend list...";
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
    
    qWarning() << "[GPUBackendSelector] Backend not found:" << backendToString(backend);
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
        qDebug() << "[GPUBackendSelector] Backend changed:" 
                 << backendToString(m_currentBackend) << "->" << backendToString(newBackend);
        
        m_currentBackend = newBackend;
        
        // Update icon
        for (const auto& info : m_availableBackends) {
            if (info.backend == newBackend) {
                m_iconLabel->setText(info.icon);
                
                QString statusMsg = QString("Active: %1").arg(info.displayName);
                if (info.vramMB > 0) {
                    statusMsg += QString(" (%1 GB)").arg(info.vramMB / 1024.0, 0, 'f', 1);
                }
                m_statusLabel->setText(statusMsg);
                break;
            }
        }
        
        emit backendChanged(newBackend);
    }
}

QString GPUBackendSelector::backendToString(ComputeBackend backend) const {
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

QString GPUBackendSelector::backendToIcon(ComputeBackend backend) const {
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
