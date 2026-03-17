/**
 * @file ai_review_widget.h
 * @brief Header for AIReviewWidget - AI-powered code review interface
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QList>

class QVBoxLayout;
class QHBoxLayout;
class QTextEdit;
class QPushButton;
class QLabel;
class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QProgressBar;

struct ReviewIssue {
    QString id;
    QString severity; // "critical", "warning", "info"
    int lineNumber;
    QString code;
    QString message;
    QString suggestion;
    QString category; // "performance", "security", "style", "bug"
};

class AIReviewWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit AIReviewWidget(QWidget* parent = nullptr);
    ~AIReviewWidget();
    
public slots:
    void onStartReview();
    void onStopReview();
    void onSelectIssue(QTreeWidgetItem* item);
    void onApplySuggestion();
    void onIgnoreIssue();
    void onFilterByCategory(const QString& category);
    void onFilterBySeverity(const QString& severity);
    void onExportReport();
    void onShowStatistics();
    void updateReviewProgress(int current, int total);
    
signals:
    void reviewStarted();
    void reviewFinished(int issueCount);
    void suggestionApplied(const ReviewIssue& issue);
    void reportExported(const QString& filename);
    
private:
    void setupUI();
    void createIssuePanel();
    void connectSignals();
    void populateIssueTree();
    void restoreState();
    void saveState();
    
    // UI Components
    QVBoxLayout* mMainLayout;
    QHBoxLayout* mControlLayout;
    QHBoxLayout* mFilterLayout;
    QHBoxLayout* mDetailLayout;
    
    // Review control
    QPushButton* mReviewButton;
    QPushButton* mStopButton;
    QLabel* mStatusLabel;
    QProgressBar* mProgressBar;
    QPushButton* mStatisticsButton;
    
    // Filter controls
    QComboBox* mCategoryCombo;
    QComboBox* mSeverityCombo;
    QLabel* mIssueCountLabel;
    
    // Issue tree
    QTreeWidget* mIssueTree;
    
    // Issue details
    QTextEdit* mCodeEditor;
    QTextEdit* mMessageEditor;
    QTextEdit* mSuggestionEditor;
    
    // Action buttons
    QPushButton* mApplyButton;
    QPushButton* mIgnoreButton;
    QPushButton* mExportButton;
    
    // State tracking
    bool mReviewInProgress;
    QList<ReviewIssue> mReviewIssues;
    ReviewIssue mCurrentIssue;
};

#endif // AI_REVIEW_WIDGET_H
