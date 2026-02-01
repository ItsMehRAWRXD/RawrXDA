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
    std::cout << "[Controller] Added Suggestion: " << suggestion.suggestionId 
              << " (" << suggestion.type << ")" << std::endl;
}

void AutonomousSuggestionController::clearSuggestions() {
    suggestions.clear();
}

void AutonomousSuggestionController::acceptSuggestion(const std::string& suggestionId) {
    auto it = suggestions.find(suggestionId);
    if (it == suggestions.end()) {
        std::cerr << "[Controller] Suggestion not found: " << suggestionId << std::endl;
        return;
    }

    AutonomousSuggestion& item = it->second;
    std::cout << "[Controller] Accepting: " << item.type << " for " << item.filePath << std::endl;

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
        std::cout << "[Controller] Suggestion Applied Successfully." << std::endl;
        item.wasAccepted = true;
    } else {
        std::cerr << "[Controller] Failed to apply suggestion: " << action.error << std::endl;
    }
}

void AutonomousSuggestionController::rejectSuggestion(const std::string& suggestionId) {
    if (suggestions.erase(suggestionId)) {
        std::cout << "[Controller] Rejected: " << suggestionId << std::endl;
    }
}

// --- Security Alert Controller ---

void SecurityAlertController::addIssue(const SecurityIssue& issue) {
    // Fixed field name access: issue.issueId instead of issue.id
    issues[issue.issueId] = issue;
    std::cout << "[Controller] Security Issue: " << issue.issueId 
              << " [" << issue.severity << "]" << std::endl;
}

void SecurityAlertController::clearIssues() {
    issues.clear();
}

void SecurityAlertController::fixIssue(const std::string& issueId) {
    auto it = issues.find(issueId);
    if (it == issues.end()) {
        std::cout << "[Controller] Issue not found: " << issueId << std::endl;
        return;
    }

    const SecurityIssue& issue = it->second;
    std::cout << "[Controller] Auto-Fixing Security Issue: " << issueId << std::endl;

    if (issue.suggestedFix.empty()) {
        std::cerr << "[Controller] No fix available for this issue." << std::endl;
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

    if (executor.executeAction(action)) {
        std::cout << "[Controller] Security Fix Applied." << std::endl;
    } else {
        std::cerr << "[Controller] Failed to apply security fix: " << action.error << std::endl;
    }
}

void SecurityAlertController::ignoreIssue(const std::string& issueId) {
    issues.erase(issueId);
    std::cout << "[Controller] Ignored Issue: " << issueId << std::endl;
}

// --- Optimization Controller ---

void OptimizationController::addOptimization(const PerformanceOptimization& opt) {
    // Fixed field name access: opt.optimizationId instead of opt.id
    optimizations[opt.optimizationId] = opt;
    std::cout << "[Controller] Optimization Opportunity: " << opt.optimizationId 
              << " (" << opt.type << ")" << std::endl;
}

void OptimizationController::clearOptimizations() {
    optimizations.clear();
}

void OptimizationController::applyOptimization(const std::string& id) {
    auto it = optimizations.find(id);
    if (it == optimizations.end()) {
        std::cerr << "[Controller] Optimization not found: " << id << std::endl;
        return;
    }

    const PerformanceOptimization& opt = it->second;
    std::cout << "[Controller] Applying Optimization: " << id << std::endl;

    ActionExecutor executor = createExecutor();
    Action action;
    action.type = ActionType::FileEdit;
    action.target = opt.filePath;
    action.description = "Applying performance optimization: " + id;

    action.params["op"] = "replace";
    action.params["old_text"] = opt.currentImplementation;
    action.params["new_text"] = opt.optimizedImplementation;

    if (executor.executeAction(action)) {
        std::cout << "[Controller] Optimization Applied." << std::endl;
    } else {
        std::cerr << "[Controller] Failed to apply optimization: " << action.error << std::endl;
    }
}

void OptimizationController::dismissOptimization(const std::string& id) {
    optimizations.erase(id);
}
