#pragma once
#include "swarmlink_v2_residency.hpp"
#include "swarmlink_v2_prefetch.hpp"
#include <stdint.h>

extern "C" {
    // P23-C: Speculative Path Validator
    __declspec(dllexport) BOOL SwarmV23_InitSpeculativeTree(uint32_t draft_depth);
    __declspec(dllexport) BOOL SwarmV23_ScoreDraftToken(uint32_t token_id, uint32_t step, float draft_prob);
    __declspec(dllexport) uint32_t SwarmV23_CommitSpeculativePath(void); 
}
