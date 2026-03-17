/**
 * @file ai_review_widget.cpp
 * @brief Implementation of AIReviewWidget - AI code review
 */

#include "ai_review_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QComboBox>
#include <QProgressBar>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QDebug>

AIReviewWidget::AIReviewWidget(QWidget* parent)
    : QWidget(parent), mReviewInProgress(false)
{
    RAWRXD_INIT_TIMED("AIReviewWidget");
    setupUI();
    createIssuePanel();
    connectSignals();
    restoreState();
    
    setWindowTitle("AI Code Review");
}

AIReviewWidget::~AIReviewWidget()
{
    saveState();
}

void AIReviewWidget::setupUI()
{
    RAWRXD_TIMED_FUNC();
    mMainLayout = new QVBoxLayout(this);
    
    // Control panel
    mControlLayout = new QHBoxLayout();
    
    mStatusLabel = new QLabel("Ready", this);
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    mControlLayout->addWidget(mStatusLabel);
    
    mReviewButton = new QPushButton("Start Review", this);
    mReviewButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;");
    mControlLayout->addWidget(mReviewButton);
    
    mStopButton = new QPushButton("Stop Review", this);
    mStopButton->setStyleSheet("background-color: #f44336; color: white; padding: 8px;");
    mStopButton->setEnabled(false);
    mControlLayout->addWidget(mStopButton);
    
    mProgressBar = new QProgressBar(this);
    mProgressBar->setMaximumWidth(200);
    mControlLayout->addWidget(mProgressBar);
    
    mStatisticsButton = new QPushButton("Statistics", this);
    mControlLayout->addWidget(mStatisticsButton);
    
    mExportButton = new QPushButton("Export Report", this);
    mControlLayout->addWidget(mExportButton);
    
    mControlLayout->addStretch();
    
    mMainLayout->addLayout(mControlLayout);
    
    // Filter panel
    mFilterLayout = new QHBoxLayout();
    
    mFilterLayout->addWidget(new QLabel("Category:", this));
    mCategoryCombo = new QComboBox(this);
    mCategoryCombo->addItem("All");
    mCategoryCombo->addItem("Performance");
    mCategoryCombo->addItem("Security");
    mCategoryCombo->addItem("Style");
    mCategoryCombo->addItem("Bug");
    mFilterLayout->addWidget(mCategoryCombo);
    
    mFilterLayout->addSpacing(20);
    
    mFilterLayout->addWidget(new QLabel("Severity:", this));
    mSeverityCombo = new QComboBox(this);
    mSeverityCombo->addItem("All");
    mSeverityCombo->addItem("Critical");
    mSeverityCombo->addItem("Warning");
    mSeverityCombo->addItem("Info");
    mFilterLayout->addWidget(mSeverityCombo);
    
    mFilterLayout->addSpacing(20);
    
    mIssueCountLabel = new QLabel("Issues: 0", this);
    mFilterLayout->addWidget(mIssueCountLabel);
    
    mFilterLayout->addStretch();
    
    mMainLayout->addLayout(mFilterLayout);
}

void AIReviewWidget::createIssuePanel()
{
    // Issue tree
    mIssueTree = new QTreeWidget(this);
    mIssueTree->setHeaderLabels({"Line", "Severity", "Category", "Message"});
    mIssueTree->header()->setStretchLastSection(true);
    mMainLayout->addWidget(mIssueTree);
    
    // Detail panel
    mDetailLayout = new QHBoxLayout();
    
    // Left side - code and message
    QWidget* leftWidget = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    
    leftLayout->addWidget(new QLabel("Code Context:", this));
    mCodeEditor = new QTextEdit(this);
    mCodeEditor->setReadOnly(true);
    mCodeEditor->setFont(QFont("Courier", 9));
    mCodeEditor->setMaximumHeight(80);
    leftLayout->addWidget(mCodeEditor);
    
    leftLayout->addWidget(new QLabel("Issue:", this));
    mMessageEditor = new QTextEdit(this);
    mMessageEditor->setReadOnly(true);
    mMessageEditor->setMaximumHeight(60);
    leftLayout->addWidget(mMessageEditor);
    
    mDetailLayout->addWidget(leftWidget, 1);
    
    // Right side - suggestion
    QWidget* rightWidget = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    rightLayout->addWidget(new QLabel("Suggestion:", this));
    mSuggestionEditor = new QTextEdit(this);
    mSuggestionEditor->setReadOnly(true);
    mSuggestionEditor->setFont(QFont("Courier", 9));
    rightLayout->addWidget(mSuggestionEditor);
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    mApplyButton = new QPushButton("Apply Suggestion", this);
    mApplyButton->setEnabled(false);
    mApplyButton->setStyleSheet("background-color: #2196F3; color: white; padding: 8px;");
    buttonLayout->addWidget(mApplyButton);
    
    mIgnoreButton = new QPushButton("Ignore Issue", this);
    mIgnoreButton->setEnabled(false);
    mIgnoreButton->setStyleSheet("background-color: #757575; color: white; padding: 8px;");
    buttonLayout->addWidget(mIgnoreButton);
    
    rightLayout->addLayout(buttonLayout);
    
    mDetailLayout->addWidget(rightWidget, 1);
    
    mMainLayout->addLayout(mDetailLayout);
}

void AIReviewWidget::connectSignals()
{
    connect(mReviewButton, &QPushButton::clicked, this, &AIReviewWidget::onStartReview);
    connect(mStopButton, &QPushButton::clicked, this, &AIReviewWidget::onStopReview);
    connect(mIssueTree, &QTreeWidget::itemSelectionChanged, this, [this]() {
        QList<QTreeWidgetItem*> selected = mIssueTree->selectedItems();
        if (!selected.isEmpty()) {
            onSelectIssue(selected.first());
        }
    });
    connect(mApplyButton, &QPushButton::clicked, this, &AIReviewWidget::onApplySuggestion);
    connect(mIgnoreButton, &QPushButton::clicked, this, &AIReviewWidget::onIgnoreIssue);
    connect(mCategoryCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged), 
            this, &AIReviewWidget::onFilterByCategory);
    connect(mSeverityCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged), 
            this, &AIReviewWidget::onFilterBySeverity);
    connect(mExportButton, &QPushButton::clicked, this, &AIReviewWidget::onExportReport);
    connect(mStatisticsButton, &QPushButton::clicked, this, &AIReviewWidget::onShowStatistics);
}

void AIReviewWidget::onStartReview()
{
    RAWRXD_TIMED_FUNC();
    mReviewInProgress = true;
    mStatusLabel->setText("Review in progress...");
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: green;");
    mReviewButton->setEnabled(false);
    mStopButton->setEnabled(true);
    mProgressBar->setValue(0);
    
    emit reviewStarted();
}

void AIReviewWidget::onStopReview()
{
    RAWRXD_TIMED_FUNC();
    mReviewInProgress = false;
    mStatusLabel->setText("Review stopped");
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: red;");
    mReviewButton->setEnabled(true);
    mStopButton->setEnabled(false);
    
    emit reviewFinished(mReviewIssues.size());
}

void AIReviewWidget::onSelectIssue(QTreeWidgetItem* item)
{
    RAWRXD_TIMED_FUNC();
    int row = mIssueTree->indexOfTopLevelItem(item);
    if (row >= 0 && row < mReviewIssues.size()) {
        mCurrentIssue = mReviewIssues.at(row);
        
        mCodeEditor->setText(mCurrentIssue.code);
        mMessageEditor->setText(mCurrentIssue.message);
        mSuggestionEditor->setText(mCurrentIssue.suggestion);
        
        mApplyButton->setEnabled(true);
        mIgnoreButton->setEnabled(true);
    }
}

void AIReviewWidget::onApplySuggestion()
{
    emit suggestionApplied(mCurrentIssue);
    QMessageBox::information(this, "Applied", "Suggestion applied successfully!");
    
    // Remove from tree
    QList<QTreeWidgetItem*> selected = mIssueTree->selectedItems();
    if (!selected.isEmpty()) {
        delete mIssueTree->takeTopLevelItem(mIssueTree->indexOfTopLevelItem(selected.first()));
        mIssueCountLabel->setText(QString("Issues: %1").arg(mIssueTree->topLevelItemCount()));
    }
}

void AIReviewWidget::onIgnoreIssue()
{
    QList<QTreeWidgetItem*> selected = mIssueTree->selectedItems();
    if (!selected.isEmpty()) {
        delete mIssueTree->takeTopLevelItem(mIssueTree->indexOfTopLevelItem(selected.first()));
        mIssueCountLabel->setText(QString("Issues: %1").arg(mIssueTree->topLevelItemCount()));
    }
}

void AIReviewWidget::onFilterByCategory(const QString& category)
{
    RAWRXD_TIMED_FUNC();
    qDebug() << "Filter by category:" << category;
    
    // Show or hide tree items based on selected category
    if (category.isEmpty() || category == "All Categories") {
        // Show all items
        for (int i = 0; i < mIssueTree->topLevelItemCount(); ++i) {
            mIssueTree->topLevelItem(i)->setHidden(false);
        }
    } else {
        // Filter items by category
        for (int i = 0; i < mIssueTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = mIssueTree->topLevelItem(i);
            QString itemCategory = item->data(0, Qt::UserRole).toString();  // Assuming category is in role
            bool matches = itemCategory.contains(category, Qt::CaseInsensitive);
            item->setHidden(!matches);
        }
    }
    
    // Update issue count display
    int visibleCount = 0;
    for (int i = 0; i < mIssueTree->topLevelItemCount(); ++i) {
        if (!mIssueTree->topLevelItem(i)->isHidden()) {
            visibleCount++;
        }
    }
    mIssueCountLabel->setText(QString("Issues: %1 (%2 shown)").arg(mIssueTree->topLevelItemCount()).arg(visibleCount));
}

void AIReviewWidget::onFilterBySeverity(const QString& severity)
{
    RAWRXD_TIMED_FUNC();
    qDebug() << "Filter by severity:" << severity;
    
    // Show or hide tree items based on selected severity
    if (severity.isEmpty() || severity == "All Severities") {
        // Show all items
        for (int i = 0; i < mIssueTree->topLevelItemCount(); ++i) {
            mIssueTree->topLevelItem(i)->setHidden(false);
        }
    } else {
        // Filter items by severity
        for (int i = 0; i < mIssueTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = mIssueTree->topLevelItem(i);
            
            // Extract severity from item text (usually in column 1 or stored in role)
            QString itemText = item->text(1);  // Assuming severity is in column 1
            QString itemSeverity = item->data(1, Qt::UserRole).toString();
            
            // Match severity level
            bool matches = false;
            if (severity == "Critical") {
                matches = itemSeverity.contains("Critical", Qt::CaseInsensitive) || itemText.contains("Critical", Qt::CaseInsensitive);
            } else if (severity == "Warning") {
                matches = itemSeverity.contains("Warning", Qt::CaseInsensitive) || itemText.contains("Warning", Qt::CaseInsensitive);
            } else if (severity == "Info") {
                matches = itemSeverity.contains("Info", Qt::CaseInsensitive) || itemText.contains("Info", Qt::CaseInsensitive);
            }
            
            item->setHidden(!matches);
        }
    }
    
    // Update issue count display
    int visibleCount = 0;
    for (int i = 0; i < mIssueTree->topLevelItemCount(); ++i) {
        if (!mIssueTree->topLevelItem(i)->isHidden()) {
            visibleCount++;
        }
    }
    mIssueCountLabel->setText(QString("Issues: %1 (%2 shown)").arg(mIssueTree->topLevelItemCount()).arg(visibleCount));
}

void AIReviewWidget::onExportReport()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Report", "", "HTML Files (*.html);;Text Files (*.txt)");
    if (!filename.isEmpty()) {
        emit reportExported(filename);
        QMessageBox::information(this, "Exported", "Report exported successfully!");
    }
}

void AIReviewWidget::onShowStatistics()
{
    int total = mReviewIssues.size();
    int critical = 0, warning = 0, info = 0;
    
    for (const ReviewIssue& issue : mReviewIssues) {
        if (issue.severity == "critical") critical++;
        else if (issue.severity == "warning") warning++;
        else if (issue.severity == "info") info++;
    }
    
    QString stats = QString(
        "Review Statistics:\n\n"
        "Total Issues: %1\n"
        "Critical: %2\n"
        "Warnings: %3\n"
        "Info: %4")
        .arg(total).arg(critical).arg(warning).arg(info);
    
    QMessageBox::information(this, "Statistics", stats);
}

void AIReviewWidget::updateReviewProgress(int current, int total)
{
    mProgressBar->setMaximum(total);
    mProgressBar->setValue(current);
}

void AIReviewWidget::populateIssueTree()
{
    RAWRXD_TIMED_FUNC();
    mIssueTree->clear();
    for (const ReviewIssue& issue : mReviewIssues) {
        QTreeWidgetItem* item = new QTreeWidgetItem(mIssueTree);
        item->setText(0, QString::number(issue.lineNumber));
        item->setText(1, issue.severity);
        item->setText(2, issue.category);
        item->setText(3, issue.message);
    }
    mIssueCountLabel->setText(QString("Issues: %1").arg(mReviewIssues.size()));
}

void AIReviewWidget::restoreState()
{
    RAWRXD_TIMED_FUNC();
    QSettings settings("RawrXD", "IDE");
    // Restore last review state
}

void AIReviewWidget::saveState()
{
    RAWRXD_TIMED_FUNC();
    QSettings settings("RawrXD", "IDE");
    // Save current state
}
