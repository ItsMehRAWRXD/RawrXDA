// autonomous_widgets.h - Custom UI Widgets for AI Features
#ifndef AUTONOMOUS_WIDGETS_H
#define AUTONOMOUS_WIDGETS_H

#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include "autonomous_feature_engine.h"

// AI Suggestions Widget
class AutonomousSuggestionWidget : public QWidget {
    Q_OBJECT

public:
    explicit AutonomousSuggestionWidget(QWidget *parent = nullptr);
    void addSuggestion(const AutonomousSuggestion& suggestion);
    void clearSuggestions();

signals:
    void suggestionAccepted(const QString& suggestionId);
    void suggestionRejected(const QString& suggestionId);

private slots:
    void onSuggestionClicked(QListWidgetItem* item);
    void onAcceptClicked();
    void onRejectClicked();

private:
    QListWidget* suggestionList;
    QTextEdit* detailsView;
    QPushButton* acceptButton;
    QPushButton* rejectButton;
    QLabel* confidenceLabel;
    
    QMap<QString, AutonomousSuggestion> suggestions;
    QString currentSuggestionId;
};

// Security Alerts Widget
class SecurityAlertWidget : public QWidget {
    Q_OBJECT

public:
    explicit SecurityAlertWidget(QWidget *parent = nullptr);
    void addIssue(const SecurityIssue& issue);
    void clearIssues();

signals:
    void issueFixed(const QString& issueId);
    void issueIgnored(const QString& issueId);

private slots:
    void onIssueClicked(QListWidgetItem* item);
    void onFixClicked();
    void onIgnoreClicked();

private:
    QListWidget* issueList;
    QTextEdit* issueDetails;
    QPushButton* fixButton;
    QPushButton* ignoreButton;
    QLabel* riskScoreLabel;
    
    QMap<QString, SecurityIssue> issues;
    QString currentIssueId;
    
    QString getSeverityColor(const QString& severity) const;
};

// Performance Optimization Widget
class OptimizationPanelWidget : public QWidget {
    Q_OBJECT

public:
    explicit OptimizationPanelWidget(QWidget *parent = nullptr);
    void addOptimization(const PerformanceOptimization& optimization);
    void clearOptimizations();

signals:
    void optimizationApplied(const QString& optimizationId);
    void optimizationDismissed(const QString& optimizationId);

private slots:
    void onOptimizationClicked(QListWidgetItem* item);
    void onApplyClicked();
    void onDismissClicked();

private:
    QListWidget* optimizationList;
    QTextEdit* optimizationDetails;
    QPushButton* applyButton;
    QPushButton* dismissButton;
    QLabel* speedupLabel;
    QProgressBar* confidenceBar;
    
    QMap<QString, PerformanceOptimization> optimizations;
    QString currentOptimizationId;
};

#endif // AUTONOMOUS_WIDGETS_H
