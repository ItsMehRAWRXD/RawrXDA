#include "RawrXD_Inference_Wrapper.h"

// RawrXD_Inference_Wrapper.cpp - C++ wrapper for calling Heretic MASM kernel

void RawrXD_Sovereign_Sample(llama_token_data_array* candidates) {
    // 1. Audit: Ensure we have a valid candidate pool
    if (!candidates || candidates->size == 0) return;

    // 2. Execution: Call the x64 MASM Kernel
    // This performs the O(n) scan and -100.0f logit suppression in-place.
    Heretic_Main_Entry(candidates);

    // 3. Telemetry: Signal the UI Bridge if a refusal was suppressed
    // (Logic moved to the Beacon side for high-TPS efficiency)
    // if (Sovereign_Telemetry_Enabled()) {
    //     Hotpatch_TraceBeacon("[SOVEREIGN] Logit Steering Applied\n", 31);
    // }
}