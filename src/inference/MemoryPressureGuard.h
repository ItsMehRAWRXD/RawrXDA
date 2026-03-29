#pragma once
#include <cstdint>
#include <string>

namespace RawrXD::Inference
{
class MemoryPressureGuard
{
  public:
    struct SystemMemory
    {
        uint64_t total_ram;
        uint64_t available_ram;
        uint64_t total_vram;  // 0 if unknown
        uint64_t available_vram;
    };

    struct LoadRequest
    {
        uint64_t required_bytes;
        bool requires_gpu;
        float safety_margin;  // 1.2 = 20% headroom
    };

    enum class Verdict
    {
        Allow,
        Warn,
        Block
    };

    static SystemMemory query_system();
    /// If RAWRXD_BYPASS_RAM_GATES or RAWRXD_SKIP_RAM_PREFLIGHT is 1/y/Y, returns Allow (skips available-RAM check).
    static Verdict check_load(const LoadRequest& req, std::string& out_message);

    // Pre-flight check before mmap
    static bool acquire_resources(uint64_t bytes, bool gpu);
    static void release_resources(uint64_t bytes, bool gpu);
};
}  // namespace RawrXD::Inference
