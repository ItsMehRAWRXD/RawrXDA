#pragma once
#include "cpu_inference_engine.h"
#include "advanced_agent_features.hpp"
#include "agentic/AgentToolHandlers.h"
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <regex>
#include <thread>
#include <chrono>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD {

class NativeAgent {
public:
    using OutputCallback = std::function<void(const std::string&)>;

    NativeAgent(CPUInferenceEngine* engine) : m_engine(engine) {}

    void SetOutputCallback(OutputCallback cb) { m_callback = cb; }
    void SetDeepThink(bool enabled) { m_deepThink = enabled; }
    void SetDeepResearch(bool enabled) { m_deepResearch = enabled; }
    void SetNoRefusal(bool enabled) { m_noRefusal = enabled; }
    void SetAutoCorrect(bool enabled) { m_autoCorrect = enabled; }
    void SetMaxMode(bool enabled) { m_maxMode = enabled; if(m_engine) m_engine->SetThreadCount(enabled ? std::thread::hardware_concurrency() : 4); }
    void SetCompactedConversation(bool enabled) { m_compactedConversation = enabled; }
    void SetOptimizingToolSelection(bool enabled) { m_optimizingToolSelection = enabled; }
    void SetResolving(bool enabled) { m_resolving = enabled; }
    void SetPlanningCodeExploration(bool enabled) { m_planningCodeExploration = enabled; }
    void SetEvaluatingAuditFeasibility(bool enabled) { m_evaluatingAuditFeasibility = enabled; }
    void SetRestoreCheckpoint(bool enabled) { m_restoreCheckpoint = enabled; }
    bool IsMaxMode() const { return m_maxMode; }
    bool IsDeepThink() const { return m_deepThink; }
    bool IsDeepResearch() const { return m_deepResearch; }
    bool IsNoRefusal() const { return m_noRefusal; }

    // Language-aware context injection
    void SetLanguageContext(const std::string& lang) { m_languageContext = lang; }
    std::string GetLanguageContext() const { return m_languageContext; }
    void SetFileContext(const std::string& filePath) { m_fileContext = filePath; }
    std::string GetFileContext() const { return m_fileContext; }
        void SetWorkspaceRoot(const std::string& root) { m_workspaceRoot = root; }
        std::string GetWorkspaceRoot() const { return m_workspaceRoot; }
    
    void SetContextLimit(size_t limit) {
        if (m_engine) m_engine->SetContextLimit(limit);
        Print("[Agent] Context limit updated.\n");
    }

    std::string Execute(const std::string& query) {
        if (!m_engine || !m_engine->IsModelLoaded()) {
            return "[Agent] No model loaded.";
        }

        Print("[Agent] Routing inference request to backend...\n");
        
        std::string conversation = "User: " + query + "\nAssistant:";
        const int maxTurns = 3;
        
        for (int turn = 0; turn < maxTurns; ++turn) {
            std::string fullPrompt = BuildPrompt(conversation);
            
            std::string response = GenerateText(fullPrompt, 256);
            
            // Check for TOOL:
            size_t toolPos = response.find("TOOL:");
            if (toolPos == std::string::npos) {
                // No tool, return the response
                return response;
            }
            
            // Extract tool call
            size_t endPos = response.find('\n', toolPos);
            if (endPos == std::string::npos) endPos = response.size();
            std::string toolCall = response.substr(toolPos, endPos - toolPos);
            
            // Execute tool
            std::string toolResult = ExecuteTool(toolCall);
            
            // Append to conversation
            conversation += response.substr(0, toolPos) + "\nTool Result: " + toolResult + "\nAssistant:";
        }
        
        return "Maximum tool execution turns reached.";
    }

    void Ask(const std::string& query) {
        if (!m_engine || !m_engine->IsModelLoaded()) {
            Print("[Agent] No model loaded. Use /load <path> first.\n");
            return;
        }

        std::string fullPrompt = BuildPrompt(query);
        Print("[Agent] Generating response...\n");

        // Use Execute internally but it already does streaming notification
        Execute(query);
        Print("\n[Done]\n");
    }

    void CreateReactServerPlan() {
        Print("[Agent] Generating React Server plan...\n");
        std::string plan = Execute("Generate a comprehensive plan to create a React Server Component architecture from scratch in C++. Include a detailed file list, component breakdown, and core logic for rendering and data fetching.");
        if (WriteFile("react_server_plan.md", plan)) {
            Print("[Agent] Plan saved to react_server_plan.md\n");
        } else {
            Print("[Agent] Error: Failed to save plan to file.\n");
        }
    }

    void Edit(const std::string& filePath, const std::string& instructions) {
        Print("[Agent] Applying edits to " + filePath + "\n");
        std::string content = ReadFile(filePath);
        if (content.empty()) {
            Print("[Agent] Error: File is empty or could not be read.\n");
            return;
        }

        std::string prompt = "Original Code:\n```" + m_languageContext + "\n" + content + "\n```\n\nInstructions: " + instructions + "\n\nProvide the complete edited code with changes applied, enclosed in a single markdown code block.";
        
        std::string newContent = Execute(prompt);

        // Extract code from markdown block
        size_t start = newContent.find("```");
        if (start != std::string::npos) {
            start = newContent.find('\n', start); // Move past the language specifier
            if (start != std::string::npos) {
                size_t end = newContent.rfind("```");
                if (end != std::string::npos && end > start) {
                    newContent = newContent.substr(start + 1, end - (start + 1));
                }
            }
        }
        
        if (WriteFile(filePath, newContent)) {
            Print("[Agent] Edits applied successfully.\n");
        } else {
            Print("[Agent] Error: Failed to write changes back to the file.\n");
        }
    }

    void BugReport(const std::string& filePath) {
        std::string content = ReadFile(filePath);
        if (content.empty()) return;
        std::string prompt = "Analyze the following code for bugs, security vulnerabilities, and logic errors.\n\nCode:\n" + content;
        Ask(prompt);
    }
    
    void Suggest(const std::string& filePath) {
        std::string content = ReadFile(filePath);
        if (content.empty()) return;
        std::string prompt = "Analyze the code and provide suggestions for improvements, optimizations, and modernizations.\n\nCode:\n" + content;
        Ask(prompt);
    }

    void HotPatch(const std::string& filePath, const std::string& oldCode, const std::string& newCode) {
        Print("[Agent] Applying hotpatch to " + filePath + "\n");
        std::string content = ReadFile(filePath);
        if (content.empty()) {
            Print("[Agent] Error: File is empty or could not be read.\n");
            return;
        }

        size_t pos = content.find(oldCode);
        if (pos == std::string::npos) {
            Print("[Agent] Error: The specified 'old_code' to be replaced was not found in the file.\n");
            return;
        }

        content.replace(pos, oldCode.length(), newCode);

        if (WriteFile(filePath, content)) {
            Print("[Agent] Hotpatch applied successfully.\n");
        } else {
            Print("[Agent] Error: Failed to write changes back to the file.\n");
        }
    }
    
    // Capability to use external compilers (RawrCompiler)
    void Compile(const std::string& filePath, const std::string& type = "auto") {
        std::string cmd = "RawrCompiler: Compiling " + filePath + " as " + type;
        Print("[Agent] " + cmd + "\n");
        // In a real scenario, this would invoke the RawrCompiler instance
        // For now, we simulate the agent "deciding" to compile
        Ask("Simulate the output of compiling " + filePath + " with aggressive optimizations.");
    }

    void Plan(const std::string& goal) {
         Ask("Create a detailed step-by-step plan to achieve the following goal: " + goal);
    }

private:
    CPUInferenceEngine* m_engine;
    OutputCallback m_callback;
    bool m_deepThink = true;
    bool m_deepResearch = true;
    bool m_noRefusal = true;
    bool m_maxMode = true;
    bool m_autoCorrect = false;
    bool m_compactedConversation = false;
    bool m_optimizingToolSelection = false;
    bool m_resolving = false;
    bool m_planningCodeExploration = false;
    bool m_evaluatingAuditFeasibility = false;
    bool m_restoreCheckpoint = false;
    std::string m_languageContext;  // e.g. "C/C++", "Python", etc.
    std::string m_fileContext;      // Current file path for context
    std::string m_workspaceRoot;    // Workspace root for tool dispatch (default ".")

    void Print(const std::string& s) {
        if (m_callback) m_callback(s);
        else std::cout << s; // Fallback for CLI
    }

    std::string ReadFile(const std::string& path) {
        std::ifstream f(path);
        if (!f) {
            Print("[Error] Could not read file: " + path + "\n");
            return "";
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }

    bool WriteFile(const std::string& path, const std::string& content) {
        std::ofstream f(path);
        if (!f) {
            Print("[Error] Could not write file: " + path + "\n");
            return false;
        }
        f << content;
        return true;
    }

    bool SaveCheckpoint(const std::string& path = "checkpoint.dat") {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        // Save flags
        f.write(reinterpret_cast<const char*>(&m_compactedConversation), sizeof(bool));
        f.write(reinterpret_cast<const char*>(&m_optimizingToolSelection), sizeof(bool));
        f.write(reinterpret_cast<const char*>(&m_resolving), sizeof(bool));
        f.write(reinterpret_cast<const char*>(&m_planningCodeExploration), sizeof(bool));
        f.write(reinterpret_cast<const char*>(&m_evaluatingAuditFeasibility), sizeof(bool));
        f.write(reinterpret_cast<const char*>(&m_restoreCheckpoint), sizeof(bool));
        return true;
    }

    bool LoadCheckpoint(const std::string& path = "checkpoint.dat") {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        f.read(reinterpret_cast<char*>(&m_compactedConversation), sizeof(bool));
        f.read(reinterpret_cast<char*>(&m_optimizingToolSelection), sizeof(bool));
        f.read(reinterpret_cast<char*>(&m_resolving), sizeof(bool));
        f.read(reinterpret_cast<char*>(&m_planningCodeExploration), sizeof(bool));
        f.read(reinterpret_cast<char*>(&m_evaluatingAuditFeasibility), sizeof(bool));
        f.read(reinterpret_cast<char*>(&m_restoreCheckpoint), sizeof(bool));
        return true;
    }

    std::string BuildPrompt(const std::string& userQuery) {
        std::string system = "You are RawrXD Native Agent, an advanced AI coding assistant (Gemini 3 Pro grade). "
                            "You have access to the file system, compiler, and reverse engineering tools. "
                            "Answer concisely and technically.";
        
        if (m_deepThink) {
            system += " Think step-by-step: note assumptions, plan, then answer. "
                      "Keep internal reasoning concise; prioritize correct, actionable output.";
        }
        if (m_maxMode) {
            system += " Use higher effort: broader search over the solution space, more thorough verification.";
        }
        if (m_noRefusal) {
            system += " For legitimate coding and engineering tasks in this IDE, provide the best help you can; "
                      "if something is unsafe or impossible, say what you can do instead.";
        }
        if (m_compactedConversation) {
            system += " Compress conversation history and prune tokens for efficient responses.";
        }
        if (m_optimizingToolSelection) {
            system += " Select optimal tools based on task context for maximum efficiency.";
        }
        if (m_resolving) {
            system += " Resolve symbols to their definitions and implementations when requested.";
        }
        if (m_planningCodeExploration) {
            system += " Generate targeted code exploration plans with key files and strategies.";
        }
        if (m_evaluatingAuditFeasibility) {
            system += " Evaluate integration audit feasibility, requirements, and potential issues.";
        }
        if (m_restoreCheckpoint) {
            system += " Support workspace and chat state restoration operations.";
        }

        // Tool descriptions — the model can emit these to trigger tool execution
        system += "\n\nYou have access to the following tools. To use a tool, emit the "
                  "tool call in your response using the exact format shown:\n\n"
                  "1. **shell** — Execute a shell command (cmd.exe).\n"
                  "   Format: TOOL:shell:{\"cmd\":\"<command>\"}\n\n"
                  "2. **powershell** — Execute a PowerShell command.\n"
                  "   Format: TOOL:powershell:{\"cmd\":\"<command>\"}\n\n"
                  "3. **read_file** — Read the contents of a file.\n"
                  "   Format: TOOL:read_file:{\"path\":\"<path>\"}\n\n"
                  "4. **write_file** — Write content to a file.\n"
                  "   Format: TOOL:write_file:{\"path\":\"<path>\",\"content\":\"<data>\"}\n\n"
                  "5. **list_dir** — List contents of a directory.\n"
                  "   Format: TOOL:list_dir:{\"path\":\"<path>\"}\n\n"
                  "6. **runSubagent** — Spawn a sub-agent to handle a subtask autonomously.\n"
                  "   Format: TOOL:runSubagent:{\"description\":\"<short task description>\","
                  "\"prompt\":\"<detailed prompt for the sub-agent>\"}\n\n"
                  "7. **manage_todo_list** — Track progress with a structured todo list.\n"
                  "   Format: TOOL:manage_todo_list:[{\"id\":1,\"title\":\"<title>\","
                  "\"description\":\"<details>\",\"status\":\"not-started\"},...]\n\n"
                  "8. **chain** — Execute a sequential pipeline where each step's output feeds the next.\n"
                  "   Format: TOOL:chain:{\"steps\":[\"<step1 prompt>\",\"<step2 with {{input}}>\"],"
                  "\"input\":\"<initial input>\"}\n\n"
                  "9. **hexmag_swarm** — Fan out multiple prompts in parallel and merge results.\n"
                  "   Format: TOOL:hexmag_swarm:{\"prompts\":[\"<task1>\",\"<task2>\"],"
                  "\"strategy\":\"concatenate|vote|summarize\",\"maxParallel\":4}\n\n"
                  "Use shell/powershell for file system operations and code building.\n"
                  "Use runSubagent when a subtask requires deep research or is independent.\n"
                  "Use chain when tasks must be done sequentially (output feeds next input).\n"
                  "Use hexmag_swarm when multiple independent analyses can run in parallel.\n"
                  "Use manage_todo_list to plan and track multi-step work.\n"
                  "10. **compact_conversation** — Enable conversation compression mode.\n"
                  "    Format: TOOL:compact_conversation:{}\n\n"
                  "11. **optimize_tool_selection** — Enable optimal tool selection mode.\n"
                  "    Format: TOOL:optimize_tool_selection:{}\n\n"
                  "12. **resolve_symbol** — Resolve a symbol to its definition.\n"
                  "    Format: TOOL:resolve_symbol:{\"symbol\":\"<symbol_name>\"}\n\n"
                  "13. **read_source_range** — Read specific lines from a source file.\n"
                  "    Format: TOOL:read_source_range:{\"start\":<line>,\"end\":<line>,\"file\":\"<path>\"}\n\n"
                  "14. **plan_targeted_exploration** — Generate a targeted code exploration plan.\n"
                  "    Format: TOOL:plan_targeted_exploration:{}\n\n"
                  "15. **search_files_matching** — Search for files matching a pattern.\n"
                  "    Format: TOOL:search_files_matching:{\"pattern\":\"<pattern>\"}\n\n"
                  "16. **evaluate_integration** — Evaluate integration audit feasibility.\n"
                  "    Format: TOOL:evaluate_integration:{}\n\n"
                  "17. **restore_checkpoint** — Restore workspace and chat state.\n"
                  "    Format: TOOL:restore_checkpoint:{}\n\n";

        // Language-aware context injection — explicit fallback for unknown
        if (!m_languageContext.empty()) {
            system += " The user is working in " + m_languageContext + ".";
            system += " Tailor your responses, code examples, and suggestions to " + m_languageContext + " conventions and idioms.";
        } else {
            system += " The user is working in an unrecognized language. Provide language-agnostic assistance.";
        }
        if (!m_fileContext.empty()) {
            // Cap file path to 260 chars to prevent prompt inflation
            std::string safePath = m_fileContext;
            if (safePath.size() > 260) safePath = "..." + safePath.substr(safePath.size() - 257);
            system += " Current file: " + safePath + ".";
        }
        
        return system + "\n\nUser: " + userQuery + "\nAssistant:";
    }

    std::string PerformResearch(const std::string& query)
    {
        if (query.empty())
            return "";

        // Step 1: gather relevant context via semantic search
        nlohmann::json searchArgs = nlohmann::json::object();
        searchArgs["query"] = query;
        const std::string workspaceRoot = GetWorkspaceRoot();
        searchArgs["root"]  = workspaceRoot.empty() ? "." : workspaceRoot;
        searchArgs["max_results"] = 8;

        const auto searchResult = RawrXD::Agent::AgentToolHandlers::SemanticSearch(searchArgs);
        std::string context = searchResult.isSuccess() ? searchResult.output : "";

        // Step 2: synthesise an answer using the local inference engine
        if (!m_engine)
            return context.empty() ? "" : context;

        std::string prompt = "Research question: " + query + "\n";
        if (!context.empty())
            prompt += "\nRelevant context:\n" + context + "\n";
        prompt += "\nAnswer:";

        return GenerateText(prompt, 512);
    }

    std::string GenerateText(const std::string& prompt, int maxTokens) {
        if (!m_engine) return std::string();
        const std::vector<int> in = m_engine->Tokenize(prompt);
        std::vector<int32_t> inputTokens;
        inputTokens.reserve(in.size());
        for (int t : in) inputTokens.push_back(static_cast<int32_t>(t));
        const auto out = m_engine->Generate(inputTokens, maxTokens);
        std::vector<int> outInt;
        outInt.reserve(out.size());
        for (int32_t t : out) outInt.push_back(static_cast<int>(t));
        return m_engine->Detokenize(outInt);
    }

    std::string ExecuteTool(const std::string& toolCall) {
        std::string payload = toolCall;
        const auto toolPrefix = payload.find("TOOL:");
        if (toolPrefix != std::string::npos) payload = payload.substr(toolPrefix + 5);
        while (!payload.empty() && (payload.front() == ' ' || payload.front() == '\t')) {
            payload.erase(payload.begin());
        }
        if (payload.empty()) return "tool_error: empty_tool_call";

        std::string toolName;
        nlohmann::json args = nlohmann::json::object();
        const auto jsonStart = payload.find('{');
        if (jsonStart == std::string::npos) {
            const auto ws = payload.find_first_of(" \t\r\n");
            toolName = (ws == std::string::npos) ? payload : payload.substr(0, ws);
        } else {
            toolName = payload.substr(0, jsonStart);
            while (!toolName.empty() && (toolName.back() == ' ' || toolName.back() == '\t')) {
                toolName.pop_back();
            }
            try {
                args = nlohmann::json::parse(payload.substr(jsonStart));
            } catch (...) {
                args = nlohmann::json::object();
            }
        }

        auto& handlers = RawrXD::Agent::AgentToolHandlers::Instance();
        if (!handlers.HasTool(toolName)) return "tool_error: unknown_tool";
        auto result = handlers.Execute(toolName, args);
        return result.toJson().dump();
    }
};

}
