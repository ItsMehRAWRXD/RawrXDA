/**
 * @file time_tracker_widget.h
 * @brief Header for TimeTrackerWidget - Time tracking for coding sessions
 */

#pragma once

#include <QWidget>
#include <QTimer>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QLabel;
class QComboBox;

class TimeTrackerWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TimeTrackerWidget(QWidget* parent = nullptr);
    ~TimeTrackerWidget();
    
public slots:
    void onStartTimer();
    void onStopTimer();
    void onResetTimer();
    void onTaskSelected(int index);
    void updateDisplay();
    
signals:
    void timeUpdated(int seconds);
    void taskSwitched(const QString& task);
    
private:
    void setupUI();
    void connectSignals();
    
    QVBoxLayout* mMainLayout;
    QLabel* mTimeLabel;
    QPushButton* mStartButton;
    QPushButton* mStopButton;
    QPushButton* mResetButton;
    QComboBox* mTaskCombo;
    QLabel* mCurrentTaskLabel;
    
    QTimer* mTimer;
    int mElapsedSeconds;
};

#endif // TIME_TRACKER_WIDGET_H
