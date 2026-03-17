// src/direct_io/jit_lba_mapper.h
// ═══════════════════════════════════════════════════════════════════════════════
// RAWRXD v1.2.0 — JIT-LBA MAPPER C++ INTERFACE
// ═══════════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

// ─────────────────────────────────────────────────────────────────────────────
// JITMAP FILE FORMAT
// ─────────────────────────────────────────────────────────────────────────────

#pragma pack(push, 1)

struct JitMapHeader {
    uint32_t magic;         // 'JLBA' (0x41424C4A)
    uint32_t version;       // 0x00010200 for v1.2.0
    uint32_t entryCount;
    uint32_t reserved;
};

struct JitMapEntry {
    uint64_t tensorUID;     // FNV-1a hash of tensor name
    uint8_t  driveIndex;    // 0-4 for 5-drive grid
    uint8_t  reserved1[3];
    uint32_t sectorCount;   // Number of 512-byte sectors
    uint64_t startLBA;      // Physical LBA on the drive
    uint64_t reserved2;
};

#pragma pack(pop)

static_assert(sizeof(JitMapEntry) == 32, "JitMapEntry must be 32 bytes");

constexpr uint32_t JITMAP_MAGIC   = 0x41424C4A; // 'JLBA'
constexpr uint32_t JITMAP_VERSION = 0x00010200;

// ─────────────────────────────────────────────────────────────────────────────
// ASSEMBLY INTERFACE (implemented in jit_lba_mapper.asm)
// ─────────────────────────────────────────────────────────────────────────────

extern "C" {
    // Initialize JIT-LBA mapper from memory-mapped .jitmap data
    int64_t JitLBA_Init(const void* jitmapData, size_t jitmapSize);
    
    // Lookup tensor by UID, returns pointer to entry or nullptr
    JitMapEntry* JitLBA_Lookup(uint64_t tensorUID);
    
    // Submit NVMe read command directly to controller
    // Returns command ID or negative error code
    int64_t JitLBA_SubmitRead(uint32_t driveIndex, uint64_t startLBA, 
                               uint32_t sectorCount, void* dmaDestination);
    
    // High-level: lookup + submit in one call
    int64_t JitLBA_BurstTensor(uint64_t tensorUID, void* dmaDestination);
    
    // Configure NVMe controller MMIO addresses
    int64_t JitLBA_SetDriveMMIO(uint32_t driveIndex, uint64_t bar0Base,
                                 uint64_t doorbellOffset, void* sqBase,
                                 uint32_t sqSize);
    
    // Hash tensor name to UID
    uint64_t JitLBA_HashTensorName(const char* name);
}

// ─────────────────────────────────────────────────────────────────────────────
// C++ HELPER CLASS
// ─────────────────────────────────────────────────────────────────────────────

class JitLBAMapper {
public:
    // Load .jitmap file
    bool LoadFromFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;
        
        size_t size = file.tellg();
        file.seekg(0);
        
        m_data.resize(size);
        file.read(reinterpret_cast<char*>(m_data.data()), size);
        
        return JitLBA_Init(m_data.data(), m_data.size()) == 0;
    }
    
    // Generate .jitmap from GGUF tensor metadata
    static bool GenerateFromGGUF(const std::string& ggufPath, 
                                  const std::string& outputPath,
                                  const std::vector<std::string>& driveDevices);
    
    // Hash a tensor name
    static uint64_t HashName(const std::string& name) {
        return JitLBA_HashTensorName(name.c_str());
    }
    
    // Burst-load a tensor by name
    int64_t BurstByName(const std::string& tensorName, void* dest) {
        return JitLBA_BurstTensor(HashName(tensorName), dest);
    }
    
private:
    std::vector<uint8_t> m_data;
};

// ─────────────────────────────────────────────────────────────────────────────
// DRIVE TOPOLOGY CONFIGURATION
// ─────────────────────────────────────────────────────────────────────────────

struct DriveTopology {
    // 5-Drive Grid Configuration for 7800X3D + 7800XT System
    // Drive 0: Primary OS + KV Cache (fastest seek)
    // Drive 1: Model Shard A (Q tensors)
    // Drive 2: Model Shard B (K tensors)  
    // Drive 3: Model Shard C (V tensors)
    // Drive 4: Attention/Output heads
    
    struct DriveConfig {
        std::string devicePath;     // \\.\PhysicalDriveN or /dev/nvmeXn1
        uint64_t    bar0Address;    // PCIe BAR0 (from lspci/RW-Everything)
        uint32_t    sqDepth;        // Submission queue depth
        uint32_t    sectors;        // Total sectors on drive
        bool        isKVCache;      // Optimized for random access?
    };
    
    DriveConfig drives[5];
    
    // Auto-detect from Windows device enumeration
    static DriveTopology DetectWindows();
    
    // Load from calibration file
    static DriveTopology LoadFromFile(const std::string& path);
    
    // Save calibration
    void SaveToFile(const std::string& path) const;
};

// ─────────────────────────────────────────────────────────────────────────────
// JITMAP GENERATOR (Offline Tool)
// ─────────────────────────────────────────────────────────────────────────────

class JitMapGenerator {
public:
    JitMapGenerator(const DriveTopology& topology) : m_topology(topology) {}
    
    // Scan GGUF and generate optimal LBA mapping
    // Considers: tensor size, access pattern, drive speeds
    bool GenerateMap(const std::string& ggufPath, const std::string& outputPath);
    
    // Get filesystem LBA for a file region (requires admin/raw disk access)
    static bool GetFileLBA(const std::string& filePath, uint64_t fileOffset,
                           uint64_t length, uint32_t& outDriveIndex,
                           uint64_t& outStartLBA, uint32_t& outSectorCount);
    
private:
    DriveTopology m_topology;
    
    // Assign tensor to optimal drive based on access pattern
    uint8_t AssignDrive(const std::string& tensorName, uint64_t tensorSize);
};
