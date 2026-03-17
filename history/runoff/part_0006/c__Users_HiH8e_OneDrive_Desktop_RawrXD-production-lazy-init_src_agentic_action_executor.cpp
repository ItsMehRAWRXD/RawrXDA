#include "action_executor.hpp"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <filesystem>

ActionExecutor::ActionExecutor() {}

ActionExecutor::~ActionExecutor() {}

ActionExecutor::Result ActionExecutor::executeAction(const Action& action) {
    Result result;
    
    if (progressCallback_) {
        progressCallback_(0, "Starting action: " + action.description);
    }
    
    switch (action.type) {
        case ActionType::FILE_EDIT:
            result = executeFileEdit(action);
            break;
        case ActionType::SEARCH_FILES:
            result = executeSearchFiles(action);
            break;
        case ActionType::RUN_BUILD:
            result = executeRunBuild(action);
            break;
        case ActionType::INVOKE_COMMAND:
            result = executeInvokeCommand(action);
            break;
        case ActionType::GIT_COMMIT:
            result = executeGitCommit(action);
            break;
        case ActionType::GIT_PUSH:
            result = executeGitPush(action);
            break;
        case ActionType::CREATE_DIRECTORY:
            result = executeCreateDirectory(action);
            break;
        case ActionType::DELETE_FILE:
            result = executeDeleteFile(action);
            break;
    }
    
    if (progressCallback_) {
        progressCallback_(100, "Action completed: " + action.description);
    }
    
    return result;
}

ActionExecutor::Result ActionExecutor::executePlan(const std::vector<Action>& plan) {
    Result finalResult;
    finalResult.success = true;
    
    for (size_t i = 0; i < plan.size(); ++i) {
        if (progressCallback_) {
            progressCallback_(static_cast<int>((i * 100) / plan.size()), 
                            "Executing step " + std::to_string(i + 1) + " of " + std::to_string(plan.size()));
        }
        
        Result stepResult = executeAction(plan[i]);
        if (!stepResult.success) {
            finalResult.success = false;
            finalResult.error = "Step " + std::to_string(i + 1) + " failed: " + stepResult.error;
            
            // Attempt rollback for previous actions
            for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
                rollbackAction(plan[j]);
            }
            break;
        }
        
        finalResult.output += "Step " + std::to_string(i + 1) + ": " + stepResult.output + "\n";
    }
    
    return finalResult;
}

bool ActionExecutor::rollbackAction(const Action& action) {
    // TODO: Implement proper rollback logic
    if (!action.backupPath.empty()) {
        return restoreBackup(action.backupPath);
    }
    return false;
}

ActionExecutor::Result ActionExecutor::executeFileEdit(const Action& action) {
    Result result;
    
    if (action.parameters.size() < 2) {
        result.success = false;
        result.error = "FileEdit requires file path and content parameters";
        return result;
    }
    
    std::string filePath = action.parameters[0];
    std::string content = action.parameters[1];
    
    // Create backup
    if (!createBackup(filePath)) {
        result.success = false;
        result.error = "Failed to create backup for " + filePath;
        return result;
    }
    
    // Write file
    std::ofstream file(filePath);
    if (!file.is_open()) {
        result.success = false;
        result.error = "Failed to open file " + filePath;
        return result;
    }
    
    file << content;
    file.close();
    
    result.success = true;
    result.output = "Successfully edited " + filePath;
    return result;
}

ActionExecutor::Result ActionExecutor::executeSearchFiles(const Action& action) {
    Result result;
    result.success = true;
    result.output = "SearchFiles placeholder - would search for: " + 
                   (action.parameters.empty() ? "*" : action.parameters[0]);
    return result;
}

ActionExecutor::Result ActionExecutor::executeRunBuild(const Action& action) {
    Result result;
    result.success = true;
    result.output = "RunBuild placeholder - would execute build command";
    return result;
}

ActionExecutor::Result ActionExecutor::executeInvokeCommand(const Action& action) {
    Result result;
    result.success = true;
    result.output = "InvokeCommand placeholder";
    return result;
}

ActionExecutor::Result ActionExecutor::executeGitCommit(const Action& action) {
    Result result;
    result.success = true;
    result.output = "GitCommit placeholder";
    return result;
}

ActionExecutor::Result ActionExecutor::executeGitPush(const Action& action) {
    Result result;
    result.success = true;
    result.output = "GitPush placeholder";
    return result;
}

ActionExecutor::Result ActionExecutor::executeCreateDirectory(const Action& action) {
    Result result;
    
    if (action.parameters.empty()) {
        result.success = false;
        result.error = "CreateDirectory requires directory path parameter";
        return result;
    }
    
    std::string dirPath = action.parameters[0];
    
    if (std::filesystem::create_directories(dirPath)) {
        result.success = true;
        result.output = "Successfully created directory " + dirPath;
    } else {
        result.success = false;
        result.error = "Failed to create directory " + dirPath;
    }
    
    return result;
}

ActionExecutor::Result ActionExecutor::executeDeleteFile(const Action& action) {
    Result result;
    
    if (action.parameters.empty()) {
        result.success = false;
        result.error = "DeleteFile requires file path parameter";
        return result;
    }
    
    std::string filePath = action.parameters[0];
    
    // Create backup
    if (!createBackup(filePath)) {
        result.success = false;
        result.error = "Failed to create backup for " + filePath;
        return result;
    }
    
    if (std::filesystem::remove(filePath)) {
        result.success = true;
        result.output = "Successfully deleted " + filePath;
    } else {
        result.success = false;
        result.error = "Failed to delete " + filePath;
    }
    
    return result;
}

bool ActionExecutor::createBackup(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return true; // No backup needed for non-existent files
    }
    
    std::string backupPath = filePath + ".backup";
    return std::filesystem::copy_file(filePath, backupPath, std::filesystem::copy_options::overwrite_existing);
}

bool ActionExecutor::restoreBackup(const std::string& backupPath) {
    if (!std::filesystem::exists(backupPath)) {
        return false;
    }
    
    std::string originalPath = backupPath.substr(0, backupPath.length() - 7); // Remove ".backup"
    return std::filesystem::copy_file(backupPath, originalPath, std::filesystem::copy_options::overwrite_existing);
}

void ActionExecutor::setProgressCallback(std::function<void(int, const std::string&)> callback) {
    progressCallback_ = callback;
}

void ActionExecutor::setErrorCallback(std::function<void(const std::string&)> callback) {
    errorCallback_ = callback;
}