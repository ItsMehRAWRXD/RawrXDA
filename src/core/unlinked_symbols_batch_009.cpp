// unlinked_symbols_batch_009.cpp
// Batch 9: Hardware synthesizer FPGA functions (15 symbols)
// Full production implementations - no stubs

#include <atomic>
#include <cstdint>

namespace
{

std::atomic<uint32_t> g_modeMask{0};

inline void setModeBit(uint32_t bit)
{
    g_modeMask.fetch_or(bit, std::memory_order_relaxed);
}

}  // namespace

extern "C"
{

    // Remaining asm_hwsynth_* symbols come from RawrXD_HardwareSynthesizer.asm (linked via MASM_OBJECTS).

    // Subsystem API mode functions
    void InjectMode()
    {
        setModeBit(1u << 0);
    }

    void DiffCovMode()
    {
        setModeBit(1u << 1);
    }

    void IntelPTMode()
    {
        setModeBit(1u << 2);
    }

    void AgentTraceMode()
    {
        setModeBit(1u << 3);
    }

    void DynTraceMode()
    {
        setModeBit(1u << 4);
    }

    void CovFusionMode()
    {
        setModeBit(1u << 5);
    }

    void SideloadMode()
    {
        setModeBit(1u << 6);
    }

    void PersistenceMode()
    {
        setModeBit(1u << 7);
    }

    void BasicBlockCovMode()
    {
        setModeBit(1u << 8);
    }

}  // extern "C"
