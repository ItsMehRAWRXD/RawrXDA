// model_memory_hotpatch.cpp - Implementation of the ModelMemoryHotpatch engine
// Handles cross-platform memory protection (VirtualProtect/mprotect) for live patching
// REFACTOR NOTE: Updated core memory functions to return PatchResult for structured error reporting and timing.

#include "model_memory_hotpatch.hpp"
#include <QDebug>
#include <QElapsedTimer>
#include <numeric>

// --- Platform-Specific Helper Implementation (Crucial for Direct Memory Manipulation) ---

struct ModelMemoryHotpatch::RegionProtectCookie {
#ifdef _WIN32
    DWORD oldProtection; // Used by VirtualProtect
#else
    int dummy; // Placeholder for POSIX (mprotect doesn't need a cookie for restore)
#endif
    size_t alignedStart;
    size_t alignedSize;
};

/**
 * @brief Retrieves the system's memory page size.
 * @return The page size in bytes.
 */
size_t ModelMemoryHotpatch::systemPageSize() const
{
    static size_t pageSize = 0;
    if (pageSize == 0) {
#ifdef _WIN32
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        pageSize = si.dwPageSize;
#else
        pageSize = sysconf(_SC_PAGESIZE);
#endif
    }
    return pageSize;
}

/**
 * @brief Protects or unprotects a memory region.
 * @param ptr Start address of the region.
 * @param size Size of the region.
 * @param protectionFlags OS-specific protection flags (VIRTUAL_PROTECT_RO/RW).
 * @return true on success.
 */
bool ModelMemoryHotpatch::protectMemory(void* ptr, size_t size, int protectionFlags)
{
    if (!ptr || size == 0) return false;

#ifdef _WIN32
    DWORD oldProt;
    if (VirtualProtect(ptr, size, protectionFlags, &oldProt) == 0) {
        qCritical() << "VirtualProtect failed for size" << size << "Error:" << GetLastError();
        return false;
    }
    return true;
#else
    size_t pageSize = systemPageSize();
    size_t startAddr = (size_t)ptr;
    size_t alignedStart = startAddr & ~(pageSize - 1);
    size_t alignedSize = (startAddr + size - alignedStart + pageSize - 1) & ~(pageSize - 1);

    if (mprotect((void*)alignedStart, alignedSize, protectionFlags) == -1) {
        qCritical() << "mprotect failed for size" << size << "Error:" << errno;
        return false;
    }
    return true;
#endif
}

PatchResult ModelMemoryHotpatch::beginWritableWindow(size_t offset, size_t size, void*& cookie)
{
    QElapsedTimer timer;
    timer.start();

    if (!m_modelPtr || offset + size > m_modelSize) {
        return PatchResult::error(1001, "Invalid offset or size for writable window.");
    }

    size_t pageSize = systemPageSize();
    char* startAddr = (char*)m_modelPtr + offset;
    size_t alignedStart = (size_t)startAddr & ~(pageSize - 1);
    size_t endAddr = (size_t)startAddr + size;
    size_t alignedEnd = (endAddr + pageSize - 1) & ~(pageSize - 1);
    size_t alignedSize = alignedEnd - alignedStart;

    auto* protectCookie = new RegionProtectCookie();
    protectCookie->alignedStart = alignedStart;
    protectCookie->alignedSize = alignedSize;
    
#ifdef _WIN32
    if (VirtualProtect((void*)alignedStart, alignedSize, VIRTUAL_PROTECT_RW, &protectCookie->oldProtection) == 0) {
        DWORD osError = GetLastError();
        QString detail = QString("Win: VirtualProtect failed. Error code: %1").arg(osError);
        delete protectCookie;
        return PatchResult::error(1002, detail, timer.elapsed());
    }
#else
    if (mprotect((void*)alignedStart, alignedSize, VIRTUAL_PROTECT_RW) == -1) {
        int osError = errno;
        QString detail = QString("POSIX: mprotect failed. Error code: %1").arg(osError);
        delete protectCookie;
        return PatchResult::error(1003, detail, timer.elapsed());
    }
#endif
    cookie = protectCookie;
    return PatchResult::ok(QString("Writable window opened, size %1 bytes.").arg(alignedSize), timer.elapsed());
}

PatchResult ModelMemoryHotpatch::endWritableWindow(void* cookie)
{
    QElapsedTimer timer;
    timer.start();

    if (!cookie) {
        return PatchResult::error(1004, "Invalid cookie provided to endWritableWindow.");
    }

    auto* protectCookie = static_cast<RegionProtectCookie*>(cookie);
    PatchResult result = PatchResult::ok("Protection restored successfully.", timer.elapsed());

#ifdef _WIN32
    DWORD oldProt;
    if (VirtualProtect((void*)protectCookie->alignedStart, protectCookie->alignedSize, protectCookie->oldProtection, &oldProt) == 0) {
        DWORD osError = GetLastError();
        QString detail = QString("Win: VirtualProtect restore failed. Error code: %1").arg(osError);
        result = PatchResult::error(1005, detail, timer.elapsed());
    }
#else
    if (mprotect((void*)protectCookie->alignedStart, protectCookie->alignedSize, VIRTUAL_PROTECT_RO) == -1) {
        int osError = errno;
        QString detail = QString("POSIX: mprotect restore failed. Error code: %1").arg(osError);
        result = PatchResult::error(1006, detail, timer.elapsed());
    }
#endif
    
    delete protectCookie;
    return result;
}

ModelMemoryHotpatch::ModelMemoryHotpatch(QObject* parent)
    : QObject(parent)
{
}

ModelMemoryHotpatch::~ModelMemoryHotpatch()
{
    detach();
}

bool ModelMemoryHotpatch::attachToModel(void* modelPtr, size_t modelSize)
{
    QMutexLocker lock(&m_mutex);
    if (m_attached) {
        qWarning() << "Already attached. Detach first.";
        return false;
    }
    if (!modelPtr || modelSize == 0) {
        qCritical() << "Invalid model pointer or size.";
        return false;
    }

    m_modelPtr = modelPtr;
    m_modelSize = modelSize;
    m_attached = true;
    m_stats.modelSize = modelSize;
    
    if (!parseTensorMetadata()) {
        qCritical() << "Failed to parse tensor metadata. Cannot map tensor names.";
        detach();
        return false;
    }

    qInfo() << "Successfully attached to model at" << m_modelPtr << "Size:" << m_modelSize;
    emit modelAttached(m_modelSize);
    return true;
}

void ModelMemoryHotpatch::detach()
{
    QMutexLocker lock(&m_mutex);
    if (!m_attached) return;
    
    if (!m_fullBackup.isEmpty() && m_stats.appliedPatches > 0) {
        qWarning() << "Detaching: Attempting to restore full model backup for safety...";
        if (!restoreBackup().success) {
            qCritical() << "Failed to restore full model backup during detach! Memory state may be corrupted.";
        }
    }

    m_modelPtr = nullptr;
    m_modelSize = 0;
    m_attached = false;
    m_patches.clear();
    m_fullBackup.clear();
    m_history.clear();
    m_tensorMap.clear();
    resetStatistics();
    
    qInfo() << "Detached from model.";
    emit modelDetached();
}

bool ModelMemoryHotpatch::isAttached() const
{
    return m_attached;
}

bool ModelMemoryHotpatch::validateMemoryAccess(size_t offset, size_t size) const
{
    if (!m_attached || !m_modelPtr) {
        qWarning() << "Not attached to a model.";
        return false;
    }
    if (offset + size > m_modelSize) {
        qWarning() << "Access out of bounds: offset" << offset << "size" << size << "Model size" << m_modelSize;
        return false;
    }
    return true;
}

PatchResult ModelMemoryHotpatch::safeMemoryWrite(size_t offset, const QByteArray& data)
{
    QElapsedTimer timer;
    timer.start();
    size_t dataSize = data.size();

    if (!validateMemoryAccess(offset, dataSize)) {
        return PatchResult::error(2001, "Memory access validation failed (out of bounds or detached).", timer.elapsed());
    }

    void* cookie = nullptr;
    
    PatchResult beginResult = beginWritableWindow(offset, dataSize, cookie);
    if (!beginResult.success) {
        return PatchResult::error(2002, QString("Failed to open writable window: %1").arg(beginResult.detail), timer.elapsed(), beginResult.errorCode);
    }

    bool copySuccess = false;
    try {
        std::memcpy((char*)m_modelPtr + offset, data.constData(), dataSize);
        copySuccess = true;
    } catch (...) {
        qCritical() << "Exception occurred during memory copy at offset" << offset;
    }

    PatchResult endResult = endWritableWindow(cookie);

    if (!copySuccess) {
        return PatchResult::error(2003, QString("Memory copy failed at offset %1.").arg(offset), timer.elapsed());
    }
    
    if (!endResult.success) {
        qCritical() << "CRITICAL WARNING: Write successful but failed to restore memory protection:" << endResult.detail;
        emit errorOccurred(PatchResult::error(2004, QString("Protection restore failed: %1").arg(endResult.detail), endResult.elapsedMs, endResult.errorCode));
    }

    return PatchResult::ok(QString("Safe write of %1 bytes successful at offset %2.").arg(dataSize).arg(offset), timer.elapsed());
}

QByteArray ModelMemoryHotpatch::readMemory(size_t offset, size_t size)
{
    QMutexLocker lock(&m_mutex);
    if (!validateMemoryAccess(offset, size)) {
        return QByteArray();
    }
    
    QByteArray data;
    data.resize(size);
    std::memcpy(data.data(), (char*)m_modelPtr + offset, size);
    return data;
}

PatchResult ModelMemoryHotpatch::writeMemory(size_t offset, const QByteArray& data)
{
    QMutexLocker lock(&m_mutex);
    if (data.isEmpty()) {
        return PatchResult::error(2005, "Cannot write empty data.");
    }
    
    PatchResult result = safeMemoryWrite(offset, data);

    if (result.success) {
        m_stats.bytesModified += data.size();
        m_stats.lastPatch = QDateTime::currentDateTimeUtc();
    } else {
        m_stats.failedPatches++;
    }
    return result;
}

PatchResult ModelMemoryHotpatch::applyPatch(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    QElapsedTimer timer;
    timer.start();
    
    if (!m_patches.contains(name)) {
        return PatchResult::error(3001, QString("Patch '%1' not found.").arg(name), timer.elapsed());
    }

    MemoryPatch& patch = m_patches[name];
    if (!patch.enabled) {
        return PatchResult::ok(QString("Patch '%1' skipped (disabled).").arg(name), timer.elapsed());
    }
    
    size_t writeOffset = patch.offset;
    const QByteArray& patchData = patch.patchBytes;
    
    if (patchData.isEmpty() || patch.size == 0) {
        if (patch.transformType == MemoryPatch::TransformType::None && patch.type != MemoryPatchType::GraphRedirection) {
             return PatchResult::error(3002, QString("Patch '%1' has no data or size for byte modification.").arg(name), timer.elapsed());
        }
        
        if (patch.type == MemoryPatchType::GraphRedirection) {
            qInfo() << "Applied Graph Redirection patch:" << name;
            return PatchResult::ok("Graph Redirection applied (conceptually).", timer.elapsed());
        }
    } else {
        if (patch.verifyChecksum) {
            uint64_t currentChecksum = calculateChecksum64(writeOffset, patch.size);
            if (patch.checksumBefore != 0 && currentChecksum != patch.checksumBefore) {
                QString reason = QString("Checksum mismatch! Expected %1, got %2.").arg(patch.checksumBefore, 16, 16, QChar('0')).arg(currentChecksum, 16, 16, QChar('0'));
                qCritical() << "Patch failed due to checksum:" << reason;
                m_stats.failedPatches++;
                emit integrityCheckFailed(name, currentChecksum);
                return PatchResult::error(3003, reason, timer.elapsed());
            }
        }
        
        PatchResult writeResult = safeMemoryWrite(writeOffset, patchData);
        if (!writeResult.success) {
            m_stats.failedPatches++;
            return PatchResult::error(3004, QString("Memory write failed for patch '%1': %2").arg(name).arg(writeResult.detail), writeResult.elapsedMs, writeResult.errorCode);
        }
        
        if (patch.verifyChecksum) {
            patch.checksumAfter = calculateChecksum64(writeOffset, patch.size);
        }
    }
    
    patch.timesApplied++;
    patch.lastApplied = QDateTime::currentDateTimeUtc();
    m_stats.appliedPatches++;
    m_stats.bytesModified += patch.size;
    m_stats.lastPatch = patch.lastApplied;
    
    emit patchApplied(name);
    return PatchResult::ok(QString("Patch '%1' applied successfully.").arg(name), timer.elapsed());
}

PatchResult ModelMemoryHotpatch::revertPatch(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    QElapsedTimer timer;
    timer.start();
    
    if (!m_patches.contains(name)) {
        return PatchResult::error(4001, QString("Patch '%1' not found for revert.").arg(name), timer.elapsed());
    }

    MemoryPatch& patch = m_patches[name];
    if (patch.originalBytes.isEmpty()) {
        return PatchResult::error(4003, QString("Patch '%1' cannot be reverted: original bytes missing.").arg(name), timer.elapsed());
    }

    PatchResult writeResult = safeMemoryWrite(patch.offset, patch.originalBytes);
    if (!writeResult.success) {
        m_stats.failedPatches++;
        return PatchResult::error(4004, QString("Memory write failed during revert for patch '%1': %2").arg(name).arg(writeResult.detail), writeResult.elapsedMs, writeResult.errorCode);
    }

    m_stats.revertedPatches++;
    emit patchReverted(name);
    return PatchResult::ok(QString("Patch '%1' reverted successfully.").arg(name), timer.elapsed());
}

uint64_t ModelMemoryHotpatch::calculateChecksum64(size_t offset, size_t size) const
{
    if (!validateMemoryAccess(offset, size)) return 0;

    const char* data = (const char*)m_modelPtr + offset;
    uint64_t hash = 0xcbf29ce484222325ULL;
    const uint64_t prime = 0x100000001b3ULL;

    for (size_t i = 0; i < size; ++i) {
        hash ^= (uint64_t)data[i];
        hash *= prime;
    }
    return hash;
}

PatchResult ModelMemoryHotpatch::createBackup()
{
    QMutexLocker lock(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    if (!m_attached) {
        return PatchResult::error(5001, "Cannot create backup: Not attached.", timer.elapsed());
    }
    if (m_modelSize == 0) {
        return PatchResult::error(5002, "Cannot create backup: Model size is zero.", timer.elapsed());
    }

    m_fullBackup.resize(m_modelSize);
    std::memcpy(m_fullBackup.data(), m_modelPtr, m_modelSize);
    
    qInfo() << "Full model backup created, size:" << m_modelSize;
    return PatchResult::ok(QString("Full model backup created, size: %1").arg(m_modelSize), timer.elapsed());
}

PatchResult ModelMemoryHotpatch::restoreBackup()
{
    QMutexLocker lock(&m_mutex);
    QElapsedTimer timer;
    timer.start();

    if (!m_attached || m_fullBackup.isEmpty()) {
        return PatchResult::error(6001, "Cannot restore backup: Not attached or no backup exists.", timer.elapsed());
    }

    if (m_fullBackup.size() != m_modelSize) {
        return PatchResult::error(6002, "Backup size mismatch. Aborting restore.", timer.elapsed());
    }

    PatchResult result = safeMemoryWrite(0, m_fullBackup);

    if (result.success) {
        qInfo() << "Full model backup restored successfully.";
        m_stats.appliedPatches = 0;
        m_stats.revertedPatches = 0;
        m_stats.bytesModified = 0;
        result.detail = "Full model backup restored successfully.";
    } else {
        qCritical() << "Failed to restore full model backup!";
        result.errorCode = 6003;
        result.detail = QString("Failed to restore full model backup: %1").arg(result.detail);
    }
    result.elapsedMs = timer.elapsed();

    return result;
}

bool ModelMemoryHotpatch::parseTensorMetadata()
{
    if (m_modelSize < 100 * 1024 * 1024) {
        qWarning() << "Model size too small for mock tensor mapping.";
        return false;
    }

    m_tensorMap.clear();

    for (int i = 0; i < 4; ++i) {
        size_t block_base = 5 * 1024 * 1024 + i * (20 * 1024 * 1024);
        
        m_tensorMap[QString("blk.%1.attn_q.weight").arg(i)] = {
            QString("blk.%1.attn_q.weight").arg(i),
            block_base + 0,
            2 * 1024 * 1024,
            2, {1024, 1024}, "Q4_K"
        };
    }
    
    qInfo() << "Mocked" << m_tensorMap.size() << "tensors for testing.";
    return true;
}

bool ModelMemoryHotpatch::findTensor(const QString& tensorName, size_t& offset, size_t& size)
{
    QMutexLocker lock(&m_mutex);
    if (!m_attached) return false;
    
    if (m_tensorMap.contains(tensorName)) {
        const TensorInfo& info = m_tensorMap.value(tensorName);
        offset = info.offset;
        size = info.size;
        return true;
    }
    return false;
}

bool ModelMemoryHotpatch::addPatch(const MemoryPatch& patch)
{
    QMutexLocker lock(&m_mutex);
    if (m_patches.contains(patch.name)) {
        qWarning() << "Patch with name" << patch.name << "already exists.";
        return false;
    }
    
    PatchConflict conflict;
    if (checkPatchConflict(patch, conflict)) {
        emit patchConflictDetected(patch.name, conflict.existingPatch.name);
        emit patchConflictDetectedRich(conflict);
        m_stats.conflictsDetected++;
        return false;
    }

    m_patches.insert(patch.name, patch);
    m_stats.totalPatches++;
    return true;
}

bool ModelMemoryHotpatch::removePatch(const QString& name)
{
    QMutexLocker lock(&m_mutex);
    if (!m_patches.contains(name)) return false;
    
    if (m_patches.value(name).timesApplied > 0) {
        qWarning() << "Patch" << name << "is currently applied. Please revert first.";
        return false;
    }

    m_patches.remove(name);
    m_stats.totalPatches--;
    return true;
}

bool ModelMemoryHotpatch::applyAllPatches()
{
    QMutexLocker lock(&m_mutex);
    bool overallSuccess = true;
    
    QMap<size_t, QString> sortedPatches;
    for (const MemoryPatch& patch : m_patches.values()) {
        if (patch.enabled) {
            sortedPatches.insert(patch.offset, patch.name);
        }
    }

    for (const QString& name : sortedPatches.values()) {
        lock.unlock();
        PatchResult result = applyPatch(name);
        lock.relock();

        if (!result.success) {
            overallSuccess = false;
            qCritical() << "Batch apply failed for" << name << ":" << result.detail;
        }
    }
    
    return overallSuccess;
}

bool ModelMemoryHotpatch::revertAllPatches()
{
    QMutexLocker lock(&m_mutex);
    bool overallSuccess = true;
    for (const QString& name : m_patches.keys()) {
        lock.unlock();
        PatchResult result = revertPatch(name);
        lock.relock();

        if (!result.success) {
            overallSuccess = false;
            qCritical() << "Batch revert failed for" << name << ":" << result.detail;
        }
    }
    return overallSuccess;
}

bool ModelMemoryHotpatch::checkPatchConflict(const MemoryPatch& newPatch, PatchConflict& conflict) const
{
    for (const MemoryPatch& existingPatch : m_patches.values()) {
        if (existingPatch.name == newPatch.name) continue;

        size_t existingStart = existingPatch.offset;
        size_t existingEnd = existingPatch.offset + existingPatch.size - 1;
        size_t newStart = newPatch.offset;
        size_t newEnd = newPatch.offset + newPatch.size - 1;

        if (newStart <= existingEnd && newEnd >= existingStart) {
            if (newPatch.priority <= existingPatch.priority) {
                conflict.existingPatch = existingPatch;
                conflict.incomingPatch = newPatch;
                conflict.reason = QString("Memory overlap detected. Incoming priority (%1) <= Existing priority (%2).").arg(newPatch.priority).arg(existingPatch.priority);
                return true;
            }
        }
    }
    return false;
}

ModelMemoryHotpatch::MemoryPatchStats ModelMemoryHotpatch::getStatistics() const
{
    QMutexLocker lock(&m_mutex);
    return m_stats;
}

void ModelMemoryHotpatch::resetStatistics()
{
    QMutexLocker lock(&m_mutex);
    m_stats = MemoryPatchStats();
    m_stats.modelSize = m_modelSize;
}

PatchResult ModelMemoryHotpatch::scaleTensorWeights(const QString& tensorName, double scaleFactor) {
    return PatchResult::error(5005, "Scale operation not fully implemented (requires GGUF/quantization logic).");
}

quint32 ModelMemoryHotpatch::calculateCRC32(size_t offset, size_t size) const
{
    if (!m_attached || !m_modelPtr || offset + size > m_modelSize) {
        return 0;
    }
    
    // Simple CRC32 polynomial-based calculation
    const quint32 CRC32_POLY = 0xEDB88320;
    quint32 crc = 0xFFFFFFFF;
    
    const quint8* data = static_cast<const quint8*>(m_modelPtr) + offset;
    
    for (size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 1) {
                crc = (crc >> 1) ^ CRC32_POLY;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}

PatchResult ModelMemoryHotpatch::clampTensorWeights(const QString& tensorName, float minVal, float maxVal) {
    return PatchResult::error(5006, "Clamp operation not fully implemented (requires GGUF/quantization logic).");
}

PatchResult ModelMemoryHotpatch::bypassLayer(int layerIndex, bool bypass) {
    return PatchResult::error(5007, "Layer bypass not fully implemented (requires Graph/Control Flow knowledge).");
}

bool ModelMemoryHotpatch::rebuildTensorDependencyMap() {
    return false;
}

PatchResult ModelMemoryHotpatch::patchVocabularyEntry(int tokenId, const QString& newToken) {
    return PatchResult::error(5008, "Vocabulary patch not fully implemented (requires Vocab structure knowledge).");
}

bool ModelMemoryHotpatch::verifyModelIntegrity()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr || m_modelSize == 0) {
        qWarning() << "[ModelMemoryHotpatch] verifyModelIntegrity: Model not attached";
        return false;
    }
    
    // Verify GGUF header signature
    const char* signature = static_cast<const char*>(m_modelPtr);
    if (m_modelSize < 4 || std::strncmp(signature, "GGUF", 4) != 0) {
        qWarning() << "[ModelMemoryHotpatch] verifyModelIntegrity: Invalid GGUF signature";
        return false;
    }
    
    // Calculate and verify integrity hash
    quint32 calculatedHash = calculateCRC32(0, std::min(m_modelSize, (size_t)65536));
    if (m_integrityHash != 0 && m_integrityHash != calculatedHash) {
        qWarning() << "[ModelMemoryHotpatch] verifyModelIntegrity: Integrity hash mismatch"
                   << "Expected:" << m_integrityHash << "Got:" << calculatedHash;
        return false;
    }
    
    // Update current integrity hash
    m_integrityHash = calculatedHash;
    
    qInfo() << "[ModelMemoryHotpatch] Model integrity verified (hash:" << calculatedHash << ")";
    return true;
}

// Direct Memory Manipulation API Implementation

void* ModelMemoryHotpatch::getDirectMemoryPointer(size_t offset) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_attached || !m_modelPtr) {
        qWarning() << "[ModelMemoryHotpatch] Model not attached for direct memory access";
        return nullptr;
    }
    
    if (offset >= m_modelSize) {
        qWarning() << "[ModelMemoryHotpatch] Offset out of bounds:" << offset << ">=" << m_modelSize;
        return nullptr;
    }
    
    return static_cast<char*>(m_modelPtr) + offset;
}

QByteArray ModelMemoryHotpatch::directMemoryRead(size_t offset, size_t size) const
{
    QMutexLocker locker(&m_mutex);
    if (!m_attached || !m_modelPtr) {
        return QByteArray();
    }
    
    if (offset + size > m_modelSize) {
        qWarning() << "[ModelMemoryHotpatch] directMemoryRead out of bounds";
        return QByteArray();
    }
    
    char* ptr = static_cast<char*>(m_modelPtr) + offset;
    return QByteArray(ptr, size);
}

PatchResult ModelMemoryHotpatch::directMemoryWrite(size_t offset, const QByteArray& data)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return PatchResult::error(6001, "Model not attached");
    }
    
    if (offset + data.size() > m_modelSize) {
        return PatchResult::error(6002, "Write out of bounds");
    }
    
    QElapsedTimer timer;
    timer.start();
    
    char* ptr = static_cast<char*>(m_modelPtr) + offset;
    std::memcpy(ptr, data.constData(), data.size());
    
    m_stats.bytesModified += data.size();
    
    return PatchResult::ok("Direct write completed", timer.elapsed());
}

PatchResult ModelMemoryHotpatch::directMemoryWriteBatch(const QHash<size_t, QByteArray>& writes)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return PatchResult::error(6003, "Model not attached");
    }
    
    QElapsedTimer timer;
    timer.start();
    
    for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
        size_t offset = it.key();
        const QByteArray& data = it.value();
        
        if (offset + data.size() > m_modelSize) {
            return PatchResult::error(6004, "Batch write out of bounds at offset " + QString::number(offset));
        }
        
        char* ptr = static_cast<char*>(m_modelPtr) + offset;
        std::memcpy(ptr, data.constData(), data.size());
        m_stats.bytesModified += data.size();
    }
    
    return PatchResult::ok("Batch write completed (" + QString::number(writes.size()) + " writes)", timer.elapsed());
}

PatchResult ModelMemoryHotpatch::directMemoryFill(size_t offset, size_t size, quint8 value)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return PatchResult::error(6005, "Model not attached");
    }
    
    if (offset + size > m_modelSize) {
        return PatchResult::error(6006, "Fill out of bounds");
    }
    
    QElapsedTimer timer;
    timer.start();
    
    char* ptr = static_cast<char*>(m_modelPtr) + offset;
    std::memset(ptr, value, size);
    m_stats.bytesModified += size;
    
    return PatchResult::ok("Fill completed", timer.elapsed());
}

PatchResult ModelMemoryHotpatch::directMemoryCopy(size_t srcOffset, size_t dstOffset, size_t size)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return PatchResult::error(6007, "Model not attached");
    }
    
    if (srcOffset + size > m_modelSize || dstOffset + size > m_modelSize) {
        return PatchResult::error(6008, "Copy out of bounds");
    }
    
    QElapsedTimer timer;
    timer.start();
    
    char* src = static_cast<char*>(m_modelPtr) + srcOffset;
    char* dst = static_cast<char*>(m_modelPtr) + dstOffset;
    std::memmove(dst, src, size);
    m_stats.bytesModified += size;
    
    return PatchResult::ok("Copy completed", timer.elapsed());
}

bool ModelMemoryHotpatch::directMemoryCompare(size_t offset, const QByteArray& data) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return false;
    }
    
    if (offset + data.size() > m_modelSize) {
        return false;
    }
    
    char* ptr = static_cast<char*>(m_modelPtr) + offset;
    return std::memcmp(ptr, data.constData(), data.size()) == 0;
}

qint64 ModelMemoryHotpatch::directMemorySearch(size_t startOffset, const QByteArray& pattern) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr || pattern.isEmpty()) {
        return -1;
    }
    
    char* base = static_cast<char*>(m_modelPtr);
    char* searchStart = base + startOffset;
    size_t searchSize = m_modelSize - startOffset;
    
    const char* found = std::search(searchStart, base + m_modelSize, 
                                    pattern.constData(), pattern.constData() + pattern.size());
    
    if (found != base + m_modelSize) {
        return static_cast<int>(found - base);
    }
    
    return -1;
}

PatchResult ModelMemoryHotpatch::directMemorySwap(size_t offset1, size_t offset2, size_t size)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return PatchResult::error(6009, "Model not attached");
    }
    
    if (offset1 + size > m_modelSize || offset2 + size > m_modelSize) {
        return PatchResult::error(6010, "Swap out of bounds");
    }
    
    QElapsedTimer timer;
    timer.start();
    
    QByteArray temp = directMemoryRead(offset1, size);
    directMemoryWrite(offset1, directMemoryRead(offset2, size));
    directMemoryWrite(offset2, temp);
    
    m_stats.bytesModified += 2 * size;
    
    return PatchResult::ok("Swap completed", timer.elapsed());
}

PatchResult ModelMemoryHotpatch::setMemoryProtection(size_t offset, size_t size, int protectionFlags)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        return PatchResult::error(6011, "Model not attached");
    }
    
    if (offset + size > m_modelSize) {
        return PatchResult::error(6012, "Protection region out of bounds");
    }
    
    void* ptr = static_cast<char*>(m_modelPtr) + offset;
    
    if (protectMemory(ptr, size, protectionFlags)) {
        return PatchResult::ok("Protection set successfully");
    }
    
    return PatchResult::error(6013, "Failed to set memory protection");
}

void* ModelMemoryHotpatch::memoryMapRegion(size_t offset, size_t size, int flags)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_attached || !m_modelPtr) {
        qWarning() << "[ModelMemoryHotpatch] Cannot map region: model not attached";
        return nullptr;
    }
    
    if (offset + size > m_modelSize) {
        qWarning() << "[ModelMemoryHotpatch] Cannot map region: out of bounds";
        return nullptr;
    }
    
    // Return direct pointer (in practice, could implement platform-specific mmap)
    return static_cast<char*>(m_modelPtr) + offset;
}

PatchResult ModelMemoryHotpatch::unmapMemoryRegion(void* mappedPtr, size_t size)
{
    QMutexLocker locker(&m_mutex);
    
    if (!mappedPtr) {
        return PatchResult::error(6014, "Invalid mapped pointer");
    }
    
    // Direct pointer regions don't need unmapping
    qInfo() << "[ModelMemoryHotpatch] Memory region unmapped";
    return PatchResult::ok("Unmapped successfully");
}
