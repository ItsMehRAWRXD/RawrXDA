#pragma once

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <windows.h>

#include "telemetry/sovereign_stats_block.h"

class SovereignDashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit SovereignDashboardWidget(QWidget *parent = nullptr);
    ~SovereignDashboardWidget();

    void attachSharedMemory(const QString &name);

private slots:
    void updateDashboard();

private:
    QTimer *m_timer;
    HANDLE m_mmfHandle;
    void* m_mmfView;
    
    QLabel *m_lblTokens;
    QLabel *m_lblSkip;
    QProgressBar *m_barThermal;
};
