#pragma once
// 150 TPS OVERDRIVE: Physical Cycles/Token Tracer

#include "../draft_integration/drafter_wiring.hpp"

struct HardwareTrace {
    double Draft_VNNI_Cycles_Per_Token;
    double Draft_FMA_Cycles_Per_Token;
    double VRAM_Bandwidth_Saturation_Pct;
};

class OverdriveTracer {
public:
    OverdriveTracer();
    HardwareTrace ExecuteDraftTrace(KernelDrafterEngine* Engine, UINT32 Iterations);
};
