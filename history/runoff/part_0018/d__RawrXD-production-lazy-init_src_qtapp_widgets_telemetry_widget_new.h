/**
 * @file telemetry_widget.h
 * @brief Header for TelemetryWidget - Telemetry and analytics display
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QLabel;
class QProgressBar;

class TelemetryWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TelemetryWidget(QWidget* parent = nullptr);
    ~TelemetryWidget();
    
public slots:
    void updateStats();
    
private:
    void setupUI();
    
    QVBoxLayout* mMainLayout;
    QLabel* mSessionTimeLabel;
    QLabel* mLinesEditedLabel;
    QLabel* mFilesOpenedLabel;
    QLabel* mSearchesLabel;
    QLabel* mBuildCountLabel;
    QProgressBar* mActivityBar;
};

#endif // TELEMETRY_WIDGET_H
