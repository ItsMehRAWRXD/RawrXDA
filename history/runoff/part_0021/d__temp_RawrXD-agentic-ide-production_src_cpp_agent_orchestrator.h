// ============================================================================
// agent_orchestrator.h - Header for autonomous AI workflow orchestration
// ============================================================================

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <functional>

// Forward declarations
namespace nlohmann {
    class json;
}

// ============================================================================
// Task Status Enum
// ============================================================================

enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED
};

// ============================================================================
// TaskNode - Individual step in workflow
// ============================================================================

class TaskNode {
public:
    TaskNode(const std::string& toolName, const nlohmann::json& params);
    
    void AddDependency(TaskNode* dependency);
    bool AreDependenciesComplete() const;
    
    std::string id;
    std::string toolName;
    nlohmann::json parameters;
    nlohmann::json result;
    TaskStatus status;
    
private:
    std::string GenerateTaskId();
    std::vector<TaskNode*> dependencies;
};

// ============================================================================
// TaskGraph - Complete workflow with dependencies
// ============================================================================

class TaskGraph {
public:
    TaskGraph(const std::string& rootPrompt);
    
    TaskNode* AddNode(const std::string& toolName, const nlohmann::json& params);
    void AddEdge(TaskNode* from, TaskNode* to);
    
    std::vector<TaskNode*> GetReadyNodes() const;
    bool IsComplete() const;
    nlohmann::json GetFinalResult() const;
    
    std::string rootPrompt;
    TaskStatus status;
    
private:
    std::unique_ptr<TaskNode> rootNode;
    std::vector<std::unique_ptr<TaskNode>> nodes;
};

// ============================================================================
// BackgroundTask - Async execution unit
// ============================================================================

class BackgroundTask {
public:
    std::function<nlohmann::json()> executeFunction;
    std::function<void(const nlohmann::json&)> completionCallback;
    nlohmann::json result;
    TaskNode* node;
    std::string graphId;
    
    void Execute() {
        result = executeFunction();
    }
};

// ============================================================================
// BackgroundWorker - Thread pool for async execution
// ============================================================================

class BackgroundWorker {
public:
    BackgroundWorker();
    ~BackgroundWorker();
    
    void Start();
    void Stop();
    void SubmitTask(std::unique_ptr<BackgroundTask> task);
    
private:
    void WorkerLoop();
    
    std::thread workerThread;
    std::atomic<bool> isRunning;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::queue<std::unique_ptr<BackgroundTask>> taskQueue;
};

// ============================================================================
// ITool Interface - Abstract tool execution
// ============================================================================

class ITool {
public:
    virtual ~ITool() = default;
    virtual nlohmann::json Execute(const nlohmann::json& params) = 0;
};

// ============================================================================
// AgentOrchestrator - Main coordinator class
// ============================================================================

class AgentOrchestrator {
public:
    AgentOrchestrator();
    ~AgentOrchestrator();
    
    // Core orchestration
    std::string ExecuteTaskAsync(const std::string& naturalLanguagePrompt);
    std::unique_ptr<TaskGraph> BuildExecutionGraph(const std::string& prompt);
    
    // Tool management
    void RegisterTool(const std::string& name, ITool* tool);
    
    // Callback for UI integration
    using GraphCompletionCallback = std::function<void(const std::string& graphId, const nlohmann::json& result)>;
    void SetGraphCompletionCallback(GraphCompletionCallback callback);
    
private:
    // Graph execution
    void ExecuteGraphAsync(const std::string& graphId);
    void ExecuteNodeAsync(TaskNode* node, const std::string& graphId);
    void OnNodeCompleted(TaskNode* node, const std::string& graphId, const nlohmann::json& result);
    
    // Tool execution
    nlohmann::json ExecuteTool(const std::string& toolName, const nlohmann::json& params);
    
    // Prompt parsing
    nlohmann::json ParsePrompt(const std::string& prompt);
    
    // Graph building helpers
    void BuildCodeGenerationGraph(TaskGraph* graph, const nlohmann::json& parsed);
    void BuildBugFixGraph(TaskGraph* graph, const nlohmann::json& parsed);
    void BuildFileOperationGraph(TaskGraph* graph, const nlohmann::json& parsed);
    void BuildSimpleResponseGraph(TaskGraph* graph, const nlohmann::json& parsed);
    
    // Internal state
    BackgroundWorker backgroundWorker;
    std::map<std::string, ITool*> toolRegistry;
    std::map<std::string, std::unique_ptr<TaskGraph>> activeGraphs;
    GraphCompletionCallback graphCompletionCallback;
    static std::atomic<uint64_t> graphCounter;
};

// ============================================================================
// C Interface for MASM Integration
// ============================================================================

extern "C" {

// Forward declarations for C interface
struct AgentOrchestrator;
struct ITool;

// Creation/destruction
__declspec(dllexport) AgentOrchestrator* __stdcall AgentOrchestrator_Create();
__declspec(dllexport) void __stdcall AgentOrchestrator_Destroy(AgentOrchestrator* orchestrator);

// Core operations
__declspec(dllexport) const char* __stdcall AgentOrchestrator_ExecuteTask(
    AgentOrchestrator* orchestrator, const char* prompt);

// Tool registration
__declspec(dllexport) void __stdcall AgentOrchestrator_RegisterTool(
    AgentOrchestrator* orchestrator, const char* toolName, ITool* tool);

} // extern "C"