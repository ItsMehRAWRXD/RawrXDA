#include "autonomous_widgets.h"
#include "action_executor.h"
#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

// Helper to set up the executor with default context
static ActionExecutor createExecutor() {
    ActionExecutor executor;
    ExecutionContext context;
    // In a real scenario, this might need to be injectable or discovered
    context.projectRoot = fs::current_path().string(); 
    executor.setContext(context);
    return executor;
}

// ============================================================================
// Logic Backend for Autonomous Feature Controllers
// ============================================================================

// --- Suggestion Controller ---

void AutonomousSuggestionController::addSuggestion(const AutonomousSuggestion& suggestion) {
    suggestions[suggestion.suggestionId] = suggestion;
}

void AutonomousSuggestionController::clearSuggestions() {
    suggestions.clear();
}

void AutonomousSuggestionController::acceptSuggestion(const std::string& suggestionId) {
    auto it = suggestions.find(suggestionId);
    if (it == suggestions.end()) {
        return;
    }

    AutonomousSuggestion& item = it->second;

    ActionExecutor executor = createExecutor();
    Action action;
    action.type = ActionType::FileEdit;
    action.target = item.filePath;
    action.description = "Applying autonomous suggestion: " + suggestionId;

    // Use replace operation
    action.params["op"] = "replace";
    action.params["old_text"] = item.originalCode;
    action.params["new_text"] = item.suggestedCode;

    if (executor.executeAction(action)) {
        item.wasAccepted = true;
    }
}

void AutonomousSuggestionController::rejectSuggestion(const std::string& suggestionId) {
    suggestions.erase(suggestionId);
}

// --- Security Alert Controller ---

void SecurityAlertController::addIssue(const SecurityIssue& issue) {
    issues[issue.issueId] = issue;
}

void SecurityAlertController::clearIssues() {
    issues.clear();
}

void SecurityAlertController::fixIssue(const std::string& issueId) {
    auto it = issues.find(issueId);
    if (it == issues.end()) {
        return;
    }

    const SecurityIssue& issue = it->second;

    if (issue.suggestedFix.empty()) {
        return;
    }

    ActionExecutor executor = createExecutor();
    Action action;
    action.type = ActionType::FileEdit;
    action.target = issue.filePath;
    action.description = "Fixing security issue: " + issueId;
    
    // Security fixes usually replace the vulnerable code block
    action.params["op"] = "replace";
    action.params["old_text"] = issue.vulnerableCode;
    action.params["new_text"] = issue.suggestedFix;

    executor.executeAction(action);
}

void SecurityAlertController::ignoreIssue(const std::string& issueId) {
    issues.erase(issueId);
}

// --- Optimization Controller ---

void OptimizationController::addOptimization(const PerformanceOptimization& opt) {
    optimizations[opt.optimizationId] = opt;
}

void OptimizationController::clearOptimizations() {
    optimizations.clear();
}

void OptimizationController::applyOptimization(const std::string& id) {
    auto it = optimizations.find(id);
    if (it == optimizations.end()) {
        return;
    }

    const PerformanceOptimization& opt = it->second;

    ActionExecutor executor = createExecutor();
    Action action;
    action.type = ActionType::FileEdit;
    action.target = opt.filePath;
    action.description = "Applying performance optimization: " + id;

    action.params["op"] = "replace";
    action.params["old_text"] = opt.currentImplementation;
    action.params["new_text"] = opt.optimizedImplementation;

    executor.executeAction(action);
}

void OptimizationController::dismissOptimization(const std::string& id) {
    optimizations.erase(id);
}
