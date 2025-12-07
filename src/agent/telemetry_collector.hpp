#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QHash>

/**
 * @brief Privacy-respecting telemetry for feature usage and crash analysis
 * 
 * PRODUCTION-READY PRIVACY GUARANTEES:
 * - Opt-in only (disabled by default, requires user consent)
 * - No PII collection (no usernames, emails, IP addresses)
 * - Anonymous session IDs (no persistent user tracking)
 * - Local aggregation before sending (minimize data transmission)
 * - GDPR/CCPA compliant data retention (30 days)
 * - User can view/delete all collected data
 * - Configurable via environment: TELEMETRY_ENABLED=1
 */
class TelemetryCollector : public QObject
{
    Q_OBJECT

public:
    static TelemetryCollector* instance();
    
    /**
     * @brief Initialize telemetry (checks user consent)
     * @return true if user has opted in
     */
    bool initialize();
    
    /**
     * @brief Check if user has consented to telemetry
     * @return true if telemetry is enabled
     */
    bool isEnabled() const { return m_enabled; }
    
    /**
     * @brief Enable telemetry (stores user consent)
     */
    void enableTelemetry();
    
    /**
     * @brief Disable telemetry (removes consent)
     */
    void disableTelemetry();
    
    /**
     * @brief Track feature usage (no PII)
     * @param featureName Feature identifier (e.g., "model.load", "inference.generate")
     * @param metadata Additional context (no PII allowed)
     */
    void trackFeatureUsage(const QString& featureName, const QJsonObject& metadata = QJsonObject());
    
    /**
     * @brief Track application crash (minimal data)
     * @param crashReason Crash reason (sanitized, no PII)
     */
    void trackCrash(const QString& crashReason);
    
    /**
     * @brief Track performance metric
     * @param metricName Metric identifier (e.g., "inference.latency")
     * @param value Numeric value
     * @param unit Unit of measurement (ms, MB, etc.)
     */
    void trackPerformance(const QString& metricName, double value, const QString& unit = QString());
    
    /**
     * @brief Get all collected telemetry data (for user transparency)
     * @return JSON object with all telemetry
     */
    QJsonObject getAllTelemetryData() const;
    
    /**
     * @brief Clear all telemetry data (user-initiated)
     */
    void clearAllData();
    
    /**
     * @brief Flush aggregated data to server (respects opt-in)
     */
    void flushData();

signals:
    void telemetryEnabled();
    void telemetryDisabled();
    void dataFlushed(int eventCount);

private:
    explicit TelemetryCollector(QObject* parent = nullptr);
    ~TelemetryCollector();
    
    static TelemetryCollector* s_instance;
    
    bool m_enabled;
    QString m_sessionId;  ///< Anonymous session identifier (not persistent)
    QHash<QString, int> m_featureUsage;  ///< Feature name -> usage count
    QList<QJsonObject> m_events;  ///< Buffered events for batch sending
    qint64 m_sessionStartTime;  ///< Session start timestamp
    
    // PRODUCTION-READY: Sanitize data to remove any PII
    QString sanitize(const QString& input) const;
    
    // PRODUCTION-READY: Send telemetry to server (non-blocking)
    void sendTelemetry(const QJsonObject& payload);
    
    // PRODUCTION-READY: Load user consent from settings
    bool loadUserConsent() const;
    
    // PRODUCTION-READY: Save user consent to settings
    void saveUserConsent(bool enabled);
};
