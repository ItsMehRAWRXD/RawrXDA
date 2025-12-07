// proxy_hotpatcher.hpp - Agentic correction proxy with token reverse proxy byte hacking
#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QHash>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <functional>
#include "model_memory_hotpatch.hpp"

// Agent output validation result
struct AgentValidation {
    bool isValid = true;
    QString errorMessage;
    QString correctedOutput;
    QStringList violations;
    
    static AgentValidation valid() {
        return AgentValidation{true, QString(), QString(), QStringList()};
    }
    
    static AgentValidation invalid(const QString& error, const QString& corrected = QString()) {
        return AgentValidation{false, error, corrected, QStringList()};
    }
};

// Byte-level pattern matching result
struct PatternMatch {
    qint64 position = -1;
    qint64 length = 0;
    QByteArray matchedData;
    
    bool isValid() const { return position >= 0; }
};

// Proxy hotpatch rule
struct ProxyHotpatchRule {
    QString name;
    bool enabled = true;
    
    enum RuleType {
        ParameterOverride,        // Override request parameters
        ResponseCorrection,       // Fix agent output errors
        StreamTermination,        // RST injection
        AgentValidation,          // Validate agent responses
        MemoryInjection,          // Direct byte patching
        TokenLogitBias           // Bias token probabilities
    };
    RuleType type;
    
    // Rule-specific data
    QByteArray searchPattern;
    QByteArray replacement;
    QString parameterName;
    QVariant parameterValue;
    int abortAfterChunks = -1;
    
    // Agent validation rules
    QStringList forbiddenPatterns;
    QStringList requiredPatterns;
    bool enforcePlanFormat = false;
    bool enforceAgentFormat = false;
    
    // Custom validator callback pointer (store as void* to avoid template issues)
    void* customValidator = nullptr;
};

class ProxyHotpatcher : public QObject
{
    Q_OBJECT

public:
    explicit ProxyHotpatcher(QObject* parent = nullptr);
    ~ProxyHotpatcher() override;

    // Rule management
    void addRule(const ProxyHotpatchRule& rule);
    void removeRule(const QString& name);
    void enableRule(const QString& name, bool enable);
    bool hasRule(const QString& name) const;
    ProxyHotpatchRule getRule(const QString& name) const;
    QStringList listRules() const;
    void clearAllRules();

    // Request processing (Memory Injection via Proxy)
    QByteArray processRequest(const QByteArray& requestData);
    QJsonObject processRequestJson(const QJsonObject& request);
    
    // Response processing (Agent Correction)
    QByteArray processResponse(const QByteArray& responseData);
    QJsonObject processResponseJson(const QJsonObject& response);
    QByteArray processStreamChunk(const QByteArray& chunk, int chunkIndex);
    
    // Zero-copy byte patching (production-ready)
    QByteArray bytePatchInPlace(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement);
    PatternMatch findPattern(const QByteArray& data, const QByteArray& pattern, qint64 startPos = 0) const;
    QByteArray findAndReplace(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement);
    
    // Boyer-Moore pattern matching (high-performance)
    PatternMatch boyerMooreSearch(const QByteArray& data, const QByteArray& pattern) const;
    
    // Agent output validation
    AgentValidation validateAgentOutput(const QByteArray& output);
    AgentValidation validatePlanMode(const QByteArray& output);
    AgentValidation validateAgentMode(const QByteArray& output);
    AgentValidation validateAskMode(const QByteArray& output);
    
    // Agent correction logic
    QByteArray correctAgentOutput(const QByteArray& output, const AgentValidation& validation);
    QByteArray enforcePlanFormat(const QByteArray& output);
    QByteArray enforceAgentFormat(const QByteArray& output);
    
    // RST Injection (Response Stream Termination)
    bool shouldTerminateStream(int chunkIndex) const;
    void setStreamTerminationPoint(int chunkCount);
    void clearStreamTermination();
    
    // Statistics
    struct Stats {
        qint64 requestsProcessed = 0;
        qint64 responsesProcessed = 0;
        qint64 chunksProcessed = 0;
        qint64 bytesPatched = 0;
        qint64 patchesApplied = 0;
        qint64 validationFailures = 0;
        qint64 correctionsApplied = 0;
        qint64 streamsTerminated = 0;
        double avgProcessingTimeMs = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/Disable
    void setEnabled(bool enable);
    bool isEnabled() const;
    
    // Direct Memory Manipulation API (Proxy-Layer)
    PatchResult directMemoryInject(size_t offset, const QByteArray& data);
    PatchResult directMemoryInjectBatch(const QHash<size_t, QByteArray>& injections);
    QByteArray directMemoryExtract(size_t offset, size_t size) const;
    
    PatchResult replaceInRequestBuffer(const QByteArray& pattern, const QByteArray& replacement);
    PatchResult replaceInResponseBuffer(const QByteArray& pattern, const QByteArray& replacement);
    
    PatchResult injectIntoStream(const QByteArray& chunk, int chunkIndex, const QByteArray& injection);
    QByteArray extractFromStream(const QByteArray& chunk, int startOffset, int length);
    
    PatchResult overwriteTokenBuffer(const QByteArray& tokenData);
    PatchResult modifyLogitsBatch(const QHash<size_t, float>& logitModifications);
    
    qint64 searchInRequestBuffer(const QByteArray& pattern) const;
    qint64 searchInResponseBuffer(const QByteArray& pattern) const;
    
    PatchResult swapBufferRegions(size_t region1Offset, size_t region2Offset, size_t size);
    PatchResult cloneBufferRegion(size_t sourceOffset, size_t destOffset, size_t size);

signals:
    void ruleApplied(const QString& name, const QString& context);
    void requestModified(const QByteArray& original, const QByteArray& modified);
    void responseModified(const QByteArray& original, const QByteArray& modified);
    void agentOutputCorrected(const QString& error, const QByteArray& corrected);
    void validationFailed(const QString& error, const QStringList& violations);
    void streamTerminated(int chunkIndex, const QString& reason);
    void errorOccurred(const QString& error);

private:
    // Boyer-Moore preprocessing
    QHash<quint8, qint64> buildBadCharTable(const QByteArray& pattern) const;
    QVector<qint64> buildGoodSuffixTable(const QByteArray& pattern) const;
    
    // Agent validation helpers
    bool checkForbiddenPatterns(const QByteArray& output, QStringList& violations);
    bool checkRequiredPatterns(const QByteArray& output, QStringList& violations);
    bool isPlanFormatValid(const QByteArray& output);
    bool isAgentFormatValid(const QByteArray& output);
    
    // Data members
    mutable QMutex m_mutex;
    QHash<QString, ProxyHotpatchRule> m_rules;
    
    Stats m_stats;
    bool m_enabled = true;
    int m_streamTerminationPoint = -1;
    int m_currentChunkIndex = 0;
    
    QElapsedTimer m_timer;
};
