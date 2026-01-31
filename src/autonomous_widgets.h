// autonomous_widgets.h - Custom UI Widgets for AI Features
#ifndef AUTONOMOUS_WIDGETS_H
#define AUTONOMOUS_WIDGETS_H


#include "autonomous_feature_engine.h"

// AI Suggestions Widget
class AutonomousSuggestionWidget : public void {

public:
    explicit AutonomousSuggestionWidget(void *parent = nullptr);
    void addSuggestion(const AutonomousSuggestion& suggestion);
    void clearSuggestions();


    void suggestionAccepted(const std::string& suggestionId);
    void suggestionRejected(const std::string& suggestionId);

private:
    void onSuggestionClicked(QListWidgetItem* item);
    void onAcceptClicked();
    void onRejectClicked();

private:
    QListWidget* suggestionList;
    void* detailsView;
    void* acceptButton;
    void* rejectButton;
    void* confidenceLabel;
    
    std::map<std::string, AutonomousSuggestion> suggestions;
    std::string currentSuggestionId;
};

// Security Alerts Widget
class SecurityAlertWidget : public void {

public:
    explicit SecurityAlertWidget(void *parent = nullptr);
    void addIssue(const SecurityIssue& issue);
    void clearIssues();


    void issueFixed(const std::string& issueId);
    void issueIgnored(const std::string& issueId);

private:
    void onIssueClicked(QListWidgetItem* item);
    void onFixClicked();
    void onIgnoreClicked();

private:
    QListWidget* issueList;
    void* issueDetails;
    void* fixButton;
    void* ignoreButton;
    void* riskScoreLabel;
    
    std::map<std::string, SecurityIssue> issues;
    std::string currentIssueId;
    
    std::string getSeverityColor(const std::string& severity) const;
};

// Performance Optimization Widget
class OptimizationPanelWidget : public void {

public:
    explicit OptimizationPanelWidget(void *parent = nullptr);
    void addOptimization(const PerformanceOptimization& optimization);
    void clearOptimizations();


    void optimizationApplied(const std::string& optimizationId);
    void optimizationDismissed(const std::string& optimizationId);

private:
    void onOptimizationClicked(QListWidgetItem* item);
    void onApplyClicked();
    void onDismissClicked();

private:
    QListWidget* optimizationList;
    void* optimizationDetails;
    void* applyButton;
    void* dismissButton;
    void* speedupLabel;
    void* confidenceBar;
    
    std::map<std::string, PerformanceOptimization> optimizations;
    std::string currentOptimizationId;
};

#endif // AUTONOMOUS_WIDGETS_H

