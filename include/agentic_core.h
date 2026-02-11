// Agentic Core - Integration Layer
// Provides a unified agentic automation interface
// Delegates to AgenticEngine for actual execution

#ifndef AGENTIC_CORE_H_
#define AGENTIC_CORE_H_

#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace AgenticCore {

// Task types supported by the agentic system
enum class TaskType {
    CodeGeneration,
    CodeRefactoring,
    BugFix,
    TestGeneration,
    Documentation,
    CodeReview,
    FileOperation,
    TerminalCommand,
    Search,
    Custom
};

// Result of an agentic task execution
struct TaskResult {
    bool success = false;
    std::string output;
    std::string errorMessage;
    int tokensUsed = 0;
    double latencyMs = 0.0;
    std::vector<std::string> filesModified;
};

// Configuration for the agentic core
struct CoreConfig {
    std::string modelPath;           // Path to GGUF model
    std::string workspaceRoot;       // Workspace root directory
    int maxIterations = 10;          // Max agentic iterations per task
    float temperature = 0.7f;        // Inference temperature
    bool enableToolUse = true;       // Allow tool execution
    bool enableFileEdits = true;     // Allow file modifications
    bool enableTerminal = true;      // Allow terminal commands
};

// Callback for streaming task progress
using ProgressCallback = std::function<void(const std::string& status, float progress)>;

// Core interface for agentic automation
class IAgenticCore {
public:
    virtual ~IAgenticCore() = default;
    
    virtual bool initialize(const CoreConfig& config) = 0;
    virtual void shutdown() = 0;
    virtual bool isReady() const = 0;
    
    virtual TaskResult executeTask(const std::string& instruction, TaskType type = TaskType::Custom) = 0;
    virtual TaskResult executeTaskAsync(const std::string& instruction, 
                                        ProgressCallback onProgress = nullptr) = 0;
    
    virtual void cancelCurrentTask() = 0;
    virtual std::string getStatus() const = 0;
};

// Factory function (implemented in agentic_core.cpp)
std::unique_ptr<IAgenticCore> createAgenticCore();

} // namespace AgenticCore

#endif // AGENTIC_CORE_H_
