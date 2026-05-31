#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QObject>
#include <cstdint>
#include <string>

// Telemetry snapshot structure
struct TelemetrySnapshot {
    uint64_t timeMs = 0;
    
    // CPU metrics
    bool cpuTempValid = false;
    double cpuTempC = 0.0;
    double cpuUsagePercent = 0.0;
    
    // GPU metrics
    bool gpuTempValid = false;
    double gpuTempC = 0.0;
    double gpuUsagePercent = 0.0;
    std::string gpuVendor;
};

namespace telemetry {
    bool Initialize();
    bool InitializeHardware();  // Two-phase init: call after QApplication exists
    bool Poll(TelemetrySnapshot& out);
    void Shutdown();
}

class Telemetry : public QObject {
    Q_OBJECT
public:
    Telemetry();
    ~Telemetry();
    
    // Two-phase initialization: call this after QApplication is running
    void initializeHardware();
    
    void recordEvent(const QString& event_name, const QJsonObject& metadata = QJsonObject());
    bool saveTelemetry(const QString& filepath);
    void enableTelemetry(bool enable);
    
private:
    bool is_enabled_;
    QJsonArray events_;
};
