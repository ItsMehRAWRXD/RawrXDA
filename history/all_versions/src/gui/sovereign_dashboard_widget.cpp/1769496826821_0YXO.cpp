#include "sovereign_dashboard_widget.h"
#include <QGridLayout>

SovereignDashboardWidget::SovereignDashboardWidget(QWidget *parent) : QWidget(parent), m_mmfHandle(nullptr), m_mmfView(nullptr) {
    auto *layout = new QVBoxLayout(this);
    
    m_lblTokens = new QLabel("0.0 t/s", this);
    m_lblTokens->setStyleSheet("font-size: 24pt; font-weight: bold; color: #00FF00;");
    layout->addWidget(new QLabel("Inference Rate:"));
    layout->addWidget(m_lblTokens);

    m_barThermal = new QProgressBar(this);
    m_barThermal->setRange(0, 100);
    m_barThermal->setStyleSheet("QProgressBar::chunk { background-color: #FF4444; }");
    layout->addWidget(new QLabel("Drive Temperature:"));
    layout->addWidget(m_barThermal);
    
    m_lblSkip = new QLabel("0% Skipped", this);
    layout->addWidget(m_lblSkip);
    
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SovereignDashboardWidget::updateDashboard);
    m_timer->start(16); // 60 FPS
}

SovereignDashboardWidget::~SovereignDashboardWidget() {
    if (m_mmfView) UnmapViewOfFile(m_mmfView);
    if (m_mmfHandle) CloseHandle(m_mmfHandle);
}

void SovereignDashboardWidget::attachSharedMemory(const QString &name) {
    if (m_mmfView) return;
    
    m_mmfHandle = OpenFileMappingA(FILE_MAP_READ, FALSE, name.toUtf8().constData());
    if (m_mmfHandle) {
        m_mmfView = MapViewOfFile(m_mmfHandle, FILE_MAP_READ, 0, 0, sizeof(SovereignStats));
    }
}

void SovereignDashboardWidget::updateDashboard() {
    if (!m_mmfView) {
        attachSharedMemory("Global\\SOVEREIGN_STATS");
        return;
    }
    
    auto* stats = reinterpret_cast<SovereignStats*>(m_mmfView);
    // 0x534F56 = 'SOV' implies 0x53='S', 0x4F='O', 0x56='V'. 
    // In memory 'S','O','V' -> 53 4F 56 ... Little Endian would be read as 0x...564F53
    // 'S' is low byte? 'S' is 0x53. 
    // ASCII "SOV" -> 0x53, 0x4F, 0x56.
    // Little Endian: 0x00564F53.
    // User prompt: `stats->magic == 0x534F56`. This assumes 0x534F56 is the number.
    // In MASM: `DWORD 534F56h`. 
    if (stats->magic == 0x534F56) {
        m_lblTokens->setText(QString::number(stats->tokensPerSec, 'f', 1) + " t/s");
        m_barThermal->setValue(stats->driveTemps[0]); 
        m_lblSkip->setText(QString::number(stats->skipRate) + "% Skipped");
    }
}
