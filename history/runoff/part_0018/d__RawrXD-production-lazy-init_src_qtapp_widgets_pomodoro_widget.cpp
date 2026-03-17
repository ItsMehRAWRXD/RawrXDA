/**
 * @file pomodoro_widget.cpp
 * @brief Implementation of PomodoroWidget - Pomodoro timer
 */

#include "pomodoro_widget.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

PomodoroWidget::PomodoroWidget(QWidget* parent)
    : QWidget(parent), mTimeLeft(1500), mIsWorkSession(true), mSessionCount(0)
{
    RawrXD::Integration::ScopedInitTimer init("PomodoroWidget");
    setupUI();
    connectSignals();
    setWindowTitle("Pomodoro Timer");
}

PomodoroWidget::~PomodoroWidget() = default;

void PomodoroWidget::setupUI()
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    mStatusLabel = new QLabel("Work Session", this);
    mStatusLabel->setStyleSheet("font-weight: bold; color: green; font-size: 14px;");
    layout->addWidget(mStatusLabel);
    
    mSessionLabel = new QLabel("Sessions completed: 0", this);
    layout->addWidget(mSessionLabel);
    
    mTimerLabel = new QLabel("25:00", this);
    mTimerLabel->setStyleSheet("font-size: 48px; font-weight: bold; text-align: center;");
    layout->addWidget(mTimerLabel);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    mStartButton = new QPushButton("Start", this);
    mStartButton->setStyleSheet("background-color: #4CAF50; color: white;");
    buttonLayout->addWidget(mStartButton);
    
    mStopButton = new QPushButton("Stop", this);
    mStopButton->setEnabled(false);
    buttonLayout->addWidget(mStopButton);
    
    mResetButton = new QPushButton("Reset", this);
    buttonLayout->addWidget(mResetButton);
    
    layout->addLayout(buttonLayout);
    layout->addStretch();
    
    mTimer = new QTimer(this);
}

void PomodoroWidget::connectSignals()
{
    connect(mStartButton, &QPushButton::clicked, this, &PomodoroWidget::onStartPomodoro);
    connect(mStopButton, &QPushButton::clicked, this, &PomodoroWidget::onStopPomodoro);
    connect(mResetButton, &QPushButton::clicked, this, &PomodoroWidget::onResetPomodoro);
    connect(mTimer, &QTimer::timeout, this, &PomodoroWidget::updateTimer);
}

void PomodoroWidget::onStartPomodoro()
{
    mStartButton->setEnabled(false);
    mStopButton->setEnabled(true);
    mTimer->start(1000);
}

void PomodoroWidget::onStopPomodoro()
{
    mTimer->stop();
    mStartButton->setEnabled(true);
    mStopButton->setEnabled(false);
}

void PomodoroWidget::onResetPomodoro()
{
    mTimer->stop();
    mTimeLeft = mIsWorkSession ? 1500 : 300;
    mStartButton->setEnabled(true);
    mStopButton->setEnabled(false);
    updateTimer();
}

void PomodoroWidget::updateTimer()
{
    mTimeLeft--;
    int minutes = mTimeLeft / 60;
    int seconds = mTimeLeft % 60;
    mTimerLabel->setText(QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0')));
    
    if (mTimeLeft <= 0) {
        mTimer->stop();
        if (mIsWorkSession) {
            RawrXD::Integration::logInfo("PomodoroWidget", "session_complete", "Work session finished");
            RawrXD::Integration::recordMetric("pomodoro_work_sessions", 1);
            mSessionCount++;
            mSessionLabel->setText(QString("Sessions completed: %1").arg(mSessionCount));
            mIsWorkSession = false;
            mStatusLabel->setText("Break Time");
            mStatusLabel->setStyleSheet("font-weight: bold; color: blue; font-size: 14px;");
            mTimeLeft = 300;
        } else {
            RawrXD::Integration::logInfo("PomodoroWidget", "session_complete", "Break finished");
            RawrXD::Integration::recordMetric("pomodoro_break_sessions", 1);
            mIsWorkSession = true;
            mStatusLabel->setText("Work Session");
            mStatusLabel->setStyleSheet("font-weight: bold; color: green; font-size: 14px;");
            mTimeLeft = 1500;
        }
        mStartButton->setEnabled(true);
    }
}
