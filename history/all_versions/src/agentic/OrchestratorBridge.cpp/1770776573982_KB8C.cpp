// =============================================================================
// OrchestratorBridge.cpp — Wiring Layer Implementation
// =============================================================================
#include "OrchestratorBridge.h"
#include <chrono>

using RawrXD::Agent::OrchestratorBridge;
using RawrXD::Agent::LLMChatRequest;
using RawrXD::Agent::LLMChatResponse;
using RawrXD::Agent::ToolExecResult;

namespace {

uint64_t NowMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()
        ).count()
    );
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

OrchestratorBridge& OrchestratorBridge::Instance() {
    static OrchestratorBridge instance;
    return instance;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

bool OrchestratorBridge::Initialize(const std::string& workingDir,
                                     const std::string& ollamaUrl)
{
    if (m_initialized) return true;

    m_workingDir = workingDir;

    // Configure Ollama
    m_ollamaConfig.host = "127.0.0.1";
    m_ollamaConfig.port = 11434;
    m_ollamaConfig.chat_model = "qwen2.5-coder:14b";
    m_ollamaConfig.fim_model = "qwen2.5-coder:7b";
    m_ollamaConfig.timeout_ms = 120000;
    m_ollamaConfig.temperature = 0.1f;  // Low temp for tool use
    m_ollamaConfig.num_ctx = 8192;

    // Parse URL if provided
    if (!ollamaUrl.empty()) {
        // Extract host:port from URL
        std::string url = ollamaUrl;
        if (url.find("http://") == 0) url = url.substr(7);
        if (url.find("https://") == 0) url = url.substr(8);
        auto colonPos = url.find(':');
        if (colonPos != std::string::npos) {
            m_ollamaConfig.host = url.substr(0, colonPos);
            try {
                m_ollamaConfig.port = static_cast<uint16_t>(
                    std::stoi(url.substr(colonPos + 1)));
            } catch (...) {}
        } else {
            m_ollamaConfig.host = url;
        }
    }

    // Create Ollama client
    m_ollamaClient = std::make_unique<AgentOllamaClient>(m_ollamaConfig);

    // Test connection
    if (!m_ollamaClient->TestConnection()) {
        // Non-fatal: agent loop will still work but will error on first call
    }

    // Wire tool handlers
    WireToolHandlers();

    // Wire tool schemas
    WireToolSchemas();

    // Configure BoundedAgentLoop
    AgentLoopConfig loopConfig;
    loopConfig.maxSteps = 8;
    loopConfig.maxTokensPerRequest = 8192;
    loopConfig.model = m_ollamaConfig.chat_model;
    loopConfig.ollamaBaseUrl = ollamaUrl.empty() ? "http://localhost:11434" : ollamaUrl;
    loopConfig.workingDirectory = workingDir;
    loopConfig.autoVerify = true;
    m_agentLoop.Configure(loopConfig);

    // Wire LLM backend adapter into BoundedAgentLoop
    m_agentLoop.SetLLMBackend(BuildLLMAdapter());

    // Configure orchestrator
    OrchestratorConfig orchConfig;
    orchConfig.max_tool_rounds = 15;
    orchConfig.working_directory = workingDir;
    orchConfig.auto_build_after_edit = true;
    orchConfig.auto_diagnostics = true;
    m_orchestrator.SetConfig(orchConfig);
    m_orchestrator.SetOllamaConfig(m_ollamaConfig);

    // Configure FIM builder
    m_fimBuilder.SetFormat(FIMFormat::Qwen);
    m_fimBuilder.SetMaxContextTokens(4096);
    m_fimBuilder.SetPrefixRatio(0.75f);

    // Set guardrails for tool handlers
    ToolGuardrails guards;
    guards.allowedRoots.push_back(workingDir);
    guards.commandTimeoutMs = 30000;
    guards.requireBackupOnWrite = true;
    AgentToolHandlers::SetGuardrails(guards);

    m_initialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// LLM Backend Adapter
// ---------------------------------------------------------------------------
// Adapts AgentOllamaClient (WinHTTP streaming) into the LLMChatFunction
// signature expected by BoundedAgentLoop.

LLMChatFunction OrchestratorBridge::BuildLLMAdapter() {
    // Capture the client pointer — safe because OrchestratorBridge is a singleton
    // and outlives the agent loop.
    auto* client = m_ollamaClient.get();
    auto& registry = m_xMacroRegistry;

    return [client, &registry](const LLMChatRequest& request) -> LLMChatResponse {
        LLMChatResponse response;

        // Convert LLMChatRequest messages to ChatMessage format
        std::vector<ChatMessage> messages;
        messages.reserve(request.messages.size());

        for (const auto& msgJson : request.messages) {
            ChatMessage msg;
            msg.role = msgJson.value("role", "user");
            msg.content = msgJson.value("content", "");
            if (msgJson.contains("tool_call_id")) {
                msg.tool_call_id = msgJson["tool_call_id"].get<std::string>();
            }
            if (msgJson.contains("tool_calls")) {
                msg.tool_calls = msgJson["tool_calls"];
            }
            messages.push_back(std::move(msg));
        }

        // Use X-Macro registry schemas for tool definitions
        nlohmann::json tools = request.tools.empty() ? registry.GetToolSchemas() : request.tools;

        // Call Ollama via the new client
        auto start = std::chrono::high_resolution_clock::now();
        InferenceResult result = client->ChatSync(messages, tools);
        auto end = std::chrono::high_resolution_clock::now();

        if (!result.success) {
            response.success = false;
            response.error = result.error_message;
            return response;
        }

        response.success = true;
        response.content = result.response;
        response.promptTokens = static_cast<int>(result.prompt_tokens);
        response.completionTokens = static_cast<int>(result.completion_tokens);

        // Map tool calls
        if (result.has_tool_calls && !result.tool_calls.empty()) {
            response.hasToolCall = true;
            // BoundedAgentLoop processes one tool call at a time
            auto& [name, args] = result.tool_calls[0];
            response.toolName = name;
            response.toolArgs = args;
            response.toolCallId = "call_0";
        }

        return response;
    };
}

// ---------------------------------------------------------------------------
// Tool Schema Wiring
// ---------------------------------------------------------------------------
// Generates OpenAI function-calling schemas from the X-Macro registry
// and feeds them into the BoundedAgentLoop's tool schema slot.

void OrchestratorBridge::WireToolSchemas() {
    // The X-Macro AgentToolRegistry is the source of truth for tool schemas.
    // Inject X-Macro schemas into the BoundedAgentLoop's tool schema slot
    // so both systems use identical definitions.
    json xMacroSchemas = m_xMacroRegistry.GetToolSchemas();

    // Merge AgentToolHandlers schemas (which include detailed param schemas)
    // with X-Macro schemas. X-Macro definitions take priority for names/descriptions.
    json handlerSchemas = AgentToolHandlers::GetAllSchemas();

    // Build unified tool list: start with X-Macro, overlay any handler-specific
    // parameter refinements (e.g., more detailed descriptions, enum constraints).
    json unifiedTools = json::array();
    std::unordered_map<std::string, json> handlerMap;

    // Index handler schemas by name
    for (const auto& tool : handlerSchemas) {
        if (tool.contains("function") && tool["function"].contains("name")) {
            handlerMap[tool["function"]["name"].get<std::string>()] = tool;
        }
    }

    // Iterate X-Macro schemas and merge handler parameter details
    for (const auto& xTool : xMacroSchemas) {
        if (!xTool.contains("function") || !xTool["function"].contains("name")) {
            unifiedTools.push_back(xTool);
            continue;
        }

        std::string name = xTool["function"]["name"].get<std::string>();
        auto it = handlerMap.find(name);
        if (it != handlerMap.end()) {
            // Merge: use X-Macro description but handler's detailed parameter schema
            json merged = xTool;
            if (it->second["function"].contains("parameters")) {
                merged["function"]["parameters"] = it->second["function"]["parameters"];
            }
            unifiedTools.push_back(merged);
            handlerMap.erase(it);
        } else {
            unifiedTools.push_back(xTool);
        }
    }

    // Add any handler-only tools not in X-Macro (shouldn't normally happen)
    for (const auto& [name, tool] : handlerMap) {
        unifiedTools.push_back(tool);
    }

    // Store unified schemas for the LLM adapter to reference
    m_unifiedSchemas = unifiedTools;
}

// ---------------------------------------------------------------------------
// Tool Handler Wiring
// ---------------------------------------------------------------------------
// Maps the X-Macro AgentToolRegistry dispatch to the existing
// AgentToolHandlers implementations.

void OrchestratorBridge::WireToolHandlers() {
    auto& registry = m_xMacroRegistry;

    // Adapt AgentToolHandlers (ToolCallResult) to AgentToolRegistry (ToolExecResult)
    // read_file
    registry.RegisterHandler("read_file", [](const nlohmann::json& args) -> ToolExecResult {
        auto tcr = AgentToolHandlers::ToolReadFile(args);
        if (tcr.outcome == ToolOutcome::Success)
            return ToolExecResult::ok(tcr.output, static_cast<double>(tcr.durationMs));
        return ToolExecResult::error(tcr.error.empty() ? tcr.output : tcr.error);
    });

    // write_file
    registry.RegisterHandler("write_file", [](const nlohmann::json& args) -> ToolExecResult {
        auto tcr = AgentToolHandlers::WriteFile(args);
        if (tcr.outcome == ToolOutcome::Success)
            return ToolExecResult::ok(tcr.output, static_cast<double>(tcr.durationMs));
        return ToolExecResult::error(tcr.error.empty() ? tcr.output : tcr.error);
    });

    // replace_in_file
    registry.RegisterHandler("replace_in_file", [](const nlohmann::json& args) -> ToolExecResult {
        auto tcr = AgentToolHandlers::ReplaceInFile(args);
        if (tcr.outcome == ToolOutcome::Success)
            return ToolExecResult::ok(tcr.output, static_cast<double>(tcr.durationMs));
        return ToolExecResult::error(tcr.error.empty() ? tcr.output : tcr.error);
    });

    // list_directory → ListDir
    registry.RegisterHandler("list_directory", [](const nlohmann::json& args) -> ToolExecResult {
        // Remap "path" to the format AgentToolHandlers expects
        auto tcr = AgentToolHandlers::ListDir(args);
        if (tcr.outcome == ToolOutcome::Success)
            return ToolExecResult::ok(tcr.output, static_cast<double>(tcr.durationMs));
        return ToolExecResult::error(tcr.error.empty() ? tcr.output : tcr.error);
    });

    // execute_command
    registry.RegisterHandler("execute_command", [](const nlohmann::json& args) -> ToolExecResult {
        auto tcr = AgentToolHandlers::ExecuteCommand(args);
        ToolExecResult r;
        r.success = (tcr.outcome == ToolOutcome::Success);
        r.output = tcr.output;
        r.elapsed_ms = static_cast<double>(tcr.durationMs);
        r.exit_code = r.success ? 0 : -1;
        return r;
    });

    // search_code
    registry.RegisterHandler("search_code", [](const nlohmann::json& args) -> ToolExecResult {
        auto tcr = AgentToolHandlers::SearchCode(args);
        if (tcr.outcome == ToolOutcome::Success)
            return ToolExecResult::ok(tcr.output, static_cast<double>(tcr.durationMs));
        return ToolExecResult::error(tcr.error.empty() ? tcr.output : tcr.error);
    });

    // get_diagnostics
    registry.RegisterHandler("get_diagnostics", [](const nlohmann::json& args) -> ToolExecResult {
        auto tcr = AgentToolHandlers::GetDiagnostics(args);
        if (tcr.outcome == ToolOutcome::Success)
            return ToolExecResult::ok(tcr.output, static_cast<double>(tcr.durationMs));
        return ToolExecResult::error(tcr.error.empty() ? tcr.output : tcr.error);
    });

    // get_coverage, run_build, apply_hotpatch keep their default handlers from ToolRegistry.cpp
}

// ---------------------------------------------------------------------------
// Agent Execution
// ---------------------------------------------------------------------------

std::string OrchestratorBridge::RunAgent(const std::string& userPrompt) {
    if (!m_initialized) return "[ERROR] OrchestratorBridge not initialized";
    return m_agentLoop.Execute(userPrompt);
}

void OrchestratorBridge::RunAgentAsync(const std::string& userPrompt) {
    if (!m_initialized) return;
    m_agentLoop.ExecuteAsync(userPrompt);
}

// ---------------------------------------------------------------------------
// Ghost Text / FIM
// ---------------------------------------------------------------------------

Prediction::PredictionResult OrchestratorBridge::RequestGhostText(
    const Prediction::PredictionContext& ctx)
{
    if (!m_initialized || !m_ollamaClient) {
        return Prediction::PredictionResult::Error("OrchestratorBridge not initialized");
    }

    // Build FIM prompt from prediction context
    EditorContext edCtx;
    edCtx.filename = ctx.filePath;
    edCtx.filepath = ctx.filePath;
    edCtx.language = ctx.language;
    edCtx.cursor_line = ctx.cursorLine;
    edCtx.cursor_column = ctx.cursorColumn;
    // Reconstruct full content from prefix + suffix
    edCtx.full_content = ctx.prefix + ctx.suffix;

    FIMBuildResult buildResult = m_fimBuilder.Build(edCtx);
    if (!buildResult.success) {
        return Prediction::PredictionResult::Error("FIM build failed: " + buildResult.error);
    }

    auto start = std::chrono::high_resolution_clock::now();
    InferenceResult result = m_ollamaClient->FIMSync(
        buildResult.prompt.prefix,
        buildResult.prompt.suffix,
        buildResult.prompt.filename);
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (!result.success) {
        return Prediction::PredictionResult::Error(result.error_message);
    }

    return Prediction::PredictionResult::Ok(
        result.response,
        static_cast<int>(result.completion_tokens),
        ms);
}

void OrchestratorBridge::RequestGhostTextStream(
    const Prediction::PredictionContext& ctx,
    Prediction::StreamTokenCallback onToken)
{
    if (!m_initialized || !m_ollamaClient) return;

    EditorContext edCtx;
    edCtx.filename = ctx.filePath;
    edCtx.filepath = ctx.filePath;
    edCtx.language = ctx.language;
    edCtx.cursor_line = ctx.cursorLine;
    edCtx.cursor_column = ctx.cursorColumn;
    edCtx.full_content = ctx.prefix + ctx.suffix;

    FIMBuildResult buildResult = m_fimBuilder.Build(edCtx);
    if (!buildResult.success) return;

    m_ollamaClient->FIMStream(
        buildResult.prompt.prefix,
        buildResult.prompt.suffix,
        buildResult.prompt.filename,
        [onToken](const std::string& token) {
            if (onToken) onToken(token, false);
        },
        [onToken](const std::string&, uint64_t, uint64_t, double) {
            if (onToken) onToken("", true);
        },
        [](const std::string&) {}
    );
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void OrchestratorBridge::SetModel(const std::string& model) {
    m_ollamaConfig.chat_model = model;
    if (m_ollamaClient) m_ollamaClient->SetConfig(m_ollamaConfig);
}

void OrchestratorBridge::SetFIMModel(const std::string& model) {
    m_ollamaConfig.fim_model = model;
    if (m_ollamaClient) m_ollamaClient->SetConfig(m_ollamaConfig);
}

void OrchestratorBridge::SetMaxSteps(int steps) {
    // Merge into existing config to preserve all other fields
    AgentLoopConfig config = m_agentLoop.GetConfig();
    config.maxSteps = steps;
    m_agentLoop.Configure(config);
}

void OrchestratorBridge::SetWorkingDirectory(const std::string& dir) {
    m_workingDir = dir;
    ToolGuardrails guards = AgentToolHandlers::GetGuardrails();
    guards.allowedRoots.clear();
    guards.allowedRoots.push_back(dir);
    AgentToolHandlers::SetGuardrails(guards);
}
