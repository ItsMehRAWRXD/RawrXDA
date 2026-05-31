#include "system_scout.h"
#include <intrin.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <shlobj.h>
#include <powerbase.h>
#include <powrprof.h>

#pragma comment(lib, "PowrProf.lib")
#pragma comment(lib, "version.lib")

namespace RawrXD {
namespace HAL {

std::string SystemCapabilities::GetOptimalKernelSuffix() const {
    if (has_avx512f && has_avx512dq) return "avx512";
    if (has_avx2 && has_fma3) return "avx2";
    if (has_sse42) return "sse42";
    return "generic";
}

std::string SystemCapabilities::ToJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"os\":\"" << os_name << "\","
        << "\"cpu\":\"" << cpu_brand << "\","
        << "\"ram_mb\":" << (total_ram_bytes / 1024 / 1024) << ","
        << "\"kernel\":\"" << GetOptimalKernelSuffix() << "\""
        << "}";
    return oss.str();
}

SystemCapabilities SystemScout::Scout() {
    SystemCapabilities caps;
    
    // CPUID
    int cpuInfo[4];
    char brand[49] = {0};
    __cpuid(cpuInfo, 0x80000002); memcpy(brand,      cpuInfo, 16);
    __cpuid(cpuInfo, 0x80000003); memcpy(brand + 16, cpuInfo, 16);
    __cpuid(cpuInfo, 0x80000004); memcpy(brand + 32, cpuInfo, 16);
    caps.cpu_brand = brand;

    __cpuid(cpuInfo, 0);
    int nIds = cpuInfo[0];
    if (nIds >= 1) {
        __cpuid(cpuInfo, 1);
        caps.has_sse42 = (cpuInfo[2] & (1 << 20));
        caps.has_avx = (cpuInfo[2] & (1 << 28));
        caps.has_fma3 = (cpuInfo[2] & (1 << 12));
    }
    if (nIds >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        caps.has_avx2 = (cpuInfo[1] & (1 << 5));
        caps.has_avx512f = (cpuInfo[1] & (1 << 16));
        caps.has_avx512dq = (cpuInfo[1] & (1 << 17));
    }

    // Memory
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        caps.total_ram_bytes = statex.ullTotalPhys;
        caps.available_ram_bytes = statex.ullAvailPhys;
    }

    // OS
    OSVERSIONINFOEXW osvi = { sizeof(osvi) };
    // Note: Relying on ntdll for real version if needed, simplified here
    caps.os_name = "Windows (Generic)";

    return caps;
}

bool SystemScout::IsPortableMode() {
    std::string exe_dir = GetExecutableDir();
    return std::filesystem::exists(exe_dir + "\\.portable") || 
           std::filesystem::exists(exe_dir + "\\portable.ini");
}

std::string SystemScout::GetExecutableDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    return path.substr(0, path.find_last_of("\\/"));
}

std::string SystemScout::GetConfigPath() {
    if (IsPortableMode()) return GetExecutableDir() + "\\config";
    char path[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path);
    return std::string(path) + "\\RawrXD";
}

} // namespace HAL
} // namespace RawrXD
