/**
 * @file profiler_widget.cpp
 * @brief Full Performance Profiler implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "profiler_widget.h"
#include "profiler/ProfilerPanel.h"
#include "profiler/ProfileData.h"
#include "profiler/MemoryProfiler.h"
#include "logging/structured_logger.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QToolBar>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QtMath>
#include <algorithm>

// No namespace - classes are at global scope (matching header)

// Forward declarations for utility functions defined at end of file
static QString formatTime(quint64 nanoseconds);
static QString formatBytes(quint64 bytes);

// =============================================================================
// ProfileConfig JSON Serialization
// =============================================================================

QJsonObject ProfileConfig::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["program"] = program;
    obj["cwd"] = cwd;
    obj["profilerType"] = profilerType;
    obj["samplingRate"] = samplingRate;
    obj["trackAllocations"] = trackAllocations;
    obj["trackFrees"] = trackFrees;
    obj["collectStackTraces"] = collectStackTraces;
    obj["stackDepth"] = stackDepth;
    
    QJsonArray argsArray;
    for (const QString& arg : args) argsArray.append(arg);
    obj["args"] = argsArray;
    
    QJsonArray symbolsArray;
    for (const QString& path : symbolPaths) symbolsArray.append(path);
    obj["symbolPaths"] = symbolsArray;
    
    return obj;
}

ProfileConfig ProfileConfig::fromJson(const QJsonObject& obj) {
    ProfileConfig config;
    config.name = obj["name"].toString();
    config.program = obj["program"].toString();
    config.cwd = obj["cwd"].toString();
    config.profilerType = obj["profilerType"].toString("sampling");
    config.samplingRate = obj["samplingRate"].toInt(1000);
    config.trackAllocations = obj["trackAllocations"].toBool(true);
    config.trackFrees = obj["trackFrees"].toBool(true);
    config.collectStackTraces = obj["collectStackTraces"].toBool(true);
    config.stackDepth = obj["stackDepth"].toInt(64);
    
    for (const QJsonValue& v : obj["args"].toArray()) {
        config.args.append(v.toString());
    }
    for (const QJsonValue& v : obj["symbolPaths"].toArray()) {
        config.symbolPaths.append(v.toString());
    }
    
    return config;
}

// =============================================================================
// FlameGraphView Implementation
// =============================================================================

FlameGraphView::FlameGraphView(QWidget* parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setBackgroundBrush(QColor("#1e1e1e"));
    setMinimumHeight(300);
}

void FlameGraphView::setData(const QVector<FlameFrame>& frames) {
    if (frames.isEmpty()) {
        clear();
        return;
    }
    
    // Build root from frames
    m_rootFrame.name = "all";
    m_rootFrame.value = 0;
    m_rootFrame.children = frames;
    
    for (const FlameFrame& f : frames) {
        m_rootFrame.value += f.value;
    }
    
    rebuild();
}

void FlameGraphView::setRootFrame(const FlameFrame& root) {
    m_rootFrame = root;
    rebuild();
}

void FlameGraphView::clear() {
    m_scene->clear();
    m_rootFrame = FlameFrame();
    m_itemToFrame.clear();
}

void FlameGraphView::rebuild() {
    m_scene->clear();
    m_itemToFrame.clear();
    
    if (m_rootFrame.value == 0) return;
    
    double width = viewport()->width() - 20;
    addFrameRect(m_rootFrame, 0, 0, width);
    
    m_scene->setSceneRect(m_scene->itemsBoundingRect().adjusted(-10, -10, 10, 10));
}

void FlameGraphView::addFrameRect(const FlameFrame& frame, int depth, double x, double totalWidth) {
    const int rowHeight = 20;
    const int maxDepth = 100;
    
    if (depth > maxDepth || frame.value == 0) return;
    
    double frameWidth = (static_cast<double>(frame.value) / m_rootFrame.value) * totalWidth;
    if (frameWidth < 1.0) return;  // Too small to display
    
    // Create rectangle for this frame
    double y = depth * rowHeight;
    QGraphicsRectItem* rect = m_scene->addRect(x, y, frameWidth - 1, rowHeight - 1);
    
    QColor color = colorForFunction(frame.name);
    rect->setBrush(color);
    rect->setPen(QPen(color.darker(120)));
    rect->setToolTip(QString("%1\n%2 samples (%.2f%%)")
        .arg(frame.name)
        .arg(frame.value)
        .arg(100.0 * frame.value / m_rootFrame.value));
    rect->setData(0, frame.name);
    rect->setData(1, QVariant::fromValue(frame.value));
    
    m_itemToFrame[rect] = frame;
    
    // Add text if wide enough
    if (frameWidth > 50) {
        QString displayName = frame.name;
        QFontMetrics fm(font());
        if (fm.horizontalAdvance(displayName) > frameWidth - 10) {
            displayName = fm.elidedText(displayName, Qt::ElideRight, static_cast<int>(frameWidth - 10));
        }
        
        QGraphicsTextItem* text = m_scene->addText(displayName);
        text->setPos(x + 2, y);
        text->setDefaultTextColor(Qt::white);
        QFont f = text->font();
        f.setPointSize(8);
        text->setFont(f);
    }
    
    // Recursively add children
    double childX = x;
    for (const FlameFrame& child : frame.children) {
        double childWidth = (static_cast<double>(child.value) / m_rootFrame.value) * totalWidth;
        addFrameRect(child, depth + 1, childX, totalWidth);
        childX += childWidth;
    }
}

QColor FlameGraphView::colorForFunction(const QString& function) {
    // Color based on function category
    if (function.contains("::") || function.contains("__")) {
        // C++ functions - warm colors
        uint hash = qHash(function);
        int hue = 0 + (hash % 60);  // Red to yellow
        return QColor::fromHsl(hue, 200, 120);
    } else if (function.startsWith("_") || function.contains("libc")) {
        // System functions - cool colors
        uint hash = qHash(function);
        int hue = 180 + (hash % 60);  // Cyan to blue
        return QColor::fromHsl(hue, 150, 100);
    } else if (function.contains("kernel") || function.contains("[k]")) {
        // Kernel functions - purple
        return QColor::fromHsl(280, 150, 100);
    } else {
        // User functions - green/yellow
        uint hash = qHash(function);
        int hue = 60 + (hash % 60);  // Yellow to green
        return QColor::fromHsl(hue, 200, 120);
    }
}

void FlameGraphView::mousePressEvent(QMouseEvent* event) {
    QGraphicsItem* item = itemAt(event->pos());
    if (item && event->button() == Qt::LeftButton) {
        QString function = item->data(0).toString();
        if (!function.isEmpty()) {
            emit frameClicked(function);
        }
    }
    QGraphicsView::mousePressEvent(event);
}

void FlameGraphView::mouseMoveEvent(QMouseEvent* event) {
    QGraphicsItem* item = itemAt(event->pos());
    if (item) {
        QString function = item->data(0).toString();
        quint64 value = item->data(1).toULongLong();
        if (!function.isEmpty() && m_rootFrame.value > 0) {
            double percent = 100.0 * value / m_rootFrame.value;
            emit frameHovered(function, value, percent);
        }
    }
    QGraphicsView::mouseMoveEvent(event);
}

void FlameGraphView::wheelEvent(QWheelEvent* event) {
    double factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    m_scale *= factor;
    m_scale = qBound(0.1, m_scale, 10.0);
    setTransform(QTransform::fromScale(m_scale, 1.0));
}

void FlameGraphView::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    rebuild();
}

// =============================================================================
// ProfilerWidget Implementation
// =============================================================================

ProfilerWidget::ProfilerWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
    , m_liveUpdateTimer(new QTimer(this))
{
    RawrXD::Integration::ScopedInitTimer init("ProfilerWidget");
    setupUI();
    
    // Load saved configs
    int count = m_settings->beginReadArray("ProfilerConfigs");
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        QJsonObject obj = QJsonDocument::fromJson(
            m_settings->value("config").toByteArray()).object();
        if (!obj.isEmpty()) {
            ProfileConfig config = ProfileConfig::fromJson(obj);
            m_configs.append(config);
            m_configSelector->addItem(config.name);
        }
    }
    m_settings->endArray();
    
    // Add default config if empty
    if (m_configs.isEmpty()) {
        ProfileConfig defaultConfig;
        defaultConfig.name = "CPU Sampling Profile";
        defaultConfig.profilerType = "sampling";
        defaultConfig.samplingRate = 1000;
        m_configs.append(defaultConfig);
        m_configSelector->addItem(defaultConfig.name);
    }
    
    // Setup live update timer
    connect(m_liveUpdateTimer, &QTimer::timeout, this, &ProfilerWidget::updateLiveData);
}

ProfilerWidget::~ProfilerWidget() {
    if (m_profilerProcess && m_profilerProcess->state() != QProcess::NotRunning) {
        stopProfiling();
    }
    
    // Save configs
    m_settings->beginWriteArray("ProfilerConfigs");
    for (int i = 0; i < m_configs.size(); ++i) {
        m_settings->setArrayIndex(i);
        QJsonDocument doc(m_configs[i].toJson());
        m_settings->setValue("config", doc.toJson(QJsonDocument::Compact));
    }
    m_settings->endArray();
}

void ProfilerWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    // Main splitter
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    
    // Tab widget for different views
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::South);
    
    setupHotSpotsView();
    setupCallTreeView();
    setupFlameGraphView();
    setupMemoryView();
    setupTimelineView();
    setupStatsPanel();
    
    // Phase 7 integrated ProfilerPanel tab
    m_phase7Panel = new RawrXD::ProfilerPanel(this);
    auto *phase7Session = new RawrXD::ProfileSession(m_phase7Panel);
    phase7Session->setProcessName(qApp ? qApp->applicationName() : QStringLiteral("RawrXD IDE"));
    m_phase7Panel->attachSession(phase7Session);

    m_tabWidget->addTab(m_hotSpotsTree, "Hot Spots");
    m_tabWidget->addTab(m_callTreeView, "Call Tree");
    m_tabWidget->addTab(m_flameGraphView, "Flame Graph");
    m_tabWidget->addTab(m_memoryTree, "Memory");
    m_tabWidget->addTab(m_timelineChart, "Timeline");
    m_tabWidget->addTab(m_statsTable, "Statistics");
    m_tabWidget->addTab(m_phase7Panel, "Profiler (Phase 7)");
    
    m_mainSplitter->addWidget(m_tabWidget);
    mainLayout->addWidget(m_mainSplitter);

    // Metrics & tracing hooks for Phase 7 panel
    connect(m_phase7Panel, &RawrXD::ProfilerPanel::profilingStarted, this, []() {
        RawrXD::StructuredLogger::instance().incrementCounter("profiler.phase7.starts");
        RawrXD::StructuredLogger::instance().startSpan("Phase7Profiling");
        RawrXD::StructuredLogger::instance().info("Phase 7 profiling started");
    });
    connect(m_phase7Panel, &RawrXD::ProfilerPanel::profilingStopped, this, []() {
        RawrXD::StructuredLogger::instance().incrementCounter("profiler.phase7.stops");
        RawrXD::StructuredLogger::instance().endSpan("Phase7Profiling");
        RawrXD::StructuredLogger::instance().info("Phase 7 profiling stopped");
    });
}

void ProfilerWidget::setupToolbar() {
    m_toolbar = new QToolBar("Profiler Toolbar", this);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // Config selector
    m_configSelector = new QComboBox(this);
    m_configSelector->setMinimumWidth(180);
    m_toolbar->addWidget(new QLabel(" Profile: "));
    m_toolbar->addWidget(m_configSelector);
    connect(m_configSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfilerWidget::onConfigChanged);
    
    m_toolbar->addSeparator();
    
    // Control buttons
    m_startBtn = new QPushButton("▶ Start", this);
    m_startBtn->setToolTip("Start Profiling");
    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        if (m_configSelector->currentIndex() >= 0 && m_configSelector->currentIndex() < m_configs.size()) {
            startProfiling(m_configs[m_configSelector->currentIndex()]);
        }
    });
    m_toolbar->addWidget(m_startBtn);
    
    m_pauseBtn = new QPushButton("⏸ Pause", this);
    m_pauseBtn->setToolTip("Pause Profiling");
    m_pauseBtn->setEnabled(false);
    connect(m_pauseBtn, &QPushButton::clicked, this, &ProfilerWidget::pauseProfiling);
    m_toolbar->addWidget(m_pauseBtn);
    
    m_stopBtn = new QPushButton("⏹ Stop", this);
    m_stopBtn->setToolTip("Stop Profiling");
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &ProfilerWidget::stopProfiling);
    m_toolbar->addWidget(m_stopBtn);
    
    m_toolbar->addSeparator();
    
    m_clearBtn = new QPushButton("🗑 Clear", this);
    m_clearBtn->setToolTip("Clear Data");
    connect(m_clearBtn, &QPushButton::clicked, this, &ProfilerWidget::clearData);
    m_toolbar->addWidget(m_clearBtn);
    
    m_exportBtn = new QPushButton("📤 Export", this);
    m_exportBtn->setToolTip("Export Report");
    connect(m_exportBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Export Report", QString(),
            "HTML Report (*.html);;CSV (*.csv);;JSON (*.json)");
        if (!path.isEmpty()) {
            QString format = path.endsWith(".html") ? "html" : 
                            path.endsWith(".csv") ? "csv" : "json";
            exportReport(path, format);
        }
    });
    m_toolbar->addWidget(m_exportBtn);
    
    m_toolbar->addSeparator();
    
    m_liveUpdateCheck = new QCheckBox("Live Update", this);
    m_liveUpdateCheck->setChecked(true);
    connect(m_liveUpdateCheck, &QCheckBox::toggled, this, &ProfilerWidget::toggleLiveUpdate);
    m_toolbar->addWidget(m_liveUpdateCheck);
}

void ProfilerWidget::setupHotSpotsView() {
    m_hotSpotsTree = new QTreeWidget(this);
    m_hotSpotsTree->setHeaderLabels({"Function", "Self %", "Self Time", "Total %", "Total Time", "Samples", "Module"});
    m_hotSpotsTree->setColumnWidth(0, 300);
    m_hotSpotsTree->setColumnWidth(1, 80);
    m_hotSpotsTree->setColumnWidth(2, 100);
    m_hotSpotsTree->setColumnWidth(3, 80);
    m_hotSpotsTree->setColumnWidth(4, 100);
    m_hotSpotsTree->setRootIsDecorated(false);
    m_hotSpotsTree->setSortingEnabled(true);
    m_hotSpotsTree->sortByColumn(1, Qt::DescendingOrder);
    
    connect(m_hotSpotsTree, &QTreeWidget::itemDoubleClicked,
            this, &ProfilerWidget::onFunctionDoubleClicked);
    
    m_hotSpotsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_hotSpotsTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_hotSpotsTree->itemAt(pos);
        if (!item) return;
        
        QMenu menu(this);
        
        QAction* goToSource = menu.addAction("Go to Source");
        connect(goToSource, &QAction::triggered, this, [this, item]() {
            QString file = item->data(0, Qt::UserRole).toString();
            int line = item->data(0, Qt::UserRole + 1).toInt();
            if (!file.isEmpty()) {
                emit navigateToSource(file, line);
            }
        });
        
        QAction* copyAction = menu.addAction("Copy Function Name");
        connect(copyAction, &QAction::triggered, this, [item]() {
            QApplication::clipboard()->setText(item->text(0));
        });
        
        QAction* focusFlame = menu.addAction("Focus in Flame Graph");
        connect(focusFlame, &QAction::triggered, this, [this, item]() {
            onFlameFrameClicked(item->text(0));
            m_tabWidget->setCurrentWidget(m_flameGraphView);
        });
        
        menu.exec(m_hotSpotsTree->mapToGlobal(pos));
    });
}

void ProfilerWidget::setupCallTreeView() {
    m_callTreeView = new QTreeWidget(this);
    m_callTreeView->setHeaderLabels({"Function", "Self %", "Total %", "Calls", "Module"});
    m_callTreeView->setColumnWidth(0, 350);
    m_callTreeView->setColumnWidth(1, 80);
    m_callTreeView->setColumnWidth(2, 80);
    m_callTreeView->setColumnWidth(3, 80);
    
    connect(m_callTreeView, &QTreeWidget::itemDoubleClicked,
            this, &ProfilerWidget::onFunctionDoubleClicked);
}

void ProfilerWidget::setupFlameGraphView() {
    m_flameGraphView = new FlameGraphView(this);
    
    connect(m_flameGraphView, &FlameGraphView::frameClicked,
            this, &ProfilerWidget::onFlameFrameClicked);
    
    connect(m_flameGraphView, &FlameGraphView::frameHovered,
            this, [this](const QString& function, quint64 value, double percent) {
        QString tooltip = QString("%1\n%2 samples (%.2f%%)").arg(function).arg(value).arg(percent);
        QToolTip::showText(QCursor::pos(), tooltip);
    });
}

void ProfilerWidget::setupMemoryView() {
    m_memoryTree = new QTreeWidget(this);
    m_memoryTree->setHeaderLabels({"Address", "Size", "Allocator", "Status", "Stack Trace"});
    m_memoryTree->setColumnWidth(0, 150);
    m_memoryTree->setColumnWidth(1, 100);
    m_memoryTree->setColumnWidth(2, 100);
    m_memoryTree->setColumnWidth(3, 80);
    m_memoryTree->setRootIsDecorated(true);
}

void ProfilerWidget::setupTimelineView() {
    m_cpuChart = new QChart();
    m_cpuChart->setTitle("CPU Usage Over Time");
    m_cpuChart->setAnimationOptions(QChart::NoAnimation);
    m_cpuChart->setBackgroundBrush(QColor("#1e1e1e"));
    m_cpuChart->setTitleBrush(QBrush(Qt::white));
    
    m_cpuSeries = new QLineSeries();
    m_cpuSeries->setName("CPU %");
    m_cpuSeries->setColor(QColor("#4ec9b0"));
    m_cpuChart->addSeries(m_cpuSeries);
    
    m_cpuChart->createDefaultAxes();
    m_cpuChart->axes(Qt::Horizontal).first()->setTitleText("Time (s)");
    m_cpuChart->axes(Qt::Vertical).first()->setTitleText("CPU %");
    m_cpuChart->axes(Qt::Vertical).first()->setRange(0, 100);
    
    m_timelineChart = new QChartView(m_cpuChart, this);
    m_timelineChart->setRenderHint(QPainter::Antialiasing);
}

void ProfilerWidget::setupStatsPanel() {
    m_statsTable = new QTableWidget(this);
    m_statsTable->setColumnCount(2);
    m_statsTable->setHorizontalHeaderLabels({"Metric", "Value"});
    m_statsTable->horizontalHeader()->setStretchLastSection(true);
    m_statsTable->setAlternatingRowColors(true);
    m_statsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    // Add initial stats rows
    QStringList metrics = {
        "Total Samples",
        "Profiling Duration",
        "Samples/Second",
        "Peak Memory",
        "Current Memory",
        "Total Allocations",
        "Total Frees",
        "Memory Leaks",
        "Top Function",
        "Top Function %"
    };
    
    m_statsTable->setRowCount(metrics.size());
    for (int i = 0; i < metrics.size(); ++i) {
        m_statsTable->setItem(i, 0, new QTableWidgetItem(metrics[i]));
        m_statsTable->setItem(i, 1, new QTableWidgetItem("-"));
    }
}

// =============================================================================
// Profiler Control
// =============================================================================

void ProfilerWidget::startProfiling(const ProfileConfig& config) {
    RawrXD::Integration::logInfo("ProfilerWidget", "start", "Started profiling: " + config.name);
    RawrXD::Integration::recordMetric("profiler_runs", 1);
    
    if (m_state != ProfilerState::Idle) {
        stopProfiling();
    }
    
    clearData();
    m_activeConfig = config;
    setState(ProfilerState::Starting);
    
    // Choose profiler backend based on config type
    if (config.profilerType == "sampling") {
#ifdef Q_OS_LINUX
        startPerfProfiler(config);
#else
        startSamplingProfiler(config);
#endif
    } else if (config.profilerType == "instrumented") {
        startInstrumentedProfiler(config);
    } else if (config.profilerType == "memory") {
        startMemoryProfiler(config);
    } else {
        startSamplingProfiler(config);
    }
    
    m_sessionTimer.start();
    
    if (m_liveUpdate) {
        m_liveUpdateTimer->start(500);  // Update every 500ms
    }
}

void ProfilerWidget::stopProfiling() {
    RawrXD::Integration::logInfo("ProfilerWidget", "stop", "Stopped profiling");
    if (m_state == ProfilerState::Idle) return;
    
    setState(ProfilerState::Stopping);
    m_liveUpdateTimer->stop();
    
    if (m_profilerProcess && m_profilerProcess->state() != QProcess::NotRunning) {
        // Send SIGINT to gracefully stop
#ifdef Q_OS_WIN
        m_profilerProcess->kill();
#else
        m_profilerProcess->terminate();
#endif
        if (!m_profilerProcess->waitForFinished(5000)) {
            m_profilerProcess->kill();
            m_profilerProcess->waitForFinished(1000);
        }
    }
    
    // Analyze collected data
    setState(ProfilerState::Analyzing);
    analyzeHotSpots();
    buildCallTree();
    buildFlameGraph();
    calculateStatistics();
    
    setState(ProfilerState::Idle);
    emit profilingStopped();
}

void ProfilerWidget::pauseProfiling() {
    if (m_state != ProfilerState::Running) return;
    
    m_liveUpdateTimer->stop();
    
    if (m_profilerProcess) {
#ifndef Q_OS_WIN
        // Send SIGSTOP on Unix
        ::kill(m_profilerProcess->processId(), SIGSTOP);
#endif
    }
    
    emit profilingPaused();
}

void ProfilerWidget::resumeProfiling() {
    if (m_profilerProcess) {
#ifndef Q_OS_WIN
        ::kill(m_profilerProcess->processId(), SIGCONT);
#endif
    }
    
    if (m_liveUpdate) {
        m_liveUpdateTimer->start(500);
    }
    
    setState(ProfilerState::Running);
    emit profilingResumed();
}

void ProfilerWidget::clearData() {
    QMutexLocker lock(&m_dataMutex);
    
    m_samples.clear();
    m_allocations.clear();
    m_callTreeRoot.reset();
    m_flameRoot = FlameFrame();
    m_symbolCache.clear();
    
    m_totalSamples = 0;
    m_totalTime = 0;
    m_peakMemory = 0;
    m_currentMemory = 0;
    m_totalAllocations = 0;
    m_totalFrees = 0;
    
    // Clear UI
    m_hotSpotsTree->clear();
    m_callTreeView->clear();
    m_flameGraphView->clear();
    m_memoryTree->clear();
    m_cpuSeries->clear();
    
    updateStatsPanel();
}

// =============================================================================
// Profiler Backends
// =============================================================================

void ProfilerWidget::startPerfProfiler(const ProfileConfig& config) {
    m_profilerProcess = new QProcess(this);
    
    connect(m_profilerProcess, &QProcess::readyReadStandardOutput,
            this, &ProfilerWidget::processProfilerOutput);
    connect(m_profilerProcess, &QProcess::readyReadStandardError,
            this, &ProfilerWidget::processProfilerError);
    connect(m_profilerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProfilerWidget::onProfilerFinished);
    
    if (!config.cwd.isEmpty()) {
        m_profilerProcess->setWorkingDirectory(config.cwd);
    }
    
    QStringList args;
    args << "record"
         << "-F" << QString::number(config.samplingRate)
         << "-g"  // Call graph
         << "--call-graph" << "dwarf"
         << "-o" << "-";  // Output to stdout
    
    if (!config.program.isEmpty()) {
        args << "--" << config.program << config.args;
    }
    
    m_profilerProcess->start("perf", args);
    
    if (!m_profilerProcess->waitForStarted(5000)) {
        emit errorOccurred(QString("Failed to start perf: %1").arg(m_profilerProcess->errorString()));
        setState(ProfilerState::Idle);
        return;
    }
    
    setState(ProfilerState::Running);
    emit profilingStarted();
}

void ProfilerWidget::startSamplingProfiler(const ProfileConfig& config) {
    // Built-in sampling profiler using timer-based sampling
    m_profilerProcess = new QProcess(this);
    
    connect(m_profilerProcess, &QProcess::readyReadStandardOutput,
            this, &ProfilerWidget::processProfilerOutput);
    connect(m_profilerProcess, &QProcess::readyReadStandardError,
            this, &ProfilerWidget::processProfilerError);
    connect(m_profilerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProfilerWidget::onProfilerFinished);
    
    if (!config.cwd.isEmpty()) {
        m_profilerProcess->setWorkingDirectory(config.cwd);
    }
    
    // On Windows, use ETW via xperf or custom sampling
#ifdef Q_OS_WIN
    QStringList args;
    args << "-on" << "PROC_THREAD+LOADER+PROFILE"
         << "-stackwalk" << "Profile";
    m_profilerProcess->start("xperf", args);
#else
    // Use sample on macOS
    QStringList args;
    args << "-wait" << config.program;
    m_profilerProcess->start("sample", args);
#endif
    
    if (m_profilerProcess->waitForStarted(5000)) {
        setState(ProfilerState::Running);
        emit profilingStarted();
    } else {
        emit errorOccurred("Failed to start sampling profiler");
        setState(ProfilerState::Idle);
    }
}

void ProfilerWidget::startInstrumentedProfiler(const ProfileConfig& config) {
    Q_UNUSED(config)
    // Instrumented profiling requires compile-time support
    // This would integrate with -finstrument-functions or similar
    emit errorOccurred("Instrumented profiling requires recompilation with instrumentation flags");
    setState(ProfilerState::Idle);
}

void ProfilerWidget::startMemoryProfiler(const ProfileConfig& config) {
    m_profilerProcess = new QProcess(this);
    
    connect(m_profilerProcess, &QProcess::readyReadStandardOutput,
            this, &ProfilerWidget::processProfilerOutput);
    connect(m_profilerProcess, &QProcess::readyReadStandardError,
            this, &ProfilerWidget::processProfilerError);
    connect(m_profilerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProfilerWidget::onProfilerFinished);
    
#ifdef Q_OS_LINUX
    // Use valgrind massif or heaptrack
    QStringList args;
    args << "--tool=massif"
         << "--stacks=yes"
         << "--massif-out-file=/dev/stdout"
         << config.program << config.args;
    m_profilerProcess->start("valgrind", args);
#elif defined(Q_OS_WIN)
    // Use UMDH or custom allocator hooks
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("_NT_SYMBOL_PATH", m_symbolPaths.join(";"));
    m_profilerProcess->setProcessEnvironment(env);
    m_profilerProcess->start(config.program, config.args);
#else
    // macOS - use leaks or malloc_history
    QStringList args;
    args << "-allBySize" << config.program;
    m_profilerProcess->start("leaks", args);
#endif
    
    if (m_profilerProcess->waitForStarted(5000)) {
        setState(ProfilerState::Running);
        emit profilingStarted();
    } else {
        emit errorOccurred("Failed to start memory profiler");
        setState(ProfilerState::Idle);
    }
}

// =============================================================================
// Data Processing
// =============================================================================

void ProfilerWidget::processSample(const ProfileSample& sample) {
    QMutexLocker lock(&m_dataMutex);
    
    m_samples.append(sample);
    m_totalSamples++;
    m_totalTime += sample.selfTime;
    
    emit sampleCollected(sample);
}

void ProfilerWidget::buildCallTree() {
    QMutexLocker lock(&m_dataMutex);
    
    m_callTreeRoot = std::make_shared<CallTreeNode>();
    m_callTreeRoot->function = "[root]";
    
    // Group samples by call chain and build tree
    QMap<QString, std::shared_ptr<CallTreeNode>> nodeMap;
    
    for (const ProfileSample& sample : m_samples) {
        QString key = sample.function;
        
        if (!nodeMap.contains(key)) {
            auto node = std::make_shared<CallTreeNode>();
            node->function = sample.function;
            node->module = sample.module;
            node->parent = m_callTreeRoot.get();
            nodeMap[key] = node;
            m_callTreeRoot->children.append(node);
        }
        
        auto& node = nodeMap[key];
        node->selfTime += sample.selfTime;
        node->totalTime += sample.totalTime;
        node->callCount += sample.callCount;
    }
    
    // Calculate percentages
    quint64 totalRootTime = 0;
    for (const auto& child : m_callTreeRoot->children) {
        totalRootTime += child->totalTime;
    }
    
    for (auto& child : m_callTreeRoot->children) {
        if (totalRootTime > 0) {
            child->selfPercent = 100.0 * child->selfTime / totalRootTime;
            child->totalPercent = 100.0 * child->totalTime / totalRootTime;
        }
    }
    
    // Sort by total time descending
    std::sort(m_callTreeRoot->children.begin(), m_callTreeRoot->children.end(),
        [](const auto& a, const auto& b) {
            return a->totalTime > b->totalTime;
        });
}

void ProfilerWidget::buildFlameGraph() {
    QMutexLocker lock(&m_dataMutex);
    
    m_flameRoot.name = "all";
    m_flameRoot.value = m_totalSamples;
    m_flameRoot.children.clear();
    
    // Group samples by function for flat flame graph
    QMap<QString, FlameFrame> frameMap;
    
    for (const ProfileSample& sample : m_samples) {
        if (!frameMap.contains(sample.function)) {
            FlameFrame frame;
            frame.name = sample.function;
            frame.value = 0;
            frameMap[sample.function] = frame;
        }
        frameMap[sample.function].value += sample.callCount;
    }
    
    // Convert to vector and sort
    for (auto it = frameMap.begin(); it != frameMap.end(); ++it) {
        m_flameRoot.children.append(it.value());
    }
    
    std::sort(m_flameRoot.children.begin(), m_flameRoot.children.end(),
        [](const FlameFrame& a, const FlameFrame& b) {
            return a.value > b.value;
        });
}

void ProfilerWidget::calculateStatistics() {
    QMutexLocker lock(&m_dataMutex);
    
    // Already tracked: m_totalSamples, m_totalTime, m_peakMemory, etc.
    
    // Find memory leaks - note: base MemoryAllocation doesn't track freed status
    quint64 leaks = m_allocations.size();  // Assume all tracked for now
    
    // Update stats table
    updateStatsPanel();
}

// =============================================================================
// Analysis Methods
// =============================================================================

void ProfilerWidget::analyzeHotSpots() {
    QMutexLocker lock(&m_dataMutex);
    
    // Aggregate samples by function
    QMap<QString, ProfileSample> aggregated;
    
    for (const ProfileSample& sample : m_samples) {
        if (!aggregated.contains(sample.function)) {
            aggregated[sample.function] = sample;
            aggregated[sample.function].callCount = 0;
            aggregated[sample.function].selfTime = 0;
            aggregated[sample.function].totalTime = 0;
        }
        
        aggregated[sample.function].callCount += sample.callCount;
        aggregated[sample.function].selfTime += sample.selfTime;
        aggregated[sample.function].totalTime += sample.totalTime;
    }
    
    // Convert to list and sort by self time
    QVector<ProfileSample> hotSpots;
    for (auto it = aggregated.begin(); it != aggregated.end(); ++it) {
        hotSpots.append(it.value());
    }
    
    std::sort(hotSpots.begin(), hotSpots.end(),
        [](const ProfileSample& a, const ProfileSample& b) {
            return a.selfTime > b.selfTime;
        });
    
    // Update view
    lock.unlock();
    
    m_hotSpotsTree->clear();
    for (const ProfileSample& sample : hotSpots) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_hotSpotsTree);
        item->setText(0, sample.function);
        
        double selfPercent = m_totalTime > 0 ? 100.0 * sample.selfTime / m_totalTime : 0;
        double totalPercent = m_totalTime > 0 ? 100.0 * sample.totalTime / m_totalTime : 0;
        
        item->setText(1, QString::number(selfPercent, 'f', 2) + "%");
        item->setText(2, formatTime(sample.selfTime));
        item->setText(3, QString::number(totalPercent, 'f', 2) + "%");
        item->setText(4, formatTime(sample.totalTime));
        item->setText(5, QString::number(sample.callCount));
        item->setText(6, sample.module);
        
        item->setData(0, Qt::UserRole, sample.file);
        item->setData(0, Qt::UserRole + 1, sample.line);
        
        // Color code based on percentage
        if (selfPercent > 20) {
            item->setForeground(1, QColor("#f44747"));  // Red for hot
        } else if (selfPercent > 5) {
            item->setForeground(1, QColor("#dcdcaa"));  // Yellow for warm
        }
    }
}

void ProfilerWidget::analyzeMemoryLeaks() {
    QMutexLocker lock(&m_dataMutex);
    
    m_memoryTree->clear();
    
    // Note: RawrXD::MemoryAllocation doesn't track freed status
    // In production, we'd use MemoryAnalyzer::analyzeForLeaks() instead
    for (const RawrXD::MemoryAllocation& alloc : m_allocations) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_memoryTree);
        item->setText(0, QString("0x%1").arg(alloc.address, 16, 16, QChar('0')));
        item->setText(1, formatBytes(alloc.size));
        item->setText(2, alloc.allocator);
        item->setText(3, QStringLiteral("TRACKED"));
        item->setForeground(3, QColor("#4ec9b0"));
        
        // Add stack trace as children using callStack field
        for (const QString& frame : alloc.callStack) {
            QTreeWidgetItem* child = new QTreeWidgetItem(item);
            child->setText(0, frame);
        }
    }
}

void ProfilerWidget::generateFlameGraph() {
    buildFlameGraph();
    m_flameGraphView->setRootFrame(m_flameRoot);
}

void ProfilerWidget::exportReport(const QString& path, const QString& format) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred(QString("Failed to open file for writing: %1").arg(path));
        return;
    }
    
    QTextStream out(&file);
    
    if (format == "html") {
        out << "<!DOCTYPE html>\n<html>\n<head>\n";
        out << "<title>RawrXD Profiler Report</title>\n";
        out << "<style>body{font-family:sans-serif;margin:20px;background:#1e1e1e;color:#d4d4d4;}"
               "table{border-collapse:collapse;width:100%;}"
               "th,td{border:1px solid #444;padding:8px;text-align:left;}"
               "th{background:#333;}.hot{color:#f44747;}</style>\n";
        out << "</head>\n<body>\n";
        out << "<h1>Profile Report</h1>\n";
        out << QString("<p>Generated: %1</p>\n").arg(QDateTime::currentDateTime().toString());
        out << QString("<p>Duration: %1</p>\n").arg(formatTime(m_totalTime));
        out << QString("<p>Total Samples: %1</p>\n").arg(m_totalSamples);
        
        out << "<h2>Hot Spots</h2>\n";
        out << "<table>\n<tr><th>Function</th><th>Self %</th><th>Self Time</th><th>Samples</th></tr>\n";
        
        for (int i = 0; i < m_hotSpotsTree->topLevelItemCount() && i < 50; ++i) {
            QTreeWidgetItem* item = m_hotSpotsTree->topLevelItem(i);
            QString cssClass = item->text(1).remove('%').toDouble() > 10 ? " class=\"hot\"" : "";
            out << QString("<tr%5><td>%1</td><td>%2</td><td>%3</td><td>%4</td></tr>\n")
                   .arg(item->text(0).toHtmlEscaped(), item->text(1), item->text(2), item->text(5), cssClass);
        }
        
        out << "</table>\n</body>\n</html>\n";
        
    } else if (format == "csv") {
        out << "Function,Self %,Self Time,Total %,Total Time,Samples,Module\n";
        
        for (int i = 0; i < m_hotSpotsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_hotSpotsTree->topLevelItem(i);
            out << QString("\"%1\",%2,%3,%4,%5,%6,\"%7\"\n")
                   .arg(item->text(0), item->text(1), item->text(2),
                        item->text(3), item->text(4), item->text(5), item->text(6));
        }
        
    } else {  // JSON
        QJsonObject root;
        root["generated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["duration_ns"] = static_cast<qint64>(m_totalTime);
        root["total_samples"] = static_cast<qint64>(m_totalSamples);
        
        QJsonArray hotSpots;
        for (int i = 0; i < m_hotSpotsTree->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_hotSpotsTree->topLevelItem(i);
            QJsonObject spot;
            spot["function"] = item->text(0);
            spot["self_percent"] = item->text(1).remove('%').toDouble();
            spot["samples"] = item->text(5).toLongLong();
            spot["module"] = item->text(6);
            hotSpots.append(spot);
        }
        root["hot_spots"] = hotSpots;
        
        out << QJsonDocument(root).toJson(QJsonDocument::Indented);
    }
    
    file.close();
}

// =============================================================================
// Process Slots
// =============================================================================

void ProfilerWidget::processProfilerOutput() {
    if (!m_profilerProcess) return;
    
    m_outputBuffer += QString::fromUtf8(m_profilerProcess->readAllStandardOutput());
    
    // Process complete lines
    int newlineIndex;
    while ((newlineIndex = m_outputBuffer.indexOf('\n')) != -1) {
        QString line = m_outputBuffer.left(newlineIndex).trimmed();
        m_outputBuffer = m_outputBuffer.mid(newlineIndex + 1);
        
        if (!line.isEmpty()) {
            parsePerfOutput(line);
        }
    }
}

void ProfilerWidget::processProfilerError() {
    if (!m_profilerProcess) return;
    
    QString error = QString::fromUtf8(m_profilerProcess->readAllStandardError());
    if (!error.trimmed().isEmpty()) {
        // Don't emit as error if it's just info/warning
        if (error.contains("error", Qt::CaseInsensitive)) {
            emit errorOccurred(error);
        }
    }
}

void ProfilerWidget::onProfilerFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    
    if (m_state == ProfilerState::Running) {
        stopProfiling();
    }
    
    m_profilerProcess->deleteLater();
    m_profilerProcess = nullptr;
}

void ProfilerWidget::parsePerfOutput(const QString& output) {
    // Parse perf output format
    // Sample format: cycles:ppp: addr func+offset (module)
    
    static QRegularExpression perfLineRe(
        R"(^\s*(\d+(?:\.\d+)?%?)\s+(\S+)\s+\[(\S+)\]\s+(\S+)\s+(.+))");
    
    QRegularExpressionMatch match = perfLineRe.match(output);
    if (match.hasMatch()) {
        ProfileSample sample;
        
        QString percentStr = match.captured(1);
        if (percentStr.endsWith('%')) {
            percentStr.chop(1);
        }
        double percent = percentStr.toDouble();
        
        sample.function = match.captured(5).trimmed();
        sample.module = match.captured(3);
        sample.callCount = 1;
        sample.selfTime = static_cast<quint64>(percent * 1000000);  // Convert to pseudo-ns
        sample.totalTime = sample.selfTime;
        
        // Extract file:line if present
        int parenPos = sample.function.indexOf('(');
        if (parenPos > 0) {
            QString location = sample.function.mid(parenPos + 1);
            location.chop(1);  // Remove closing paren
            int colonPos = location.lastIndexOf(':');
            if (colonPos > 0) {
                sample.file = location.left(colonPos);
                sample.line = location.mid(colonPos + 1).toInt();
            }
            sample.function = sample.function.left(parenPos).trimmed();
        }
        
        processSample(sample);
    }
}

void ProfilerWidget::updateLiveData() {
    if (m_state != ProfilerState::Running) return;
    
    // Update CPU series with elapsed time
    double elapsed = m_sessionTimer.elapsed() / 1000.0;
    double cpuPercent = m_totalSamples > 0 ? 
        qMin(100.0, static_cast<double>(m_samples.size()) / (m_activeConfig.samplingRate * elapsed) * 100) : 0;
    
    m_cpuSeries->append(elapsed, cpuPercent);
    
    // Keep last 60 seconds of data
    while (m_cpuSeries->count() > 120) {
        m_cpuSeries->remove(0);
    }
    
    // Update axis range
    if (m_cpuChart->axes(Qt::Horizontal).size() > 0) {
        m_cpuChart->axes(Qt::Horizontal).first()->setRange(
            qMax(0.0, elapsed - 60), elapsed);
    }
    
    updateStatsPanel();
}

// =============================================================================
// UI Update Methods
// =============================================================================

void ProfilerWidget::updateHotSpotsView() {
    analyzeHotSpots();
}

void ProfilerWidget::updateCallTreeView() {
    m_callTreeView->clear();
    
    if (!m_callTreeRoot) return;
    
    std::function<void(QTreeWidgetItem*, const std::shared_ptr<CallTreeNode>&)> addNode;
    addNode = [&](QTreeWidgetItem* parent, const std::shared_ptr<CallTreeNode>& node) {
        QTreeWidgetItem* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_callTreeView);
        item->setText(0, node->function);
        item->setText(1, QString::number(node->selfPercent, 'f', 2) + "%");
        item->setText(2, QString::number(node->totalPercent, 'f', 2) + "%");
        item->setText(3, QString::number(node->callCount));
        item->setText(4, node->module);
        
        for (const auto& child : node->children) {
            addNode(item, child);
        }
    };
    
    for (const auto& child : m_callTreeRoot->children) {
        addNode(nullptr, child);
    }
}

void ProfilerWidget::updateMemoryView() {
    analyzeMemoryLeaks();
}

void ProfilerWidget::updateTimelineChart() {
    // Already updated in updateLiveData()
}

void ProfilerWidget::updateStatsPanel() {
    double durationSec = m_sessionTimer.elapsed() / 1000.0;
    double samplesPerSec = durationSec > 0 ? m_totalSamples / durationSec : 0;
    
    // Find top function
    QString topFunction = "-";
    double topPercent = 0;
    if (m_hotSpotsTree->topLevelItemCount() > 0) {
        QTreeWidgetItem* top = m_hotSpotsTree->topLevelItem(0);
        topFunction = top->text(0);
        topPercent = top->text(1).remove('%').toDouble();
    }
    
    quint64 leaks = 0;
    // Note: RawrXD::MemoryAllocation doesn't track freed status
    // Count all tracked allocations as potential leaks for statistics
    for (const RawrXD::MemoryAllocation& a : m_allocations) {
        // In production, compare with deallocation map
        leaks++;
    }
    
    QStringList values = {
        QString::number(m_totalSamples),
        formatTime(static_cast<quint64>(durationSec * 1000000000)),
        QString::number(samplesPerSec, 'f', 1),
        formatBytes(m_peakMemory),
        formatBytes(m_currentMemory),
        QString::number(m_totalAllocations),
        QString::number(m_totalFrees),
        QString::number(leaks),
        topFunction,
        QString::number(topPercent, 'f', 2) + "%"
    };
    
    for (int i = 0; i < values.size() && i < m_statsTable->rowCount(); ++i) {
        m_statsTable->item(i, 1)->setText(values[i]);
    }
}

void ProfilerWidget::setState(ProfilerState newState) {
    ProfilerState oldState = m_state;
    m_state = newState;
    
    // Update button states
    bool idle = (newState == ProfilerState::Idle);
    bool running = (newState == ProfilerState::Running);
    
    m_startBtn->setEnabled(idle);
    m_stopBtn->setEnabled(!idle);
    m_pauseBtn->setEnabled(running);
    m_clearBtn->setEnabled(idle);
    
    if (oldState != newState) {
        emit stateChanged(newState);
    }
}

// =============================================================================
// Slot Implementations
// =============================================================================

void ProfilerWidget::onConfigChanged(int index) {
    if (index >= 0 && index < m_configs.size()) {
        m_activeConfig = m_configs[index];
    }
}

void ProfilerWidget::onFunctionDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    QString file = item->data(0, Qt::UserRole).toString();
    int line = item->data(0, Qt::UserRole + 1).toInt();
    
    if (!file.isEmpty() && line > 0) {
        emit navigateToSource(file, line);
    }
}

void ProfilerWidget::onFlameFrameClicked(const QString& function) {
    // Find and select in hot spots tree
    for (int i = 0; i < m_hotSpotsTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_hotSpotsTree->topLevelItem(i);
        if (item->text(0) == function) {
            m_hotSpotsTree->setCurrentItem(item);
            m_hotSpotsTree->scrollToItem(item);
            break;
        }
    }
}

void ProfilerWidget::refreshViews() {
    analyzeHotSpots();
    updateCallTreeView();
    generateFlameGraph();
    updateMemoryView();
    updateStatsPanel();
    
    emit analysisComplete();
}

void ProfilerWidget::toggleLiveUpdate(bool enabled) {
    m_liveUpdate = enabled;
    
    if (enabled && m_state == ProfilerState::Running) {
        m_liveUpdateTimer->start(500);
    } else {
        m_liveUpdateTimer->stop();
    }
}

// =============================================================================
// Configuration
// =============================================================================

void ProfilerWidget::setWorkingDirectory(const QString& dir) {
    m_workingDirectory = dir;
}

void ProfilerWidget::addSymbolPath(const QString& path) {
    if (!m_symbolPaths.contains(path)) {
        m_symbolPaths.append(path);
    }
}

QVector<ProfileConfig> ProfilerWidget::getConfigs() const {
    return m_configs;
}

void ProfilerWidget::addConfig(const ProfileConfig& config) {
    m_configs.append(config);
    m_configSelector->addItem(config.name);
}

// =============================================================================
// Data Access
// =============================================================================

QVector<ProfileSample> ProfilerWidget::getSamples() const {
    QMutexLocker lock(const_cast<QMutex*>(&m_dataMutex));
    return m_samples;
}

QVector<RawrXD::MemoryAllocation> ProfilerWidget::getAllocations() const {
    QMutexLocker lock(const_cast<QMutex*>(&m_dataMutex));
    // Convert QVector to QList for compatibility
    QVector<RawrXD::MemoryAllocation> result;
    result.reserve(m_allocations.size());
    for (const auto& alloc : m_allocations) {
        result.append(alloc);
    }
    return result;
}

std::shared_ptr<CallTreeNode> ProfilerWidget::getCallTree() const {
    return m_callTreeRoot;
}

// =============================================================================
// Symbol Resolution
// =============================================================================

QString ProfilerWidget::resolveSymbol(quint64 address) {
    if (m_symbolCache.contains(address)) {
        return m_symbolCache[address];
    }
    
    // Would use addr2line, dbghelp, or similar here
    QString symbol = QString("0x%1").arg(address, 16, 16, QChar('0'));
    m_symbolCache[address] = symbol;
    return symbol;
}

void ProfilerWidget::loadSymbols(const QString& path) {
    Q_UNUSED(path)
    // Would load DWARF/PDB symbols here
}

// =============================================================================
// Utility Functions
// =============================================================================

static QString formatTime(quint64 nanoseconds) {
    if (nanoseconds < 1000) {
        return QString("%1 ns").arg(nanoseconds);
    } else if (nanoseconds < 1000000) {
        return QString("%1 µs").arg(nanoseconds / 1000.0, 0, 'f', 2);
    } else if (nanoseconds < 1000000000) {
        return QString("%1 ms").arg(nanoseconds / 1000000.0, 0, 'f', 2);
    } else {
        return QString("%1 s").arg(nanoseconds / 1000000000.0, 0, 'f', 3);
    }
}

static QString formatBytes(quint64 bytes) {
    if (bytes < 1024) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < 1024 * 1024) {
        return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    } else if (bytes < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024.0 * 1024), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(bytes / (1024.0 * 1024 * 1024), 0, 'f', 2);
    }
}
