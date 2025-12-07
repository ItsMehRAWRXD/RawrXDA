// ollama_hotpatch_proxy.hpp - Ollama-specific hotpatch proxy with memory injection
#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QHash>
#include <QQueue>
#include <QTimer>
#include <functional>

// Ollama-specific patch rule
struct OllamaHotpatchRule {
    QString name;
    QString description;
    bool enabled = true;
    
    enum RuleType {
        ParameterInjection,      // Inject/override request parameters
        ResponseTransform,       // Transform response data
        MemoryBypass,           // Bypass safety checks via memory manipulation
        TokenBiasing,           // Bias token probabilities
        ContextInjection,       // Inject system context
        LayerSkipping,          // Skip inference layers
        WeightModification      // Direct weight tensor modification
    };
    RuleType ruleType;
    
    // Rule parameters
    QString targetModel;        // Model name pattern (empty = all)
    QHash<QString, QVariant> parameters;
    QByteArray searchPattern;
    QByteArray replacementData;
    int priority = 0;
    
    // Custom transformation function
    std::function<QByteArray(const QByteArray&)> customTransform;
};

// Ollama request/response structure
struct OllamaMessage {
    QString role;               // "user", "assistant", "system"
    QString content;
    QJsonObject metadata;
};

class OllamaHotpatchProxy : public QObject {
    Q_OBJECT

public:
    explicit OllamaHotpatchProxy(QObject* parent = nullptr);
    ~OllamaHotpatchProxy() override;

    // Rule management
    void addRule(const OllamaHotpatchRule& rule);
    void removeRule(const QString& name);
    void enableRule(const QString& name, bool enable);
    bool hasRule(const QString& name) const;
    OllamaHotpatchRule getRule(const QString& name) const;
    QStringList listRules(const QString& modelPattern = QString()) const;
    void clearAllRules();
    void setPriorityOrder(const QStringList& ruleNames);

    // Request processing (pre-inference)
    QJsonObject processRequestJson(const QJsonObject& request);
    QByteArray processRequestBytes(const QByteArray& requestData);
    
    // Response processing (post-inference)
    QJsonObject processResponseJson(const QJsonObject& response);
    QByteArray processResponseBytes(const QByteArray& responseData);
    
    // Streaming chunk processing
    QByteArray processStreamChunk(const QByteArray& chunk, int chunkIndex);
    void beginStreamProcessing(const QString& streamId);
    void endStreamProcessing(const QString& streamId);

    // Direct memory manipulation for requests
    PatchResult injectIntoRequest(const QString& key, const QVariant& value);
    PatchResult injectIntoRequestBatch(const QHash<QString, QVariant>& injections);
    QVariant extractFromRequest(const QString& key) const;
    QHash<QString, QVariant> extractAllRequestParams() const;

    // Direct memory manipulation for responses
    PatchResult modifyInResponse(const QString& jsonPath, const QVariant& newValue);
    QVariant readFromResponse(const QString& jsonPath) const;
    
    // Parameter override system
    void setParameterOverride(const QString& paramName, const QVariant& value);
    void clearParameterOverride(const QString& paramName);
    QHash<QString, QVariant> getParameterOverrides() const;
    
    // Model-specific targeting
    bool matchesModel(const QString& modelName, const QString& pattern) const;
    void setActiveModel(const QString& modelName);
    QString getActiveModel() const;

    // Caching for performance
    void setResponseCachingEnabled(bool enable);
    bool isResponseCachingEnabled() const;
    void clearResponseCache();
    
    // Statistics
    struct Stats {
        qint64 requestsProcessed = 0;
        qint64 responsesProcessed = 0;
        qint64 chunksProcessed = 0;
        qint64 rulesApplied = 0;
        qint64 bytesModified = 0;
        qint64 cachesHits = 0;
        qint64 transformationsApplied = 0;
        double avgProcessingTimeMs = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/Disable entire proxy
    void setEnabled(bool enable);
    bool isEnabled() const;
    
    // Diagnostics
    void enableDiagnostics(bool enable);
    QStringList getDiagnosticLog() const;
    void clearDiagnosticLog();

signals:
    void ruleApplied(const QString& name, const QString& type);
    void requestModified(const QJsonObject& original, const QJsonObject& modified);
    void responseModified(const QJsonObject& original, const QJsonObject& modified);
    void parameterInjected(const QString& paramName, const QVariant& value);
    void streamChunkProcessed(int chunkIndex, int bytesModified);
    void modelChanged(const QString& modelName);
    void errorOccurred(const QString& error);
    void diagnosticMessage(const QString& message);

private:
    // Helper methods
    QJsonObject applyParameterInjection(const QJsonObject& request, const OllamaHotpatchRule& rule);
    QJsonObject applyResponseTransform(const QJsonObject& response, const OllamaHotpatchRule& rule);
    QJsonObject applyContextInjection(const QJsonObject& request, const QString& context);
    QByteArray applyBytePatching(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement);
    
    bool shouldApplyRule(const OllamaHotpatchRule& rule, const QString& modelName) const;
    QString getCacheKey(const QJsonObject& request) const;
    
    void logDiagnostic(const QString& message);

    // Data members
    mutable QMutex m_mutex;
    QHash<QString, OllamaHotpatchRule> m_rules;
    QStringList m_ruleOrder;            // Priority-ordered rule names
    QHash<QString, QVariant> m_parameterOverrides;
    QHash<QString, QJsonObject> m_responseCache;
    
    QString m_activeModel;
    Stats m_stats;
    bool m_enabled = true;
    bool m_cachingEnabled = false;
    bool m_diagnosticsEnabled = false;
    QStringList m_diagnosticLog;
    
    // Active streams being processed
    QHash<QString, int> m_activeStreams;
    
    QTimer* m_statsReportTimer = nullptr;
};

#endif // OLLAMA_HOTPATCH_PROXY_HPP
