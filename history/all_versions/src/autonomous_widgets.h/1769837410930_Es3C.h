// autonomous_widgets.h - Custom UI Widgets for AI Features
#ifndef AUTONOMOUS_WIDGETS_H
#define AUTONOMOUS_WIDGETS_H

#include "autonomous_feature_engine.h"
#include <map>
#include <string>

// Forward decls to avoid Qt includes
class QListWidgetItem;
class QListWidget;

// Logic/Controller for Suggestions
class AutonomousSuggestionWidget { // Removed inheritance from void which is invalid C++

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
    // UI Pointers - kept null in headless mode
    QListWidget* suggestionList = nullptr;
    void* detailsView = nullptr;
    void* acceptButton = nullptr;
    void* rejectButton = nullptr;
    void* confidenceLabel = nullptr;

    std::map<std::string, AutonomousSuggestion> suggestions;
    std::string currentSuggestionId;
};

// Security Alerts Controller
class SecurityAlertWidget {

public:
    explicit SecurityAlertWidget(void *parent = nullptr);
    void addIssue(const SecurityIssue& issue);
    void clearIssues();

    void issueFixed(const std::string& issueId);
    void issueIgnored(const std::string& issueId);

private:
    void onIssueClicked(QListWidgetItem* item);
    void onFixClicked();
    
    // Minimal state storage
    // std::map<std::string, SecurityIssue> issues; 
};

#endif
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

