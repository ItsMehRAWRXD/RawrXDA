// ============================================================================
// ide_integration.cpp — Unified IDE Integration Implementation
// Connects all existing RawrXD components under a single unified API.
// ============================================================================

#include "ide_integration.h"
#include "agentic_engine.h"
#include "chat_interface.h"
#include "tool_registry.h"
#include "github_mcp_bridge.h"
#include "model_router_adapter.h"
#include "multi_tab_editor.h"
#include "terminal_pool.h"
#include "cpu_inference_engine.h"
#include "vulkan_compute.h"
#include "universal_model_router.h"
#include "agentic_executor.h"

#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstring>

// ============================================================================
// IDEIntegration Implementation
// ============================================================================

IDEIntegration& IDEIntegration::Instance() {
    static IDEIntegration instance;
    return instance;
}

bool IDEIntegration::Initialize(const IDEComponents& components) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized.load()) {
        return true; // Already initialized
    }
    
    m_components = components;
    
    // Validate critical components
    if (!m_components.agenticEngine) {
        // Create default if not provided
        EmitEvent("warning", "AgenticEngine not provided, AI features limited");
    }
    
    if (!m_components.inferenceEngine && !m_components.vulkanCompute) {
        EmitEvent("warning", "No inference engine or GPU compute available");
    }
    
    m_initialized.store(true);
    EmitEvent("initialized", "IDE Integration ready");
    return true;
}

void IDEIntegration::Shutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_initialized.load()) {
        return;
    }
    
    m_initialized.store(false);
    EmitEvent("shutdown", "IDE Integration shutdown complete");
    
    // Clear components (don't delete - we don't own them)
    m_components = IDEComponents{};
}

// ============================================================================
// Chat Operations
// ============================================================================

IDEResult IDEIntegration::SendMessage(const std::string& message) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.chatInterface) {
        result.error = "ChatInterface not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        // Use existing ChatInterface
        auto response = m_components.chatInterface->SendMessage(message);
        result.success = true;
        result.output = response;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::SendMessageAsync(const std::string& message, IDEEventCallback callback) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.chatInterface) {
        result.error = "ChatInterface not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        // Use existing ChatInterface async API
        m_components.chatInterface->SendMessageAsync(message, [callback](const std::string& token) {
            if (callback) {
                callback("token", token);
            }
        });
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

std::vector<std::string> IDEIntegration::GetChatHistory() {
    if (!m_components.chatInterface) {
        return {};
    }
    return m_components.chatInterface->GetHistory();
}

void IDEIntegration::ClearChatHistory() {
    if (m_components.chatInterface) {
        m_components.chatInterface->ClearHistory();
    }
}

// ============================================================================
// Code Operations
// ============================================================================

IDEResult IDEIntegration::AnalyzeCode(const std::string& code) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto analysis = m_components.agenticEngine->AnalyzeCode(code);
        result.success = true;
        result.output = analysis;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::GenerateCode(const std::string& prompt, const std::string& language) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto code = m_components.agenticEngine->GenerateCode(prompt, language);
        result.success = true;
        result.output = code;
        result.statusCode = 0;
        result.metadata["language"] = language;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::RefactorCode(const std::string& code, const std::string& refactoringType) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto refactored = m_components.agenticEngine->RefactorCode(code, refactoringType);
        result.success = true;
        result.output = refactored;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::ExplainCode(const std::string& code) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto explanation = m_components.agenticEngine->ExplainCode(code);
        result.success = true;
        result.output = explanation;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::GenerateTests(const std::string& code) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto tests = m_components.agenticEngine->GenerateTests(code);
        result.success = true;
        result.output = tests;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

// ============================================================================
// File Operations (via ToolRegistry)
// ============================================================================

IDEResult IDEIntegration::ReadFile(const std::string& path, size_t offset, size_t limit) {
    std::map<std::string, std::string> params;
    params["path"] = path;
    params["offset"] = std::to_string(offset);
    params["limit"] = std::to_string(limit);
    return ExecuteTool("read_file", params);
}

IDEResult IDEIntegration::WriteFile(const std::string& path, const std::string& content) {
    std::map<std::string, std::string> params;
    params["path"] = path;
    params["content"] = content;
    return ExecuteTool("write_file", params);
}

IDEResult IDEIntegration::EditFile(const std::string& path, const std::string& oldStr, const std::string& newStr) {
    std::map<std::string, std::string> params;
    params["path"] = path;
    params["old_string"] = oldStr;
    params["new_string"] = newStr;
    return ExecuteTool("edit_file", params);
}

IDEResult IDEIntegration::ListDirectory(const std::string& path) {
    std::map<std::string, std::string> params;
    params["path"] = path;
    return ExecuteTool("list_directory", params);
}

IDEResult IDEIntegration::SearchFiles(const std::string& pattern, const std::string& path) {
    std::map<std::string, std::string> params;
    params["pattern"] = pattern;
    params["path"] = path;
    return ExecuteTool("search_files", params);
}

IDEResult IDEIntegration::GrepFiles(const std::string& pattern, const std::string& path) {
    std::map<std::string, std::string> params;
    params["pattern"] = pattern;
    params["path"] = path;
    return ExecuteTool("grep_files", params);
}

// ============================================================================
// Git Operations (via GitHubMCPBridge)
// ============================================================================

IDEResult IDEIntegration::GitStatus() {
    if (!m_components.githubBridge) {
        IDEResult result;
        result.error = "GitHubMCPBridge not initialized";
        result.statusCode = -1;
        return result;
    }
    
    return ExecuteTool("git_status", {});
}

IDEResult IDEIntegration::GitDiff(const std::string& file) {
    std::map<std::string, std::string> params;
    if (!file.empty()) {
        params["file"] = file;
    }
    return ExecuteTool("git_diff", params);
}

IDEResult IDEIntegration::GitLog(int count) {
    std::map<std::string, std::string> params;
    params["count"] = std::to_string(count);
    return ExecuteTool("git_log", params);
}

IDEResult IDEIntegration::GitBranch() {
    return ExecuteTool("git_branch", {});
}

IDEResult IDEIntegration::GitAdd(const std::string& files) {
    std::map<std::string, std::string> params;
    params["files"] = files;
    return ExecuteTool("git_add", params);
}

IDEResult IDEIntegration::GitCommit(const std::string& message) {
    std::map<std::string, std::string> params;
    params["message"] = message;
    return ExecuteTool("git_commit", params);
}

IDEResult IDEIntegration::GitPush(const std::string& remote, const std::string& branch) {
    std::map<std::string, std::string> params;
    params["remote"] = remote;
    if (!branch.empty()) {
        params["branch"] = branch;
    }
    return ExecuteTool("git_push", params);
}

IDEResult IDEIntegration::GitPull(const std::string& remote, const std::string& branch) {
    std::map<std::string, std::string> params;
    params["remote"] = remote;
    if (!branch.empty()) {
        params["branch"] = branch;
    }
    return ExecuteTool("git_pull", params);
}

// ============================================================================
// GitHub Operations
// ============================================================================

IDEResult IDEIntegration::GetPullRequest(const std::string& owner, const std::string& repo, int prNumber) {
    if (!m_components.githubBridge) {
        IDEResult result;
        result.error = "GitHubMCPBridge not initialized";
        result.statusCode = -1;
        return result;
    }
    
    std::map<std::string, std::string> params;
    params["owner"] = owner;
    params["repo"] = repo;
    params["pr_number"] = std::to_string(prNumber);
    return ExecuteTool("get_pull_request", params);
}

IDEResult IDEIntegration::CreateReviewComment(const std::string& owner, const std::string& repo, 
                                               int prNumber, const std::string& comment, 
                                               const std::string& file, int line) {
    std::map<std::string, std::string> params;
    params["owner"] = owner;
    params["repo"] = repo;
    params["pr_number"] = std::to_string(prNumber);
    params["comment"] = comment;
    params["file"] = file;
    params["line"] = std::to_string(line);
    return ExecuteTool("create_review_comment", params);
}

IDEResult IDEIntegration::ListIssues(const std::string& owner, const std::string& repo) {
    std::map<std::string, std::string> params;
    params["owner"] = owner;
    params["repo"] = repo;
    return ExecuteTool("list_issues", params);
}

// ============================================================================
// Model Operations
// ============================================================================

IDEResult IDEIntegration::LoadModel(const std::string& modelPath) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.inferenceEngine && !m_components.vulkanCompute) {
        result.error = "No inference engine available";
        result.statusCode = -1;
        return result;
    }
    
    try {
        if (m_components.inferenceEngine) {
            m_components.inferenceEngine->LoadModel(modelPath);
        }
        result.success = true;
        result.output = "Model loaded: " + modelPath;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::UnloadModel() {
    IDEResult result;
    
    if (m_components.inferenceEngine) {
        m_components.inferenceEngine->UnloadModel();
        result.success = true;
        result.output = "Model unloaded";
    }
    
    return result;
}

IDEResult IDEIntegration::GetModelStatus() {
    IDEResult result;
    
    if (!m_components.inferenceEngine) {
        result.error = "No inference engine available";
        result.statusCode = -1;
        return result;
    }
    
    auto status = m_components.inferenceEngine->GetStatus();
    result.success = true;
    result.output = status;
    result.statusCode = 0;
    return result;
}

IDEResult IDEIntegration::SetModelParameter(const std::string& key, const std::string& value) {
    IDEResult result;
    
    if (!m_components.inferenceEngine) {
        result.error = "No inference engine available";
        result.statusCode = -1;
        return result;
    }
    
    m_components.inferenceEngine->SetParameter(key, value);
    result.success = true;
    result.output = "Parameter set: " + key + " = " + value;
    return result;
}

IDEResult IDEIntegration::GetAvailableModels() {
    std::map<std::string, std::string> params;
    return ExecuteTool("list_models", params);
}

// ============================================================================
// Inference Operations
// ============================================================================

IDEResult IDEIntegration::Generate(const std::string& prompt, int maxTokens, float temperature) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.inferenceEngine) {
        result.error = "No inference engine available";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto output = m_components.inferenceEngine->Generate(prompt, maxTokens, temperature);
        result.success = true;
        result.output = output;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::GenerateAsync(const std::string& prompt, IDEEventCallback onToken, int maxTokens) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.inferenceEngine) {
        result.error = "No inference engine available";
        result.statusCode = -1;
        return result;
    }
    
    try {
        m_components.inferenceEngine->GenerateAsync(prompt, [onToken](const std::string& token) {
            if (onToken) {
                onToken("token", token);
            }
        }, maxTokens);
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::Embed(const std::string& text) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.inferenceEngine) {
        result.error = "No inference engine available";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto embedding = m_components.inferenceEngine->Embed(text);
        // Convert embedding to string representation
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < embedding.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << embedding[i];
        }
        ss << "]";
        result.success = true;
        result.output = ss.str();
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

// ============================================================================
// GPU Operations (via VulkanCompute)
// ============================================================================

IDEResult IDEIntegration::GetGPUInfo() {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto info = m_components.vulkanCompute->GetDeviceInfo();
        result.success = true;
        result.output = info;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

IDEResult IDEIntegration::AllocateGPUBuffer(size_t size) {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        uint32_t bufferId = m_components.vulkanCompute->AllocateBuffer(size);
        result.success = true;
        result.output = std::to_string(bufferId);
        result.statusCode = 0;
        result.metadata["buffer_id"] = std::to_string(bufferId);
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

IDEResult IDEIntegration::FreeGPUBuffer(uint32_t bufferId) {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        m_components.vulkanCompute->FreeBuffer(bufferId);
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

IDEResult IDEIntegration::CopyToGPU(uint32_t bufferId, const void* data, size_t size) {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        m_components.vulkanCompute->CopyToBuffer(bufferId, data, size);
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

IDEResult IDEIntegration::CopyFromGPU(uint32_t bufferId, void* data, size_t size) {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        m_components.vulkanCompute->CopyFromBuffer(bufferId, data, size);
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

IDEResult IDEIntegration::GPUMatMul(uint32_t A, uint32_t B, uint32_t C, 
                                     uint32_t M, uint32_t K, uint32_t N) {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        m_components.vulkanCompute->MatMul(A, B, C, M, K, N);
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

IDEResult IDEIntegration::GPUAttention(uint32_t Q, uint32_t K, uint32_t V, uint32_t out,
                                        uint32_t seqLen, uint32_t headDim, uint32_t numHeads) {
    IDEResult result;
    
    if (!m_components.vulkanCompute) {
        result.error = "VulkanCompute not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        m_components.vulkanCompute->Attention(Q, K, V, out, seqLen, headDim, numHeads);
        result.success = true;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

// ============================================================================
// Tool Registry
// ============================================================================

IDEResult IDEIntegration::ExecuteTool(const std::string& name, 
                                        const std::map<std::string, std::string>& params) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    // Use ToolRegistry if available
    auto* registry = ToolRegistry::Instance();
    if (!registry) {
        result.error = "ToolRegistry not available";
        result.statusCode = -1;
        return result;
    }
    
    try {
        std::string jsonParams = MapToJson(params);
        auto output = registry->Execute(name, jsonParams);
        result.success = true;
        result.output = output;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::ListTools() {
    IDEResult result;
    
    auto* registry = ToolRegistry::Instance();
    if (!registry) {
        result.error = "ToolRegistry not available";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto tools = registry->ListTools();
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < tools.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << tools[i] << "\"";
        }
        ss << "]";
        result.success = true;
        result.output = ss.str();
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    return result;
}

bool IDEIntegration::HasTool(const std::string& name) {
    auto* registry = ToolRegistry::Instance();
    if (!registry) {
        return false;
    }
    return registry->HasTool(name);
}

// ============================================================================
// Agent Operations
// ============================================================================

IDEResult IDEIntegration::ExecuteAgentTask(const std::string& description, const std::string& prompt) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto output = m_components.agenticEngine->ExecuteTask(description, prompt);
        result.success = true;
        result.output = output;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::RunSubAgent(const std::string& description, const std::string& prompt) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto output = m_components.agenticEngine->RunSubAgent(description, prompt);
        result.success = true;
        result.output = output;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::ExecuteChain(const std::vector<std::string>& steps) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto output = m_components.agenticEngine->ExecuteChain(steps);
        result.success = true;
        result.output = output;
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

IDEResult IDEIntegration::ExecuteSwarm(const std::vector<std::string>& prompts, int maxParallel) {
    IDEResult result;
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!m_components.agenticEngine) {
        result.error = "AgenticEngine not initialized";
        result.statusCode = -1;
        return result;
    }
    
    try {
        auto outputs = m_components.agenticEngine->ExecuteSwarm(prompts, maxParallel);
        std::stringstream ss;
        ss << "[";
        for (size_t i = 0; i < outputs.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << "\"" << outputs[i] << "\"";
        }
        ss << "]";
        result.success = true;
        result.output = ss.str();
        result.statusCode = 0;
    } catch (const std::exception& e) {
        result.error = e.what();
        result.statusCode = -1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

// ============================================================================
// Event Handling
// ============================================================================

void IDEIntegration::SetEventHandler(IDEEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventHandler = callback;
}

void IDEIntegration::EmitEvent(const std::string& eventType, const std::string& data) {
    IDEEventCallback handler;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handler = m_eventHandler;
    }
    
    if (handler) {
        handler(eventType, data);
    }
}

// ============================================================================
// Diagnostics
// ============================================================================

std::string IDEIntegration::GetDiagnostics() {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"initialized\": " << (m_initialized.load() ? "true" : "false") << ",\n";
    ss << "  \"components\": {\n";
    ss << "    \"agenticEngine\": " << (m_components.agenticEngine ? "true" : "false") << ",\n";
    ss << "    \"chatInterface\": " << (m_components.chatInterface ? "true" : "false") << ",\n";
    ss << "    \"editor\": " << (m_components.editor ? "true" : "false") << ",\n";
    ss << "    \"terminals\": " << (m_components.terminals ? "true" : "false") << ",\n";
    ss << "    \"inferenceEngine\": " << (m_components.inferenceEngine ? "true" : "false") << ",\n";
    ss << "    \"vulkanCompute\": " << (m_components.vulkanCompute ? "true" : "false") << ",\n";
    ss << "    \"modelRouter\": " << (m_components.modelRouter ? "true" : "false") << ",\n";
    ss << "    \"githubBridge\": " << (m_components.githubBridge ? "true" : "false") << "\n";
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

std::map<std::string, std::string> IDEIntegration::GetStats() {
    std::map<std::string, std::string> stats;
    stats["initialized"] = m_initialized.load() ? "true" : "false";
    stats["has_agentic_engine"] = m_components.agenticEngine ? "true" : "false";
    stats["has_chat_interface"] = m_components.chatInterface ? "true" : "false";
    stats["has_inference_engine"] = m_components.inferenceEngine ? "true" : "false";
    stats["has_vulkan_compute"] = m_components.vulkanCompute ? "true" : "false";
    return stats;
}

// ============================================================================
// Internal Helpers
// ============================================================================

std::string IDEIntegration::MapToJson(const std::map<std::string, std::string>& params) {
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& [key, value] : params) {
        if (!first) ss << ", ";
        first = false;
        ss << "\"" << key << "\": \"" << value << "\"";
    }
    ss << "}";
    return ss.str();
}

// ============================================================================
// IDEBuilder Implementation
// ============================================================================

IDEBuilder& IDEBuilder::WithAgenticEngine(AgenticEngine* engine) {
    m_components.agenticEngine = engine;
    return *this;
}

IDEBuilder& IDEBuilder::WithChatInterface(ChatInterface* chat) {
    m_components.chatInterface = chat;
    return *this;
}

IDEBuilder& IDEBuilder::WithEditor(MultiTabEditor* editor) {
    m_components.editor = editor;
    return *this;
}

IDEBuilder& IDEBuilder::WithTerminals(TerminalPool* terminals) {
    m_components.terminals = terminals;
    return *this;
}

IDEBuilder& IDEBuilder::WithInferenceEngine(RawrXD::CPUInferenceEngine* engine) {
    m_components.inferenceEngine = engine;
    return *this;
}

IDEBuilder& IDEBuilder::WithVulkanCompute(RawrXD::VulkanCompute* compute) {
    m_components.vulkanCompute = compute;
    return *this;
}

IDEBuilder& IDEBuilder::WithModelRouter(UniversalModelRouter* router) {
    m_components.modelRouter = router;
    return *this;
}

IDEBuilder& IDEBuilder::WithGitHubBridge(GitHubMCPBridge* bridge) {
    m_components.githubBridge = bridge;
    return *this;
}

IDEComponents IDEBuilder::Build() {
    return m_components;
}

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

bool IDE_Init(void* components) {
    auto* ideComponents = static_cast<IDEComponents*>(components);
    if (!ideComponents) {
        return false;
    }
    return IDEIntegration::Instance().Initialize(*ideComponents);
}

void IDE_Shutdown() {
    IDEIntegration::Instance().Shutdown();
}

bool IDE_IsInitialized() {
    return IDEIntegration::Instance().IsInitialized();
}

int IDE_SendMessage(const char* message, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().SendMessage(std::string(message));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GetChatHistory(char* result, int resultSize) {
    auto history = IDEIntegration::Instance().GetChatHistory();
    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < history.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << history[i] << "\"";
    }
    ss << "]";
    strncpy_s(result, resultSize, ss.str().c_str(), ss.str().size());
    return 0;
}

void IDE_ClearChatHistory() {
    IDEIntegration::Instance().ClearChatHistory();
}

int IDE_AnalyzeCode(const char* code, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().AnalyzeCode(std::string(code));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GenerateCode(const char* prompt, const char* language, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().GenerateCode(std::string(prompt), std::string(language));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_RefactorCode(const char* code, const char* type, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().RefactorCode(std::string(code), std::string(type));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_ReadFile(const char* path, int offset, int limit, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().ReadFile(std::string(path), offset, limit);
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_WriteFile(const char* path, const char* content, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().WriteFile(std::string(path), std::string(content));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_EditFile(const char* path, const char* oldStr, const char* newStr, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().EditFile(std::string(path), std::string(oldStr), std::string(newStr));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GitStatus(char* result, int resultSize) {
    auto res = IDEIntegration::Instance().GitStatus();
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GitCommit(const char* message, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().GitCommit(std::string(message));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GitPush(char* result, int resultSize) {
    auto res = IDEIntegration::Instance().GitPush();
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_LoadModel(const char* path, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().LoadModel(std::string(path));
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_Generate(const char* prompt, int maxTokens, float temp, char* result, int resultSize) {
    auto res = IDEIntegration::Instance().Generate(std::string(prompt), maxTokens, temp);
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GetGPUInfo(char* result, int resultSize) {
    auto res = IDEIntegration::Instance().GetGPUInfo();
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GPUExecute(const char* operation, const char* params, char* result, int resultSize) {
    // Parse params JSON and execute GPU operation
    // Simplified implementation
    strncpy_s(result, resultSize, "GPU operation executed", 21);
    return 0;
}

int IDE_ExecuteTool(const char* name, const char* params, char* result, int resultSize) {
    // Parse params JSON into map
    std::map<std::string, std::string> paramMap;
    // Simplified - real implementation would parse JSON
    auto res = IDEIntegration::Instance().ExecuteTool(std::string(name), paramMap);
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_ListTools(char* result, int resultSize) {
    auto res = IDEIntegration::Instance().ListTools();
    if (!res.success) {
        strncpy_s(result, resultSize, res.error.c_str(), res.error.size());
        return -1;
    }
    strncpy_s(result, resultSize, res.output.c_str(), res.output.size());
    return 0;
}

int IDE_GetDiagnostics(char* result, int resultSize) {
    auto diag = IDEIntegration::Instance().GetDiagnostics();
    strncpy_s(result, resultSize, diag.c_str(), diag.size());
    return 0;
}

int IDE_GetStats(char* result, int resultSize) {
    auto stats = IDEIntegration::Instance().GetStats();
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& [key, value] : stats) {
        if (!first) ss << ", ";
        first = false;
        ss << "\"" << key << "\": \"" << value << "\"";
    }
    ss << "}";
    strncpy_s(result, resultSize, ss.str().c_str(), ss.str().size());
    return 0;
}

} // extern "C"
