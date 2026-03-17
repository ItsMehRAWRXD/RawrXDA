#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

// SCALAR-ONLY: Autonomous agentic capabilities with no threading

namespace RawrXD {

enum class AgentTaskType {
    CODE_GENERATION,
    FILE_EDITING,
    FILE_CREATION,
    FILE_DELETION,
    SEARCH_CODE,
    REFACTOR,
    DEBUG,
    EXPLAIN_CODE,
    RUN_COMMAND
};

struct AgentTask {
    std::string id;
    AgentTaskType type;
    std::string description;
    std::map<std::string, std::string> parameters;
    std::string status;  // pending, running, completed, failed
    std::string result;
};

struct AgentTool {
    std::string name;
    std::string description;
    std::vector<std::string> parameters;
    std::function<std::string(const std::map<std::string, std::string>&)> execute;
};

class AgenticEngine {
public:
    AgenticEngine();
    ~AgenticEngine();

    // Task execution (scalar)
    std::string ExecuteTask(const AgentTask& task);
    void QueueTask(const AgentTask& task);
    void ProcessQueue();  // Called from IDE event loop
    
    // Tool registration
    void RegisterTool(const AgentTool& tool);
    std::vector<AgentTool> GetAvailableTools() const;
    
    // Autonomous capabilities (scalar)
    std::string GenerateCode(const std::string& prompt, const std::string& context);
    std::string EditFile(const std::string& file_path, const std::string& instructions);
    std::string CreateFile(const std::string& file_path, const std::string& content);
    std::string DeleteFile(const std::string& file_path);
    std::string SearchCode(const std::string& query, const std::string& directory);
    std::string RefactorCode(const std::string& file_path, const std::string& refactor_type);
    std::string DebugCode(const std::string& error_message, const std::string& context);
    std::string ExplainCode(const std::string& code_snippet);
    std::string RunCommand(const std::string& command);
    
    // Context management
    void SetWorkingDirectory(const std::string& directory);
    std::string GetWorkingDirectory() const { return working_directory_; }
    void SetProjectContext(const std::string& context);
    
    // Callbacks
    void SetOnTaskComplete(std::function<void(const AgentTask&, const std::string&)> callback);
    void SetOnProgress(std::function<void(const std::string&)> callback);

private:
    std::string working_directory_;
    std::string project_context_;
    std::vector<AgentTask> task_queue_;
    std::map<std::string, AgentTool> tools_;
    
    std::function<void(const AgentTask&, const std::string&)> on_task_complete_;
    std::function<void(const std::string&)> on_progress_;
    
    // Scalar task processing
    std::string ProcessCodeGeneration(const std::map<std::string, std::string>& params);
    std::string ProcessFileEdit(const std::map<std::string, std::string>& params);
    std::string ProcessFileCreate(const std::map<std::string, std::string>& params);
    std::string ProcessFileDelete(const std::map<std::string, std::string>& params);
    std::string ProcessCodeSearch(const std::map<std::string, std::string>& params);
    std::string ProcessRefactor(const std::map<std::string, std::string>& params);
    std::string ProcessDebug(const std::map<std::string, std::string>& params);
    std::string ProcessExplain(const std::map<std::string, std::string>& params);
    std::string ProcessCommand(const std::map<std::string, std::string>& params);
    
    std::string GenerateTaskId();
    std::string ReadFile(const std::string& path);
    bool WriteFile(const std::string& path, const std::string& content);
};

} // namespace RawrXD
