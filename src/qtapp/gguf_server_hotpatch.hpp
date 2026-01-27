// gguf_server_hotpatch.hpp - Server-side GGUF request/response hotpatcher
#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QMutex>
#include <QHash>
#include <QDateTime>
#include <functional>
#include "model_memory_hotpatch.hpp"

// Hotpatch application points in the request/response pipeline
enum class HotpatchPoint {
    PreRequest,      // Before request is sent to model
    PostRequest,     // After request processing, before inference
    PreResponse,     // Before response is returned to client
    PostResponse,    // After response is fully generated
    StreamChunk      // During streaming response (per-chunk)
};

// Server-side hotpatch structure
struct ServerHotpatch {
    QString name;
    HotpatchPoint applicationPoint;
    bool enabled = true;
    
    // Transform types
    enum TransformType {
        InjectSystemPrompt,       // Add system prompt to request
        ModifyParameter,          // Change parameter value (temperature, top_p, etc.)
        FilterResponse,           // Filter/censor response content
        TerminateStream,          // RST injection - abort stream early
        CacheResponse,            // Cache response for identical requests
        ModifyTokenLogits         // Modify token probabilities
    };
    TransformType transformType;
    
    // Configuration data
    QString systemPromptInjection;
    QString parameterName;
    QVariant parameterValue;
    QStringList filterPatterns;      // For response filtering
    int abortAfterChunks = -1;       // For stream termination (-1 = disabled)
    
    // Transform function (for custom logic)
    std::function<QByteArray(const QByteArray&)> customTransform;
};

class GGUFServerHotpatch : public QObject
{
    Q_OBJECT

public:
    explicit GGUFServerHotpatch(QObject* parent = nullptr);
    ~GGUFServerHotpatch() override;

    // Hotpatch management
    void addHotpatch(const ServerHotpatch& patch);
    void removeHotpatch(const QString& name);
    void enableHotpatch(const QString& name, bool enable);
    bool hasHotpatch(const QString& name) const;
    ServerHotpatch getHotpatch(const QString& name) const;
    QStringList listHotpatches() const;
    void clearAllHotpatches();

    // Request/Response processing
    QJsonObject processRequest(const QJsonObject& request);
    QJsonObject processResponse(const QJsonObject& response);
    QByteArray processStreamChunk(const QByteArray& chunk, int chunkIndex);
    
    // Parameter manipulation (zero-copy byte patching)
    QByteArray patchRequestBytes(const QByteArray& requestData);
    QByteArray patchResponseBytes(const QByteArray& responseData);
    
    // Default parameter overrides
    void setDefaultParameter(const QString& name, const QVariant& value);
    void clearDefaultParameter(const QString& name);
    QHash<QString, QVariant> getDefaultParameters() const;
    
    // Response caching
    void setCachingEnabled(bool enable);
    bool isCachingEnabled() const;
    void clearCache();
    QString getCacheKey(const QJsonObject& request) const;
    bool hasCachedResponse(const QString& key) const;
    QJsonObject getCachedResponse(const QString& key);
    void cacheResponse(const QString& key, const QJsonObject& response);
    
    // Statistics
    struct Stats {
        qint64 requestsProcessed = 0;
        qint64 responsesProcessed = 0;
        qint64 chunksProcessed = 0;
        qint64 cacheHits = 0;
        qint64 cacheMisses = 0;
        qint64 bytesPatched = 0;
        qint64 patchesApplied = 0;
        double avgProcessingTimeMs = 0.0;
    };
    
    Stats getStatistics() const;
    void resetStatistics();
    
    // Enable/Disable entire system
    void setEnabled(bool enable);
    bool isEnabled() const;
    
    // Direct Memory Manipulation API for Model Access
    void* attachToModelMemory(const QString& modelPath);
    PatchResult detachFromModelMemory();
    
    QByteArray readModelMemory(size_t offset, size_t size) const;
    PatchResult writeModelMemory(size_t offset, const QByteArray& data);
    
    PatchResult modifyWeight(const QString& tensorName, size_t indexOffset, const QByteArray& newValue);
    PatchResult modifyWeightsBatch(const QHash<QString, QHash<size_t, QByteArray>>& modifications);
    
    PatchResult injectTemporaryData(size_t offset, const QByteArray& data, int durationMs);
    QByteArray extractTensorWeights(const QString& tensorName, size_t offset, size_t size) const;
    PatchResult transformTensorWeights(const QString& tensorName, std::function<QByteArray(const QByteArray&)> transform);
    
    PatchResult cloneTensor(const QString& sourceTensor, const QString& destTensor);
    PatchResult swapTensors(const QString& tensor1, const QString& tensor2);
    PatchResult applyMemoryPatch(const QHash<size_t, QByteArray>& patches);
    qint64 searchModelMemory(size_t startOffset, const QByteArray& pattern) const;
    
    void* getModelMemoryPointer(size_t offset = 0);
    PatchResult lockMemoryRegion(size_t offset, size_t size);
    PatchResult unlockMemoryRegion(size_t offset, size_t size);
    
    // Tensor dependency tracking
    bool hasTensorDependency(const QString& tensorName, const QString& dependencyName) const;
    QStringList getTensorDependencies(const QString& tensorName) const;
    
    // Vocabulary patching
    PatchResult patchVocabularyEntry(int tokenId, const QString& newToken);

signals:
    void hotpatchApplied(const QString& name, HotpatchPoint point);
    void requestModified(const QJsonObject& original, const QJsonObject& modified);
    void responseModified(const QJsonObject& original, QJsonObject& modified);
    void streamTerminated(int chunkIndex, const QString& reason);
    void cacheHit(const QString& key);
    void errorOccurred(const QString& error);

private:
    // Helper methods
    QByteArray bytePatchInPlace(const QByteArray& data, const QByteArray& pattern, const QByteArray& replacement);
    qint64 findPattern(const QByteArray& data, const QByteArray& pattern, qint64 startPos = 0) const;
    QJsonObject injectSystemPrompt(const QJsonObject& request, const QString& prompt);
    QJsonObject modifyParameter(const QJsonObject& request, const QString& param, const QVariant& value);
    QJsonObject filterResponse(const QJsonObject& response, const QStringList& patterns);
    
    // Data members
    mutable QMutex m_mutex;
    QHash<QString, ServerHotpatch> m_hotpatches;
    QHash<QString, QVariant> m_defaultParams;
    QHash<QString, QJsonObject> m_responseCache;
    
    QByteArray m_modelData;         // Model data for direct memory operations
    QString m_modelPath;            // Current model path
    QHash<QString, size_t> m_tensorOffsets;  // Tensor name -> offset mapping
    QHash<QString, QStringList> m_tensorDependencies;  // Tensor name -> list of dependencies
    
    Stats m_stats;
    bool m_enabled = true;
    bool m_cachingEnabled = false;
    int m_currentChunkIndex = 0;
    
    QDateTime m_lastProcessTime;
};
