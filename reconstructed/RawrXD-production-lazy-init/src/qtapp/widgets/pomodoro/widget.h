/**
 * @file pomodoro_widget.h
 * @brief Header for PomodoroWidget - Pomodoro timer
 */

#pragma once

#include <QWidget>
#include <QTimer>

class QLabel;
class QPushButton;
class QVBoxLayout;

class PomodoroWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit PomodoroWidget(QWidget* parent = nullptr);
    ~PomodoroWidget();
    
private slots:
    void onStartPomodoro();
    void onStopPomodoro();
    void onResetPomodoro();
    void updateTimer();
    
private:
    void setupUI();
    void connectSignals();
    
    QLabel* mTimerLabel;
    QPushButton* mStartButton;
    QPushButton* mStopButton;
    QPushButton* mResetButton;
    QLabel* mStatusLabel;
    QLabel* mSessionLabel;
    
    QTimer* mTimer;
    int mTimeLeft;
    bool mIsWorkSession;
    int mSessionCount;
};

#endif // POMODORO_WIDGET_H
