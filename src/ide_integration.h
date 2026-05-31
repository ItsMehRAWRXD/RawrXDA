// ============================================================================
// ide_integration.h — Unified IDE Integration for RawrXD
// Connects all existing components: AgenticEngine, ChatInterface, ToolRegistry,
// GitHubMCPBridge, ModelRouterAdapter, VulkanCompute, etc.
// Zero external dependencies beyond what RawrXD already uses.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>

// Forward declarations from existing RawrXD components
class AgenticEngine;
class ChatInterface;
class ToolRegistry;
class GitHubMCPBridge;
class ModelRouterAdapter;
class MultiTabEditor;
class TerminalPool;
class AgenticExecutor;

namespace RawrXD {
    class CPUInferenceEngine;
    class VulkanCompute;
    class UniversalModelRouter;
    struct VulkanDeviceInfo;
}

// ============================================================================
// IDE Component Handles (opaque pointers to existing implementations)
// ============================================================================
struct IDEComponents {
    AgenticEngine* agenticEngine = nullptr;
    ChatInterface* chatInterface = nullptr;
    MultiTabEditor* editor = nullptr;
    TerminalPool* terminals = nullptr;
    RawrXD::CPUInferenceEngine* inferenceEngine = nullptr;
    RawrXD::VulkanCompute* vulkanCompute = nullptr;
    UniversalModelRouter* modelRouter = nullptr;
    GitHubMCPBridge* githubBridge = nullptr;
};

// ============================================================================
// IDE Integration Result
// ============================================================================
struct IDEResult {
    bool success = false;
    int32_t statusCode = 0;
    std::string output;
    std::string error;
    std::map<std::string, std::string> metadata;
    double durationMs = 0.0;
};

// ============================================================================
// IDE Event Callbacks
// ============================================================================
using IDEEventCallback = std::function<void(const std::string& eventType, const std::string& data)>;

// ============================================================================
// IDE Integration — Main Entry Point
// ============================================================================
class IDEIntegration {
public:
    static IDEIntegration& Instance();
    
    // === Lifecycle ===
    bool Initialize(const IDEComponents& components);
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }
    
    // === Component Access ===
    AgenticEngine* GetAgenticEngine() { return m_components.agenticEngine; }
    ChatInterface* GetChatInterface() { return m_components.chatInterface; }
    RawrXD::CPUInferenceEngine* GetInferenceEngine() { return m_components.inferenceEngine; }
    RawrXD::VulkanCompute* GetVulkanCompute() { return m_components.vulkanCompute; }
    
    // === Chat Operations ===
    IDEResult SendMessage(const std::string& message);
    IDEResult SendMessageAsync(const std::string& message, IDEEventCallback callback);
    std::vector<std::string> GetChatHistory();
    void ClearChatHistory();
    
    // === Code Operations ===
    IDEResult AnalyzeCode(const std::string& code);
    IDEResult GenerateCode(const std::string& prompt, const std::string& language = "cpp");
    IDEResult RefactorCode(const std::string& code, const std::string& refactoringType);
    IDEResult ExplainCode(const std::string& code);
    IDEResult GenerateTests(const std::string& code);
    
    // === File Operations (via ToolRegistry) ===
    IDEResult ReadFile(const std::string& path, size_t offset = 0, size_t limit = 65536);
    IDEResult WriteFile(const std::string& path, const std::string& content);
    IDEResult EditFile(const std::string& path, const std::string& oldStr, const std::string& newStr);
    IDEResult ListDirectory(const std::string& path);
    IDEResult SearchFiles(const std::string& pattern, const std::string& path = ".");
    IDEResult GrepFiles(const std::string& pattern, const std::string& path = ".");
    
    // === Git Operations (via GitHubMCPBridge) ===
    IDEResult GitStatus();
    IDEResult GitDiff(const std::string& file = "");
    IDEResult GitLog(int count = 10);
    IDEResult GitBranch();
    IDEResult GitAdd(const std::string& files);
    IDEResult GitCommit(const std::string& message);
    IDEResult GitPush(const std::string& remote = "origin", const std::string& branch = "");
    IDEResult GitPull(const std::string& remote = "origin", const std::string& branch = "");
    
    // === GitHub Operations ===
    IDEResult GetPullRequest(const std::string& owner, const std::string& repo, int prNumber);
    IDEResult CreateReviewComment(const std::string& owner, const std::string& repo, int prNumber,
                                   const std::string& comment, const std::string& file, int line);
    IDEResult ListIssues(const std::string& owner, const std::string& repo);
    
    // === Model Operations ===
    IDEResult LoadModel(const std::string& modelPath);
    IDEResult UnloadModel();
    IDEResult GetModelStatus();
    IDEResult SetModelParameter(const std::string& key, const std::string& value);
    IDEResult GetAvailableModels();
    
    // === Inference Operations ===
    IDEResult Generate(const std::string& prompt, int maxTokens = 2048, float temperature = 0.8f);
    IDEResult GenerateAsync(const std::string& prompt, IDEEventCallback onToken, int maxTokens = 2048);
    IDEResult Embed(const std::string& text);
    
    // === GPU Operations (via VulkanCompute) ===
    IDEResult GetGPUInfo();
    IDEResult AllocateGPUBuffer(size_t size);
    IDEResult FreeGPUBuffer(uint32_t bufferId);
    IDEResult CopyToGPU(uint32_t bufferId, const void* data, size_t size);
    IDEResult CopyFromGPU(uint32_t bufferId, void* data, size_t size);
    IDEResult GPUMatMul(uint32_t A, uint32_t B, uint32_t C, uint32_t M, uint32_t K, uint32_t N);
    IDEResult GPUAttention(uint32_t Q, uint32_t K, uint32_t V, uint32_t out,
                           uint32_t seqLen, uint32_t headDim, uint32_t numHeads);
    
    // === Tool Registry ===
    IDEResult ExecuteTool(const std::string& name, const std::map<std::string, std::string>& params);
    IDEResult ListTools();
    bool HasTool(const std::string& name);
    
    // === Agent Operations ===
    IDEResult ExecuteAgentTask(const std::string& description, const std::string& prompt);
    IDEResult RunSubAgent(const std::string& description, const std::string& prompt);
    IDEResult ExecuteChain(const std::vector<std::string>& steps);
    IDEResult ExecuteSwarm(const std::vector<std::string>& prompts, int maxParallel = 4);
    
    // === Event Handling ===
    void SetEventHandler(IDEEventCallback callback);
    void EmitEvent(const std::string& eventType, const std::string& data);
    
    // === Diagnostics ===
    std::string GetDiagnostics();
    std::map<std::string, std::string> GetStats();
    
private:
    IDEIntegration() = default;
    ~IDEIntegration() = default;
    IDEIntegration(const IDEIntegration&) = delete;
    IDEIntegration& operator=(const IDEIntegration&) = delete;
    
    IDEComponents m_components;
    std::atomic<bool> m_initialized{false};
    std::mutex m_mutex;
    IDEEventCallback m_eventHandler;
    
    // Internal helpers
    IDEResult ExecuteToolInternal(const std::string& name, const std::string& jsonParams);
    std::string MapToJson(const std::map<std::string, std::string>& params);
};

// ============================================================================
// IDE Builder — Fluent API for Configuration
// ============================================================================
class IDEBuilder {
public:
    IDEBuilder& WithAgenticEngine(AgenticEngine* engine);
    IDEBuilder& WithChatInterface(ChatInterface* chat);
    IDEBuilder& WithEditor(MultiTabEditor* editor);
    IDEBuilder& WithTerminals(TerminalPool* terminals);
    IDEBuilder& WithInferenceEngine(RawrXD::CPUInferenceEngine* engine);
    IDEBuilder& WithVulkanCompute(RawrXD::VulkanCompute* compute);
    IDEBuilder& WithModelRouter(UniversalModelRouter* router);
    IDEBuilder& WithGitHubBridge(GitHubMCPBridge* bridge);
    
    IDEComponents Build();
    
private:
    IDEComponents m_components;
};

// ============================================================================
// C API for MASM/Native Integration
// ============================================================================
extern "C" {
    // Lifecycle
    __declspec(dllexport) bool IDE_Init(void* components);
    __declspec(dllexport) void IDE_Shutdown();
    __declspec(dllexport) bool IDE_IsInitialized();
    
    // Chat
    __declspec(dllexport) int IDE_SendMessage(const char* message, char* result, int resultSize);
    __declspec(dllexport) int IDE_GetChatHistory(char* result, int resultSize);
    __declspec(dllexport) void IDE_ClearChatHistory();
    
    // Code
    __declspec(dllexport) int IDE_AnalyzeCode(const char* code, char* result, int resultSize);
    __declspec(dllexport) int IDE_GenerateCode(const char* prompt, const char* language, char* result, int resultSize);
    __declspec(dllexport) int IDE_RefactorCode(const char* code, const char* type, char* result, int resultSize);
    
    // Files
    __declspec(dllexport) int IDE_ReadFile(const char* path, int offset, int limit, char* result, int resultSize);
    __declspec(dllexport) int IDE_WriteFile(const char* path, const char* content, char* result, int resultSize);
    __declspec(dllexport) int IDE_EditFile(const char* path, const char* oldStr, const char* newStr, char* result, int resultSize);
    
    // Git
    __declspec(dllexport) int IDE_GitStatus(char* result, int resultSize);
    __declspec(dllexport) int IDE_GitCommit(const char* message, char* result, int resultSize);
    __declspec(dllexport) int IDE_GitPush(char* result, int resultSize);
    
    // Model
    __declspec(dllexport) int IDE_LoadModel(const char* path, char* result, int resultSize);
    __declspec(dllexport) int IDE_Generate(const char* prompt, int maxTokens, float temp, char* result, int resultSize);
    
    // GPU
    __declspec(dllexport) int IDE_GetGPUInfo(char* result, int resultSize);
    __declspec(dllexport) int IDE_GPUExecute(const char* operation, const char* params, char* result, int resultSize);
    
    // Tools
    __declspec(dllexport) int IDE_ExecuteTool(const char* name, const char* params, char* result, int resultSize);
    __declspec(dllexport) int IDE_ListTools(char* result, int resultSize);
    
    // Diagnostics
    __declspec(dllexport) int IDE_GetDiagnostics(char* result, int resultSize);
    __declspec(dllexport) int IDE_GetStats(char* result, int resultSize);
}
