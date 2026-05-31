// ============================================================================
// agentic_controller_wiring.h
// Wiring layer to integrate MinimalAgentController with AgenticBridge
// ============================================================================

#pragma once

#include "agent_controller_minimal.h"
#include "agent_controller_promoted.h"
#include "../cpu_inference_engine.h"
#include <string>

class AgenticBridge;

namespace rawrxd {

struct ExecutePlanTelemetry {
    int classification = 0;
    double inferenceLatencyMs = 0.0;
    int retryLevelAttempted = -1;
    unsigned int inputTokens = 0;
    unsigned int outputTokens = 0;
    int fallbackUsed = 0;
};

/// Initialize the agent controller and wire it with the inference engine
/// Should be called once during AgenticBridge initialization
void initializeAgentControllerWiring(RawrXD::InferenceEngine* inference_engine);

/// Process an agentic request using the minimal agent controller
/// Returns true if successfully processed via agentic path, false if should fall back
MinimalAgenticResponse processAgenticRequest(const MinimalAgenticRequest& request);

/// Check if agentic layer is available and ready
bool isAgenticLayerAvailable();

/// Call LLM through the integrated inference engine
/// This is called by the agent controller to generate responses
std::string getLLMResponse(const std::string& system_prompt,
                           const std::string& user_message,
                           const std::string& model_path);

bool executePlanWithTelemetry(const std::string& userIntent,
                              std::string* finalOutput,
                              ExecutePlanTelemetry* telemetry);

CompletionResult requestInlineCompletion(const std::string& prefix,
                                         const EditorContext& context);
std::string getSessionPlanGraphSummary(const std::string& sessionId,
                                       size_t maxNodes = 8);
void reportInlineCompletionFeedback(const std::string& sessionId,
                                    const std::string& planId,
                                    bool accepted,
                                    const std::string& detail);

extern "C" __declspec(dllexport) int ExecutePlanWithTelemetry(const char* user_intent,
                                                                char* out_buf,
                                                                unsigned int out_buf_size,
                                                                unsigned int* out_required,
                                                                ExecutePlanTelemetry* out_telemetry);

} // namespace rawrxd
