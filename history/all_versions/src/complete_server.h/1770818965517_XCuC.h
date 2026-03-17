#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include "cpu_inference_engine.h"

// Forward declarations
class AgenticEngine;
class SubAgentManager;
class AgentHistoryRecorder;
class PolicyEngine;
class ExplainabilityEngine;
class AIBackendManager;

namespace RawrXD {

class CompletionServer {
public:
    CompletionServer();
    ~CompletionServer();

    bool Start(uint16_t port, CPUInferenceEngine* engine, std::string model_path);
    void Stop();

    // Agentic integration — set before Start() or anytime
    void SetAgenticEngine(AgenticEngine* engine) { agentic_engine_ = engine; }
    void SetSubAgentManager(SubAgentManager* mgr) { subagent_mgr_ = mgr; }
    void SetHistoryRecorder(AgentHistoryRecorder* rec) { history_recorder_ = rec; }
    void SetPolicyEngine(PolicyEngine* engine)         { policy_engine_ = engine; }
    void SetExplainabilityEngine(ExplainabilityEngine* engine) { explain_engine_ = engine; }
    void SetBackendManager(AIBackendManager* mgr)              { backend_mgr_ = mgr; }

private:
    void Run(uint16_t port);
    void HandleClient(int client_fd);
    std::string HandleCompleteRequest(const std::string& body);
    void HandleCompleteStreamRequest(int client_fd, const std::string& body);

    // Agentic API handlers
    std::string HandleChatRequest(const std::string& body);
    std::string HandleSubAgentRequest(const std::string& body);
    std::string HandleChainRequest(const std::string& body);
    std::string HandleSwarmRequest(const std::string& body);
    std::string HandleAgentsListRequest();
    std::string HandleAgentsStatusRequest();
    std::string HandleHistoryRequest(const std::string& path, const std::string& body);
    std::string HandleReplayRequest(const std::string& body);

    // Phase 7 — Policy API handlers
    std::string HandlePoliciesRequest(const std::string& path, const std::string& body);
    std::string HandlePolicySuggestionsRequest(const std::string& body);
    std::string HandlePolicyApplyRequest(const std::string& body);
    std::string HandlePolicyRejectRequest(const std::string& body);
    std::string HandlePolicyExportRequest();
    std::string HandlePolicyImportRequest(const std::string& body);
    std::string HandlePolicyHeuristicsRequest();
    std::string HandlePolicyStatsRequest();

    // Phase 8A — Explainability API handlers
    std::string HandleExplainRequest(const std::string& path, const std::string& body);
    std::string HandleExplainStatsRequest();

    // Phase 8B — Backend Switcher API handlers
    std::string HandleBackendsListRequest();
    std::string HandleBackendsStatusRequest();
    std::string HandleBackendsUseRequest(const std::string& body);

    // Phase 10 — Speculative Decoding API handlers
    std::string HandleSpecDecStatusRequest();
    std::string HandleSpecDecConfigRequest(const std::string& body);
    std::string HandleSpecDecStatsRequest();
    std::string HandleSpecDecGenerateRequest(const std::string& body);
    std::string HandleSpecDecResetRequest();

    // Phase 11 — Flash Attention v2 API handlers
    std::string HandleFlashAttnStatusRequest();
    std::string HandleFlashAttnConfigRequest();
    std::string HandleFlashAttnBenchmarkRequest(const std::string& body);

    // Phase 12 — Extreme Compression API handlers
    std::string HandleCompressionStatusRequest();
    std::string HandleCompressionProfilesRequest();
    std::string HandleCompressionCompressRequest(const std::string& body);
    std::string HandleCompressionStatsRequest();

    std::atomic<bool> running_;
    std::thread server_thread_;
    CPUInferenceEngine* engine_;
    std::string model_path_;
    AgenticEngine* agentic_engine_ = nullptr;
    SubAgentManager* subagent_mgr_ = nullptr;
    AgentHistoryRecorder* history_recorder_ = nullptr;
    PolicyEngine* policy_engine_ = nullptr;
    ExplainabilityEngine* explain_engine_ = nullptr;
    AIBackendManager* backend_mgr_ = nullptr;
};

} // namespace RawrXD
