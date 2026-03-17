#pragma once

// Phase 150A: Integration of AVX-512 Microkernels into the CPU Drafter
// Orchestrates the 1.5B distillation model drafting via VNNI DP + FMA Unroll

#include "../../p28_hypervelocity/hyper_150tps.hpp"

extern "C" void SwarmV150_Core_Affinity_Lock(UINT32 CoreIndex);
extern "C" void SwarmV150_VNNI_Dequant_Core(void* OutAcc, void* Weights, void* Acts, UINT64 UnrollDepth);
extern "C" void SwarmV150_AVX512_FMA_Unroll(void* OutAcc, void* MatrixA, void* VectorX, UINT64 TileDepth);

class KernelDrafterEngine {
public:
    KernelDrafterEngine();
    
    // Wire: Pins the drafting thread to a physical core (e.g. Core 2) to eliminate SMT thrash
    void LockDraftThread();

    // Wire: Executes the draft forward-pass using the compiled MASM microkernels
    void ExecuteDraftPass(void* DistilWeights, void* PastActivations, SpeculativeTree* OutTree);
};
