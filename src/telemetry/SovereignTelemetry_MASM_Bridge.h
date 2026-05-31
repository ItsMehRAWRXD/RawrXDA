#pragma once
#include "sovereign_stats_block_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

// SovereignTelemetry_Update(SovereignStatsBlockV2* block, float tps, float mspt, uint32_t draftAcc, uint32_t draftRej, float pressure)
void SovereignTelemetry_Update(SovereignStatsBlockV2* block, float tps, float mspt, uint32_t draftAcc, uint32_t draftRej, float pressure);

// SovereignTelemetry_UpdateWeights(SovereignStatsBlockV2* block, float w0, float w1, float w2)
void SovereignTelemetry_UpdateWeights(SovereignStatsBlockV2* block, float w0, float w1, float w2);

#ifdef __cplusplus
}
#endif
