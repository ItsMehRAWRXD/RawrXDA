#include "autonomous_widgets.h" // Includes the modified header (now Controllers)
#include <iostream>

// ============================================================================
// Logic Backend for Autonomous Feature Controllers
// ============================================================================

// --- Suggestion Controller ---

void AutonomousSuggestionController::addSuggestion(const AutonomousSuggestion& suggestion) {
    suggestions[suggestion.suggestionId] = suggestion;
    std::cout << "[Controller] Added Suggestion: " << suggestion.explanation << std::endl;
}

void AutonomousSuggestionController::clearSuggestions() {
    suggestions.clear();
}

void AutonomousSuggestionController::acceptSuggestion(const std::string& suggestionId) {
    auto it = suggestions.find(suggestionId);
    if (it != suggestions.end()) {
        std::cout << "[Controller] Accepted: " << it->second.type << std::endl;
        // In a real agent, this would apply the patch via the Editor Agent Bridge
        // e.g. EditorBridge::applyPatch(it->second.suggestedCode);
    } else {
        std::cerr << "[Controller] Suggestion not found: " << suggestionId << std::endl;
    }
}

void AutonomousSuggestionController::rejectSuggestion(const std::string& suggestionId) {
    if (suggestions.erase(suggestionId)) {
        std::cout << "[Controller] Rejected: " << suggestionId << std::endl;
    }
}

// --- Security Alert Controller ---

void SecurityAlertController::addIssue(const SecurityIssue& issue) {
    issues[issue.id] = issue;
    std::cout << "[Controller] Security Issue: " << issue.description << " [" << issue.severity << "]" << std::endl;
}

void SecurityAlertController::clearIssues() {
    issues.clear();
}

void SecurityAlertController::fixIssue(const std::string& issueId) {
    auto it = issues.find(issueId);
    if(it != issues.end()) {
        std::cout << "[Controller] Auto-Fixing Security Issue: " << issueId << std::endl;
        // Trigger Fix Logic
    }
}

void SecurityAlertController::ignoreIssue(const std::string& issueId) {
     issues.erase(issueId);
     std::cout << "[Controller] Ignored Issue: " << issueId << std::endl;
}

// --- Optimization Controller ---

void OptimizationController::addOptimization(const PerformanceOptimization& opt) {
    optimizations[opt.id] = opt;
    std::cout << "[Controller] Optimization Opportunity: " << opt.description << std::endl;
}

void OptimizationController::clearOptimizations() {
    optimizations.clear();
}

void OptimizationController::applyOptimization(const std::string& id) {
    std::cout << "[Controller] Applying Optimization: " << id << std::endl;
}

void OptimizationController::dismissOptimization(const std::string& id) {
    optimizations.erase(id);
}
