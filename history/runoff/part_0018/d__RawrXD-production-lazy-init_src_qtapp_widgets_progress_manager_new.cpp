/**
 * @file progress_manager.cpp
 * @brief Implementation of ProgressManager - Progress tracking
 */

#include "progress_manager.h"
#include <QVBoxLayout>
#include <QProgressBar>
#include <QLabel>

ProgressManager::ProgressManager(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowTitle("Progress Manager");
}

ProgressManager::~ProgressManager() = default;

void ProgressManager::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    mMessageLabel = new QLabel("Ready", this);
    mMainLayout->addWidget(mMessageLabel);
    
    mProgressBar = new QProgressBar(this);
    mProgressBar->setRange(0, 100);
    mMainLayout->addWidget(mProgressBar);
    
    mPercentLabel = new QLabel("0%", this);
    mMainLayout->addWidget(mPercentLabel);
    
    mMainLayout->addStretch();
}

void ProgressManager::setProgress(int value)
{
    mProgressBar->setValue(value);
    mPercentLabel->setText(QString::number(value) + "%");
}

void ProgressManager::setMessage(const QString& message)
{
    mMessageLabel->setText(message);
}

void ProgressManager::reset()
{
    mProgressBar->setValue(0);
    mPercentLabel->setText("0%");
    mMessageLabel->setText("Ready");
}
