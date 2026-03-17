#include "ProfilerPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>

using namespace RawrXD;

ProfilerPanel::ProfilerPanel(QWidget *parent)
    : QDockWidget(parent), m_root(new QWidget(this)), m_tabs(new QTabWidget(m_root)),
      m_toolbar(new QToolBar(m_root)), m_startBtn(new QPushButton("Start", m_root)),
      m_stopBtn(new QPushButton("Stop", m_root)), m_rateCombo(new QComboBox(m_root)),
      m_rateSpin(new QSpinBox(m_root)), m_statusLabel(new QLabel("Idle", m_root)),
      m_cpuTree(new QTreeWidget(m_root)), m_memTree(new QTreeWidget(m_root)),
      m_flame(new FlamegraphRenderer(m_root)), m_cpu(new CPUProfiler(this)),
      m_mem(new MemoryProfiler(this)), m_session(nullptr) {

    setObjectName("ProfilerPanel");
    setWindowTitle("Profiler");
    setWidget(m_root);

    setupUi();

    connect(m_startBtn, &QPushButton::clicked, this, &ProfilerPanel::onStart);
    connect(m_stopBtn, &QPushButton::clicked, this, &ProfilerPanel::onStop);
    connect(m_cpu, &CPUProfiler::sampleCollected, this, &ProfilerPanel::onSampleCollected);
    connect(m_cpu, &CPUProfiler::error, this, &ProfilerPanel::onError);

    connect(m_mem, &MemoryProfiler::snapshotTaken, this, &ProfilerPanel::onMemorySnapshot);
}

void ProfilerPanel::attachSession(ProfileSession *session) {
    m_session = session;
    m_flame->setSession(session);
    refreshCpuTree();
    refreshMemTree();
}

void ProfilerPanel::setupUi() {
    auto *vl = new QVBoxLayout(m_root);
    auto *hl = new QHBoxLayout();

    m_rateCombo->addItems({"Low (50 Hz)", "Medium (100 Hz)", "High (200 Hz)", "Custom"});
    m_rateSpin->setRange(10, 1000);
    m_rateSpin->setValue(100);
    m_rateSpin->setEnabled(false);

    connect(m_rateCombo, &QComboBox::currentTextChanged, this, [this](const QString &txt){
        m_rateSpin->setEnabled(txt == "Custom");
    });

    m_toolbar->addWidget(new QLabel("Sampling:"));
    m_toolbar->addWidget(m_rateCombo);
    m_toolbar->addWidget(m_rateSpin);
    m_toolbar->addSeparator();
    m_toolbar->addWidget(m_startBtn);
    m_toolbar->addWidget(m_stopBtn);
    m_toolbar->addSeparator();
    m_toolbar->addWidget(m_statusLabel);

    hl->addWidget(m_toolbar);
    vl->addLayout(hl);

    m_cpuTree->setColumnCount(4);
    m_cpuTree->setHeaderLabels({"Function", "Total (ms)", "Self (ms)", "Calls"});
    m_cpuTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_memTree->setColumnCount(3);
    m_memTree->setHeaderLabels({"Timestamp", "RSS (MB)", "Alloc Count"});
    m_memTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    m_tabs->addTab(m_cpuTree, "CPU");
    m_tabs->addTab(m_memTree, "Memory");
    m_tabs->addTab(m_flame, "Flamegraph");

    vl->addWidget(m_tabs);
}

void ProfilerPanel::onStart() {
    int rate = 100;
    switch (m_rateCombo->currentIndex()) {
        case 0: rate = 50; break;
        case 1: rate = 100; break;
        case 2: rate = 200; break;
        case 3: rate = m_rateSpin->value(); break;
        default: break;
    }

    // Determine process name
    QString procName = qApp ? qApp->applicationName() : QStringLiteral("RawrXD IDE");
    if (m_rateCombo->currentIndex() == 0) {
        m_cpu->startProfiling(procName, SamplingRate::Low);
    } else if (m_rateCombo->currentIndex() == 1) {
        m_cpu->startProfiling(procName, SamplingRate::Normal);
    } else if (m_rateCombo->currentIndex() == 2) {
        m_cpu->startProfiling(procName, SamplingRate::High);
    } else {
        m_cpu->startProfilingHz(procName, rate);
    }

    m_mem->startProfiling();
    m_statusLabel->setText("Running");
    emit profilingStarted();
}

void ProfilerPanel::onStop() {
    m_cpu->stopProfiling();
    m_mem->stopProfiling();
    m_statusLabel->setText("Stopped");
    emit profilingStopped();
}

void ProfilerPanel::onSampleCollected(const CallStack &stack) {
    if (!m_session) return;
    m_session->addCallStack(stack);
    refreshCpuTree();
    m_flame->refresh();
}

void ProfilerPanel::onMemorySnapshot(const MemorySnapshot &snap) {
    QTreeWidgetItem *it = new QTreeWidgetItem(m_memTree);
    // Convert timestampUs to human readable format
    QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(snap.timestampUs / 1000));
    it->setText(0, timestamp.toString(Qt::ISODate));
    it->setText(1, QString::number(snap.currentUsageBytes / (1024.0*1024.0), 'f', 2) + " MB");
    it->setText(2, QString::number(snap.allocations.size()));
}

void ProfilerPanel::refreshCpuTree() {
    m_cpuTree->clear();
    if (!m_session) return;
    QList<FunctionStat> stats = m_session->functionStats().values();
    // sort
    std::sort(stats.begin(), stats.end(), [](const FunctionStat &a, const FunctionStat &b){
        return a.totalTimeUs > b.totalTimeUs;
    });

    for (const auto &fs : stats) {
        QTreeWidgetItem *it = new QTreeWidgetItem(m_cpuTree);
        it->setText(0, fs.functionName);
        it->setText(1, QString::number(fs.totalTimeUs / 1000.0, 'f', 2));
        it->setText(2, QString::number(fs.selfTimeUs / 1000.0, 'f', 2));
        it->setText(3, QString::number(fs.callCount));
    }
}

void ProfilerPanel::refreshMemTree() {
    // Memory tree populated via snapshots; no aggregation here
}

void ProfilerPanel::onError(const QString &msg) {
    QMessageBox::warning(this, "Profiler Error", msg);
}
