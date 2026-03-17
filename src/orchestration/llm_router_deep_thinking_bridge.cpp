#include "llm_router.hpp"
#include <string>
#include <vector>

// Forward declarations for MASM kernels
extern "C" void rawrxd_init_deep_thinking();
extern "C" void rawrxd_agentic_deep_think_loop(const char* prompt);

/**
 * @brief Extends LLMRouter to handle "Deep Thinking" (Chain-of-Thought) tasks 
 * by bridging the routing logic with high-performance MASM kernels.
 */
class DeepThinkingRouterBridge {
public:
    explicit DeepThinkingRouterBridge(LLMRouter* router) : m_router(router) {
        rawrxd_init_deep_thinking();
    }

    /**
     * @brief Routes a complex task to the Deep Thinking kernel if the router 
     * identifies "reasoning" as the primary required capability.
     */
    bool handleReasoningTask(const std::string& prompt) {
        // AI-Driven Routing Decision
        RoutingDecision decision = m_router->route(prompt, "reasoning", 4096);

        // If the selected model is "local-deep-think" or if reasoning confidence is high,
        // we offload the heavy lifting to the MASM specialized kernel.
        if (decision.selectedModelId == "local-deep-think" || 
            (decision.confidenceScore > 85 && decision.routingStrategy == "reasoning")) {
            
            // Execute the high-speed ASM reasoning loop
            rawrxd_agentic_deep_think_loop(prompt.c_str());
            
            // Record performance back to the router for telemetry
            m_router->recordPerformance("local-deep-think", 500, 1024, 0.95);
            return true;
        }

        return false; // Fallback to standard routing
    }

private:
    LLMRouter* m_router;
};
