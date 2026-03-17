#include "progress_manager.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

ProgressManager::ProgressManager(QWidget* parent) : QWidget(parent)
{
    setupUI();
    mProgressTimer = new QTimer(this);
    connect(mProgressTimer, &QTimer::timeout, this, &ProgressManager::updateProgress);
    mProgressTimer->start(1000); // Update every second
}

ProgressManager::~ProgressManager()
{
    // Cleanup if needed
}

void ProgressManager::updateProgress()
{
    // Simulate progress updates
    int buildVal = mBuildProgress->value() + 5;
    if (buildVal > 100) buildVal = 0;
    mBuildProgress->setValue(buildVal);

    int testVal = mTestProgress->value() + 3;
    if (testVal > 100) testVal = 0;
    mTestProgress->setValue(testVal);

    int deployVal = mDeployProgress->value() + 2;
    if (deployVal > 100) deployVal = 0;
    mDeployProgress->setValue(deployVal);
}

void ProgressManager::setupUI()
{
    mMainLayout = new QVBoxLayout(this);

    mTitleLabel = new QLabel("Progress Manager", this);
    mTitleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    mMainLayout->addWidget(mTitleLabel);

    QLabel* activeLabel = new QLabel("Active Tasks", this);
    activeLabel->setStyleSheet("font-size: 14px; color: #cccccc;");
    mMainLayout->addWidget(activeLabel);

    mBuildProgress = new QProgressBar(this);
    mBuildProgress->setRange(0, 100);
    mBuildProgress->setValue(75);
    mBuildProgress->setFormat("Build: %p%");
    mMainLayout->addWidget(mBuildProgress);

    mTestProgress = new QProgressBar(this);
    mTestProgress->setRange(0, 100);
    mTestProgress->setValue(50);
    mTestProgress->setFormat("Tests: %p%");
    mMainLayout->addWidget(mTestProgress);

    mDeployProgress = new QProgressBar(this);
    mDeployProgress->setRange(0, 100);
    mDeployProgress->setValue(25);
    mDeployProgress->setFormat("Deploy: %p%");
    mMainLayout->addWidget(mDeployProgress);

    mCancelButton = new QPushButton("Cancel", this);
    mMainLayout->addWidget(mCancelButton);
}

void ProgressManager::setProgress(int buildProgress, int testProgress, int deployProgress)
{
    mBuildProgress->setValue(buildProgress);
    mTestProgress->setValue(testProgress);
    mDeployProgress->setValue(deployProgress);
}

void ProgressManager::resetProgress()
{
    mBuildProgress->setValue(0);
    mTestProgress->setValue(0);
    mDeployProgress->setValue(0);
}

void ProgressManager::onBuildStarted()
{
    mBuildProgress->setValue(0);
    mBuildProgress->setStyleSheet("QProgressBar::chunk { background-color: #3498db; }");
}

void ProgressManager::onBuildCompleted()
{
    mBuildProgress->setValue(100);
    mBuildProgress->setStyleSheet("QProgressBar::chunk { background-color: #27ae60; }");
}

void ProgressManager::onTestStarted()
{
    mTestProgress->setValue(0);
    mTestProgress->setStyleSheet("QProgressBar::chunk { background-color: #9b59b6; }");
}

void ProgressManager::onTestCompleted()
{
    mTestProgress->setValue(100);
    mTestProgress->setStyleSheet("QProgressBar::chunk { background-color: #27ae60; }");
}