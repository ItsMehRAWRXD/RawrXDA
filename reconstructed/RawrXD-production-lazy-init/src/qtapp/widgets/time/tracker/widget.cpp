/**
 * @file time_tracker_widget.cpp
 * @brief Implementation of TimeTrackerWidget - Time tracking
 */

#include "time_tracker_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTimer>

TimeTrackerWidget::TimeTrackerWidget(QWidget* parent)
    : QWidget(parent), mElapsedSeconds(0)
{
    RawrXD::Integration::ScopedInitTimer init("TimeTrackerWidget");
    setupUI();
    connectSignals();
    setWindowTitle("Time Tracker");
}

TimeTrackerWidget::~TimeTrackerWidget() = default;

void TimeTrackerWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    mTimeLabel = new QLabel("00:00:00", this);
    mTimeLabel->setStyleSheet("font-size: 32px; font-weight: bold; text-align: center;");
    mMainLayout->addWidget(mTimeLabel);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    mStartButton = new QPushButton("Start", this);
    mStartButton->setStyleSheet("background-color: #4CAF50; color: white;");
    buttonLayout->addWidget(mStartButton);
    
    mStopButton = new QPushButton("Stop", this);
    mStopButton->setEnabled(false);
    buttonLayout->addWidget(mStopButton);
    
    mResetButton = new QPushButton("Reset", this);
    buttonLayout->addWidget(mResetButton);
    
    mMainLayout->addLayout(buttonLayout);
    
    QHBoxLayout* taskLayout = new QHBoxLayout();
    taskLayout->addWidget(new QLabel("Task:", this));
    mTaskCombo = new QComboBox(this);
    mTaskCombo->addItems({"Coding", "Debugging", "Testing", "Documentation", "Review"});
    taskLayout->addWidget(mTaskCombo);
    mMainLayout->addLayout(taskLayout);
    
    mCurrentTaskLabel = new QLabel("Current: Coding", this);
    mMainLayout->addWidget(mCurrentTaskLabel);
    
    mMainLayout->addStretch();
    
    mTimer = new QTimer(this);
}

void TimeTrackerWidget::connectSignals()
{
    connect(mStartButton, &QPushButton::clicked, this, &TimeTrackerWidget::onStartTimer);
    connect(mStopButton, &QPushButton::clicked, this, &TimeTrackerWidget::onStopTimer);
    connect(mResetButton, &QPushButton::clicked, this, &TimeTrackerWidget::onResetTimer);
    connect(mTaskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TimeTrackerWidget::onTaskSelected);
    connect(mTimer, &QTimer::timeout, this, &TimeTrackerWidget::updateDisplay);
}

void TimeTrackerWidget::onStartTimer()
{
    RawrXD::Integration::logInfo("TimeTrackerWidget", "start", "Started task: " + mTaskCombo->currentText());
    mStartButton->setEnabled(false);
    mStopButton->setEnabled(true);
    mTimer->start(1000);
}

void TimeTrackerWidget::onStopTimer()
{
    RawrXD::Integration::logInfo("TimeTrackerWidget", "stop", QString("Stopped task: %1, duration: %2s").arg(mTaskCombo->currentText()).arg(mElapsedSeconds));
    mTimer->stop();
    mStartButton->setEnabled(true);
    mStopButton->setEnabled(false);
}

void TimeTrackerWidget::onResetTimer()
{
    mTimer->stop();
    mElapsedSeconds = 0;
    mTimeLabel->setText("00:00:00");
    mStartButton->setEnabled(true);
    mStopButton->setEnabled(false);
}

void TimeTrackerWidget::onTaskSelected(int index)
{
    mCurrentTaskLabel->setText(QString("Current: %1").arg(mTaskCombo->currentText()));
    emit taskSwitched(mTaskCombo->currentText());
}

void TimeTrackerWidget::updateDisplay()
{
    mElapsedSeconds++;
    int hours = mElapsedSeconds / 3600;
    int minutes = (mElapsedSeconds % 3600) / 60;
    int seconds = mElapsedSeconds % 60;
    
    mTimeLabel->setText(QString("%1:%2:%3")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0')));
    
    emit timeUpdated(mElapsedSeconds);
}
