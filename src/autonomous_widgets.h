// autonomous_widgets.h - Logic Controllers for AI Features (Headless/Native)
#ifndef AUTONOMOUS_CONTROL_H
#define AUTONOMOUS_CONTROL_H

#include "autonomous_feature_engine.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>

// Logic/Controller for Suggestions
class AutonomousSuggestionController {
public:
    explicit AutonomousSuggestionController() = default;
    void addSuggestion(const AutonomousSuggestion& suggestion);
    void clearSuggestions();
    
    // Logic Actions
    void acceptSuggestion(const std::string& suggestionId);
    void rejectSuggestion(const std::string& suggestionId);
    
    const std::map<std::string, AutonomousSuggestion>& getSuggestions() const { return suggestions; }

private:
    std::map<std::string, AutonomousSuggestion> suggestions;
};

// Security Alerts Controller
class SecurityAlertController {
public:
    explicit SecurityAlertController() = default;
    void addIssue(const SecurityIssue& issue);
    void clearIssues();

    void fixIssue(const std::string& issueId);
    void ignoreIssue(const std::string& issueId);

private:
    std::map<std::string, SecurityIssue> issues; 
};

// Performance Optimization Controller
class OptimizationController {
public:
    explicit OptimizationController() = default;
    void addOptimization(const PerformanceOptimization& optimization);
    void clearOptimizations();

    void applyOptimization(const std::string& optimizationId);
    void dismissOptimization(const std::string& optimizationId);

private:
    std::map<std::string, PerformanceOptimization> optimizations;
};

#endif

