// unified_hotpatch_manager.hpp - Coordinates all three hotpatch systems
// Provides single interface for memory, byte-level, and server hotpatching

#pragma once

#include "model_memory_hotpatch.hpp"
#include "byte_level_hotpatcher.hpp"
#include "gguf_server_hotpatch.hpp"
#include <QObject>
#include <QString>
#include <QHash>
#include <memory>
#include <QVariant>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonObject>
#include <QDateTime>

enum class PatchLayer {
    System,
    Memory,
    Byte,
    Server
};

struct UnifiedResult {
    bool success = false;
    PatchLayer layer = PatchLayer::System;
    QString operationName;
    QString errorDetail;
    QDateTime timestamp = QDateTime::currentDateTime();
    int errorCode = 0;

    static UnifiedResult successResult(const QString& op, PatchLayer layer = PatchLayer::System, const QString& detail = "OK") {
        UnifiedResult r; r.success = true; r.operationName = op; r.layer = layer; r.errorDetail = detail; return r;
    }
    static UnifiedResult failureResult(const QString& op, const QString& detail, PatchLayer layer = PatchLayer::System, int code = -1) {
        UnifiedResult r; r.success = false; r.operationName = op; r.layer = layer; r.errorDetail = detail; r.errorCode = code; return r;
    }
};

class UnifiedHotpatchManager : public QObject {
    Q_OBJECT

public:
    explicit UnifiedHotpatchManager(QObject* parent = nullptr);
    ~UnifiedHotpatchManager();

    UnifiedResult initialize();
    bool isInitialized() const;
    
    ModelMemoryHotpatch* memoryHotpatcher() const;
    ByteLevelHotpatcher* byteHotpatcher() const;
    GGUFServerHotpatch* serverHotpatcher() const;
    
    UnifiedResult attachToModel(void* modelPtr, size_t modelSize, const QString& modelPath);
    UnifiedResult detachAll();
    
    // Memory-level operations
    PatchResult applyMemoryPatch(const QString& name, const MemoryPatch& patch);
    PatchResult scaleWeights(const QString& tensorName, double factor);
    PatchResult bypassLayer(int layerIndex);
    
    // Byte-level operations
    UnifiedResult applyBytePatch(const QString& name, const BytePatch& patch);
    UnifiedResult savePatchedModel(const QString& outputPath);
    UnifiedResult patchGGUFMetadata(const QString& key, const QVariant& value);
    
    // Server-level operations
    UnifiedResult addServerHotpatch(const QString& name, const ServerHotpatch& patch);
    UnifiedResult enableSystemPromptInjection(const QString& prompt);
    UnifiedResult setTemperatureOverride(double temperature);
    UnifiedResult enableResponseCaching(bool enable);
    
    // Coordinated operations
    QList<UnifiedResult> optimizeModel();
    QList<UnifiedResult> applySafetyFilters();
    QList<UnifiedResult> boostInferenceSpeed();
    
    struct UnifiedStats {
        ModelMemoryHotpatch::MemoryPatchStats memoryStats;
        quint64 totalPatchesApplied = 0;
        quint64 totalBytesModified = 0;
        QDateTime sessionStarted;
        QDateTime lastCoordinatedAction;
        quint64 coordinatedActionsCompleted = 0;
    };
    
    UnifiedStats getStatistics() const;
    void resetStatistics();
    
    UnifiedResult savePreset(const QString& name);
    UnifiedResult loadPreset(const QString& name);
    UnifiedResult deletePreset(const QString& name);
    QStringList listPresets() const;
    
    UnifiedResult exportConfiguration(const QString& filePath);
    UnifiedResult importConfiguration(const QString& filePath);

signals:
    void initialized();
    void modelAttached(const QString& modelPath, size_t modelSize);
    void modelDetached();
    void patchApplied(const QString& name, PatchLayer layer);
    void optimizationComplete(const QString& type, int improvementPercent);
    void errorOccurred(const UnifiedResult& error);

public slots:
    void setMemoryHotpatchEnabled(bool enabled);
    void setByteHotpatchEnabled(bool enabled);
    void setServerHotpatchEnabled(bool enabled);
    void enableAllLayers();
    void disableAllLayers();
    void resetAllLayers();

private:
    std::unique_ptr<ModelMemoryHotpatch> m_memoryHotpatch;
    std::unique_ptr<ByteLevelHotpatcher> m_byteHotpatch;
    std::unique_ptr<GGUFServerHotpatch> m_serverHotpatch;
    
    bool m_initialized = false;
    QString m_currentModelPath;
    void* m_currentModelPtr = nullptr;
    size_t m_currentModelSize = 0;
    
    bool m_memoryEnabled = true;
    bool m_byteEnabled = true;
    bool m_serverEnabled = true;
    
    UnifiedStats m_stats;
    QDateTime m_sessionStart;
    QHash<QString, QJsonObject> m_presets;
    mutable QMutex m_mutex;
    
    void connectSignals();
    void updateStatistics(const UnifiedResult& result);
    QList<UnifiedResult> logCoordinatedResults(const QString& operation, const QList<UnifiedResult>& results);
};
