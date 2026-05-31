#pragma once
// C++ bridge for src/x64/GGUF_REVERSE_STREAM.asm (matches in-tree MASM exports).

#include <cstddef>
#include <cstdint>

extern "C"
{

    // Expand compressed seed block to F32/Q8 destination.
    // RCX=pSeedBlock, RDX=pNormalBuffer, R8=mirror_mode (0=exact, 1=stochastic), R9=weight_count
    std::size_t GGUF_Reverse_Stream(const void* pSeedBlock, void* pNormalBuffer, std::uint32_t mirrorMode,
                                    std::uint64_t weightCount);

    // Atomic clutch dump: returns previous active pointer.
    std::uint64_t GGUF_Clutch_Dump_Reverse(std::uint32_t tensorId, void* pNormalMirrorBuffer);

    // Mirror tensor list into contiguous output buffer.
    std::uint64_t GGUF_Mirror_To_Normal(const std::uint16_t* pTensorList, std::uint64_t listCount, void* pOutputBuffer);

    // Single-token diagonal backprop + clutch commit.
    void GGUF_Backpropagate_Step(std::uint32_t tensorId, const float* pActivation, const float* pGradient,
                                 std::uint64_t vectorDim);

    // MASM returns telemetry in RAX, RDX, R8, R9 (not a single C++ return value).
    struct GGUF_ClutchTelemetryRegs
    {
        std::uint64_t clutchDumps = 0;
        std::uint64_t mirrorBytes = 0;
        std::uint64_t backpropSteps = 0;
        std::uint64_t gradientAccum = 0;
    };

    void GGUF_Get_Clutch_Telemetry();

#if defined(_MSC_VER) && defined(_M_X64)
    inline GGUF_ClutchTelemetryRegs ggufGetClutchTelemetry()
    {
        GGUF_ClutchTelemetryRegs t{};
        __asm {
        call GGUF_Get_Clutch_Telemetry
        mov t.clutchDumps, rax
        mov t.mirrorBytes, rdx
        mov t.backpropSteps, r8
        mov t.gradientAccum, r9
        }
        return t;
    }
#endif

    // Stub — file I/O handled by C++ layer in production.
    std::uint32_t GGUF_Export_GGUF_v3(const char* pFilePath, std::uint64_t pathLen, const std::uint16_t* pTensorList,
                                      std::uint64_t listCount);

#pragma pack(push, 1)
    struct GGUF_MirrorDesc
    {
        const void* pSeedBlock;
        void* pNormalBuffer;
        std::uint64_t weightCount;
        std::uint64_t mirrorMode;
    };
#pragma pack(pop)

    // Batch mirror; optional total written to pTotalBytesOut when non-null.
    std::uint64_t GGUF_Reverse_Stream_Batch(std::uint64_t count, const GGUF_MirrorDesc* pMirrorDesc,
                                            std::uint64_t* pTotalBytesOut);

}  // extern "C"

// Note: GGUF_Reverse_Stream in-tree uses (pSeedBlock, pNormalBuffer, mirrorMode, weightCount),
// not the tensor-id table variant from alternate drafts.
