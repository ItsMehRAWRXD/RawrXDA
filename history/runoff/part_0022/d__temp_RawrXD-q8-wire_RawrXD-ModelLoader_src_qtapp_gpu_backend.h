#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <vector>
#include <map>

class GPUBackend : public QObject {
    Q_OBJECT

public:
    enum class BackendType {
        Vulkan,
        ROCm,
        CUDA,
        OpenCL,
        CPU
    };

    explicit GPUBackend(QObject* parent = nullptr);
    ~GPUBackend();

    // Backend initialization
    bool initialize(BackendType type, const QJsonObject& config = QJsonObject());
    bool isInitialized() const;

    // Device management
    QJsonArray getAvailableDevices() const;
    bool selectDevice(int deviceId);
    QJsonObject getDeviceInfo(int deviceId) const;

    // Memory management
    size_t getTotalMemory() const;
    size_t getFreeMemory() const;
    bool allocateBuffer(size_t size, const QString& bufferId);
    bool freeBuffer(const QString& bufferId);

    // Computation
    bool executeKernel(const QString& kernelName, const QJsonObject& parameters);
    QJsonObject benchmarkKernel(const QString& kernelName, const QJsonObject& parameters);

    // Monitoring
    QJsonObject getPerformanceStats() const;
    float getUtilization() const;

signals:
    void backendInitialized(BackendType type, bool success);
    void deviceSelected(int deviceId);
    void memoryAllocated(const QString& bufferId, size_t size);
    void kernelExecuted(const QString& kernelName, bool success);

private:
    BackendType m_backendType;
    bool m_initialized;
    int m_selectedDevice;
    QJsonObject m_config;
    std::map<QString, size_t> m_buffers;
};
