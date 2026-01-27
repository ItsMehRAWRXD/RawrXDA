// byte_level_hotpatcher.cpp - Implementation
#include "byte_level_hotpatcher.hpp"
#include <QFile>
#include <QDebug>

ByteLevelHotpatcher::ByteLevelHotpatcher(QObject* parent)
    : QObject(parent)
{
}

ByteLevelHotpatcher::~ByteLevelHotpatcher()
{
}

bool ByteLevelHotpatcher::loadModel(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Failed to open file: " + filePath);
        return false;
    }
    
    m_modelData = file.readAll();
    m_modelPath = filePath;
    m_stats.modelSize = m_modelData.size();
    
    emit modelLoaded(filePath, m_modelData.size());
    return true;
}

bool ByteLevelHotpatcher::saveModel(const QString& filePath)
{
    QMutexLocker lock(&m_mutex);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit errorOccurred("Failed to save file: " + filePath);
        return false;
    }
    
    file.write(m_modelData);
    emit modelSaved(filePath);
    return true;
}

bool ByteLevelHotpatcher::addPatch(const BytePatch& patch)
{
    QMutexLocker lock(&m_mutex);
    if (m_patches.contains(patch.name)) {
        return false;
    }
    m_patches[patch.name] = patch;
    m_stats.totalPatches++;
    return true;
}

bool ByteLevelHotpatcher::removePatch(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    if (!m_patches.contains(name)) return false;
    m_patches.remove(name);
    m_stats.totalPatches--;
    return true;
}

bool ByteLevelHotpatcher::applyPatch(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    if (!m_patches.contains(name)) return false;
    
    BytePatch& patch = m_patches[name];
    if (!patch.enabled || patch.offset + patch.length > (size_t)m_modelData.size()) {
        return false;
    }
    
    // Store original bytes for revert
    patch.originalBytes = m_modelData.mid(patch.offset, patch.length);
    
    // Apply operation based on type
    switch (patch.operation) {
        case ByteOperation::Replace:
            if (patch.operand.size() == (int)patch.length) {
                std::memcpy(m_modelData.data() + patch.offset, patch.operand.constData(), patch.length);
            }
            break;
        default:
            qWarning() << "Unimplemented byte operation for patch:" << name;
            return false;
    }
    
    patch.timesApplied++;
    m_stats.patchesApplied++;
    m_stats.bytesPatched += patch.length;
    
    emit patchApplied(name, patch.offset, patch.length);
    return true;
}

bool ByteLevelHotpatcher::revertPatch(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    if (!m_patches.contains(name)) return false;
    
    BytePatch& patch = m_patches[name];
    if (patch.originalBytes.isEmpty()) return false;
    
    std::memcpy(m_modelData.data() + patch.offset, patch.originalBytes.constData(), patch.length);
    m_stats.patchesReverted++;
    
    emit patchReverted(name);
    return true;
}

void ByteLevelHotpatcher::revertAllPatches()
{
    QMutexLocker lock(&m_mutex);
    for (const QString& name : m_patches.keys()) {
        lock.unlock();
        revertPatch(name);
        lock.relock();
    }
}

bool ByteLevelHotpatcher::replaceByte(size_t offset, uint8_t oldValue, uint8_t newValue)
{
    QMutexLocker lock(&m_mutex);
    if (offset >= (size_t)m_modelData.size()) return false;
    if ((uint8_t)m_modelData.at(offset) != oldValue) return false;
    
    m_modelData[offset] = newValue;
    return true;
}

bool ByteLevelHotpatcher::replaceBytes(size_t offset, const QByteArray& oldBytes, const QByteArray& newBytes)
{
    QMutexLocker lock(&m_mutex);
    if (offset + oldBytes.size() > (size_t)m_modelData.size()) return false;
    if (newBytes.size() != oldBytes.size()) return false;
    
    if (m_modelData.mid(offset, oldBytes.size()) != oldBytes) return false;
    
    std::memcpy(m_modelData.data() + offset, newBytes.constData(), newBytes.size());
    return true;
}

bool ByteLevelHotpatcher::flipBits(size_t offset, uint8_t bitMask)
{
    QMutexLocker lock(&m_mutex);
    if (offset >= (size_t)m_modelData.size()) return false;
    
    m_modelData[offset] = m_modelData[offset] ^ bitMask;
    return true;
}

QVector<size_t> ByteLevelHotpatcher::findPattern(const QByteArray& pattern) const
{
    QMutexLocker lock(&m_mutex);
    QVector<size_t> offsets;
    
    for (size_t i = 0; i <= (size_t)m_modelData.size() - pattern.size(); ++i) {
        if (m_modelData.mid(i, pattern.size()) == pattern) {
            offsets.append(i);
        }
    }
    
    return offsets;
}

bool ByteLevelHotpatcher::replacePattern(const QByteArray& pattern, const QByteArray& replacement, int maxOccurrences)
{
    QMutexLocker lock(&m_mutex);
    if (pattern.size() != replacement.size()) return false;
    
    QVector<size_t> offsets = findPattern(pattern);
    int count = 0;
    
    for (size_t offset : offsets) {
        if (maxOccurrences > 0 && count >= maxOccurrences) break;
        std::memcpy(m_modelData.data() + offset, replacement.constData(), replacement.size());
        count++;
    }
    
    return count > 0;
}

uint32_t ByteLevelHotpatcher::calculateCRC32(size_t offset, size_t length) const
{
    QMutexLocker lock(&m_mutex);
    if (offset + length > (size_t)m_modelData.size()) return 0;
    
    // Simple CRC32 implementation
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* data = (const uint8_t*)m_modelData.constData() + offset;
    
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

uint64_t ByteLevelHotpatcher::calculateFNV1a_64(size_t offset, size_t length) const
{
    QMutexLocker lock(&m_mutex);
    if (offset + length > (size_t)m_modelData.size()) return 0;
    
    const uint8_t* data = (const uint8_t*)m_modelData.constData() + offset;
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;
    
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= prime;
    }
    
    return hash;
}

QByteArray ByteLevelHotpatcher::hexDump(size_t offset, size_t length, int bytesPerLine) const
{
    QMutexLocker lock(&m_mutex);
    if (offset + length > (size_t)m_modelData.size()) return QByteArray();
    
    QString result;
    const uint8_t* data = (const uint8_t*)m_modelData.constData() + offset;
    
    for (size_t i = 0; i < length; i += bytesPerLine) {
        result += QString("%1: ").arg(offset + i, 8, 16, QChar('0'));
        
        size_t lineLen = qMin((size_t)bytesPerLine, length - i);
        for (size_t j = 0; j < lineLen; ++j) {
            result += QString("%1 ").arg(data[i + j], 2, 16, QChar('0'));
        }
        
        result += "\n";
    }
    
    return result.toUtf8();
}

ByteLevelHotpatcher::BytePatchStats ByteLevelHotpatcher::getStatistics() const
{
    QMutexLocker lock(&m_mutex);
    return m_stats;
}

QJsonObject BytePatch::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["description"] = description;
    obj["enabled"] = enabled;
    obj["offset"] = (qint64)offset;
    obj["length"] = (qint64)length;
    return obj;
}

BytePatch BytePatch::fromJson(const QJsonObject& json, PatchResult& result)
{
    BytePatch patch;
    patch.name = json["name"].toString();
    patch.description = json["description"].toString();
    patch.enabled = json["enabled"].toBool(true);
    patch.offset = json["offset"].toInteger();
    patch.length = json["length"].toInteger();
    result = PatchResult::ok(QString("Loaded patch: %1").arg(patch.name));
    return patch;
}

// Direct Memory Manipulation API Implementation

void* ByteLevelHotpatcher::getDirectPointer(size_t offset) const
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset >= (size_t)m_modelData.size()) {
        return nullptr;
    }
    return (void*)(m_modelData.data() + offset);
}

QByteArray ByteLevelHotpatcher::directRead(size_t offset, size_t size) const
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + size > (size_t)m_modelData.size()) {
        return QByteArray();
    }
    return m_modelData.mid(offset, size);
}

PatchResult ByteLevelHotpatcher::directWrite(size_t offset, const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + data.size() > (size_t)m_modelData.size()) {
        return PatchResult::error(7001, "Write out of bounds");
    }
    
    std::memcpy(m_modelData.data() + offset, data.constData(), data.size());
    m_stats.bytesPatched += data.size();
    return PatchResult::ok("Direct write completed", data.size());
}

PatchResult ByteLevelHotpatcher::directWriteBatch(const QHash<size_t, QByteArray>& writes)
{
    QMutexLocker lock(&m_mutex);
    int totalBytes = 0;
    
    for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
        size_t offset = it.key();
        const QByteArray& data = it.value();
        
        if (offset + data.size() > (size_t)m_modelData.size()) {
            return PatchResult::error(7002, "Batch write out of bounds");
        }
        
        std::memcpy(m_modelData.data() + offset, data.constData(), data.size());
        totalBytes += data.size();
    }
    
    m_stats.bytesPatched += totalBytes;
    return PatchResult::ok("Batch write completed", totalBytes);
}

PatchResult ByteLevelHotpatcher::directFill(size_t offset, size_t size, quint8 value)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + size > (size_t)m_modelData.size()) {
        return PatchResult::error(7003, "Fill out of bounds");
    }
    
    std::memset(m_modelData.data() + offset, value, size);
    m_stats.bytesPatched += size;
    return PatchResult::ok("Fill completed", size);
}

PatchResult ByteLevelHotpatcher::directCopy(size_t srcOffset, size_t dstOffset, size_t size)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || srcOffset + size > (size_t)m_modelData.size() || 
        dstOffset + size > (size_t)m_modelData.size()) {
        return PatchResult::error(7004, "Copy out of bounds");
    }
    
    std::memmove(m_modelData.data() + dstOffset, m_modelData.data() + srcOffset, size);
    m_stats.bytesPatched += size;
    return PatchResult::ok("Copy completed", size);
}

bool ByteLevelHotpatcher::directCompare(size_t offset, const QByteArray& data) const
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + data.size() > (size_t)m_modelData.size()) {
        return false;
    }
    
    return std::memcmp(m_modelData.data() + offset, data.constData(), data.size()) == 0;
}

QByteArray ByteLevelHotpatcher::directXOR(size_t offset, size_t size, const QByteArray& key)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + size > (size_t)m_modelData.size() || key.isEmpty()) {
        return QByteArray();
    }
    
    QByteArray result(size, 0);
    const quint8* keyData = (quint8*)key.constData();
    size_t keyLen = key.size();
    
    for (size_t i = 0; i < size; ++i) {
        result[i] = m_modelData[offset + i] ^ keyData[i % keyLen];
    }
    
    return result;
}

PatchResult ByteLevelHotpatcher::directBitOperation(size_t offset, size_t size, ByteOperation op, uint8_t operand)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + size > (size_t)m_modelData.size()) {
        return PatchResult::error(7005, "Bit operation out of bounds");
    }
    
    for (size_t i = 0; i < size; ++i) {
        quint8* byte = (quint8*)(m_modelData.data() + offset + i);
        switch (op) {
        case ByteOperation::BitSet:
            *byte |= operand;
            break;
        case ByteOperation::BitClear:
            *byte &= ~operand;
            break;
        case ByteOperation::ByteXOR:
            *byte ^= operand;
            break;
        case ByteOperation::ByteAND:
            *byte &= operand;
            break;
        case ByteOperation::ByteOR:
            *byte |= operand;
            break;
        default:
            break;
        }
    }
    
    m_stats.bytesPatched += size;
    return PatchResult::ok("Bit operation completed", size);
}

PatchResult ByteLevelHotpatcher::directRotate(size_t offset, size_t size, int bitShift, bool leftShift)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + size > (size_t)m_modelData.size()) {
        return PatchResult::error(7006, "Rotate out of bounds");
    }
    
    bitShift = bitShift % 8;
    
    for (size_t i = 0; i < size; ++i) {
        quint8* byte = (quint8*)(m_modelData.data() + offset + i);
        if (leftShift) {
            *byte = (*byte << bitShift) | (*byte >> (8 - bitShift));
        } else {
            *byte = (*byte >> bitShift) | (*byte << (8 - bitShift));
        }
    }
    
    m_stats.bytesPatched += size;
    return PatchResult::ok("Rotate completed", size);
}

PatchResult ByteLevelHotpatcher::directReverse(size_t offset, size_t size)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset + size > (size_t)m_modelData.size()) {
        return PatchResult::error(7007, "Reverse out of bounds");
    }
    
    for (size_t i = 0; i < size / 2; ++i) {
        std::swap(m_modelData[offset + i], m_modelData[offset + size - 1 - i]);
    }
    
    m_stats.bytesPatched += size;
    return PatchResult::ok("Reverse completed", size);
}

qint64 ByteLevelHotpatcher::directSearch(size_t startOffset, const QByteArray& pattern) const
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || pattern.isEmpty() || startOffset >= (size_t)m_modelData.size()) {
        return -1;
    }
    
    const char* found = std::search(m_modelData.constData() + startOffset, m_modelData.constData() + m_modelData.size(),
                                    pattern.constData(), pattern.constData() + pattern.size());
    
    if (found != m_modelData.constData() + m_modelData.size()) {
        return std::distance(m_modelData.constData(), found);
    }
    
    return -1;
}

PatchResult ByteLevelHotpatcher::atomicByteSwap(size_t offset1, size_t offset2, size_t size)
{
    QMutexLocker lock(&m_mutex);
    if (m_modelData.isEmpty() || offset1 + size > (size_t)m_modelData.size() || 
        offset2 + size > (size_t)m_modelData.size()) {
        return PatchResult::error(7008, "Swap out of bounds");
    }
    
    QByteArray temp = m_modelData.mid(offset1, size);
    std::memcpy(m_modelData.data() + offset1, m_modelData.data() + offset2, size);
    std::memcpy(m_modelData.data() + offset2, temp.constData(), size);
    
    m_stats.bytesPatched += 2 * size;
    return PatchResult::ok("Swap completed", 2 * size);
}

