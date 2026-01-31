// model_memory_hotpatch.hpp - Live RAM model patching with cross-platform memory protection
// Supports Windows VirtualProtect and POSIX mprotect for safe memory manipulation

#pragma once


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
    std::string detail;
    int errorCode = 0;
    int64_t elapsedMs = 0;
    
    static PatchResult ok(const std::string& msg = "OK", int64_t elapsed = 0) {
        PatchResult r; r.success = true; r.detail = msg; r.elapsedMs = elapsed; return r;
    }
    static PatchResult error(int code, const std::string& msg, int64_t elapsed = 0, int osError = 0) {
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
    std::string name;
    std::string description;
    MemoryPatchType type;
    bool enabled = true;
    
    size_t offset = 0;
    size_t size = 0;
    std::vector<uint8_t> patchBytes;
    std::vector<uint8_t> originalBytes;
    
    enum class TransformType { None, Scale, Clamp, Offset, Custom };
    TransformType transformType = TransformType::None;
    double transformParam1 = 0.0;
    double transformParam2 = 0.0;
    
    bool verifyChecksum = false;
    uint64_t checksumBefore = 0;
    uint64_t checksumAfter = 0;
    
    int priority = 0;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point lastApplied;
    int timesApplied = 0;
};

struct TensorInfo {
    std::string name;
    size_t offset;
    size_t size;
    int nDims;
    std::vector<int> shape;
    std::string quantType;
};

struct PatchConflict {
    MemoryPatch existingPatch;
    MemoryPatch incomingPatch;
    std::string reason;
};

class ModelMemoryHotpatch : public void {

public:
    explicit ModelMemoryHotpatch(void* parent = nullptr);
    ~ModelMemoryHotpatch();

    // Attachment
    bool attachToModel(void* modelPtr, size_t modelSize);
    void detach();
    bool isAttached() const;

    // Patch management
    bool addPatch(const MemoryPatch& patch);
    bool removePatch(const std::string& name);
    PatchResult applyPatch(const std::string& name);
    PatchResult revertPatch(const std::string& name);
    bool applyAllPatches();
    bool revertAllPatches();

    // Memory I/O
    std::vector<uint8_t> readMemory(size_t offset, size_t size);
    PatchResult writeMemory(size_t offset, const std::vector<uint8_t>& data);

    // High-level operations
    PatchResult scaleTensorWeights(const std::string& tensorName, double scaleFactor);
    PatchResult clampTensorWeights(const std::string& tensorName, float minVal, float maxVal);
    PatchResult bypassLayer(int layerIndex, bool bypass);
    PatchResult patchVocabularyEntry(int tokenId, const std::string& newToken);

    // Safety
    PatchResult createBackup();
    PatchResult restoreBackup();
    bool verifyModelIntegrity();
    bool checkPatchConflict(const MemoryPatch& newPatch, PatchConflict& conflict) const;

    // Tensor lookup
    bool findTensor(const std::string& tensorName, size_t& offset, size_t& size);

    // Direct Memory Manipulation API
    void* getDirectMemoryPointer(size_t offset = 0) const;
    std::vector<uint8_t> directMemoryRead(size_t offset, size_t size) const;
    PatchResult directMemoryWrite(size_t offset, const std::vector<uint8_t>& data);
    PatchResult directMemoryWriteBatch(const std::unordered_map<size_t, std::vector<uint8_t>>& writes);
    PatchResult directMemoryFill(size_t offset, size_t size, uint8_t value);
    PatchResult directMemoryCopy(size_t srcOffset, size_t dstOffset, size_t size);
    bool directMemoryCompare(size_t offset, const std::vector<uint8_t>& data) const;
    int64_t directMemorySearch(size_t startOffset, const std::vector<uint8_t>& pattern) const;
    PatchResult directMemorySwap(size_t offset1, size_t offset2, size_t size);
    PatchResult setMemoryProtection(size_t offset, size_t size, int protectionFlags);
    
    // Memory mapping
    void* memoryMapRegion(size_t offset, size_t size, int flags);
    PatchResult unmapMemoryRegion(void* mappedPtr, size_t size);

    struct MemoryPatchStats {
        uint64_t totalPatches = 0;
        uint64_t appliedPatches = 0;
        uint64_t revertedPatches = 0;
        uint64_t failedPatches = 0;
        uint64_t bytesModified = 0;
        uint64_t conflictsDetected = 0;
        size_t modelSize = 0;
        std::chrono::system_clock::time_point lastPatch;
    };
    MemoryPatchStats getStatistics() const;
    void resetStatistics();

    void modelAttached(size_t modelSize);
    void modelDetached();
    void patchApplied(const std::string& name);
    void patchReverted(const std::string& name);
    void integrityCheckFailed(const std::string& patchName, uint64_t actualChecksum);
    void patchConflictDetected(const std::string& newPatch, const std::string& existingPatch);
    void patchConflictDetectedRich(const PatchConflict& conflict);
    void errorOccurred(const PatchResult& result);

private:
    struct RegionProtectCookie;
    
    bool validateMemoryAccess(size_t offset, size_t size) const;
    PatchResult safeMemoryWrite(size_t offset, const std::vector<uint8_t>& data);
    PatchResult beginWritableWindow(size_t offset, size_t size, void*& cookie);
    PatchResult endWritableWindow(void* cookie);
    bool protectMemory(void* ptr, size_t size, int protectionFlags);
    size_t systemPageSize() const;
    uint64_t calculateChecksum64(size_t offset, size_t size) const;
    uint32_t calculateCRC32(size_t offset, size_t size) const;
    bool parseTensorMetadata();
    bool rebuildTensorDependencyMap();

    void* m_modelPtr = nullptr;
    size_t m_modelSize = 0;
    bool m_attached = false;
    uint32_t m_integrityHash = 0;

    std::unordered_map<std::string, MemoryPatch> m_patches;
    std::unordered_map<std::string, TensorInfo> m_tensorMap;
    std::vector<uint8_t> m_fullBackup;
    std::vector<std::string> m_history;
    
    MemoryPatchStats m_stats;
    mutable std::mutex m_mutex;

    struct BatchConfig {
        bool enableBatching = true;
        size_t maxBatchSize = 16 * 1024 * 1024;
    } m_batchConfig;
};


