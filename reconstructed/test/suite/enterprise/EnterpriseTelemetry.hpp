#ifndef ENTERPRISE_TELEMETRY_HPP
#define ENTERPRISE_TELEMETRY_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QScopedPointer>

class EnterpriseTelemetryPrivate;

class EnterpriseTelemetry : public QObject {
    Q_OBJECT
public:
    static EnterpriseTelemetry* instance();
    
    // OpenTelemetry integration
    void initializeOpenTelemetry();
    void recordMissionSpan(const QString& missionId, 
                          const QString& missionType,
                          double durationMs,
                          bool success,
                          const QString& errorMessage = QString());
    void recordToolExecutionSpan(const QString& toolName,
                                const QStringList& parameters,
                                double durationMs,
                                bool success,
                                int exitCode);
    
    // Prometheus metrics export
    void exportToPrometheus();
    
    // Enterprise telemetry configuration
    void setTelemetryEnabled(bool enabled);
    bool isTelemetryEnabled() const;
    
    // Performance metrics
    double getMissionSuccessRate();
    double getToolExecutionSuccessRate();
    
private:
    explicit EnterpriseTelemetry(QObject* parent = nullptr);
    ~EnterpriseTelemetry();
    
    // Helper functions
    QString generateTraceId();
    QString generateSpanId();
    
    QScopedPointer<EnterpriseTelemetryPrivate> d_ptr;
    
    Q_DISABLE_COPY(EnterpriseTelemetry)
};

#endif // ENTERPRISE_TELEMETRY_HPP