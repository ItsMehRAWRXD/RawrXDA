// model_memory_hotpatch.hpp - Live RAM model patching with cross-platform memory protection
// Supports Windows VirtualProtect and POSIX mprotect for safe memory manipulation

#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <cstdint>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#define VIRTUAL_PROTECT_RO PAGE_READONLY
#define VIRTUAL_PROTECT_RW PAGE_READWRITE
#else
#include <sys/mman.h>
#include <unistd.h>
#define VIRTUAL_PROTECT_RO PROT_READ
#define VIRTUAL_PROTECT_RW (PROT_READ | PROT_WRITE)
#endif

/**
 * @brief Structured result for robust error handling with timing metadata.
 */
struct PatchResult {
    bool success = false;
    QString detail;
    int errorCode = 0;
    qint64 elapsedMs = 0;
    
    static PatchResult ok(const QString& msg = "OK", qint64 elapsed = 0) {
        PatchResult r; r.success = true; r.detail = msg; r.elapsedMs = elapsed; return r;
    }
    static PatchResult error(int code, const QString& msg, qint64 elapsed = 0, int osError = 0) {
        PatchResult r; r.success = false; r.errorCode = code; r.detail = msg; r.elapsedMs = elapsed; 
        if (osError != 0 && r.errorCode == code) r.errorCode = osError;
        return r;
    }
};

enum class MemoryPatchType {
    WeightModification,
    QuantizationChange,
    LayerBypass,
    AttentionScale,
    BiasAdjustment,
    GraphRedirection,
    VocabularyPatch,
    Custom
};

struct MemoryPatch {
    QString name;
    QString description;
    MemoryPatchType type;
    bool enabled = true;
    
    size_t offset = 0;
    size_t size = 0;
    QByteArray patchBytes;
    QByteArray originalBytes;
    
    enum class TransformType { None, Scale, Clamp, Offset, Custom };
    TransformType transformType = TransformType::None;
    double transformParam1 = 0.0;
    double transformParam2 = 0.0;
    
    bool verifyChecksum = false;
    uint64_t checksumBefore = 0;
    uint64_t checksumAfter = 0;
    
    int priority = 0;
    QDateTime created;
    QDateTime lastApplied;
    int timesApplied = 0;
};

struct TensorInfo {
    QString name;
    size_t offset;
    size_t size;
    int nDims;
    QVector<int> shape;
    QString quantType;
};

struct PatchConflict {
    MemoryPatch existingPatch;
    MemoryPatch incomingPatch;
    QString reason;
};

class ModelMemoryHotpatch : public QObject {
    Q_OBJECT

public:
    explicit ModelMemoryHotpatch(QObject* parent = nullptr);
    ~ModelMemoryHotpatch();

    // Attachment
    bool attachToModel(void* modelPtr, size_t modelSize);
    void detach();
    bool isAttached() const;

    // Patch management
    bool addPatch(const MemoryPatch& patch);
    bool removePatch(const QString& name);
    PatchResult applyPatch(const QString& name);
    PatchResult revertPatch(const QString& name);
    bool applyAllPatches();
    bool revertAllPatches();

    // Memory I/O
    QByteArray readMemory(size_t offset, size_t size);
    PatchResult writeMemory(size_t offset, const QByteArray& data);

    // High-level operations
    PatchResult scaleTensorWeights(const QString& tensorName, double scaleFactor);
    PatchResult clampTensorWeights(const QString& tensorName, float minVal, float maxVal);
    PatchResult bypassLayer(int layerIndex, bool bypass);
    PatchResult patchVocabularyEntry(int tokenId, const QString& newToken);

    // Safety
    PatchResult createBackup();
    PatchResult restoreBackup();
    bool verifyModelIntegrity();
    bool checkPatchConflict(const MemoryPatch& newPatch, PatchConflict& conflict) const;

    // Tensor lookup
    bool findTensor(const QString& tensorName, size_t& offset, size_t& size);

    // Direct Memory Manipulation API
    void* getDirectMemoryPointer(size_t offset = 0) const;
    QByteArray directMemoryRead(size_t offset, size_t size) const;
    PatchResult directMemoryWrite(size_t offset, const QByteArray& data);
    PatchResult directMemoryWriteBatch(const QHash<size_t, QByteArray>& writes);
    PatchResult directMemoryFill(size_t offset, size_t size, quint8 value);
    PatchResult directMemoryCopy(size_t srcOffset, size_t dstOffset, size_t size);
    bool directMemoryCompare(size_t offset, const QByteArray& data) const;
    qint64 directMemorySearch(size_t startOffset, const QByteArray& pattern) const;
    PatchResult directMemorySwap(size_t offset1, size_t offset2, size_t size);
    PatchResult setMemoryProtection(size_t offset, size_t size, int protectionFlags);
    
    // Memory mapping
    void* memoryMapRegion(size_t offset, size_t size, int flags);
    PatchResult unmapMemoryRegion(void* mappedPtr, size_t size);

    struct MemoryPatchStats {
        quint64 totalPatches = 0;
        quint64 appliedPatches = 0;
        quint64 revertedPatches = 0;
        quint64 failedPatches = 0;
        quint64 bytesModified = 0;
        quint64 conflictsDetected = 0;
        size_t modelSize = 0;
        QDateTime lastPatch;
    };
    MemoryPatchStats getStatistics() const;
    void resetStatistics();

signals:
    void modelAttached(size_t modelSize);
    void modelDetached();
    void patchApplied(const QString& name);
    void patchReverted(const QString& name);
    void integrityCheckFailed(const QString& patchName, uint64_t actualChecksum);
    void patchConflictDetected(const QString& newPatch, const QString& existingPatch);
    void patchConflictDetectedRich(const PatchConflict& conflict);
    void errorOccurred(const PatchResult& result);

private:
    struct RegionProtectCookie;
    
    bool validateMemoryAccess(size_t offset, size_t size) const;
    PatchResult safeMemoryWrite(size_t offset, const QByteArray& data);
    PatchResult beginWritableWindow(size_t offset, size_t size, void*& cookie);
    PatchResult endWritableWindow(void* cookie);
    bool protectMemory(void* ptr, size_t size, int protectionFlags);
    size_t systemPageSize() const;
    uint64_t calculateChecksum64(size_t offset, size_t size) const;
    quint32 calculateCRC32(size_t offset, size_t size) const;
    bool parseTensorMetadata();
    bool rebuildTensorDependencyMap();

    void* m_modelPtr = nullptr;
    size_t m_modelSize = 0;
    bool m_attached = false;
    quint32 m_integrityHash = 0;

    QHash<QString, MemoryPatch> m_patches;
    QHash<QString, TensorInfo> m_tensorMap;
    QByteArray m_fullBackup;
    QVector<QString> m_history;
    
    MemoryPatchStats m_stats;
    mutable QMutex m_mutex;

    struct BatchConfig {
        bool enableBatching = true;
        size_t maxBatchSize = 16 * 1024 * 1024;
    } m_batchConfig;
};
