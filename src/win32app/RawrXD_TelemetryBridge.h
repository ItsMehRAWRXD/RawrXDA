#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TelemetryHandle* HTelemetry;

HTelemetry Telemetry_Initialize();
int Telemetry_EmitEvent(void* agentContext, void* workflowState);
const char* Telemetry_ReadNextEvent(HTelemetry ring, uint64_t* outTimestamp);
void Telemetry_Flush(HTelemetry ring);

#ifdef __cplusplus
}

namespace RawrXD::Telemetry {

struct ExplicitState {
    int stepNumber = 0;
    const char* stepName = "IDLE";
    uint64_t durationMicros = 0;
    uint64_t timestamp = 0;
    int64_t memoryKb = 0;
    int inputTokens = 0;
    int outputTokens = 0;
    int tokensPerSecond = 0;
    int retryCount = 0;
    int parallelAgents = 1;
    bool checkpointCreated = false;
    int contextFiles = 0;
    int contextTokens = 0;
    int contextKvPages = 0;
    const char* workflowId = "0";
    const char* agentId = "0";
    const char* tools = "[]";
    const char* toolsOk = "[]";
    const char* errorValue = "null";
};

void EmitExplicitState(const ExplicitState& state);

}  // namespace RawrXD::Telemetry
#endif
