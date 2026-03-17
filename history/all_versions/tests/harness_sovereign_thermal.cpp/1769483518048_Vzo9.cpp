// tests/harness_sovereign_thermal.cpp
// ════════════════════════════════════════════════════════════════════════════════
// SovereignControlBlock Integrated Thermal & VRAM Harness
// Combines Vulkan VRAM budget stress with NVMe thermal monitoring
// ════════════════════════════════════════════════════════════════════════════════
//
// Features:
//   - Vulkan VRAM budget sampling during allocations
//   - NVMe temperature polling across SovereignControlBlock drives
//   - Unbuffered I/O stress to simulate model weight paging
//   - Temperature-based drive rotation via Governor
//   - JSON telemetry output for analysis
//
// Build: Requires Vulkan SDK and nvme_thermal_stressor.obj
// ════════════════════════════════════════════════════════════════════════════════

#include <vulkan/vulkan.h>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Link against MASM thermal stressor
extern "C" {
    int32_t NVMe_GetTemperature(uint32_t driveId);
    int32_t NVMe_PollAllDrives(int32_t* outTemps, const uint32_t* driveIds, uint32_t count);
    int32_t NVMe_GetCoolestDrive(const uint32_t* driveIds, uint32_t count);
    void* NVMe_AllocAlignedBuffer(size_t sizeBytes);
    void NVMe_FreeAlignedBuffer(void* ptr);
    uint64_t NVMe_StressRead(uint32_t driveId, void* buffer, uint64_t size,
                             uint32_t offsetLow, uint32_t offsetHigh);
    int32_t NVMe_StressBurst(uint32_t driveId, void* buffer, uint64_t totalBytes, uint32_t isWrite);
    uint32_t NVMe_GetLastError();
}

// ════════════════════════════════════════════════════════════════════════════════
// Configuration
// ════════════════════════════════════════════════════════════════════════════════

struct HarnessConfig {
    // VRAM stress
    std::string vramMode = "near-budget";   // load|near-budget|oversubscribe
    double vramTargetFrac = 0.85;           // Target fraction of budget
    size_t vramChunkMB = 256;               // Allocation chunk size
    
    // NVMe stress
    std::vector<uint32_t> driveIds = {0, 1, 2, 4, 5};
    size_t nvmeChunkMB = 64;                // I/O chunk size
    size_t nvmeTotalGB = 4;                 // Total I/O per drive
    
    // Sampling
    uint32_t sampleIntervalMs = 500;
    bool enableNvmeStress = true;
    bool enableVramStress = true;
    bool jsonOutput = true;
    bool allowIoctlFallback = false;       // If sidecar missing, allow direct IOCTL polling
    
    // Governor
    int32_t maxTempC = 70;                  // Thermal throttle threshold
    double minHeadroomFrac = 0.05;          // Pause VRAM allocs below this headroom
    double resumeHeadroomFrac = 0.15;       // Resume VRAM allocs when back above
    bool pauseNvmeOnThermal = true;         // Pause NVMe bursts when over maxTempC
    int32_t maxWearPct = 95;                // Wear throttle threshold (0-100)
    double wearWeight = 0.3;                // Wear influence in drive scoring
    double tempWeight = 0.7;                // Temperature influence in drive scoring
};

// ════════════════════════════════════════════════════════════════════════════════
// Telemetry Structures
// ════════════════════════════════════════════════════════════════════════════════

struct VRAMSample {
    uint64_t totalBytes = 0;
    uint64_t budgetBytes = 0;
    uint64_t usageBytes = 0;
    uint64_t availableBytes = 0;
    bool budgetSupported = false;
};

struct ThermalSample {
    uint32_t driveId;
    int32_t tempC;
    int32_t wearPct;
};

struct TelemetryEvent {
    std::string event;
    uint64_t timestampMs;
    VRAMSample vram;
    std::vector<ThermalSample> thermals;
    std::string note;
};

// ════════════════════════════════════════════════════════════════════════════════
// NVMe Sidecar Mapping Reader (Global\SOVEREIGN_NVME_TEMPS)
// ════════════════════════════════════════════════════════════════════════════════

constexpr uint32_t kSidecarSignature = 0x45564F53; // "SOVE"
constexpr uint32_t kSidecarVersion = 1;
constexpr uint32_t kSidecarMaxDrives = 16;
constexpr size_t kSidecarMapSize = 0x98; // 0x90 + 8

#pragma pack(push, 1)
struct SidecarLayout {
    uint32_t signature;
    uint32_t version;
    uint32_t count;
    uint32_t reserved;
    int32_t temps[kSidecarMaxDrives];
    int32_t wear[kSidecarMaxDrives];
    uint64_t timestampMs;
};
#pragma pack(pop)

static_assert(offsetof(SidecarLayout, temps) == 0x10, "SidecarLayout temps offset mismatch");
static_assert(offsetof(SidecarLayout, wear) == 0x50, "SidecarLayout wear offset mismatch");
static_assert(offsetof(SidecarLayout, timestampMs) == 0x90, "SidecarLayout timestamp offset mismatch");

class NvmeSidecarReader {
public:
    ~NvmeSidecarReader();
    bool read(std::vector<int32_t>& temps, std::vector<int32_t>& wear);
    std::string status() const { return status_; }

private:
    bool ensureOpen();
    void close();

    HANDLE mapping_ = nullptr;
    SidecarLayout* view_ = nullptr;
    std::string status_ = "sidecar_missing";
};

NvmeSidecarReader::~NvmeSidecarReader() {
    close();
}

void NvmeSidecarReader::close() {
    if (view_) {
        UnmapViewOfFile(view_);
        view_ = nullptr;
    }
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
}

bool NvmeSidecarReader::ensureOpen() {
    if (view_) {
        return true;
    }

    mapping_ = OpenFileMappingA(FILE_MAP_READ, FALSE, "Global\\SOVEREIGN_NVME_TEMPS");
    if (!mapping_) {
        status_ = "sidecar_missing";
        return false;
    }

    view_ = static_cast<SidecarLayout*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, kSidecarMapSize));
    if (!view_) {
        status_ = "sidecar_unmapped";
        close();
        return false;
    }

    if (view_->signature != kSidecarSignature || view_->version != kSidecarVersion) {
        status_ = "sidecar_invalid";
        close();
        return false;
    }

    status_ = "sidecar_active";
    return true;
}

bool NvmeSidecarReader::read(std::vector<int32_t>& temps, std::vector<int32_t>& wear) {
    if (!ensureOpen()) {
        return false;
    }

    uint32_t count = view_->count;
    if (count > kSidecarMaxDrives) {
        count = kSidecarMaxDrives;
    }
    if (count > temps.size()) {
        count = static_cast<uint32_t>(temps.size());
    }
    if (count > wear.size()) {
        count = static_cast<uint32_t>(wear.size());
    }

    for (uint32_t i = 0; i < count; ++i) {
        temps[i] = view_->temps[i];
        wear[i] = view_->wear[i];
    }

    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
// VRAM Governor (budget + thermal policy)
// ════════════════════════════════════════════════════════════════════════════════

class VRAMGovernor {
public:
    explicit VRAMGovernor(const HarnessConfig& cfg) : cfg_(cfg) {}
    struct DriveScore {
        uint32_t driveId;
        int32_t tempC;
        int32_t wearPct;
        double score;
        bool blacklisted;
    };
    void update(const VRAMSample& vram, const std::vector<ThermalSample>& thermals);
    bool allowVramAlloc() const { return !vramPaused_; }
    bool allowNvme() const { return !nvmePaused_; }
    std::vector<DriveScore> scoreDrives(const std::vector<ThermalSample>& thermals) const;
    std::vector<uint32_t> rankDrives(const std::vector<DriveScore>& scores, bool applyBlacklist) const;
    std::string formatRanking(const std::vector<DriveScore>& scores, uint64_t cycle) const;
    std::string statusNote() const { return statusNote_; }

private:
    HarnessConfig cfg_;
    bool vramPaused_ = false;
    bool nvmePaused_ = false;
    std::string statusNote_;
};

void VRAMGovernor::update(const VRAMSample& vram, const std::vector<ThermalSample>& thermals) {
    double headroomFrac = 1.0;
    if (vram.budgetBytes > 0) {
        headroomFrac = static_cast<double>(vram.availableBytes) / static_cast<double>(vram.budgetBytes);
    }

    // VRAM budget hysteresis
    if (!vramPaused_ && headroomFrac < cfg_.minHeadroomFrac) {
        vramPaused_ = true;
    } else if (vramPaused_ && headroomFrac > cfg_.resumeHeadroomFrac) {
        vramPaused_ = false;
    }

    // Thermal + wear gating for NVMe
    int32_t hottest = -1000;
    int32_t highestWear = -1;
    for (auto t : thermals) {
        if (t.tempC > hottest) hottest = t.tempC;
        if (t.wearPct >= 0 && t.wearPct > highestWear) highestWear = t.wearPct;
    }
    if (cfg_.pauseNvmeOnThermal) {
        if (!nvmePaused_ && hottest >= cfg_.maxTempC) {
            nvmePaused_ = true;
        } else if (nvmePaused_ && hottest < cfg_.maxTempC - 5) {
            nvmePaused_ = false;
        }
    }
    if (!nvmePaused_ && highestWear >= cfg_.maxWearPct) {
        nvmePaused_ = true;
    } else if (nvmePaused_ && highestWear >= 0 && highestWear < cfg_.maxWearPct - 5) {
        nvmePaused_ = false;
    }

    std::ostringstream oss;
    if (vramPaused_) {
        oss << "vram_paused headroom=" << headroomFrac;
    }
    if (nvmePaused_) {
        if (!oss.str().empty()) oss << " | ";
        oss << "nvme_paused hottest=" << hottest;
        if (highestWear >= 0) {
            oss << " wear=" << highestWear;
        }
    }
    statusNote_ = oss.str();
}

std::vector<VRAMGovernor::DriveScore> VRAMGovernor::scoreDrives(const std::vector<ThermalSample>& thermals) const {
    std::vector<DriveScore> scores;
    scores.reserve(thermals.size());
    for (const auto& t : thermals) {
        double temp = (t.tempC >= 0) ? static_cast<double>(t.tempC) : 1000.0;
        double wear = (t.wearPct >= 0) ? static_cast<double>(t.wearPct) : 1000.0;
        double score = temp * cfg_.tempWeight + wear * cfg_.wearWeight;
        bool blacklisted = (t.wearPct >= 0 && t.wearPct >= cfg_.maxWearPct);
        scores.push_back({t.driveId, t.tempC, t.wearPct, score, blacklisted});
    }

    std::sort(scores.begin(), scores.end(), [](const DriveScore& a, const DriveScore& b) {
        return a.score < b.score;
    });

    return scores;
}

std::vector<uint32_t> VRAMGovernor::rankDrives(const std::vector<DriveScore>& scores, bool applyBlacklist) const {
    std::vector<uint32_t> ordered;
    ordered.reserve(scores.size());
    for (const auto& s : scores) {
        if (applyBlacklist && s.blacklisted) {
            continue;
        }
        ordered.push_back(s.driveId);
    }
    return ordered;
}

std::string VRAMGovernor::formatRanking(const std::vector<DriveScore>& scores, uint64_t cycle) const {
    std::ostringstream log;
    log << "[Governor] Cycle #" << cycle << " | Drive Ranking:";
    for (size_t i = 0; i < scores.size(); ++i) {
        log << " #" << i << ": Device" << scores[i].driveId
            << " (Temp:" << scores[i].tempC << "C Wear:" << scores[i].wearPct
            << "% Score:" << std::fixed << std::setprecision(1) << scores[i].score;
        if (scores[i].blacklisted) {
            log << " HIGH_WEAR";
        }
        log << ")";
    }
    return log.str();
}

// ════════════════════════════════════════════════════════════════════════════════
// JSON Output
// ════════════════════════════════════════════════════════════════════════════════

static uint64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void emitJson(const TelemetryEvent& e) {
    std::cout << "{\"event\":\"" << e.event << "\","
              << "\"ts_ms\":" << e.timestampMs << ","
              << "\"vram\":{"
              << "\"budget_supported\":" << (e.vram.budgetSupported ? "true" : "false") << ","
              << "\"total\":" << e.vram.totalBytes << ","
              << "\"budget\":" << e.vram.budgetBytes << ","
              << "\"usage\":" << e.vram.usageBytes << ","
              << "\"available\":" << e.vram.availableBytes
              << "},\"thermals\":[";
    
    for (size_t i = 0; i < e.thermals.size(); ++i) {
        if (i > 0) std::cout << ",";
        std::cout << "{\"drive\":" << e.thermals[i].driveId 
                  << ",\"temp_c\":" << e.thermals[i].tempC
                  << ",\"wear_pct\":" << e.thermals[i].wearPct << "}";
    }
    
    std::cout << "]";
    if (!e.note.empty()) {
        std::cout << ",\"note\":\"" << e.note << "\"";
    }
    std::cout << "}" << std::endl;
}

static void emitText(const TelemetryEvent& e) {
    std::cout << "[" << e.event << "] ts=" << e.timestampMs << "ms"
              << " VRAM: total=" << (e.vram.totalBytes / 1e9) << "GB"
              << " budget=" << (e.vram.budgetBytes / 1e9) << "GB"
              << " avail=" << (e.vram.availableBytes / 1e9) << "GB"
              << " | Thermals:";
    for (const auto& t : e.thermals) {
        std::cout << " D" << t.driveId << "=" << t.tempC << "C";
        if (t.wearPct >= 0) {
            std::cout << " wear=" << t.wearPct << "%";
        }
    }
    if (!e.note.empty()) {
        std::cout << " [" << e.note << "]";
    }
    std::cout << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
// Vulkan VRAM Manager
// ════════════════════════════════════════════════════════════════════════════════

class VulkanVRAMManager {
public:
    bool init(bool disableBudget = false);
    void cleanup();
    VRAMSample sample() const;
    bool allocateChunk(size_t bytes);
    size_t trackedAllocation() const { return trackedBytes_; }
    
private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    bool budgetSupported_ = false;
    VkPhysicalDeviceMemoryProperties memProps_{};
    std::vector<const char*> enabledExtensions_;
    std::vector<std::pair<VkBuffer, VkDeviceMemory>> allocations_;
    size_t trackedBytes_ = 0;
    
    std::optional<uint32_t> findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags) const;
};

bool VulkanVRAMManager::init(bool disableBudget) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "SovereignHarness";
    app.apiVersion = VK_API_VERSION_1_2;
    
    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    
    if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance" << std::endl;
        return false;
    }
    
    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(instance_, &devCount, nullptr);
    if (devCount == 0) {
        std::cerr << "No Vulkan devices found" << std::endl;
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(instance_, &devCount, devices.data());
    
    // Prefer discrete GPU
    for (auto d : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice_ = d;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        physicalDevice_ = devices[0];
    }
    
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps_);
    
    // Check for memory budget extension
    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice_, nullptr, &extCount, exts.data());
    
    for (const auto& ext : exts) {
        if (std::strcmp(ext.extensionName, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME) == 0) {
            budgetSupported_ = true;
        }
    }
    
    if (disableBudget) {
        budgetSupported_ = false;
    }
    
    if (budgetSupported_) {
        enabledExtensions_.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }
    
    // Find compute queue
    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> queues(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &qCount, queues.data());
    
    for (uint32_t i = 0; i < qCount; ++i) {
        if (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queueFamily_ = i;
            break;
        }
    }
    
    float priority = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = queueFamily_;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions_.size());
    dci.ppEnabledExtensionNames = enabledExtensions_.empty() ? nullptr : enabledExtensions_.data();
    
    if (vkCreateDevice(physicalDevice_, &dci, nullptr, &device_) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device" << std::endl;
        return false;
    }
    
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
    return true;
}

void VulkanVRAMManager::cleanup() {
    for (auto& p : allocations_) {
        if (p.second != VK_NULL_HANDLE) vkFreeMemory(device_, p.second, nullptr);
        if (p.first != VK_NULL_HANDLE) vkDestroyBuffer(device_, p.first, nullptr);
    }
    allocations_.clear();
    trackedBytes_ = 0;
    
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

std::optional<uint32_t> VulkanVRAMManager::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags flags) const {
    for (uint32_t i = 0; i < memProps_.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps_.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    return std::nullopt;
}

VRAMSample VulkanVRAMManager::sample() const {
    VRAMSample s{};
    s.budgetSupported = budgetSupported_;
    
    // Total device-local heap
    for (uint32_t i = 0; i < memProps_.memoryHeapCount; ++i) {
        if (memProps_.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            s.totalBytes += memProps_.memoryHeaps[i].size;
        }
    }
    
    if (budgetSupported_) {
        VkPhysicalDeviceMemoryBudgetPropertiesEXT budget{};
        budget.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT;
        
        VkPhysicalDeviceMemoryProperties2 props2{};
        props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        props2.pNext = &budget;
        
        auto fp = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(
            vkGetInstanceProcAddr(instance_, "vkGetPhysicalDeviceMemoryProperties2"));
        
        if (fp) {
            fp(physicalDevice_, &props2);
            for (uint32_t i = 0; i < props2.memoryProperties.memoryHeapCount; ++i) {
                if (props2.memoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
                    s.budgetBytes += budget.heapBudget[i];
                    s.usageBytes += budget.heapUsage[i];
                }
            }
        }
    }
    
    if (!budgetSupported_ || s.budgetBytes == 0) {
        s.budgetBytes = s.totalBytes;
        s.usageBytes = trackedBytes_;
    }
    
    s.availableBytes = (s.budgetBytes > s.usageBytes) ? (s.budgetBytes - s.usageBytes) : 0;
    return s;
}

bool VulkanVRAMManager::allocateChunk(size_t bytes) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bytes;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer buf = VK_NULL_HANDLE;
    if (vkCreateBuffer(device_, &bci, nullptr, &buf) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device_, buf, &req);
    
    auto memType = findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!memType) {
        vkDestroyBuffer(device_, buf, nullptr);
        return false;
    }
    
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = *memType;
    
    VkDeviceMemory mem = VK_NULL_HANDLE;
    if (vkAllocateMemory(device_, &mai, nullptr, &mem) != VK_SUCCESS) {
        vkDestroyBuffer(device_, buf, nullptr);
        return false;
    }
    
    vkBindBufferMemory(device_, buf, mem, 0);
    allocations_.push_back({buf, mem});
    trackedBytes_ += req.size;
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
// NVMe Thermal Monitor (wraps MASM)
// ════════════════════════════════════════════════════════════════════════════════

class NVMeThermalMonitor {
public:
    NVMeThermalMonitor(const std::vector<uint32_t>& drives, bool allowIoctlFallback)
        : drives_(drives), allowIoctlFallback_(allowIoctlFallback) {}
    std::string sidecarStatus() const { return sidecar_.status(); }
    
    std::vector<ThermalSample> poll() {
        std::vector<ThermalSample> samples;
        std::vector<int32_t> temps(drives_.size(), -1);
        std::vector<int32_t> wear(drives_.size(), -1);

        if (sidecar_.read(temps, wear)) {
            for (size_t i = 0; i < drives_.size(); ++i) {
                samples.push_back({drives_[i], temps[i], wear[i]});
            }
            return samples;
        }
        if (allowIoctlFallback_) {
            NVMe_PollAllDrives(temps.data(), drives_.data(), static_cast<uint32_t>(drives_.size()));
        }
        for (size_t i = 0; i < drives_.size(); ++i) {
            samples.push_back({drives_[i], temps[i], -1});
        }
        return samples;
    }
    
    uint32_t getCoolestDrive() const {
        if (drives_.empty()) return static_cast<uint32_t>(-1);
        return NVMe_GetCoolestDrive(drives_.data(), static_cast<uint32_t>(drives_.size()));
    }
    
private:
    std::vector<uint32_t> drives_;
    bool allowIoctlFallback_ = false;
    NvmeSidecarReader sidecar_;
};

// ════════════════════════════════════════════════════════════════════════════════
// Integrated Harness
// ════════════════════════════════════════════════════════════════════════════════

class SovereignHarness {
public:
    explicit SovereignHarness(const HarnessConfig& cfg) 
        : cfg_(cfg), thermal_(cfg.driveIds, cfg.allowIoctlFallback), governor_(cfg) {}
    
    int run();
    
private:
    HarnessConfig cfg_;
    VulkanVRAMManager vram_;
    NVMeThermalMonitor thermal_;
    VRAMGovernor governor_;
    uint64_t governorCycle_ = 0;
    
    void emit(const std::string& event, const std::string& note = "");
    void stressNVMe();
};

void SovereignHarness::emit(const std::string& event, const std::string& note) {
    TelemetryEvent e;
    e.event = event;
    e.timestampMs = nowMs();
    if (cfg_.enableVramStress) {
        e.vram = vram_.sample();
    }
    e.thermals = thermal_.poll();
    governor_.update(e.vram, e.thermals);
    std::string combined;
    std::string sidecarNote = thermal_.sidecarStatus();
    if (!note.empty()) {
        combined = note;
    }
    if (!sidecarNote.empty()) {
        if (!combined.empty()) combined += " | ";
        combined += sidecarNote;
    }
    if (!governor_.statusNote().empty()) {
        if (!combined.empty()) combined += " | ";
        combined += governor_.statusNote();
    }
    e.note = combined;
    
    if (cfg_.jsonOutput) {
        emitJson(e);
    } else {
        emitText(e);
    }
}

void SovereignHarness::stressNVMe() {
    // Allocate aligned buffer for I/O
    size_t chunkBytes = cfg_.nvmeChunkMB * 1024 * 1024;
    void* buffer = NVMe_AllocAlignedBuffer(chunkBytes);
    if (!buffer) {
        std::cerr << "Failed to allocate aligned buffer for NVMe I/O" << std::endl;
        return;
    }
    
    // Fill buffer with test pattern
    std::memset(buffer, 0xAB, chunkBytes);
    
    size_t totalBytesPerDrive = cfg_.nvmeTotalGB * 1024ull * 1024ull * 1024ull;
    
    auto thermals = thermal_.poll();
    auto scores = governor_.scoreDrives(thermals);
    auto driveOrder = governor_.rankDrives(scores, true);
    emit("governor_rank", governor_.formatRanking(scores, ++governorCycle_));
    if (driveOrder.empty()) {
        driveOrder = cfg_.driveIds;
    }

    for (uint32_t driveId : driveOrder) {
        if (!governor_.allowNvme()) {
            emit("nvme_skipped", "nvme_paused_by_governor");
            break;
        }
        std::ostringstream note;
        note << "nvme_stress_start drive=" << driveId;
        emit("nvme_stress", note.str());
        
        // Perform burst read
        int32_t finalTemp = NVMe_StressBurst(driveId, buffer, totalBytesPerDrive, 0);
        
        note.str("");
        note << "nvme_stress_end drive=" << driveId << " final_temp=" << finalTemp << "C";
        emit("nvme_stress", note.str());
        
        // Brief cooldown between drives
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    NVMe_FreeAlignedBuffer(buffer);
}

int SovereignHarness::run() {
    // Initialize Vulkan (only if VRAM stress enabled)
    if (cfg_.enableVramStress) {
        if (!vram_.init(false)) {
            std::cerr << "Failed to initialize Vulkan" << std::endl;
            return 1;
        }
    }

    emit("init", cfg_.enableVramStress ? "vulkan_ready" : "vulkan_disabled");
    
    // VRAM stress phase
    if (cfg_.enableVramStress) {
        size_t chunkBytes = cfg_.vramChunkMB * 1024 * 1024;
        bool oversubscribe = (cfg_.vramMode == "oversubscribe");
        bool nearBudget = (cfg_.vramMode == "near-budget");
        
        bool done = false;
        while (!done) {
            auto sample = vram_.sample();
            auto thermals = thermal_.poll();
            governor_.update(sample, thermals);
            
            bool shouldAlloc = governor_.allowVramAlloc();
            if (oversubscribe) {
                shouldAlloc = shouldAlloc && true;
            } else if (nearBudget) {
                double frac = sample.budgetBytes > 0 
                    ? static_cast<double>(sample.usageBytes) / sample.budgetBytes 
                    : 0.0;
                shouldAlloc = shouldAlloc && (frac < cfg_.vramTargetFrac);
            } else { // load mode
                shouldAlloc = shouldAlloc && (sample.availableBytes > chunkBytes);
            }
            
            if (shouldAlloc) {
                bool ok = vram_.allocateChunk(chunkBytes);
                if (!ok) {
                    emit("alloc_fail", "vram_allocation_failed");
                    done = true;
                } else {
                    std::ostringstream note;
                    note << "allocated_mb=" << (chunkBytes / (1024*1024));
                    emit("alloc", note.str());
                }
            } else {
                if (!governor_.statusNote().empty()) {
                    emit("alloc_paused", governor_.statusNote());
                }
                done = true;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    // NVMe stress phase
    if (cfg_.enableNvmeStress) {
        emit("nvme_phase_start");
        stressNVMe();
        emit("nvme_phase_end");
    }
    
    // Find coolest drive
    uint32_t coolest = thermal_.getCoolestDrive();
    std::ostringstream note;
    note << "coolest_drive=" << coolest;
    emit("final", note.str());
    
    vram_.cleanup();
    return 0;
}

// ════════════════════════════════════════════════════════════════════════════════
// CLI Argument Parsing
// ════════════════════════════════════════════════════════════════════════════════

static HarnessConfig parseArgs(int argc, char** argv) {
    HarnessConfig cfg;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--mode" && i + 1 < argc) {
            cfg.vramMode = argv[++i];
        } else if (arg == "--vram-target" && i + 1 < argc) {
            cfg.vramTargetFrac = std::stod(argv[++i]);
        } else if (arg == "--vram-chunk-mb" && i + 1 < argc) {
            cfg.vramChunkMB = std::stoull(argv[++i]);
        } else if (arg == "--nvme-chunk-mb" && i + 1 < argc) {
            cfg.nvmeChunkMB = std::stoull(argv[++i]);
        } else if (arg == "--nvme-total-gb" && i + 1 < argc) {
            cfg.nvmeTotalGB = std::stoull(argv[++i]);
        } else if (arg == "--sample-ms" && i + 1 < argc) {
            cfg.sampleIntervalMs = std::stoul(argv[++i]);
        } else if (arg == "--max-temp" && i + 1 < argc) {
            cfg.maxTempC = std::stoi(argv[++i]);
        } else if (arg == "--drives" && i + 1 < argc) {
            cfg.driveIds.clear();
            std::string drives = argv[++i];
            std::istringstream ss(drives);
            std::string id;
            while (std::getline(ss, id, ',')) {
                cfg.driveIds.push_back(std::stoul(id));
            }
        } else if (arg == "--no-vram") {
            cfg.enableVramStress = false;
        } else if (arg == "--no-nvme") {
            cfg.enableNvmeStress = false;
        } else if (arg == "--text") {
            cfg.jsonOutput = false;
        } else if (arg == "--help") {
            std::cout << "SovereignControlBlock Thermal & VRAM Harness\n"
                      << "Options:\n"
                      << "  --mode <load|near-budget|oversubscribe>  VRAM stress mode\n"
                      << "  --vram-target <0.0-1.0>                  Target VRAM fraction\n"
                      << "  --vram-chunk-mb <N>                      VRAM allocation chunk size\n"
                      << "  --nvme-chunk-mb <N>                      NVMe I/O chunk size\n"
                      << "  --nvme-total-gb <N>                      Total I/O per drive\n"
                      << "  --sample-ms <N>                          Telemetry interval\n"
                      << "  --max-temp <N>                           Thermal throttle temp (C)\n"
                      << "  --drives <0,1,2,4,5>                     Drive IDs to test\n"
                      << "  --no-vram                                Skip VRAM stress\n"
                      << "  --no-nvme                                Skip NVMe stress\n"
                      << "  --text                                   Plain text output\n";
            std::exit(0);
        }
    }
    
    return cfg;
}

// ════════════════════════════════════════════════════════════════════════════════
// Main
// ════════════════════════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    HarnessConfig cfg = parseArgs(argc, argv);
    SovereignHarness harness(cfg);
    return harness.run();
}
