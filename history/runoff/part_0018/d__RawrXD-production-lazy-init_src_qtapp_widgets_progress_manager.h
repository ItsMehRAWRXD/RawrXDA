/**
 * @file progress_manager.h
 * @brief Header for ProgressManager - Progress tracking for builds and tests
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

class ProgressManager : public QWidget
{
    Q_OBJECT

public:
    explicit ProgressManager(QWidget* parent = nullptr);
    ~ProgressManager();

    void setProgress(int buildProgress, int testProgress, int deployProgress);
    void resetProgress();

public slots:
    void updateProgress();
    void onBuildStarted();
    void onBuildCompleted();
    void onTestStarted();
    void onTestCompleted();

private:
    void setupUI();

    QVBoxLayout* mMainLayout;
    QLabel* mTitleLabel;
    QProgressBar* mBuildProgress;
    QProgressBar* mTestProgress;
    QProgressBar* mDeployProgress;
    QPushButton* mCancelButton;
    QTimer* mProgressTimer;
};
