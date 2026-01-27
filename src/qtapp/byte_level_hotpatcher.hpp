// byte_level_hotpatcher.hpp - Precision byte-level model patching
#pragma once

#include "model_memory_hotpatch.hpp"
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>
#include <QHash>
#include <QDateTime>
#include <QMutex>
#include <QJsonObject>
#include <QVariant>
#include <cstdint>

enum class ByteOperation {
    Replace,
    BitFlip,
    BitSet,
    BitClear,
    ByteAND,
    ByteOR,
    ByteXOR,
    ByteAdd,
    ByteRotate,
    MASM_Compress,
    Custom
};

enum class HashAlgorithm {
    CRC32,
    SHA256,
    FNV1a_64
};

// PatchResult is defined in model_memory_hotpatch.hpp

struct BytePatch {
    QString name;
    QString description;
    bool enabled = true;
    
    size_t offset = 0;
    size_t length = 1;
    
    ByteOperation operation;
    QByteArray operand;
    uint8_t bitMask = 0xFF;
    int bitShift = 0;
    
    QByteArray expectedBefore;
    QByteArray expectedAfter;
    HashAlgorithm hashAlgo = HashAlgorithm::CRC32;
    uint32_t targetIntegrityHash = 0;
    
    QByteArray originalBytes;
    QString category;
    int priority = 0;
    QDateTime created;
    int timesApplied = 0;
    
    QStringList requiresPatches;
    QStringList conflictsWith;
    
    QJsonObject toJson() const;
    static BytePatch fromJson(const QJsonObject& json, PatchResult& result);
};

class ByteLevelHotpatcher : public QObject {
    Q_OBJECT

public:
    explicit ByteLevelHotpatcher(QObject* parent = nullptr);
    ~ByteLevelHotpatcher();

    bool loadModel(const QString& filePath);
    bool saveModel(const QString& filePath);
    const QByteArray& getModelData() const { return m_modelData; }
    bool isModelLoaded() const { return !m_modelData.isEmpty(); }

    bool addPatch(const BytePatch& patch);
    bool removePatch(const QString& name);
    bool applyPatch(const QString& name);
    bool revertPatch(const QString& name);
    void revertAllPatches();
    
    bool replaceByte(size_t offset, uint8_t oldValue, uint8_t newValue);
    bool replaceBytes(size_t offset, const QByteArray& oldBytes, const QByteArray& newBytes);
    bool flipBits(size_t offset, uint8_t bitMask);
    
    QVector<size_t> findPattern(const QByteArray& pattern) const;
    bool replacePattern(const QByteArray& pattern, const QByteArray& replacement, int maxOccurrences = -1);
    
    uint32_t calculateCRC32(size_t offset, size_t length) const;
    uint64_t calculateFNV1a_64(size_t offset, size_t length) const;
    
    QByteArray hexDump(size_t offset, size_t length, int bytesPerLine = 16) const;

    // Direct Memory Manipulation API
    void* getDirectPointer(size_t offset = 0) const;
    QByteArray directRead(size_t offset, size_t size) const;
    PatchResult directWrite(size_t offset, const QByteArray& data);
    PatchResult directWriteBatch(const QHash<size_t, QByteArray>& writes);
    PatchResult directFill(size_t offset, size_t size, quint8 value);
    PatchResult directCopy(size_t srcOffset, size_t dstOffset, size_t size);
    bool directCompare(size_t offset, const QByteArray& data) const;
    QByteArray directXOR(size_t offset, size_t size, const QByteArray& key);
    PatchResult directBitOperation(size_t offset, size_t size, ByteOperation op, uint8_t operand);
    PatchResult directRotate(size_t offset, size_t size, int bitShift, bool leftShift = true);
    PatchResult directReverse(size_t offset, size_t size);
    qint64 directSearch(size_t startOffset, const QByteArray& pattern) const;
    PatchResult atomicByteSwap(size_t offset1, size_t offset2, size_t size);

    struct BytePatchStats {
        quint64 totalPatches = 0;
        quint64 bytesPatched = 0;
        quint64 patchesApplied = 0;
        quint64 patchesReverted = 0;
        size_t modelSize = 0;
        QHash<ByteOperation, int> operationCounts;
    };
    BytePatchStats getStatistics() const;

signals:
    void patchApplied(const QString& name, size_t offset, size_t length);
    void patchReverted(const QString& name);
    void modelLoaded(const QString& filePath, size_t size);
    void modelSaved(const QString& filePath);
    void errorOccurred(const QString& error);

private:
    QByteArray m_modelData;
    QString m_modelPath;
    QHash<QString, BytePatch> m_patches;
    BytePatchStats m_stats;
    mutable QMutex m_mutex;
    
    static constexpr size_t MAX_MODEL_SIZE = 100ULL * 1024 * 1024 * 1024;
};
