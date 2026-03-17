/**
 * @file progress_manager.h
 * @brief Header for ProgressManager - Progress tracking
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QProgressBar;
class QLabel;

class ProgressManager : public QWidget {
    Q_OBJECT
    
public:
    explicit ProgressManager(QWidget* parent = nullptr);
    ~ProgressManager();
    
public slots:
    void setProgress(int value);
    void setMessage(const QString& message);
    void reset();
    
private:
    void setupUI();
    
    QVBoxLayout* mMainLayout;
    QLabel* mMessageLabel;
    QProgressBar* mProgressBar;
    QLabel* mPercentLabel;
};

#endif // PROGRESS_MANAGER_H
