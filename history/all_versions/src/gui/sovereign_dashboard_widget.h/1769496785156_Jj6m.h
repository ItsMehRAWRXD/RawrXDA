#pragma once

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <windows.h>

struct SovereignStats {
    uint32_t magic;
    float tokensPerSec;
    uint32_t skipRate;
    uint32_t gpuSplit;
    uint32_t driveTemps[4];
};

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
