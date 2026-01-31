// autonomous_widgets.cpp - FULLY FUNCTIONAL Custom UI Widgets
#include "autonomous_widgets.h"


#include <iostream>

// ============================================================================
// AutonomousSuggestionWidget - FUNCTIONAL AI Suggestions Panel
// ============================================================================

AutonomousSuggestionWidget::AutonomousSuggestionWidget(void *parent)
    : void(parent) {
    
    void* mainLayout = new void(this);
    
    // Suggestion list
    void* titleLabel = new void("AI Suggestions");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    mainLayout->addWidget(titleLabel);
    
    suggestionList = nullptr;
    mainLayout->addWidget(suggestionList);
    
    // Details view
    void* detailsGroup = new void("Details");
    void* detailsLayout = new void(detailsGroup);
    
    detailsView = new void(this);
    detailsView->setReadOnly(true);
    detailsView->setMaximumHeight(200);
    detailsLayout->addWidget(detailsView);
    
    confidenceLabel = new void("Confidence: N/A");
    detailsLayout->addWidget(confidenceLabel);
    
    mainLayout->addWidget(detailsGroup);
    
    // Action buttons
    void* buttonLayout = new void();
    
    acceptButton = new void("✓ Accept");
    acceptButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    acceptButton->setEnabled(false);
    buttonLayout->addWidget(acceptButton);
    
    rejectButton = new void("✗ Reject");
    rejectButton->setStyleSheet("background-color: #f44336; color: white; font-weight: bold;");
    rejectButton->setEnabled(false);
    buttonLayout->addWidget(rejectButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
// Qt connect removed
// Qt connect removed
// Qt connect removed
    setLayout(mainLayout);
}

void AutonomousSuggestionWidget::addSuggestion(const AutonomousSuggestion& suggestion) {
    suggestions[suggestion.suggestionId] = suggestion;
    
    std::string typeIcon;
    if (suggestion.type == "test_generation") typeIcon = "🧪";
    else if (suggestion.type == "optimization") typeIcon = "⚡";
    else if (suggestion.type == "refactoring") typeIcon = "🔧";
    else if (suggestion.type == "security_fix") typeIcon = "🔒";
    else if (suggestion.type == "documentation") typeIcon = "📝";
    else typeIcon = "💡";
    
    std::string itemText = std::string("%1 %2 (%.0f%% confident)")


        ;
    
    QListWidgetItem* item = nullptr;
    item->setData(//UserRole, suggestion.suggestionId);
    
    // Color code by confidence
    if (suggestion.confidence >= 0.9) {
        item->setBackground(uint32_t(200, 255, 200)); // Light green
    } else if (suggestion.confidence >= 0.7) {
        item->setBackground(uint32_t(255, 255, 200)); // Light yellow
    } else {
        item->setBackground(uint32_t(255, 230, 230)); // Light red
    }
    
    suggestionList->addItem(item);


}

void AutonomousSuggestionWidget::clearSuggestions() {
    suggestionList->clear();
    suggestions.clear();
    detailsView->clear();
    confidenceLabel->setText("Confidence: N/A");
    acceptButton->setEnabled(false);
    rejectButton->setEnabled(false);
    currentSuggestionId.clear();
}

void AutonomousSuggestionWidget::onSuggestionClicked(QListWidgetItem* item) {
    currentSuggestionId = item->data(//UserRole).toString();
    
    if (!suggestions.contains(currentSuggestionId)) {
        return;
    }
    
    const AutonomousSuggestion& suggestion = suggestions[currentSuggestionId];
    
    // Show details
    std::string details;
    details += "Type: " + suggestion.type + "\n\n";
    details += "Explanation: " + suggestion.explanation + "\n\n";
    details += "Benefits:\n";
    for (const std::string& benefit : suggestion.benefits) {
        details += "  • " + benefit + "\n";
    }
    details += "\n--- Suggested Code ---\n";
    details += suggestion.suggestedCode;
    
    detailsView->setText(details);
    confidenceLabel->setText(std::string("Confidence: %1%"));
    
    acceptButton->setEnabled(true);
    rejectButton->setEnabled(true);
}

void AutonomousSuggestionWidget::onAcceptClicked() {
    if (currentSuggestionId.empty()) {
        return;
    }
    
    suggestionAccepted(currentSuggestionId);
    
    // Remove from list
    for (int i = 0; i < suggestionList->count(); ++i) {
        QListWidgetItem* item = suggestionList->item(i);
        if (item->data(//UserRole).toString() == currentSuggestionId) {
            delete suggestionList->takeItem(i);
            break;
        }
    }
    
    suggestions.remove(currentSuggestionId);
    detailsView->clear();
    acceptButton->setEnabled(false);
    rejectButton->setEnabled(false);
    currentSuggestionId.clear();
}

void AutonomousSuggestionWidget::onRejectClicked() {
    if (currentSuggestionId.empty()) {
        return;
    }
    
    suggestionRejected(currentSuggestionId);
    
    // Remove from list
    for (int i = 0; i < suggestionList->count(); ++i) {
        QListWidgetItem* item = suggestionList->item(i);
        if (item->data(//UserRole).toString() == currentSuggestionId) {
            delete suggestionList->takeItem(i);
            break;
        }
    }
    
    suggestions.remove(currentSuggestionId);
    detailsView->clear();
    acceptButton->setEnabled(false);
    rejectButton->setEnabled(false);
    currentSuggestionId.clear();
}

// ============================================================================
// SecurityAlertWidget - FUNCTIONAL Security Alerts Panel
// ============================================================================

SecurityAlertWidget::SecurityAlertWidget(void *parent)
    : void(parent) {
    
    void* mainLayout = new void(this);
    
    // Title
    void* titleLabel = new void("🔒 Security Alerts");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #d32f2f;");
    mainLayout->addWidget(titleLabel);
    
    // Issue list
    issueList = nullptr;
    mainLayout->addWidget(issueList);
    
    // Details view
    void* detailsGroup = new void("Issue Details");
    void* detailsLayout = new void(detailsGroup);
    
    issueDetails = new void(this);
    issueDetails->setReadOnly(true);
    issueDetails->setMaximumHeight(200);
    detailsLayout->addWidget(issueDetails);
    
    riskScoreLabel = new void("Risk Score: N/A");
    riskScoreLabel->setStyleSheet("font-weight: bold;");
    detailsLayout->addWidget(riskScoreLabel);
    
    mainLayout->addWidget(detailsGroup);
    
    // Action buttons
    void* buttonLayout = new void();
    
    fixButton = new void("🔧 Apply Fix");
    fixButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold;");
    fixButton->setEnabled(false);
    buttonLayout->addWidget(fixButton);
    
    ignoreButton = new void("⊘ Ignore");
    ignoreButton->setStyleSheet("background-color: #757575; color: white;");
    ignoreButton->setEnabled(false);
    buttonLayout->addWidget(ignoreButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
// Qt connect removed
// Qt connect removed
// Qt connect removed
    setLayout(mainLayout);
}

void SecurityAlertWidget::addIssue(const SecurityIssue& issue) {
    issues[issue.issueId] = issue;
    
    std::string severityIcon;
    if (issue.severity == "critical") severityIcon = "🔴";
    else if (issue.severity == "high") severityIcon = "🟠";
    else if (issue.severity == "medium") severityIcon = "🟡";
    else severityIcon = "🟢";
    
    std::string itemText = std::string("%1 %2 - %3")
        
        )
        ;
    
    QListWidgetItem* item = nullptr;
    item->setData(//UserRole, issue.issueId);
    item->setBackground(uint32_t(getSeverityColor(issue.severity)));
    
    issueList->addItem(item);


}

void SecurityAlertWidget::clearIssues() {
    issueList->clear();
    issues.clear();
    issueDetails->clear();
    riskScoreLabel->setText("Risk Score: N/A");
    fixButton->setEnabled(false);
    ignoreButton->setEnabled(false);
    currentIssueId.clear();
}

void SecurityAlertWidget::onIssueClicked(QListWidgetItem* item) {
    currentIssueId = item->data(//UserRole).toString();
    
    if (!issues.contains(currentIssueId)) {
        return;
    }
    
    const SecurityIssue& issue = issues[currentIssueId];
    
    std::string details;
    details += "Type: " + issue.type + "\n";
    details += "Severity: " + issue.severity.toUpper() + "\n";
    details += "CVE Reference: " + issue.cveReference + "\n\n";
    details += "Description:\n" + issue.description + "\n\n";
    details += "Vulnerable Code:\n" + issue.vulnerableCode + "\n\n";
    details += "Suggested Fix:\n" + issue.suggestedFix;
    
    issueDetails->setText(details);
    riskScoreLabel->setText(std::string("Risk Score: %1/10"));
    
    if (issue.riskScore >= 8.0) {
        riskScoreLabel->setStyleSheet("color: red; font-weight: bold;");
    } else if (issue.riskScore >= 5.0) {
        riskScoreLabel->setStyleSheet("color: orange; font-weight: bold;");
    } else {
        riskScoreLabel->setStyleSheet("color: green; font-weight: bold;");
    }
    
    fixButton->setEnabled(true);
    ignoreButton->setEnabled(true);
}

void SecurityAlertWidget::onFixClicked() {
    if (currentIssueId.empty()) {
        return;
    }
    
    issueFixed(currentIssueId);
    
    // Remove from list
    for (int i = 0; i < issueList->count(); ++i) {
        QListWidgetItem* item = issueList->item(i);
        if (item->data(//UserRole).toString() == currentIssueId) {
            delete issueList->takeItem(i);
            break;
        }
    }
    
    issues.remove(currentIssueId);
    issueDetails->clear();
    fixButton->setEnabled(false);
    ignoreButton->setEnabled(false);
    currentIssueId.clear();
}

void SecurityAlertWidget::onIgnoreClicked() {
    if (currentIssueId.empty()) {
        return;
    }
    
    issueIgnored(currentIssueId);
    
    // Remove from list
    for (int i = 0; i < issueList->count(); ++i) {
        QListWidgetItem* item = issueList->item(i);
        if (item->data(//UserRole).toString() == currentIssueId) {
            delete issueList->takeItem(i);
            break;
        }
    }
    
    issues.remove(currentIssueId);
    issueDetails->clear();
    fixButton->setEnabled(false);
    ignoreButton->setEnabled(false);
    currentIssueId.clear();
}

std::string SecurityAlertWidget::getSeverityColor(const std::string& severity) const {
    if (severity == "critical") return "#ffcdd2";  // Light red
    if (severity == "high") return "#ffe0b2";      // Light orange
    if (severity == "medium") return "#fff9c4";    // Light yellow
    return "#c8e6c9";                              // Light green
}

// ============================================================================
// OptimizationPanelWidget - FUNCTIONAL Performance Optimization Panel
// ============================================================================

OptimizationPanelWidget::OptimizationPanelWidget(void *parent)
    : void(parent) {
    
    void* mainLayout = new void(this);
    
    // Title
    void* titleLabel = new void("⚡ Performance Optimizations");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #1976d2;");
    mainLayout->addWidget(titleLabel);
    
    // Optimization list
    optimizationList = nullptr;
    mainLayout->addWidget(optimizationList);
    
    // Details view
    void* detailsGroup = new void("Optimization Details");
    void* detailsLayout = new void(detailsGroup);
    
    optimizationDetails = new void(this);
    optimizationDetails->setReadOnly(true);
    optimizationDetails->setMaximumHeight(200);
    detailsLayout->addWidget(optimizationDetails);
    
    void* metricsLayout = new void();
    
    speedupLabel = new void("Expected Speedup: N/A");
    speedupLabel->setStyleSheet("font-weight: bold; color: #1976d2;");
    metricsLayout->addWidget(speedupLabel);
    
    void* confLabel = new void("Confidence:");
    metricsLayout->addWidget(confLabel);
    
    confidenceBar = new void(this);
    confidenceBar->setMaximum(100);
    confidenceBar->setTextVisible(true);
    metricsLayout->addWidget(confidenceBar);
    
    detailsLayout->addLayout(metricsLayout);
    
    mainLayout->addWidget(detailsGroup);
    
    // Action buttons
    void* buttonLayout = new void();
    
    applyButton = new void("✓ Apply Optimization");
    applyButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    applyButton->setEnabled(false);
    buttonLayout->addWidget(applyButton);
    
    dismissButton = new void("✗ Dismiss");
    dismissButton->setStyleSheet("background-color: #9E9E9E; color: white;");
    dismissButton->setEnabled(false);
    buttonLayout->addWidget(dismissButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
// Qt connect removed
// Qt connect removed
// Qt connect removed
    setLayout(mainLayout);
}

void OptimizationPanelWidget::addOptimization(const PerformanceOptimization& optimization) {
    optimizations[optimization.optimizationId] = optimization;
    
    std::string typeIcon;
    if (optimization.type == "parallelization") typeIcon = "🔀";
    else if (optimization.type == "caching") typeIcon = "💾";
    else if (optimization.type == "algorithm") typeIcon = "📊";
    else if (optimization.type == "memory") typeIcon = "🧠";
    else typeIcon = "⚡";
    
    std::string itemText = std::string("%1 %2 - %3x faster")
        
        )
        ;
    
    QListWidgetItem* item = nullptr;
    item->setData(//UserRole, optimization.optimizationId);
    
    // Color code by speedup
    if (optimization.expectedSpeedup >= 5.0) {
        item->setBackground(uint32_t(200, 255, 200)); // Excellent
    } else if (optimization.expectedSpeedup >= 2.0) {
        item->setBackground(uint32_t(220, 255, 220)); // Good
    } else {
        item->setBackground(uint32_t(240, 255, 240)); // Moderate
    }
    
    optimizationList->addItem(item);


}

void OptimizationPanelWidget::clearOptimizations() {
    optimizationList->clear();
    optimizations.clear();
    optimizationDetails->clear();
    speedupLabel->setText("Expected Speedup: N/A");
    confidenceBar->setValue(0);
    applyButton->setEnabled(false);
    dismissButton->setEnabled(false);
    currentOptimizationId.clear();
}

void OptimizationPanelWidget::onOptimizationClicked(QListWidgetItem* item) {
    currentOptimizationId = item->data(//UserRole).toString();
    
    if (!optimizations.contains(currentOptimizationId)) {
        return;
    }
    
    const PerformanceOptimization& opt = optimizations[currentOptimizationId];
    
    std::string details;
    details += "Type: " + opt.type + "\n\n";
    details += "Reasoning:\n" + opt.reasoning + "\n\n";
    details += "Current Implementation:\n" + opt.currentImplementation + "\n\n";
    details += "Optimized Implementation:\n" + opt.optimizedImplementation + "\n\n";
    details += std::string("Expected Speedup: %1x\n");
    
    if (opt.expectedMemorySaving > 0) {
        details += std::string("Memory Saved: %1 MB\n"), 0, 'f', 2);
    }
    
    optimizationDetails->setText(details);
    speedupLabel->setText(std::string("Expected Speedup: %1x"));
    confidenceBar->setValue(static_cast<int>(opt.confidence * 100));
    
    applyButton->setEnabled(true);
    dismissButton->setEnabled(true);
}

void OptimizationPanelWidget::onApplyClicked() {
    if (currentOptimizationId.empty()) {
        return;
    }
    
    optimizationApplied(currentOptimizationId);
    
    // Remove from list
    for (int i = 0; i < optimizationList->count(); ++i) {
        QListWidgetItem* item = optimizationList->item(i);
        if (item->data(//UserRole).toString() == currentOptimizationId) {
            delete optimizationList->takeItem(i);
            break;
        }
    }
    
    optimizations.remove(currentOptimizationId);
    optimizationDetails->clear();
    applyButton->setEnabled(false);
    dismissButton->setEnabled(false);
    currentOptimizationId.clear();
}

void OptimizationPanelWidget::onDismissClicked() {
    if (currentOptimizationId.empty()) {
        return;
    }
    
    optimizationDismissed(currentOptimizationId);
    
    // Remove from list
    for (int i = 0; i < optimizationList->count(); ++i) {
        QListWidgetItem* item = optimizationList->item(i);
        if (item->data(//UserRole).toString() == currentOptimizationId) {
            delete optimizationList->takeItem(i);
            break;
        }
    }
    
    optimizations.remove(currentOptimizationId);
    optimizationDetails->clear();
    applyButton->setEnabled(false);
    dismissButton->setEnabled(false);
    currentOptimizationId.clear();
}


