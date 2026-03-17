#pragma once
#include <string>
#include <vector>
#include <functional>

class ActionExecutor {
public:
    enum class ActionType {
        FILE_EDIT,
        SEARCH_FILES,
        RUN_BUILD,
        INVOKE_COMMAND,
        GIT_COMMIT,
        GIT_PUSH,
        CREATE_DIRECTORY,
        DELETE_FILE
    };

    struct Action {
        ActionType type;
        std::string description;
        std::vector<std::string> parameters;
        std::string backupPath; // For rollback
    };

    struct Result {
        bool success;
        std::string output;
        std::string error;
        int exitCode;
    };

    ActionExecutor();
    ~ActionExecutor();

    Result executeAction(const Action& action);
    Result executePlan(const std::vector<Action>& plan);
    bool rollbackAction(const Action& action);
    
    void setProgressCallback(std::function<void(int, const std::string&)> callback);
    void setErrorCallback(std::function<void(const std::string&)> callback);

private:
    std::function<void(int, const std::string&)> progressCallback_;
    std::function<void(const std::string&)> errorCallback_;
    
    Result executeFileEdit(const Action& action);
    Result executeSearchFiles(const Action& action);
    Result executeRunBuild(const Action& action);
    Result executeInvokeCommand(const Action& action);
    Result executeGitCommit(const Action& action);
    Result executeGitPush(const Action& action);
    Result executeCreateDirectory(const Action& action);
    Result executeDeleteFile(const Action& action);
    
    bool createBackup(const std::string& filePath);
    bool restoreBackup(const std::string& backupPath);
};