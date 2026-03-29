#include "MemoryPressureGuard.h"
#include <cstdlib>
#include <sstream>
#include <windows.h>


using namespace RawrXD::Inference;

MemoryPressureGuard::SystemMemory MemoryPressureGuard::query_system()
{
    SystemMemory mem{};
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex))
    {
        mem.total_ram = statex.ullTotalPhys;
        mem.available_ram = statex.ullAvailPhys;
    }

    // VRAM query would typically use DXGI or Vulkan/CUDA APIs
    // Simplified for this stub
    mem.total_vram = 0;
    mem.available_vram = 0;

    return mem;
}

MemoryPressureGuard::Verdict MemoryPressureGuard::check_load(const LoadRequest& req, std::string& out_message)
{
    if (const char* a = std::getenv("RAWRXD_BYPASS_RAM_GATES"))
    {
        if (a[0] == '1' || a[0] == 'y' || a[0] == 'Y')
            return Verdict::Allow;
    }
    if (const char* b = std::getenv("RAWRXD_SKIP_RAM_PREFLIGHT"))
    {
        if (b[0] == '1' || b[0] == 'y' || b[0] == 'Y')
            return Verdict::Allow;
    }

    auto sys = query_system();
    uint64_t padded_req = static_cast<uint64_t>(req.required_bytes * req.safety_margin);

    if (req.requires_gpu && sys.total_vram > 0)
    {
        if (padded_req > sys.available_vram)
        {
            std::stringstream ss;
            ss << "Insufficient VRAM: Requested " << (padded_req / (1024 * 1024))
               << "MB (with margin), Available: " << (sys.available_vram / (1024 * 1024)) << "MB";
            out_message = ss.str();
            return Verdict::Block;
        }
    }
    else
    {
        if (padded_req > sys.available_ram)
        {
            std::stringstream ss;
            ss << "Insufficient System RAM: Requested " << (padded_req / (1024 * 1024))
               << "MB (with margin), Available: " << (sys.available_ram / (1024 * 1024)) << "MB";
            out_message = ss.str();
            return Verdict::Block;
        }
    }

    return Verdict::Allow;
}

bool MemoryPressureGuard::acquire_resources(uint64_t bytes, bool gpu)
{
    // Implementation tracking active loads to prevent over-commitment
    return true;
}

void MemoryPressureGuard::release_resources(uint64_t bytes, bool gpu)
{
    // Release tracked resources
}
