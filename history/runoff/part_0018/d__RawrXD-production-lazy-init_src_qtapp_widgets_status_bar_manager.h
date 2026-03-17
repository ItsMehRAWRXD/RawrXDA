/**
 * @file status_bar_manager.h
 * @brief Header for StatusBarManager - Status bar widget management
 */

#pragma once

#include <QWidget>

class QStatusBar;
class QLabel;
class QProgressBar;

class StatusBarManager : public QWidget {
    Q_OBJECT
    
public:
    explicit StatusBarManager(QWidget* parent = nullptr);
    ~StatusBarManager();
    
public slots:
    void setStatus(const QString& message);
    void setProgress(int value);
    void clearStatus();
    
private:
    void setupUI();
    
    QStatusBar* mStatusBar;
    QLabel* mStatusLabel;
    QLabel* mPositionLabel;
    QLabel* mEncodingLabel;
    QProgressBar* mProgressBar;
};

#endif // STATUS_BAR_MANAGER_H
