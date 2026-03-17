#include "sovereign_dashboard_widget.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVariant>
#include <QtGlobal>

#include <cstring>

SovereignDashboardWidget::SovereignDashboardWidget(QWidget *parent)
    : QWidget(parent) {
    buildUi();

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(16); // ~60 FPS
    connect(m_pollTimer, &QTimer::timeout, this, &SovereignDashboardWidget::refreshTelemetry);
}

SovereignDashboardWidget::~SovereignDashboardWidget() {
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    detachSharedMemory();
}

bool SovereignDashboardWidget::attachSharedMemory(const QString &mmfName) {
#ifdef _WIN32
    detachSharedMemory();

    const QByteArray nameBytes = mmfName.toUtf8();
    m_mmfHandle = OpenFileMappingA(FILE_MAP_READ, FALSE, nameBytes.constData());
    if (!m_mmfHandle) {
        qWarning("SovereignDashboardWidget: failed to open MMF %s", nameBytes.constData());
        return false;
    }

    m_view = MapViewOfFile(m_mmfHandle, FILE_MAP_READ, 0, 0, sizeof(SovereignStatsBlock));
    if (!m_view) {
        qWarning("SovereignDashboardWidget: failed to map view for %s", nameBytes.constData());
        detachSharedMemory();
        return false;
    }

    m_pollTimer->start();
    return true;
#else
    Q_UNUSED(mmfName);
    return false;
#endif
}

void SovereignDashboardWidget::detachSharedMemory() {
#ifdef _WIN32
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = nullptr;
    }
    if (m_mmfHandle) {
        CloseHandle(m_mmfHandle);
        m_mmfHandle = nullptr;
    }
#endif
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
}

void SovereignDashboardWidget::refreshTelemetry() {
#ifdef _WIN32
    if (!m_view) {
        return;
    }

    SovereignStatsBlock snapshot{};
    std::memcpy(&snapshot, m_view, sizeof(SovereignStatsBlock));

    if (snapshot.magic != 0x00564F53) { // 'SOV' little endian with padding
        return;
    }

    applyStats(snapshot);
#endif
}

void SovereignDashboardWidget::buildUi() {
    m_tokenRate = new QLabel(tr("Tokens/s: --"), this);
    m_skipRate = new QLabel(tr("Sparse Skip: --%"), this);
    m_gpuSplitBar = new QProgressBar(this);
    m_cpuSplitBar = new QProgressBar(this);
    m_lastUpdate = new QLabel(tr("Last update: --"), this);

    m_gpuSplitBar->setRange(0, 100);
    m_cpuSplitBar->setRange(0, 100);
    m_gpuSplitBar->setFormat("GPU %p%");
    m_cpuSplitBar->setFormat("CPU %p%");

    m_tempTable = new QTableWidget(this);
    m_tempTable->setColumnCount(2);
    m_tempTable->setHorizontalHeaderLabels({tr("Drive"), tr("Temp (C)" )});
    m_tempTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tempTable->verticalHeader()->setVisible(false);
    m_tempTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tempTable->setSelectionMode(QAbstractItemView::NoSelection);

    auto *layout = new QVBoxLayout(this);

    auto *row1 = new QHBoxLayout();
    row1->addWidget(m_tokenRate);
    row1->addWidget(m_skipRate);
    layout->addLayout(row1);

    auto *row2 = new QHBoxLayout();
    row2->addWidget(m_gpuSplitBar);
    row2->addWidget(m_cpuSplitBar);
    layout->addLayout(row2);

    layout->addWidget(m_lastUpdate);
    layout->addWidget(m_tempTable);

    setLayout(layout);
}

void SovereignDashboardWidget::applyStats(const SovereignStatsBlock &stats) {
    m_tokenRate->setText(tr("Tokens/s: %1").arg(static_cast<qulonglong>(stats.tokensPerSec)));
    m_skipRate->setText(tr("Sparse Skip: %1%" ).arg(stats.skipRate));

    m_gpuSplitBar->setValue(static_cast<int>(stats.gpuSplit));
    m_cpuSplitBar->setValue(static_cast<int>(stats.cpuSplit));

    const auto nowMs = QDateTime::currentMSecsSinceEpoch();
    m_lastUpdate->setText(tr("Last update: %1 ms ago").arg(nowMs - static_cast<qint64>(stats.lastUpdateMs)));

    const int count = static_cast<int>(qMin<uint32_t>(stats.thermalCount, 16));
    m_tempTable->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const int temp = stats.temps[i];
        auto *driveItem = new QTableWidgetItem(QString::number(i));
        auto *tempItem = new QTableWidgetItem(QString::number(temp));
        m_tempTable->setItem(i, 0, driveItem);
        m_tempTable->setItem(i, 1, tempItem);
    }
}
