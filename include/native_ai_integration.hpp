#pragma once
#ifndef RAWRXD_NATIVE_AI_INTEGRATION_HPP
#define RAWRXD_NATIVE_AI_INTEGRATION_HPP

/**
 * RawrXD Native AI Integration Layer
 * 
 * Unified integration of Native AI IDE server, LLM bridge, agent tools,
 * semantic search, and code completion for both CLI and GUI modes.
 * 
 * Zero external dependencies - pure Win32/WinSock2.
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdarg>
#include <chrono>
#include <algorithm>
#include <queue>
#include <condition_variable>

namespace rawrxd {

// ============================================================================
// Minimal JSON Parser/Serializer (Zero Dependencies)
// ============================================================================
struct Json {
    enum Type { NUL, STR, NUM, BOOL, OBJ, ARR } type = NUL;
    std::string s;
    double n = 0;
    bool b = false;
    std::map<std::string, Json> o;
    std::vector<Json> a;

    Json() = default;
    Json(std::string v) : type(STR), s(std::move(v)) {}
    Json(const char* v) : type(STR), s(v) {}
    Json(double v) : type(NUM), n(v) {}
    Json(int v) : type(NUM), n(v) {}
    Json(bool v) : type(BOOL), b(v) {}
    Json(std::initializer_list<std::pair<const char*, Json>> list);

    bool is(Type t) const { return type == t; }
    const std::string& as_s() const { return s; }
    double as_n() const { return n; }
    bool as_b() const { return b; }
    Json& operator[](const std::string& k);
    const Json& operator[](const std::string& k) const;
    Json& operator[](size_t i);
    bool has(const std::string& k) const;
    std::string dump(int indent = 0) const;
    static Json parse(const std::string& text);

private:
    static Json parse_val(const char*& p);
    static std::string parse_str(const char*& p);
    static double parse_num(const char*& p);
    static void skip(const char*& p);
};

// ============================================================================
// LLM Message Types
// ============================================================================
struct LLMMessage {
    std::string role;    // "system", "user", "assistant"
    std::string content;
};

struct LLMRequest {
    std::vector<LLMMessage> messages;
    std::string model = "local";
    double temperature = 0.7;
    int maxTokens = 2048;
    bool stream = true;
    std::map<std::string, std::string> metadata;
};

struct LLMResponse {
    std::string content;
    bool done = false;
    std::string error;
    int tokensGenerated = 0;
    double latencyMs = 0;
};

using StreamCallback = std::function<void(const std::string& chunk, bool done)>;

// ============================================================================
// Tool Result and Registry
// ============================================================================
struct ToolResult {
    bool success = false;
    std::string output;
    std::string error;
    std::map<std::string, std::string> metadata;
};

using ToolFn = std::function<ToolResult(const std::map<std::string, std::string>&)>;

struct Tool {
    std::string name;
    std::string description;
    std::string category;
    std::vector<std::string> parameters;
    ToolFn execute;
    bool dangerous = false;
};

// ============================================================================
// Completion Types
// ============================================================================
struct CompletionItem {
    std::string label;
    std::string kind;        // "function", "variable", "class", "keyword", "snippet"
    std::string detail;
    std::string documentation;
    std::string insertText;
    int sortText = 0;
};

// ============================================================================
// Search Result
// ============================================================================
struct SearchResult {
    std::string filePath;
    int line;
    int column;
    std::string text;
    std::string context;
    double score;
};

// ============================================================================
// AI Integration Configuration
// ============================================================================
struct AIConfig {
    std::string llmEndpoint = "127.0.0.1";
    int llmPort = 11434;
    int serverPort = 3001;
    bool enableServer = true;
    bool enableTools = true;
    bool enableSearch = true;
    bool enableCompletion = true;
    std::string defaultModel = "codellama";
    int maxTokens = 4096;
    double temperature = 0.7;
    std::string workspacePath = ".";
};

// ============================================================================
// LLM Bridge - Ollama-compatible HTTP client
// ============================================================================
class LLMBridge {
    std::string endpoint_;
    int port_;
    std::atomic<bool> running_{false};
    mutable std::mutex mtx_;

    std::string httpPost(const std::string& path, const std::string& body);
    std::string httpGet(const std::string& path);

public:
    LLMBridge(const std::string& endpoint = "127.0.0.1", int port = 11434)
        : endpoint_(endpoint), port_(port) {}

    bool isAvailable();
    std::vector<std::string> listModels();
    LLMResponse generate(const LLMRequest& req, StreamCallback callback = nullptr);
    LLMResponse chat(const std::vector<LLMMessage>& messages, const std::string& model = "", StreamCallback cb = nullptr);
    bool loadModel(const std::string& model);
    bool unloadModel(const std::string& model);
};

// ============================================================================
// Tool Registry
// ============================================================================
class ToolRegistry {
    std::map<std::string, Tool> tools_;
    mutable std::mutex mtx_;

public:
    void registerTool(const Tool& tool);
    void registerBuiltin();
    ToolResult execute(const std::string& name, const std::map<std::string, std::string>& args) const;
    std::vector<Tool> listTools() const;
    bool hasTool(const std::string& name) const;
};

// ============================================================================
// Semantic Search Engine
// ============================================================================
class SemanticSearch {
    std::string workspacePath_;
    std::map<std::string, std::vector<std::string>> symbolIndex_;
    mutable std::mutex mtx_;

public:
    void setWorkspace(const std::string& path);
    void indexFile(const std::string& path);
    void indexWorkspace();
    std::vector<SearchResult> search(const std::string& query, int maxResults = 50);
    std::vector<SearchResult> searchSymbols(const std::string& symbol, int maxResults = 20);
};

// ============================================================================
// Code Completion Engine
// ============================================================================
class CodeCompleter {
    std::map<std::string, std::vector<std::string>> languageKeywords_;
    std::map<std::string, std::vector<CompletionItem>> snippets_;
    mutable std::mutex mtx_;

    void initLanguagePatterns();

public:
    CodeCompleter();
    std::vector<CompletionItem> complete(const std::string& code, int line, int column, const std::string& language = "");
    void addSnippet(const std::string& language, const CompletionItem& snippet);
    void indexSymbols(const std::string& code, const std::string& language);
};

// ============================================================================
// Agent Planner
// ============================================================================
struct AgentTask {
    std::string id;
    std::string description;
    std::string tool;
    std::map<std::string, std::string> args;
    std::string status;  // "pending", "running", "completed", "failed"
    std::string result;
    std::vector<std::string> dependencies;
};

class AgentPlanner {
    std::vector<AgentTask> tasks_;
    std::map<std::string, AgentTask> taskIndex_;
    mutable std::mutex mtx_;

public:
    std::string addTask(const std::string& description, const std::string& tool, const std::map<std::string, std::string>& args);
    void addDependency(const std::string& taskId, const std::string& dependsOn);
    std::vector<AgentTask> getReadyTasks();
    void updateTaskStatus(const std::string& taskId, const std::string& status, const std::string& result = "");
    std::vector<AgentTask> getAllTasks() const;
    void clear();
};

// ============================================================================
// TCP Server for IDE Integration
// ============================================================================
class TCPServer {
    SOCKET listenSock_ = INVALID_SOCKET;
    int port_;
    std::atomic<bool> running_{false};
    std::thread acceptThread_;
    std::vector<std::thread> clientThreads_;
    std::mutex clientMtx_;
    std::function<std::string(const std::string&)> handler_;

    void acceptLoop();
    void handleClient(SOCKET clientSock);

public:
    explicit TCPServer(int port = 3001) : port_(port) {}
    ~TCPServer() { stop(); }

    bool start(std::function<std::string(const std::string&)> handler);
    void stop();
    bool isRunning() const { return running_.load(); }
    int getPort() const { return port_; }
};

// ============================================================================
// Native AI Integration - Main Integration Class
// ============================================================================
class NativeAIIntegration {
    AIConfig config_;
    std::unique_ptr<LLMBridge> llm_;
    std::unique_ptr<ToolRegistry> tools_;
    std::unique_ptr<SemanticSearch> search_;
    std::unique_ptr<CodeCompleter> completer_;
    std::unique_ptr<AgentPlanner> planner_;
    std::unique_ptr<TCPServer> server_;
    std::atomic<bool> initialized_{false};
    std::atomic<bool> serverRunning_{false};
    mutable std::mutex mtx_;

    // Event callbacks
    std::function<void(const std::string&)> onLog_;
    std::function<void(const LLMResponse&)> onResponse_;
    std::function<void(const std::string&, const std::string&)> onToolExec_;

    void registerBuiltinTools();
    std::string handleRequest(const std::string& request);

public:
    NativeAIIntegration() = default;
    ~NativeAIIntegration() { shutdown(); }

    // Lifecycle
    bool initialize(const AIConfig& config);
    void shutdown();

    // LLM Operations
    LLMResponse chat(const std::string& message, StreamCallback callback = nullptr);
    LLMResponse complete(const std::string& prompt, StreamCallback callback = nullptr);
    std::vector<std::string> listModels();
    bool loadModel(const std::string& model);

    // Tool Operations
    ToolResult executeTool(const std::string& name, const std::map<std::string, std::string>& args);
    std::vector<Tool> listTools();
    void registerTool(const Tool& tool);

    // Search Operations
    std::vector<SearchResult> search(const std::string& query, int maxResults = 50);
    void indexWorkspace();

    // Completion Operations
    std::vector<CompletionItem> complete(const std::string& code, int line, int column, const std::string& language = "");

    // Agent Operations
    std::string planTask(const std::string& description);
    std::vector<AgentTask> getTasks();
    ToolResult executeTask(const std::string& taskId);

    // Server Operations
    bool startServer();
    void stopServer();
    bool isServerRunning() const { return serverRunning_.load(); }
    int getServerPort() const { return config_.serverPort; }

    // Status
    bool isInitialized() const { return initialized_.load(); }
    bool isLLMAvailable();
    AIConfig getConfig() const { return config_; }

    // Callbacks
    void setLogCallback(std::function<void(const std::string&)> cb) { onLog_ = std::move(cb); }
    void setResponseCallback(std::function<void(const LLMResponse&)> cb) { onResponse_ = std::move(cb); }
    void setToolExecCallback(std::function<void(const std::string&, const std::string&)> cb) { onToolExec_ = std::move(cb); }

    // Singleton for IDE integration
    static NativeAIIntegration& Instance();
};

// ============================================================================
// CLI Integration Helper
// ============================================================================
class CLIIntegration {
    NativeAIIntegration& ai_;
    bool interactive_ = false;
    std::string currentFile_;
    std::string currentLanguage_;

public:
    CLIIntegration() : ai_(NativeAIIntegration::Instance()) {}

    bool initialize(const AIConfig& config);
    int runInteractive();
    int runCommand(const std::string& cmd, const std::vector<std::string>& args);
    void printHelp();

    // Commands
    void cmdChat(const std::string& message);
    void cmdComplete(const std::string& code);
    void cmdSearch(const std::string& query);
    void cmdTool(const std::string& name, const std::map<std::string, std::string>& args);
    void cmdModel(const std::string& action, const std::string& model);
    void cmdFile(const std::string& path);
    void cmdStatus();
};

// ============================================================================
// GUI Integration Helper
// ============================================================================
class GUIIntegration {
    NativeAIIntegration& ai_;
    HWND parentWnd_ = nullptr;
    std::string currentFile_;
    std::string currentCode_;
    int cursorLine_ = 0;
    int cursorColumn_ = 0;

public:
    GUIIntegration() : ai_(NativeAIIntegration::Instance()) {}

    bool initialize(const AIConfig& config, HWND parentWnd);
    void shutdown();

    // Editor integration
    void setEditorContent(const std::string& code);
    void setCursorPosition(int line, int column);
    void setCurrentFile(const std::string& path);
    std::vector<CompletionItem> getCompletions();
    LLMResponse requestCompletion(const std::string& context);
    LLMResponse requestExplanation(const std::string& code);
    LLMResponse requestRefactor(const std::string& code, const std::string& instruction);

    // Chat integration
    LLMResponse sendChatMessage(const std::string& message);
    void setChatCallback(std::function<void(const LLMResponse&)> cb);

    // Status
    bool isReady() const { return ai_.isInitialized(); }
    bool isLLMAvailable() { return ai_.isLLMAvailable(); }
    std::string getStatusMessage();
};

} // namespace rawrxd

#endif // RAWRXD_NATIVE_AI_INTEGRATION_HPP