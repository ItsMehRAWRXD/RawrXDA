#pragma once
#include <cstdint>
#include <string>
#include <atomic>
#include <mutex>

namespace RawrXD::Inference {
    class MemoryPressureGuard {
    public:
        struct SystemMemory {
            uint64_t total_ram;
            uint64_t available_ram;
            uint64_t total_vram;      // 0 if unknown
            uint64_t available_vram;
        };
        
        struct LoadRequest {
            uint64_t required_bytes;
            bool requires_gpu;
            float safety_margin;      // 1.2 = 20% headroom
        };
        
        enum class Verdict { Allow, Warn, Block };
        
        static SystemMemory query_system();
        static Verdict check_load(const LoadRequest& req, std::string& out_message);
        
        // Pre-flight check before mmap
        static bool acquire_resources(uint64_t bytes, bool gpu);
        static void release_resources(uint64_t bytes, bool gpu);

        // Committed resource tracking
        static uint64_t committedRAM();
        static uint64_t committedVRAM();

    private:
        static std::mutex            s_mu;
        static std::atomic<uint64_t> s_committedRAM;
        static std::atomic<uint64_t> s_committedVRAM;
    };
}
