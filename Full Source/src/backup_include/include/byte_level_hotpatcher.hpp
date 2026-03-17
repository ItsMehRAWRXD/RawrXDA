// byte_level_hotpatcher.hpp - Precision byte-level model patching
#pragma once

#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

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
    std::string name;
    std::string description;
    bool enabled = true;
    
    size_t offset = 0;
    size_t length = 1;
    
    ByteOperation operation;
    std::vector<uint8_t> operand;
    uint8_t bitMask = 0xFF;
    int bitShift = 0;
    
    std::vector<uint8_t> expectedBefore;
    std::vector<uint8_t> expectedAfter;
    HashAlgorithm hashAlgo = HashAlgorithm::CRC32;
    uint32_t targetIntegrityHash = 0;
    
    std::vector<uint8_t> originalBytes;
    std::string category;
    int priority = 0;
    std::chrono::system_clock::time_point created;
    int timesApplied = 0;
    
    std::vector<std::string> requiresPatches;
    std::vector<std::string> conflictsWith;
};

class ByteLevelHotpatcher {
public:
    explicit ByteLevelHotpatcher(void* parent = nullptr);
    ~ByteLevelHotpatcher();

    bool loadModel(const std::string& filePath);
    bool saveModel(const std::string& filePath);
    const std::vector<uint8_t>& getModelData() const { return m_modelData; }
    bool isModelLoaded() const { return !m_modelData.empty(); }

    bool addPatch(const BytePatch& patch);
    bool removePatch(const std::string& name);
    bool applyPatch(const std::string& name);
    bool revertPatch(const std::string& name);
    void revertAllPatches();
    
    bool replaceByte(size_t offset, uint8_t oldValue, uint8_t newValue);
    bool replaceBytes(size_t offset, const std::vector<uint8_t>& oldBytes, const std::vector<uint8_t>& newBytes);
    bool flipBits(size_t offset, uint8_t bitMask);
    
    std::vector<size_t> findPattern(const std::vector<uint8_t>& pattern) const;
    bool replacePattern(const std::vector<uint8_t>& pattern, const std::vector<uint8_t>& replacement, int maxOccurrences = -1);
    
    uint32_t calculateCRC32(size_t offset, size_t length) const;
    uint64_t calculateFNV1a_64(size_t offset, size_t length) const;
    
    std::string hexDump(size_t offset, size_t length, int bytesPerLine = 16) const;

    // Direct Memory Manipulation API
    void* getDirectPointer(size_t offset = 0) const;
    std::vector<uint8_t> directRead(size_t offset, size_t size) const;
    PatchResult directWrite(size_t offset, const std::vector<uint8_t>& data);
    PatchResult directWriteBatch(const std::unordered_map<size_t, std::vector<uint8_t>>& writes);
    PatchResult directFill(size_t offset, size_t size, uint8_t value);
    PatchResult directCopy(size_t srcOffset, size_t dstOffset, size_t size);
    bool directCompare(size_t offset, const std::vector<uint8_t>& data) const;
    std::vector<uint8_t> directXOR(size_t offset, size_t size, const std::vector<uint8_t>& key);
    PatchResult directBitOperation(size_t offset, size_t size, ByteOperation op, uint8_t operand);
    PatchResult directRotate(size_t offset, size_t size, int bitShift, bool leftShift = true);
    PatchResult directReverse(size_t offset, size_t size);
    int64_t directSearch(size_t startOffset, const std::vector<uint8_t>& pattern) const;
    PatchResult atomicByteSwap(size_t offset1, size_t offset2, size_t size);

    struct BytePatchStats {
        uint64_t totalPatches = 0;
        uint64_t bytesPatched = 0;
        uint64_t patchesApplied = 0;
        uint64_t patchesReverted = 0;
        size_t modelSize = 0;
        std::unordered_map<int, int> operationCounts; // ByteOperation cast to int -> count
    };
    BytePatchStats getStatistics() const;

private:
    std::vector<uint8_t> m_modelData;
    std::string m_modelPath;
    std::unordered_map<std::string, BytePatch> m_patches;
    BytePatchStats m_stats;
    mutable std::mutex m_mutex;
    
    static constexpr size_t MAX_MODEL_SIZE = 100ULL * 1024 * 1024 * 1024;
};
