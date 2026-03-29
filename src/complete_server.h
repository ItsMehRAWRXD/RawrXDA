#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD { class InferenceEngine; }
// Forward declarations (global)
class AgenticEngine;
class SubAgentManager;
class AgentHistoryRecorder;
class PolicyEngine;
class ExplainabilityEngine;
class AIBackendManager;

namespace RawrXD
{

class CompletionServer
{
  public:
    CompletionServer();
    ~CompletionServer();

    bool Start(uint16_t port, RawrXD::InferenceEngine* engine, std::string model_path);
    void Stop();

    // Agentic integration — set before Start() or anytime
    void SetAgenticEngine(::AgenticEngine* engine) { agentic_engine_ = engine; }
    void SetSubAgentManager(::SubAgentManager* mgr) { subagent_mgr_ = mgr; }
    void SetHistoryRecorder(::AgentHistoryRecorder* rec) { history_recorder_ = rec; }
    void SetPolicyEngine(::PolicyEngine* engine) { policy_engine_ = engine; }
    void SetExplainabilityEngine(::ExplainabilityEngine* engine) { explain_engine_ = engine; }
    void SetBackendManager(::AIBackendManager* mgr) { backend_mgr_ = mgr; }

  private:
    void Run(uint16_t port);
    void HandleClient(int client_fd);
    std::string HandleCompleteRequest(const std::string& body);
    void HandleCompleteStreamRequest(int client_fd, const std::string& body);

    // Agentic API handlers
    std::string HandleChatRequest(const std::string& body);
    std::string HandleAgentWishRequest(const std::string& body);
    std::string HandleAgentOrchestrateRequest(const std::string& body);
    std::string HandleToolRequest(const std::string& body);  // POST /api/tool — Win32 IDE Run Tool parity
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

    // Phase 11 (alias) + Feature parity endpoints
    std::string HandleEngine800BStatusRequest();
    std::string HandleAVX512StatusRequest();
    std::string HandleTunerStatusRequest();

    // Phase 12 — Extreme Compression API handlers
    std::string HandleCompressionStatusRequest();
    std::string HandleCompressionProfilesRequest();
    std::string HandleCompressionCompressRequest(const std::string& body);
    std::string HandleCompressionStatsRequest();

    // Phase 13 — Distributed Pipeline Orchestrator API handlers
    std::string HandlePipelineStatusRequest();
    std::string HandlePipelineSubmitRequest(const std::string& body);
    std::string HandlePipelineTasksRequest();
    std::string HandlePipelineCancelRequest(const std::string& body);
    std::string HandlePipelineNodesRequest();

    // Phase 14 — Advanced Hotpatch Control Plane API handlers
    std::string HandleHotpatchCPStatusRequest();
    std::string HandleHotpatchCPPatchesRequest();
    std::string HandleHotpatchCPApplyRequest(const std::string& body);
    std::string HandleHotpatchCPRollbackRequest(const std::string& body);
    std::string HandleHotpatchCPAuditRequest();

    // Phase 15 — Static Analysis Engine API handlers
    std::string HandleAnalysisStatusRequest();
    std::string HandleAnalysisFunctionsRequest();
    std::string HandleAnalysisRunRequest(const std::string& body);
    std::string HandleAnalysisCfgRequest(const std::string& body);

    // Phase 16 — Semantic Code Intelligence API handlers
    std::string HandleSemanticStatusRequest();
    std::string HandleSemanticIndexRequest(const std::string& body);
    std::string HandleSemanticSearchRequest(const std::string& body);
    std::string HandleSemanticGotoRequest(const std::string& body);
    std::string HandleSemanticReferencesRequest(const std::string& body);

    // Phase 17 — Enterprise Telemetry & Compliance API handlers
    std::string HandleTelemetryStatusRequest();
    std::string HandleTelemetryAuditRequest(const std::string& body);
    std::string HandleTelemetryComplianceRequest();
    std::string HandleTelemetryLicenseRequest();
    std::string HandleTelemetryMetricsRequest();
    std::string HandleTelemetryExportRequest(const std::string& body);

    // Phase 20 — WebRTC P2P Signaling
    std::string HandleWebrtcStatusRequest();
    // Phase 21 — Swarm Bridge + Universal Model Hotpatcher
    std::string HandleSwarmBridgeStatusRequest();
    std::string HandleHotpatchModelStatusRequest();
    // Phase 22 — Production Release
    std::string HandleReleaseStatusRequest();
    // Phase 23 — GPU Kernel Auto-Tuner run
    std::string HandleTunerRunRequest(const std::string& body);
    // Phase 24 — Windows Sandbox
    std::string HandleSandboxListRequest();
    std::string HandleSandboxCreateRequest(const std::string& body);
    // Phase 25 — AMD GPU Acceleration
    std::string HandleGpuStatusRequest();
    std::string HandleGpuToggleRequest();
    std::string HandleGpuFeaturesRequest();
    std::string HandleGpuMemoryRequest();

    // Phase 51 — Security (Dork Scanner + Universal Dorker) API handlers
    std::string HandleSecurityDorkStatusRequest();
    std::string HandleSecurityDorkScanRequest(const std::string& body);
    std::string HandleSecurityDorkUniversalRequest(const std::string& body);
    std::string HandleSecurityDashboardRequest();

    // Phase 26 — ReverseEngineered Kernel API handlers
    std::string HandleSchedulerStatusRequest();
    std::string HandleSchedulerSubmitRequest(const std::string& body);
    std::string HandleConflictStatusRequest();
    std::string HandleHeartbeatStatusRequest();
    std::string HandleHeartbeatAddRequest(const std::string& body);
    std::string HandleGpuDmaStatusRequest();
    std::string HandleTensorBenchRequest(const std::string& body);
    std::string HandleTimerRequest();
    std::string HandleCrc32Request(const std::string& body);

    // Enterprise License API handlers
    std::string HandleLicenseStatusRequest();
    std::string HandleLicenseFeaturesRequest();
    std::string HandleLicenseAuditRequest();
    std::string HandleLicenseHwidRequest();
    std::string HandleLicenseSupportStatusRequest();
    std::string HandleLicenseMultiGPUStatusRequest();

    // Agentic Autonomous config (operation mode, model selection, cap 99, cycle 1x-99x)
    std::string HandleAgenticConfigGetRequest();
    std::string HandleAgenticConfigPostRequest(const std::string& body);
    std::string HandleAgenticProfileRequest(const std::string& path);
    // Production audit estimate (GET /api/agentic/audit-estimate?codebase=full&topN=20)
    std::string HandleAgenticAuditEstimateRequest(const std::string& path);
    std::string HandleParityStatusRequest();

    // Models list for Cursor Settings > Models (GET /v1/models, GET /api/models)
    std::string HandleModelsListRequest();
    // GGUF diagnostics endpoints
    std::string HandleGGUFDiagnosticsRequest();
    std::string HandleGGUFDiagnosticsJsonRequest();

    std::atomic<bool> running_;
    std::atomic<int> active_clients_{0};
    std::thread server_thread_;
    RawrXD::InferenceEngine* engine_;
    std::string model_path_;
    ::AgenticEngine* agentic_engine_ = nullptr;
    ::SubAgentManager* subagent_mgr_ = nullptr;
    ::AgentHistoryRecorder* history_recorder_ = nullptr;
    ::PolicyEngine* policy_engine_ = nullptr;
    ::ExplainabilityEngine* explain_engine_ = nullptr;
    ::AIBackendManager* backend_mgr_ = nullptr;
};

}  // namespace RawrXD

// Shared with CLI (main.cpp) for full parity: chat via Ollama when no GGUF loaded
bool OllamaGenerateSync(const std::string& host, int port, const std::string& model, const std::string& prompt,
                        std::string& outResponse);
// List Ollama models (for CLI --list). Returns true if request succeeded; outNames may be empty if none.
bool OllamaListModelsSync(const std::string& host, int port, std::vector<std::string>& outNames);
// Route generation through local HTTP loading/agentic engine (/api/chat).
bool AgenticHttpChatGenerateSync(const std::string& host, int port, const std::string& model, const std::string& prompt,
                                 std::string& outResponse);
