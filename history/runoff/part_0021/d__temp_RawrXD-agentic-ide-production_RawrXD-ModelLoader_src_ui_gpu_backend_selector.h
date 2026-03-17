// GPU Backend Quick Selector - Compact widget for toolbar/status bar
// Production-ready with live backend switching
#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QString>
#include <QIcon>

namespace RawrXD {

enum class ComputeBackend {
    CPU,
    CUDA,
    Vulkan,
    Metal,       // For future macOS support
    DirectML,    // For Windows ML acceleration
    Auto         // Automatic selection
};

struct BackendInfo {
    ComputeBackend backend;
    QString displayName;
    QString icon;
    bool available{false};
    QString deviceName;
    int vramMB{0};
};

class GPUBackendSelector : public QWidget {
    Q_OBJECT

public:
    explicit GPUBackendSelector(QWidget* parent = nullptr);
    ~GPUBackendSelector() override = default;

    // Get currently selected backend
    ComputeBackend selectedBackend() const;
    
    // Set backend programmatically
    void setBackend(ComputeBackend backend);
    
    // Query backend availability
    bool isBackendAvailable(ComputeBackend backend) const;
    
    // Refresh available backends
    void refreshBackends();

signals:
    void backendChanged(ComputeBackend backend);
    void backendSwitchFailed(const QString& error);

private slots:
    void onBackendChanged(int index);

private:
    void setupUI();
    void detectBackends();
    QString backendToString(ComputeBackend backend) const;
    QString backendToIcon(ComputeBackend backend) const;
    
    // UI Components
    QLabel* m_iconLabel{nullptr};
    QComboBox* m_backendCombo{nullptr};
    QLabel* m_statusLabel{nullptr};
    
    // State
    QVector<BackendInfo> m_availableBackends;
    ComputeBackend m_currentBackend{ComputeBackend::Auto};
};

} // namespace RawrXD
