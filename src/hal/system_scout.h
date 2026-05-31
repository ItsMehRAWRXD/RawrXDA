#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace RawrXD {
namespace HAL {

struct CpuCacheInfo {
    int level;
    int size_kb;
    int line_size;
    int associativity;
    bool fully_associative;
};

struct CpuTopology {
    int physical_cores;
    int logical_cores;
    int numa_nodes;
    int packages;
};

struct PowerProfile {
    std::string name;
    GUID guid;
    bool is_high_performance;
    int current_mhz;
};

struct SystemCapabilities {
    // OS
    std::string os_name;
    std::string os_version;
    bool is_windows_11;
    int build_number;
    
    // CPU Identification
    std::string cpu_vendor;
    std::string cpu_brand;
    
    // Features
    bool has_sse42 = false;
    bool has_avx = false;
    bool has_avx2 = false;
    bool has_avx512f = false;
    bool has_avx512dq = false;
    bool has_fma3 = false;
    bool has_popcnt = false;
    
    // Cache & Topology
    std::vector<CpuCacheInfo> caches;
    CpuTopology topology;
    
    // Memory
    uint64_t total_ram_bytes;
    uint64_t available_ram_bytes;
    
    // Power
    bool is_on_battery;
    int battery_percent;
    
    std::string ToJson() const;
    std::string GetOptimalKernelSuffix() const;
};

class SystemScout {
public:
    static SystemCapabilities Scout();
    static bool IsPortableMode();
    static std::string GetExecutableDir();
    static std::string GetConfigPath();
};

} // namespace HAL
} // namespace RawrXD
