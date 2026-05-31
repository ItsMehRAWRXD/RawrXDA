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
    // 6-Drive Grid Configuration for 7800X3D + 7800XT System (VALIDATED)
    // BAR0 (VRAM): 0xFC00000000 (16GB Large BAR)
    // BAR2 (MMIO): 0xFCA00000
    // SDMA0: BAR2 + 0x4E00 = 0xFCA04E00
    // SDMA1: BAR2 + 0x5E00 = 0xFCA05E00
    //
    // FAST TIER (NVMe + External SSD):
    // Drive 0: CT1000P3PSSD8 (1TB NVMe)    — OS + KV Cache (random access)
    // Drive 1: CT1000P3PSSD8 (1TB NVMe)    — Hot tensors (attention heads)
    // Drive 2: CT1000P3PSSD8 (1TB NVMe)    — Hot tensors (FFN/MLP)
    // Drive 4: Micron X10 Pro (4TB SSD)    — Model Shard A (Q/K tensors)
    // Drive 5: Micron X10 Pro (4TB SSD)    — Model Shard B (V/Output)
    //
    // COLD TIER (USB HDD):
    // Drive 3: WD External (2TB HDD)       — Cold storage, checkpoints, overflow
    
    struct DriveConfig {
        std::string devicePath;     // \\.\PhysicalDriveN
        uint64_t    bar0Address;    // PCIe BAR0 (for VRAM mapping)
        uint32_t    sqDepth;        // Submission queue depth (NVMe only)
        uint64_t    sizeBytes;      // Total drive capacity
        uint8_t     tier;           // 0=NVMe, 1=External SSD, 2=USB HDD
        bool        isKVCache;      // Optimized for random access?
        bool        useBufferedIO;  // True for USB/slow drives
    };
    
    DriveConfig drives[6];
    uint8_t     driveCount = 6;     // VALIDATED: 3 NVMe + 2 External SSD + 1 USB HDD
    
    // Validated addresses from WMI probe (2026-01-26)
    static constexpr uint64_t VALIDATED_BAR0 = 0xFC00000000ULL;
    static constexpr uint64_t VALIDATED_BAR2 = 0xFCA00000ULL;
    static constexpr uint64_t VALIDATED_SDMA0 = 0xFCA04E00ULL;
    static constexpr uint64_t VALIDATED_SDMA1 = 0xFCA05E00ULL;
    
    // Tier thresholds for routing decisions
    static constexpr uint32_t NVME_LATENCY_THRESHOLD_US = 200;      // Route away if > 200μs
    static constexpr uint32_t EXT_SSD_LATENCY_THRESHOLD_US = 500;   // Route away if > 500μs
    static constexpr uint32_t USB_HDD_LATENCY_THRESHOLD_US = 5000;  // Accept up to 5ms for cold
    
    // Initialize with detected topology
    static DriveTopology CreateValidated() {
        DriveTopology topo;
        topo.driveCount = 6;
        
        // NVMe tier (drives 0-2)
        for (int i = 0; i < 3; i++) {
            topo.drives[i].devicePath = "\\\\.\\PhysicalDrive" + std::to_string(i);
            topo.drives[i].sizeBytes = 1000202273280ULL;
            topo.drives[i].tier = 0;
            topo.drives[i].sqDepth = 1024;
            topo.drives[i].useBufferedIO = false;
        }
        topo.drives[0].isKVCache = true;
        
        // USB HDD (drive 3) - cold tier
        topo.drives[3].devicePath = "\\\\.\\PhysicalDrive3";
        topo.drives[3].sizeBytes = 2000363420160ULL;
        topo.drives[3].tier = 2;
        topo.drives[3].sqDepth = 32;
        topo.drives[3].useBufferedIO = true;  // Buffered for USB
        topo.drives[3].isKVCache = false;
        
        // External SSD tier (drives 4-5)
        for (int i = 4; i <= 5; i++) {
            topo.drives[i].devicePath = "\\\\.\\PhysicalDrive" + std::to_string(i);
            topo.drives[i].sizeBytes = 4000784417280ULL;
            topo.drives[i].tier = 1;
            topo.drives[i].sqDepth = 256;
            topo.drives[i].useBufferedIO = false;  // Direct IO for SSDs
            topo.drives[i].isKVCache = false;
        }
        
        return topo;
    }
    
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
