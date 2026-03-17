/**
 * @file code_stream_widget.cpp
 * @brief Implementation of CodeStreamWidget - Live code streaming
 */

#include "code_stream_widget.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QSplitter>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDateTime>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDebug>

CodeStreamWidget::CodeStreamWidget(QWidget* parent)
    : QWidget(parent), mStreamActive(false)
{
    setupUI();
    createDiffPanel();
    connectSignals();
    restoreState();
    
    setWindowTitle("Code Stream");
}

CodeStreamWidget::~CodeStreamWidget()
{
    saveState();
}

void CodeStreamWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    // Control panel
    mControlLayout = new QHBoxLayout();
    
    mStatusLabel = new QLabel("Ready", this);
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    mControlLayout->addWidget(mStatusLabel);
    
    mStreamDurationLabel = new QLabel("00:00:00", this);
    mControlLayout->addWidget(mStreamDurationLabel);
    
    mControlLayout->addSpacing(20);
    
    mStartButton = new QPushButton("Start Stream", this);
    mStartButton->setStyleSheet("background-color: #4CAF50; color: white; padding: 8px;");
    mControlLayout->addWidget(mStartButton);
    
    mStopButton = new QPushButton("Stop Stream", this);
    mStopButton->setStyleSheet("background-color: #f44336; color: white; padding: 8px;");
    mStopButton->setEnabled(false);
    mControlLayout->addWidget(mStopButton);
    
    mClearButton = new QPushButton("Clear History", this);
    mControlLayout->addWidget(mClearButton);
    
    mExportButton = new QPushButton("Export", this);
    mControlLayout->addWidget(mExportButton);
    
    mControlLayout->addStretch();
    
    mMainLayout->addLayout(mControlLayout);
}

void CodeStreamWidget::createDiffPanel()
{
    mSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Left side - diff list
    QWidget* leftWidget = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftWidget);
    
    leftLayout->addWidget(new QLabel("Code Changes:", this));
    
    mDiffListWidget = new QListWidget(this);
    leftLayout->addWidget(mDiffListWidget);
    
    QHBoxLayout* diffActionLayout = new QHBoxLayout();
    mApplyButton = new QPushButton("Apply", this);
    mApplyButton->setEnabled(false);
    diffActionLayout->addWidget(mApplyButton);
    
    mRejectButton = new QPushButton("Reject", this);
    mRejectButton->setEnabled(false);
    diffActionLayout->addWidget(mRejectButton);
    
    mViewButton = new QPushButton("View", this);
    mViewButton->setEnabled(false);
    diffActionLayout->addWidget(mViewButton);
    
    leftLayout->addLayout(diffActionLayout);
    
    mSplitter->addWidget(leftWidget);
    
    // Right side - code comparison
    QWidget* rightWidget = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightWidget);
    
    // Old code
    rightLayout->addWidget(new QLabel("Before:", this));
    mOldCodeEditor = new QTextEdit(this);
    mOldCodeEditor->setReadOnly(true);
    mOldCodeEditor->setFont(QFont("Courier", 9));
    rightLayout->addWidget(mOldCodeEditor);
    
    // New code
    rightLayout->addWidget(new QLabel("After:", this));
    mNewCodeEditor = new QTextEdit(this);
    mNewCodeEditor->setReadOnly(true);
    mNewCodeEditor->setFont(QFont("Courier", 9));
    rightLayout->addWidget(mNewCodeEditor);
    
    mSplitter->addWidget(rightWidget);
    mSplitter->setSizes({300, 500});
    
    mMainLayout->addWidget(mSplitter);
    
    // Compare section
    mCompareLayout = new QHBoxLayout();
    mVersionLabel = new QLabel("Compare Versions:", this);
    mCompareLayout->addWidget(mVersionLabel);
    
    mVersionCombo = new QComboBox(this);
    mVersionCombo->addItem("Version 1.0");
    mVersionCombo->addItem("Version 2.0");
    mVersionCombo->addItem("Current");
    mCompareLayout->addWidget(mVersionCombo);
    
    mCompareButton = new QPushButton("Compare", this);
    mCompareLayout->addWidget(mCompareButton);
    
    mCompareLayout->addStretch();
    
    mMainLayout->addLayout(mCompareLayout);
}

void CodeStreamWidget::connectSignals()
{
    connect(mStartButton, &QPushButton::clicked, this, &CodeStreamWidget::onStartStream);
    connect(mStopButton, &QPushButton::clicked, this, &CodeStreamWidget::onStopStream);
    connect(mClearButton, &QPushButton::clicked, this, &CodeStreamWidget::onClearHistory);
    connect(mApplyButton, &QPushButton::clicked, this, [this]() {
        if (mDiffListWidget->currentRow() >= 0) {
            onApplyDiff(mDiffListWidget->currentRow());
        }
    });
    connect(mRejectButton, &QPushButton::clicked, this, [this]() {
        if (mDiffListWidget->currentRow() >= 0) {
            onRejectDiff(mDiffListWidget->currentRow());
        }
    });
    connect(mViewButton, &QPushButton::clicked, this, [this]() {
        if (mDiffListWidget->currentRow() >= 0) {
            onViewDiff(mDiffListWidget->currentRow());
        }
    });
    connect(mExportButton, &QPushButton::clicked, this, &CodeStreamWidget::onExportStream);
    connect(mCompareButton, &QPushButton::clicked, this, &CodeStreamWidget::onCompareVersions);
    connect(mDiffListWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        bool enabled = mDiffListWidget->currentRow() >= 0;
        mApplyButton->setEnabled(enabled);
        mRejectButton->setEnabled(enabled);
        mViewButton->setEnabled(enabled);
    });
}

void CodeStreamWidget::onStartStream()
{
    mStreamActive = true;
    mStatusLabel->setText("Streaming...");
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: green;");
    mStartButton->setEnabled(false);
    mStopButton->setEnabled(true);
    
    emit streamStarted();
}

void CodeStreamWidget::onStopStream()
{
    mStreamActive = false;
    mStatusLabel->setText("Stream stopped");
    mStatusLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: red;");
    mStartButton->setEnabled(true);
    mStopButton->setEnabled(false);
    
    emit streamStopped();
}

void CodeStreamWidget::onClearHistory()
{
    int ret = QMessageBox::question(this, "Clear History", "Clear all code change history?");
    if (ret == QMessageBox::Yes) {
        mDiffHistory.clear();
        mDiffListWidget->clear();
        mOldCodeEditor->clear();
        mNewCodeEditor->clear();
    }
}

void CodeStreamWidget::onApplyDiff(int index)
{
    if (index >= 0 && index < mDiffHistory.size()) {
        CodeDiff diff = mDiffHistory.at(index);
        emit diffApplied(diff);
        QMessageBox::information(this, "Applied", "Diff applied successfully!");
    }
}

void CodeStreamWidget::onRejectDiff(int index)
{
    if (index >= 0 && index < mDiffHistory.size()) {
        mDiffHistory.removeAt(index);
        mDiffListWidget->takeItem(index);
        QMessageBox::information(this, "Rejected", "Diff rejected.");
    }
}

void CodeStreamWidget::onViewDiff(int index)
{
    if (index >= 0 && index < mDiffHistory.size()) {
        CodeDiff diff = mDiffHistory.at(index);
        mOldCodeEditor->setText(diff.oldCode);
        mNewCodeEditor->setText(diff.newCode);
    }
}

void CodeStreamWidget::onExportStream()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Stream", "", "Text Files (*.txt);;JSON Files (*.json)");
    if (!filename.isEmpty()) {
        emit streamExported(filename);
        QMessageBox::information(this, "Exported", "Stream exported successfully!");
    }
}

void CodeStreamWidget::onCompareVersions()
{
    QString version = mVersionCombo->currentText();
    QMessageBox::information(this, "Compare", 
        QString("Comparing current code with %1").arg(version));
}

void CodeStreamWidget::updateStreamStatus(const QString& status)
{
    mStatusLabel->setText(status);
}

void CodeStreamWidget::populateDiffList()
{
    mDiffListWidget->clear();
    for (const CodeDiff& diff : mDiffHistory) {
        QString item = QString("[%1] Line %2: %3")
            .arg(diff.timestamp)
            .arg(diff.lineNumber)
            .arg(diff.operation);
        mDiffListWidget->addItem(item);
    }
}

void CodeStreamWidget::restoreState()
{
    QSettings settings("RawrXD", "IDE");
    // Restore last known state
}

void CodeStreamWidget::saveState()
{
    QSettings settings("RawrXD", "IDE");
    // Save current state
}
