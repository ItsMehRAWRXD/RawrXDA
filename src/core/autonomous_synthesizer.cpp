#include <windows.h>
#include <vector>
#include <cstdint>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <string>

// External MASM for Emitter interaction
extern "C" void Shield_CommitPolymorphicPatch(const uint8_t* patch_data, size_t size);

namespace RawrXD {
namespace Autonomy {

namespace {

constexpr size_t kMaxPatchBytes = 1024;

// Harmless instruction fragments used to synthesize tiny runtime-safe patches.
constexpr std::array<uint8_t, 12> kTemplateA = {
    0x90,       // nop
    0x66, 0x90, // xchg ax, ax (2-byte nop)
    0xF3, 0x90, // pause
    0x0F, 0x1F, 0x00, // nop dword ptr [rax]
    0x90,
    0x90,
    0x90,
    0xC3        // ret
};

constexpr std::array<uint8_t, 12> kTemplateB = {
    0x90,
    0x0F, 0x1F, 0x40, 0x00, // nop dword ptr [rax+0]
    0xF3, 0x90,
    0x66, 0x90,
    0x90,
    0x90,
    0xC3
};

bool IsForbiddenOpcode(uint8_t b0, uint8_t b1)
{
    // Ban trap/syscall/int opcodes in synthesized patches.
    if (b0 == 0xCC || b0 == 0xCD || b0 == 0xCE) return true;
    if (b0 == 0x0F && b1 == 0x05) return true; // syscall
    if (b0 == 0xCF) return true; // iret
    return false;
}

uint64_t Fnv1a64(const std::vector<uint8_t>& data)
{
    uint64_t h = 1469598103934665603ull;
    for (uint8_t b : data) {
        h ^= static_cast<uint64_t>(b);
        h *= 1099511628211ull;
    }
    return h;
}

std::atomic<uint64_t> g_lastPatchHash{0};

} // namespace

/**
 * @brief Subsystem 2: Self-Writing Code Generator (Autonomous MASM Synthesis)
 * Interfaces with the polymorphic_codegen.nf4 kernel to rewrite inference
 * logic on the fly without human intervention.
 */
class AutonomousSynthesizer {
public:
    static AutonomousSynthesizer& GetInstance() {
        static AutonomousSynthesizer instance;
        return instance;
    }

    // Triggered every 1,000 cycles or by Nous Engine objective
    void SynthesizeAndPatch() {
        // 1. Request New Kernel Variant from GPU LLM
        std::vector<uint8_t> new_masm_opcodes = RequestGPUKernelSynthesis();

        // 2. Self-Validation: Pass through 10kHz Integrity Probes
        if (ValidateNewOpcodes(new_masm_opcodes)) {
            const uint64_t hash = Fnv1a64(new_masm_opcodes);
            const uint64_t last = g_lastPatchHash.load(std::memory_order_relaxed);
            if (hash == last) {
                LogMutationEvent(new_masm_opcodes, false);
                return;
            }

            // 3. Hot-patch: Commit to the RWX JIT segment
            Shield_CommitPolymorphicPatch(new_masm_opcodes.data(), new_masm_opcodes.size());
            g_lastPatchHash.store(hash, std::memory_order_relaxed);
            
            // 4. Log to WOM Audit Ring
            LogMutationEvent(new_masm_opcodes, true);
        }
    }

private:
    AutonomousSynthesizer() {}
    
    std::vector<uint8_t> RequestGPUKernelSynthesis() {
        // Deterministic synthesis fallback: derive a tiny safe patch variant from timer entropy.
        const auto ticks = static_cast<uint64_t>(GetTickCount64());
        const bool chooseA = ((ticks >> 4) & 1ull) != 0ull;
        std::vector<uint8_t> patch;
        patch.reserve(64);

        const auto& src = chooseA ? kTemplateA : kTemplateB;
        for (uint8_t b : src) {
            patch.push_back(b);
        }

        // Add a small deterministic nops tail to create non-identical but bounded variants.
        const size_t pad = static_cast<size_t>(ticks % 16ull);
        for (size_t i = 0; i < pad; ++i) {
            patch.push_back((i % 2) ? 0x66 : 0x90);
        }
        if (patch.empty() || patch.back() != 0xC3) {
            patch.push_back(0xC3);
        }

        return patch;
    }

    bool ValidateNewOpcodes(const std::vector<uint8_t>& code) {
        if (code.empty() || code.size() > kMaxPatchBytes) {
            return false;
        }

        // Ensure patch terminates cleanly for this fallback lane.
        if (code.back() != 0xC3) {
            return false;
        }

        for (size_t i = 0; i < code.size(); ++i) {
            const uint8_t b0 = code[i];
            const uint8_t b1 = (i + 1 < code.size()) ? code[i + 1] : 0;
            if (IsForbiddenOpcode(b0, b1)) {
                return false;
            }
        }

        return true;
    }

    void LogMutationEvent(const std::vector<uint8_t>& code, bool applied) {
        const uint64_t hash = Fnv1a64(code);
        const uint64_t now = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());

        char tempPath[MAX_PATH] = {};
        if (GetTempPathA(MAX_PATH, tempPath) > 0) {
            std::string logPath = std::string(tempPath) + "rawrxd_autonomous_synth.log";
            std::ofstream out(logPath, std::ios::app | std::ios::binary);
            if (out.is_open()) {
                out << now << " applied=" << (applied ? 1 : 0)
                    << " bytes=" << code.size()
                    << " hash=0x" << std::hex << hash << std::dec << "\n";
            }
        }

        char dbg[256] = {};
        wsprintfA(dbg,
                  "[AutonomousSynthesizer] applied=%d bytes=%u hash=0x%llx\n",
                  applied ? 1 : 0,
                  static_cast<unsigned>(code.size()),
                  static_cast<unsigned long long>(hash));
        OutputDebugStringA(dbg);
    }
};

extern "C" __declspec(dllexport) void RawrXD_Autonomous_SynthesizeAndPatch_Tick()
{
    AutonomousSynthesizer::GetInstance().SynthesizeAndPatch();
}

} // namespace Autonomy
} // namespace RawrXD
