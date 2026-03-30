#pragma once

#include <cstdint>
#include <string>

namespace rawrxd::agentic::avx512 {

struct alignas(64) AgentRegisterHotState {
    uint8_t workingState[512];
    uint8_t toolScratch[512];
    uint32_t iteration = 0;
    uint32_t retry = 0;
    uint32_t lastStepSuccess = 0;
    uint32_t reserved = 0;
};

void initializeHotState(AgentRegisterHotState& state, const std::string& goal, const std::string& workspace);
void loadContext(AgentRegisterHotState& state);
void executeStep(AgentRegisterHotState& state);
void storeContext(AgentRegisterHotState& state);

bool kernelExportsAvailable();

} // namespace rawrxd::agentic::avx512
