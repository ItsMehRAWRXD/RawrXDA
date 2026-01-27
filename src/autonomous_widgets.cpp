// autonomous_widgets.cpp - FULLY FUNCTIONAL Custom UI Widgets
#include "autonomous_widgets.h"
#include <QSplitter>
#include <QGroupBox>
#include <iostream>

// ============================================================================
// AutonomousSuggestionWidget - FUNCTIONAL AI Suggestions Panel
// ============================================================================

AutonomousSuggestionWidget::AutonomousSuggestionWidget(QWidget *parent)
    : QWidget(parent) {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Suggestion list
    QLabel* titleLabel = new QLabel("AI Suggestions");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt;");
    mainLayout->addWidget(titleLabel);
    
    suggestionList = new QListWidget(this);
    mainLayout->addWidget(suggestionList);
    
    // Details view
    QGroupBox* detailsGroup = new QGroupBox("Details");
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);
    
    detailsView = new QTextEdit(this);
    detailsView->setReadOnly(true);
    detailsView->setMaximumHeight(200);
    detailsLayout->addWidget(detailsView);
    
    confidenceLabel = new QLabel("Confidence: N/A");
    detailsLayout->addWidget(confidenceLabel);
    
    mainLayout->addWidget(detailsGroup);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    acceptButton = new QPushButton("✓ Accept");
    acceptButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    acceptButton->setEnabled(false);
    buttonLayout->addWidget(acceptButton);
    
    rejectButton = new QPushButton("✗ Reject");
    rejectButton->setStyleSheet("background-color: #f44336; color: white; font-weight: bold;");
    rejectButton->setEnabled(false);
    buttonLayout->addWidget(rejectButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(suggestionList, &QListWidget::itemClicked, this, &AutonomousSuggestionWidget::onSuggestionClicked);
    connect(acceptButton, &QPushButton::clicked, this, &AutonomousSuggestionWidget::onAcceptClicked);
    connect(rejectButton, &QPushButton::clicked, this, &AutonomousSuggestionWidget::onRejectClicked);
    
    setLayout(mainLayout);
}

void AutonomousSuggestionWidget::addSuggestion(const AutonomousSuggestion& suggestion) {
    suggestions[suggestion.suggestionId] = suggestion;
    
    QString typeIcon;
    if (suggestion.type == "test_generation") typeIcon = "🧪";
    else if (suggestion.type == "optimization") typeIcon = "⚡";
    else if (suggestion.type == "refactoring") typeIcon = "🔧";
    else if (suggestion.type == "security_fix") typeIcon = "🔒";
    else if (suggestion.type == "documentation") typeIcon = "📝";
    else typeIcon = "💡";
    
    QString itemText = QString("%1 %2 (%.0f%% confident)")
        .arg(typeIcon)
        .arg(suggestion.explanation)
        .arg(suggestion.confidence * 100);
    
    QListWidgetItem* item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, suggestion.suggestionId);
    
    // Color code by confidence
    if (suggestion.confidence >= 0.9) {
        item->setBackground(QColor(200, 255, 200)); // Light green
    } else if (suggestion.confidence >= 0.7) {
        item->setBackground(QColor(255, 255, 200)); // Light yellow
    } else {
        item->setBackground(QColor(255, 230, 230)); // Light red
    }
    
    suggestionList->addItem(item);
    
    std::cout << "[SuggestionWidget] Added suggestion: " << suggestion.type.toStdString() << std::endl;
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
    currentSuggestionId = item->data(Qt::UserRole).toString();
    
    if (!suggestions.contains(currentSuggestionId)) {
        return;
    }
    
    const AutonomousSuggestion& suggestion = suggestions[currentSuggestionId];
    
    // Show details
    QString details;
    details += "Type: " + suggestion.type + "\n\n";
    details += "Explanation: " + suggestion.explanation + "\n\n";
    details += "Benefits:\n";
    for (const QString& benefit : suggestion.benefits) {
        details += "  • " + benefit + "\n";
    }
    details += "\n--- Suggested Code ---\n";
    details += suggestion.suggestedCode;
    
    detailsView->setText(details);
    confidenceLabel->setText(QString("Confidence: %1%").arg(suggestion.confidence * 100, 0, 'f', 0));
    
    acceptButton->setEnabled(true);
    rejectButton->setEnabled(true);
}

void AutonomousSuggestionWidget::onAcceptClicked() {
    if (currentSuggestionId.isEmpty()) {
        return;
    }
    
    emit suggestionAccepted(currentSuggestionId);
    
    // Remove from list
    for (int i = 0; i < suggestionList->count(); ++i) {
        QListWidgetItem* item = suggestionList->item(i);
        if (item->data(Qt::UserRole).toString() == currentSuggestionId) {
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
    if (currentSuggestionId.isEmpty()) {
        return;
    }
    
    emit suggestionRejected(currentSuggestionId);
    
    // Remove from list
    for (int i = 0; i < suggestionList->count(); ++i) {
        QListWidgetItem* item = suggestionList->item(i);
        if (item->data(Qt::UserRole).toString() == currentSuggestionId) {
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

SecurityAlertWidget::SecurityAlertWidget(QWidget *parent)
    : QWidget(parent) {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Title
    QLabel* titleLabel = new QLabel("🔒 Security Alerts");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #d32f2f;");
    mainLayout->addWidget(titleLabel);
    
    // Issue list
    issueList = new QListWidget(this);
    mainLayout->addWidget(issueList);
    
    // Details view
    QGroupBox* detailsGroup = new QGroupBox("Issue Details");
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);
    
    issueDetails = new QTextEdit(this);
    issueDetails->setReadOnly(true);
    issueDetails->setMaximumHeight(200);
    detailsLayout->addWidget(issueDetails);
    
    riskScoreLabel = new QLabel("Risk Score: N/A");
    riskScoreLabel->setStyleSheet("font-weight: bold;");
    detailsLayout->addWidget(riskScoreLabel);
    
    mainLayout->addWidget(detailsGroup);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    fixButton = new QPushButton("🔧 Apply Fix");
    fixButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold;");
    fixButton->setEnabled(false);
    buttonLayout->addWidget(fixButton);
    
    ignoreButton = new QPushButton("⊘ Ignore");
    ignoreButton->setStyleSheet("background-color: #757575; color: white;");
    ignoreButton->setEnabled(false);
    buttonLayout->addWidget(ignoreButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(issueList, &QListWidget::itemClicked, this, &SecurityAlertWidget::onIssueClicked);
    connect(fixButton, &QPushButton::clicked, this, &SecurityAlertWidget::onFixClicked);
    connect(ignoreButton, &QPushButton::clicked, this, &SecurityAlertWidget::onIgnoreClicked);
    
    setLayout(mainLayout);
}

void SecurityAlertWidget::addIssue(const SecurityIssue& issue) {
    issues[issue.issueId] = issue;
    
    QString severityIcon;
    if (issue.severity == "critical") severityIcon = "🔴";
    else if (issue.severity == "high") severityIcon = "🟠";
    else if (issue.severity == "medium") severityIcon = "🟡";
    else severityIcon = "🟢";
    
    QString itemText = QString("%1 %2 - %3")
        .arg(severityIcon)
        .arg(issue.type.toUpper())
        .arg(issue.description);
    
    QListWidgetItem* item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, issue.issueId);
    item->setBackground(QColor(getSeverityColor(issue.severity)));
    
    issueList->addItem(item);
    
    std::cout << "[SecurityWidget] Added issue: " << issue.type.toStdString() 
              << " (risk: " << issue.riskScore << ")" << std::endl;
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
    currentIssueId = item->data(Qt::UserRole).toString();
    
    if (!issues.contains(currentIssueId)) {
        return;
    }
    
    const SecurityIssue& issue = issues[currentIssueId];
    
    QString details;
    details += "Type: " + issue.type + "\n";
    details += "Severity: " + issue.severity.toUpper() + "\n";
    details += "CVE Reference: " + issue.cveReference + "\n\n";
    details += "Description:\n" + issue.description + "\n\n";
    details += "Vulnerable Code:\n" + issue.vulnerableCode + "\n\n";
    details += "Suggested Fix:\n" + issue.suggestedFix;
    
    issueDetails->setText(details);
    riskScoreLabel->setText(QString("Risk Score: %1/10").arg(issue.riskScore, 0, 'f', 1));
    
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
    if (currentIssueId.isEmpty()) {
        return;
    }
    
    emit issueFixed(currentIssueId);
    
    // Remove from list
    for (int i = 0; i < issueList->count(); ++i) {
        QListWidgetItem* item = issueList->item(i);
        if (item->data(Qt::UserRole).toString() == currentIssueId) {
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
    if (currentIssueId.isEmpty()) {
        return;
    }
    
    emit issueIgnored(currentIssueId);
    
    // Remove from list
    for (int i = 0; i < issueList->count(); ++i) {
        QListWidgetItem* item = issueList->item(i);
        if (item->data(Qt::UserRole).toString() == currentIssueId) {
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

QString SecurityAlertWidget::getSeverityColor(const QString& severity) const {
    if (severity == "critical") return "#ffcdd2";  // Light red
    if (severity == "high") return "#ffe0b2";      // Light orange
    if (severity == "medium") return "#fff9c4";    // Light yellow
    return "#c8e6c9";                              // Light green
}

// ============================================================================
// OptimizationPanelWidget - FUNCTIONAL Performance Optimization Panel
// ============================================================================

OptimizationPanelWidget::OptimizationPanelWidget(QWidget *parent)
    : QWidget(parent) {
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Title
    QLabel* titleLabel = new QLabel("⚡ Performance Optimizations");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; color: #1976d2;");
    mainLayout->addWidget(titleLabel);
    
    // Optimization list
    optimizationList = new QListWidget(this);
    mainLayout->addWidget(optimizationList);
    
    // Details view
    QGroupBox* detailsGroup = new QGroupBox("Optimization Details");
    QVBoxLayout* detailsLayout = new QVBoxLayout(detailsGroup);
    
    optimizationDetails = new QTextEdit(this);
    optimizationDetails->setReadOnly(true);
    optimizationDetails->setMaximumHeight(200);
    detailsLayout->addWidget(optimizationDetails);
    
    QHBoxLayout* metricsLayout = new QHBoxLayout();
    
    speedupLabel = new QLabel("Expected Speedup: N/A");
    speedupLabel->setStyleSheet("font-weight: bold; color: #1976d2;");
    metricsLayout->addWidget(speedupLabel);
    
    QLabel* confLabel = new QLabel("Confidence:");
    metricsLayout->addWidget(confLabel);
    
    confidenceBar = new QProgressBar(this);
    confidenceBar->setMaximum(100);
    confidenceBar->setTextVisible(true);
    metricsLayout->addWidget(confidenceBar);
    
    detailsLayout->addLayout(metricsLayout);
    
    mainLayout->addWidget(detailsGroup);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    applyButton = new QPushButton("✓ Apply Optimization");
    applyButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    applyButton->setEnabled(false);
    buttonLayout->addWidget(applyButton);
    
    dismissButton = new QPushButton("✗ Dismiss");
    dismissButton->setStyleSheet("background-color: #9E9E9E; color: white;");
    dismissButton->setEnabled(false);
    buttonLayout->addWidget(dismissButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connections
    connect(optimizationList, &QListWidget::itemClicked, this, &OptimizationPanelWidget::onOptimizationClicked);
    connect(applyButton, &QPushButton::clicked, this, &OptimizationPanelWidget::onApplyClicked);
    connect(dismissButton, &QPushButton::clicked, this, &OptimizationPanelWidget::onDismissClicked);
    
    setLayout(mainLayout);
}

void OptimizationPanelWidget::addOptimization(const PerformanceOptimization& optimization) {
    optimizations[optimization.optimizationId] = optimization;
    
    QString typeIcon;
    if (optimization.type == "parallelization") typeIcon = "🔀";
    else if (optimization.type == "caching") typeIcon = "💾";
    else if (optimization.type == "algorithm") typeIcon = "📊";
    else if (optimization.type == "memory") typeIcon = "🧠";
    else typeIcon = "⚡";
    
    QString itemText = QString("%1 %2 - %3x faster")
        .arg(typeIcon)
        .arg(optimization.type.toUpper())
        .arg(optimization.expectedSpeedup, 0, 'f', 1);
    
    QListWidgetItem* item = new QListWidgetItem(itemText);
    item->setData(Qt::UserRole, optimization.optimizationId);
    
    // Color code by speedup
    if (optimization.expectedSpeedup >= 5.0) {
        item->setBackground(QColor(200, 255, 200)); // Excellent
    } else if (optimization.expectedSpeedup >= 2.0) {
        item->setBackground(QColor(220, 255, 220)); // Good
    } else {
        item->setBackground(QColor(240, 255, 240)); // Moderate
    }
    
    optimizationList->addItem(item);
    
    std::cout << "[OptimizationWidget] Added optimization: " << optimization.type.toStdString() 
              << " (" << optimization.expectedSpeedup << "x)" << std::endl;
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
    currentOptimizationId = item->data(Qt::UserRole).toString();
    
    if (!optimizations.contains(currentOptimizationId)) {
        return;
    }
    
    const PerformanceOptimization& opt = optimizations[currentOptimizationId];
    
    QString details;
    details += "Type: " + opt.type + "\n\n";
    details += "Reasoning:\n" + opt.reasoning + "\n\n";
    details += "Current Implementation:\n" + opt.currentImplementation + "\n\n";
    details += "Optimized Implementation:\n" + opt.optimizedImplementation + "\n\n";
    details += QString("Expected Speedup: %1x\n").arg(opt.expectedSpeedup, 0, 'f', 2);
    
    if (opt.expectedMemorySaving > 0) {
        details += QString("Memory Saved: %1 MB\n").arg(opt.expectedMemorySaving / (1024.0 * 1024.0), 0, 'f', 2);
    }
    
    optimizationDetails->setText(details);
    speedupLabel->setText(QString("Expected Speedup: %1x").arg(opt.expectedSpeedup, 0, 'f', 1));
    confidenceBar->setValue(static_cast<int>(opt.confidence * 100));
    
    applyButton->setEnabled(true);
    dismissButton->setEnabled(true);
}

void OptimizationPanelWidget::onApplyClicked() {
    if (currentOptimizationId.isEmpty()) {
        return;
    }
    
    emit optimizationApplied(currentOptimizationId);
    
    // Remove from list
    for (int i = 0; i < optimizationList->count(); ++i) {
        QListWidgetItem* item = optimizationList->item(i);
        if (item->data(Qt::UserRole).toString() == currentOptimizationId) {
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
    if (currentOptimizationId.isEmpty()) {
        return;
    }
    
    emit optimizationDismissed(currentOptimizationId);
    
    // Remove from list
    for (int i = 0; i < optimizationList->count(); ++i) {
        QListWidgetItem* item = optimizationList->item(i);
        if (item->data(Qt::UserRole).toString() == currentOptimizationId) {
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
