// ============================================================================
// agent_orchestrator.cpp - Master coordinator for autonomous AI workflows
// Connects 7 AI systems into fire-and-forget task execution
// ============================================================================

#include "agent_orchestrator.h"
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ============================================================================
// TaskNode Implementation
// ============================================================================

TaskNode::TaskNode(const std::string& toolName, const json& params)
    : toolName(toolName), parameters(params), status(TaskStatus::PENDING) {
    id = GenerateTaskId();
}

std::string TaskNode::GenerateTaskId() {
    static std::atomic<uint64_t> counter{0};
    return "task_" + std::to_string(++counter);
}

void TaskNode::AddDependency(TaskNode* dependency) {
    dependencies.push_back(dependency);
}

bool TaskNode::AreDependenciesComplete() const {
    for (auto* dep : dependencies) {
        if (dep->status != TaskStatus::COMPLETED) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// TaskGraph Implementation
// ============================================================================

TaskGraph::TaskGraph(const std::string& rootPrompt) 
    : rootPrompt(rootPrompt), status(TaskStatus::PENDING) {
    rootNode = std::make_unique<TaskNode>("prompt_parser", json{{"prompt", rootPrompt}});
}

TaskNode* TaskGraph::AddNode(const std::string& toolName, const json& params) {
    auto node = std::make_unique<TaskNode>(toolName, params);
    TaskNode* ptr = node.get();
    nodes.push_back(std::move(node));
    return ptr;
}

void TaskGraph::AddEdge(TaskNode* from, TaskNode* to) {
    to->AddDependency(from);
}

std::vector<TaskNode*> TaskGraph::GetReadyNodes() const {
    std::vector<TaskNode*> ready;
    for (const auto& node : nodes) {
        if (node->status == TaskStatus::PENDING && node->AreDependenciesComplete()) {
            ready.push_back(node.get());
        }
    }
    return ready;
}

bool TaskGraph::IsComplete() const {
    for (const auto& node : nodes) {
        if (node->status != TaskStatus::COMPLETED) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// BackgroundWorker Implementation
// ============================================================================

BackgroundWorker::BackgroundWorker() : isRunning(false) {}

BackgroundWorker::~BackgroundWorker() {
    Stop();
}

void BackgroundWorker::Start() {
    if (isRunning) return;
    
    isRunning = true;
    workerThread = std::thread(&BackgroundWorker::WorkerLoop, this);
}

void BackgroundWorker::Stop() {
    if (!isRunning) return;
    
    isRunning = false;
    condition.notify_all();
    if (workerThread.joinable()) {
        workerThread.join();
    }
}

void BackgroundWorker::SubmitTask(std::unique_ptr<BackgroundTask> task) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }
    condition.notify_one();
}

void BackgroundWorker::WorkerLoop() {
    while (isRunning) {
        std::unique_ptr<BackgroundTask> task;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            condition.wait(lock, [this] { 
                return !isRunning || !taskQueue.empty(); 
            });
            
            if (!isRunning && taskQueue.empty()) break;
            
            if (!taskQueue.empty()) {
                task = std::move(taskQueue.front());
                taskQueue.pop();
            }
        }
        
        if (task) {
            try {
                task->Execute();
                
                // Post completion to MASM UI thread
                if (task->completionCallback) {
                    task->completionCallback(task->result);
                }
            } catch (const std::exception& e) {
                std::cerr << "Background task failed: " << e.what() << std::endl;
                task->result["error"] = e.what();
            }
        }
    }
}

// ============================================================================
// AgentOrchestrator Implementation
// ============================================================================

AgentOrchestrator::AgentOrchestrator() {
    backgroundWorker.Start();
}

AgentOrchestrator::~AgentOrchestrator() {
    backgroundWorker.Stop();
}

std::string AgentOrchestrator::ExecuteTaskAsync(const std::string& naturalLanguagePrompt) {
    auto graph = BuildExecutionGraph(naturalLanguagePrompt);
    
    // Store graph for tracking
    std::string graphId = "graph_" + std::to_string(++graphCounter);
    activeGraphs[graphId] = std::move(graph);
    
    // Start execution
    ExecuteGraphAsync(graphId);
    
    return graphId;
}

std::unique_ptr<TaskGraph> AgentOrchestrator::BuildExecutionGraph(const std::string& prompt) {
    auto graph = std::make_unique<TaskGraph>(prompt);
    
    // Parse prompt to determine workflow
    auto parsed = ParsePrompt(prompt);
    
    // Build graph based on parsed intent
    if (parsed["intent"] == "code_generation") {
        BuildCodeGenerationGraph(graph.get(), parsed);
    } else if (parsed["intent"] == "bug_fix") {
        BuildBugFixGraph(graph.get(), parsed);
    } else if (parsed["intent"] == "file_operation") {
        BuildFileOperationGraph(graph.get(), parsed);
    } else {
        // Default: simple AI response
        BuildSimpleResponseGraph(graph.get(), parsed);
    }
    
    return graph;
}

json AgentOrchestrator::ParsePrompt(const std::string& prompt) {
    // Use AdvancedCodingAgent to parse intent and parameters
    // This is a simplified version - in production, use your AI backend
    
    json result;
    
    // Simple keyword-based intent detection
    if (prompt.find("write") != std::string::npos || 
        prompt.find("create") != std::string::npos ||
        prompt.find("generate") != std::string::npos) {
        result["intent"] = "code_generation";
    } else if (prompt.find("fix") != std::string::npos ||
               prompt.find("bug") != std::string::npos ||
               prompt.find("error") != std::string::npos) {
        result["intent"] = "bug_fix";
    } else if (prompt.find("file") != std::string::npos ||
               prompt.find("open") != std::string::npos ||
               prompt.find("read") != std::string::npos) {
        result["intent"] = "file_operation";
    } else {
        result["intent"] = "simple_response";
    }
    
    result["original_prompt"] = prompt;
    return result;
}

void AgentOrchestrator::BuildCodeGenerationGraph(TaskGraph* graph, const json& parsed) {
    // Example: "Write a function to sort an array"
    
    // Step 1: Analyze requirements
    auto* analyzeNode = graph->AddNode("codebase_analyzer", 
        json{{"prompt", parsed["original_prompt"]}});
    
    // Step 2: Generate code
    auto* generateNode = graph->AddNode("code_generator", 
        json{{"requirements", "TBD"}});
    graph->AddEdge(analyzeNode, generateNode);
    
    // Step 3: Validate code
    auto* validateNode = graph->AddNode("code_validator", 
        json{{"code", "TBD"}});
    graph->AddEdge(generateNode, validateNode);
    
    // Step 4: Write to file
    auto* writeNode = graph->AddNode("file_writer", 
        json{{"path", "TBD"}, {"content", "TBD"}});
    graph->AddEdge(validateNode, writeNode);
}

void AgentOrchestrator::BuildBugFixGraph(TaskGraph* graph, const json& parsed) {
    // Example: "Fix the memory leak in main.cpp"
    
    // Step 1: Analyze code for issues
    auto* analyzeNode = graph->AddNode("code_analyzer", 
        json{{"file", "main.cpp"}, {"issue_type", "memory_leak"}});
    
    // Step 2: Generate fix
    auto* fixNode = graph->AddNode("fix_generator", 
        json{{"issue", "TBD"}});
    graph->AddEdge(analyzeNode, fixNode);
    
    // Step 3: Apply fix
    auto* applyNode = graph->AddNode("code_applier", 
        json{{"file", "main.cpp"}, {"fix", "TBD"}});
    graph->AddEdge(fixNode, applyNode);
    
    // Step 4: Test fix
    auto* testNode = graph->AddNode("test_runner", 
        json{{"file", "main.cpp"}});
    graph->AddEdge(applyNode, testNode);
}

void AgentOrchestrator::RegisterTool(const std::string& name, ITool* tool) {
    toolRegistry[name] = tool;
}

void AgentOrchestrator::ExecuteGraphAsync(const std::string& graphId) {
    auto it = activeGraphs.find(graphId);
    if (it == activeGraphs.end()) return;
    
    auto& graph = it->second;
    
    // Start with ready nodes
    auto readyNodes = graph->GetReadyNodes();
    for (auto* node : readyNodes) {
        ExecuteNodeAsync(node, graphId);
    }
}

void AgentOrchestrator::ExecuteNodeAsync(TaskNode* node, const std::string& graphId) {
    auto task = std::make_unique<BackgroundTask>();
    task->node = node;
    task->graphId = graphId;
    
    // Set up execution
    task->executeFunction = [this, node]() -> json {
        return ExecuteTool(node->toolName, node->parameters);
    };
    
    // Set up completion callback
    task->completionCallback = [this, node, graphId](const json& result) {
        OnNodeCompleted(node, graphId, result);
    };
    
    node->status = TaskStatus::RUNNING;
    backgroundWorker.SubmitTask(std::move(task));
}

json AgentOrchestrator::ExecuteTool(const std::string& toolName, const json& params) {
    auto it = toolRegistry.find(toolName);
    if (it == toolRegistry.end()) {
        throw std::runtime_error("Tool not found: " + toolName);
    }
    
    return it->second->Execute(params);
}

void AgentOrchestrator::OnNodeCompleted(TaskNode* node, const std::string& graphId, const json& result) {
    node->status = TaskStatus::COMPLETED;
    node->result = result;
    
    // Check if graph is complete
    auto it = activeGraphs.find(graphId);
    if (it != activeGraphs.end()) {
        auto& graph = it->second;
        
        if (graph->IsComplete()) {
            // Graph finished - notify UI
            if (graphCompletionCallback) {
                graphCompletionCallback(graphId, graph->GetFinalResult());
            }
            activeGraphs.erase(it);
        } else {
            // Execute next ready nodes
            ExecuteGraphAsync(graphId);
        }
    }
}

void AgentOrchestrator::SetGraphCompletionCallback(GraphCompletionCallback callback) {
    graphCompletionCallback = callback;
}

// ============================================================================
// ITool Interface Implementation
// ============================================================================

// Base tool implementations would go here
// In production, these would connect to your existing 7 AI systems

// Example tool implementation
class CodebaseAnalyzerTool : public ITool {
public:
    json Execute(const json& params) override {
        // Connect to your existing CodebaseContextAnalyzer
        // This is a stub implementation
        
        json result;
        result["status"] = "success";
        result["analysis"] = "Codebase analyzed successfully";
        return result;
    }
};

// ============================================================================
// C Interface for MASM Integration
// ============================================================================

extern "C" {

__declspec(dllexport) AgentOrchestrator* __stdcall AgentOrchestrator_Create() {
    return new AgentOrchestrator();
}

__declspec(dllexport) void __stdcall AgentOrchestrator_Destroy(AgentOrchestrator* orchestrator) {
    delete orchestrator;
}

__declspec(dllexport) const char* __stdcall AgentOrchestrator_ExecuteTask(
    AgentOrchestrator* orchestrator, const char* prompt) {
    
    std::string graphId = orchestrator->ExecuteTaskAsync(prompt);
    
    // Return graph ID for tracking
    static thread_local std::string result;
    result = graphId;
    return result.c_str();
}

__declspec(dllexport) void __stdcall AgentOrchestrator_RegisterTool(
    AgentOrchestrator* orchestrator, const char* toolName, ITool* tool) {
    
    orchestrator->RegisterTool(toolName, tool);
}

} // extern "C"