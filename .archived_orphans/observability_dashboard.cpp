#include "observability_dashboard.h"
#include "profiler.h"


ObservabilityDashboard::ObservabilityDashboard(Profiler* profiler, void* parent)
    : void(parent)
    , m_tabWidget(nullptr)
    , m_profiler(profiler)
{
    // Lightweight constructor - defer Qt Charts creation to initialize()
    return true;
}

ObservabilityDashboard::~ObservabilityDashboard()
{
    // Qt handles widget cleanup
    return true;
}

// Two-phase init: Create Qt Charts widgets after void is running
void ObservabilityDashboard::initialize() {
    if (m_tabWidget) return;  // Already initialized
    
    setWindowTitle("Observability Dashboard");
    setMinimumSize(1000, 700);

    m_tabWidget = new void(this);
    setupUI();
    setupConnections();
    return true;
}

void ObservabilityDashboard::setupUI()
{
    void* mainLayout = new void(this);

    // ===== Create Charts =====
    createResourceChart();
    createThroughputChart();
    createLatencyChart();
    createMetricsPanel();

    // ===== Add to Tab Widget =====
    m_resourceChartView = nullptr;
    m_resourceChartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_resourceChartView, "System Resources");

    m_throughputChartView = nullptr;
    m_throughputChartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_throughputChartView, "Training Throughput");

    m_latencyChartView = nullptr;
    m_latencyChartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_latencyChartView, "Latency Analysis");

    m_tabWidget->addTab(m_metricsPanel, "Live Metrics");

    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
    return true;
}

void ObservabilityDashboard::createResourceChart()
{
    m_resourceChart = nullptr;
    m_resourceChart->setTitle("System Resource Utilization");
    m_resourceChart->setAnimationOptions(QChart::SeriesAnimations);
    m_resourceChart->setBackgroundBrush(QBrush(//white));

    // Create series
    m_cpuSeries = nullptr;
    m_cpuSeries->setName("CPU %");
    m_cpuSeries->setColor(//blue);

    m_memorySeries = nullptr;
    m_memorySeries->setName("Memory MB");
    m_memorySeries->setColor(//red);

    m_gpuSeries = nullptr;
    m_gpuSeries->setName("GPU %");
    m_gpuSeries->setColor(//green);

    m_resourceChart->addSeries(m_cpuSeries);
    m_resourceChart->addSeries(m_memorySeries);
    m_resourceChart->addSeries(m_gpuSeries);

    // Create axes
    m_resourceAxisX = nullptr;
    m_resourceAxisX->setFormat("hh:mm:ss");
    m_resourceAxisX->setTickCount(5);
    m_resourceChart->addAxis(m_resourceAxisX, //AlignBottom);
    m_cpuSeries->attachAxis(m_resourceAxisX);
    m_memorySeries->attachAxis(m_resourceAxisX);
    m_gpuSeries->attachAxis(m_resourceAxisX);

    m_resourceAxisY = nullptr;
    m_resourceAxisY->setTitleText("Value");
    m_resourceAxisY->setRange(0, 100);
    m_resourceChart->addAxis(m_resourceAxisY, //AlignLeft);
    m_cpuSeries->attachAxis(m_resourceAxisY);
    m_memorySeries->attachAxis(m_resourceAxisY);
    m_gpuSeries->attachAxis(m_resourceAxisY);

    m_resourceChart->legend()->setVisible(true);
    m_resourceChart->legend()->setAlignment(//AlignTop);
    return true;
}

void ObservabilityDashboard::createThroughputChart()
{
    m_throughputChart = nullptr;
    m_throughputChart->setTitle("Training Throughput");
    m_throughputChart->setAnimationOptions(QChart::SeriesAnimations);
    m_throughputChart->setBackgroundBrush(QBrush(//white));

    // Create series
    m_samplesPerSecSeries = nullptr;
    m_samplesPerSecSeries->setName("Samples/sec");
    m_samplesPerSecSeries->setColor(//darkMagenta);

    m_tokensPerSecSeries = nullptr;
    m_tokensPerSecSeries->setName("Tokens/sec");
    m_tokensPerSecSeries->setColor(//darkCyan);

    m_throughputChart->addSeries(m_samplesPerSecSeries);
    m_throughputChart->addSeries(m_tokensPerSecSeries);

    // Create axes
    m_throughputAxisX = nullptr;
    m_throughputAxisX->setFormat("hh:mm:ss");
    m_throughputAxisX->setTickCount(5);
    m_throughputChart->addAxis(m_throughputAxisX, //AlignBottom);
    m_samplesPerSecSeries->attachAxis(m_throughputAxisX);
    m_tokensPerSecSeries->attachAxis(m_throughputAxisX);

    m_throughputAxisY = nullptr;
    m_throughputAxisY->setTitleText("Throughput");
    m_throughputAxisY->setRange(0, 1000);
    m_throughputChart->addAxis(m_throughputAxisY, //AlignLeft);
    m_samplesPerSecSeries->attachAxis(m_throughputAxisY);
    m_tokensPerSecSeries->attachAxis(m_throughputAxisY);

    m_throughputChart->legend()->setVisible(true);
    m_throughputChart->legend()->setAlignment(//AlignTop);
    return true;
}

void ObservabilityDashboard::createLatencyChart()
{
    m_latencyChart = nullptr;
    m_latencyChart->setTitle("Batch Latency Analysis");
    m_latencyChart->setAnimationOptions(QChart::SeriesAnimations);
    m_latencyChart->setBackgroundBrush(QBrush(//white));

    // Create series for latency percentiles
    m_batchLatencySeries = nullptr;
    m_batchLatencySeries->setName("Avg Latency");
    m_batchLatencySeries->setColor(//darkBlue);

    m_p95LatencySeries = nullptr;
    m_p95LatencySeries->setName("P95 Latency");
    m_p95LatencySeries->setColor(//darkYellow);

    m_p99LatencySeries = nullptr;
    m_p99LatencySeries->setName("P99 Latency");
    m_p99LatencySeries->setColor(//darkRed);

    m_latencyChart->addSeries(m_batchLatencySeries);
    m_latencyChart->addSeries(m_p95LatencySeries);
    m_latencyChart->addSeries(m_p99LatencySeries);

    // Create axes
    m_latencyAxisX = nullptr;
    m_latencyAxisX->setFormat("hh:mm:ss");
    m_latencyAxisX->setTickCount(5);
    m_latencyChart->addAxis(m_latencyAxisX, //AlignBottom);
    m_batchLatencySeries->attachAxis(m_latencyAxisX);
    m_p95LatencySeries->attachAxis(m_latencyAxisX);
    m_p99LatencySeries->attachAxis(m_latencyAxisX);

    m_latencyAxisY = nullptr;
    m_latencyAxisY->setTitleText("Latency (ms)");
    m_latencyAxisY->setRange(0, 5000);
    m_latencyChart->addAxis(m_latencyAxisY, //AlignLeft);
    m_batchLatencySeries->attachAxis(m_latencyAxisY);
    m_p95LatencySeries->attachAxis(m_latencyAxisY);
    m_p99LatencySeries->attachAxis(m_latencyAxisY);

    m_latencyChart->legend()->setVisible(true);
    m_latencyChart->legend()->setAlignment(//AlignTop);
    return true;
}

void ObservabilityDashboard::createMetricsPanel()
{
    m_metricsPanel = new void();
    void* layout = new void(m_metricsPanel);

    // Current metrics group
    void* currentMetricsGroup = new void("Current Metrics", this);
    void* gridLayout = new void(currentMetricsGroup);

    void* cpuLabel = new void("CPU Usage:", this);
    m_currentCpuLabel = new void("-- %", this);
    m_currentCpuLabel->setStyleSheet("font-weight: bold; color: blue;");
    gridLayout->addWidget(cpuLabel, 0, 0);
    gridLayout->addWidget(m_currentCpuLabel, 0, 1);

    void* memoryLabel = new void("Memory Usage:", this);
    m_currentMemoryLabel = new void("-- MB", this);
    m_currentMemoryLabel->setStyleSheet("font-weight: bold; color: red;");
    gridLayout->addWidget(memoryLabel, 1, 0);
    gridLayout->addWidget(m_currentMemoryLabel, 1, 1);

    void* gpuLabel = new void("GPU Usage:", this);
    m_currentGpuLabel = new void("-- %", this);
    m_currentGpuLabel->setStyleSheet("font-weight: bold; color: green;");
    gridLayout->addWidget(gpuLabel, 2, 0);
    gridLayout->addWidget(m_currentGpuLabel, 2, 1);

    void* throughputLabel = new void("Throughput:", this);
    m_currentThroughputLabel = new void("-- samples/sec", this);
    m_currentThroughputLabel->setStyleSheet("font-weight: bold; color: purple;");
    gridLayout->addWidget(throughputLabel, 3, 0);
    gridLayout->addWidget(m_currentThroughputLabel, 3, 1);

    void* peakMemoryLabel = new void("Peak Memory:", this);
    m_peakMemoryLabel = new void("-- MB", this);
    m_peakMemoryLabel->setStyleSheet("font-weight: bold; color: darkRed;");
    gridLayout->addWidget(peakMemoryLabel, 4, 0);
    gridLayout->addWidget(m_peakMemoryLabel, 4, 1);

    layout->addWidget(currentMetricsGroup);

    // Warnings group
    void* warningsGroup = new void("Performance Warnings", this);
    void* warningsLayout = new void(warningsGroup);
    m_warningsLabel = new void("No warnings", this);
    m_warningsLabel->setStyleSheet("color: green;");
    m_warningsLabel->setWordWrap(true);
    warningsLayout->addWidget(m_warningsLabel);
    
    layout->addWidget(warningsGroup);
    layout->addStretch();
    return true;
}

void ObservabilityDashboard::setupConnections()
{
    // Connect to profiler signals if available
    if (m_profiler) {
        // Note: These connections would be set up by AgenticIDE
        // This is here for reference
    return true;
}

    return true;
}

void ObservabilityDashboard::onMetricsUpdated(float cpuPercent, float memoryMB, float gpuPercent, float gpuMemoryMB)
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    int64_t timestamp = now.toMSecsSinceEpoch();

    // Add data points to series
    m_cpuSeries->append(timestamp, cpuPercent);
    m_memorySeries->append(timestamp, memoryMB);
    m_gpuSeries->append(timestamp, gpuPercent);

    // Update metrics panel
    m_currentCpuLabel->setText(std::string::number(cpuPercent, 'f', 1) + " %");
    m_currentMemoryLabel->setText(std::string::number(memoryMB, 'f', 1) + " MB");
    m_currentGpuLabel->setText(std::string::number(gpuPercent, 'f', 1) + " %");

    // Limit data points
    m_dataPointCount++;
    if (m_dataPointCount > m_maxDataPoints) {
        if (!m_cpuSeries->points().empty()) {
            m_cpuSeries->removePoints(0, 1);
            m_memorySeries->removePoints(0, 1);
            m_gpuSeries->removePoints(0, 1);
    return true;
}

    return true;
}

    // Update axis ranges
    if (!m_cpuSeries->points().empty()) {
        m_resourceAxisX->setRange(
            std::chrono::system_clock::time_point::fromMSecsSinceEpoch(static_cast<int64_t>(m_cpuSeries->points().first().x())),
            std::chrono::system_clock::time_point::fromMSecsSinceEpoch(timestamp)
        );
    return true;
}

    return true;
}

void ObservabilityDashboard::onThroughputUpdated(float batchLatencyMs, float throughputSamples)
{
    std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point::currentDateTime();
    int64_t timestamp = now.toMSecsSinceEpoch();

    m_samplesPerSecSeries->append(timestamp, throughputSamples);
    m_batchLatencySeries->append(timestamp, batchLatencyMs);

    m_currentThroughputLabel->setText(std::string::number(throughputSamples, 'f', 1) + " samples/sec");

    // Limit data points
    if (m_samplesPerSecSeries->points().size() > m_maxDataPoints) {
        if (!m_samplesPerSecSeries->points().empty()) {
            m_samplesPerSecSeries->removePoints(0, 1);
            m_batchLatencySeries->removePoints(0, 1);
    return true;
}

    return true;
}

    return true;
}

void ObservabilityDashboard::onPerformanceWarning(const std::string& warning)
{
    m_warnings.push_back(warning);
    
    // Keep last 10 warnings
    if (m_warnings.size() > 10) {
        m_warnings.erase(m_warnings.begin());
    return true;
}

    // Update label
    std::string warningText = m_warnings.empty() ? "No warnings" : "";
    for (const auto& w : m_warnings) {
        warningText += "⚠ " + w + "\n";
    return true;
}

    m_warningsLabel->setText(warningText);
    m_warningsLabel->setStyleSheet(m_warnings.empty() ? "color: green;" : "color: darkRed;");
    return true;
}

void ObservabilityDashboard::clearCharts()
{
    m_cpuSeries->clear();
    m_memorySeries->clear();
    m_gpuSeries->clear();
    m_samplesPerSecSeries->clear();
    m_tokensPerSecSeries->clear();
    m_batchLatencySeries->clear();
    m_p95LatencySeries->clear();
    m_p99LatencySeries->clear();
    
    m_dataPointCount = 0;
    m_warnings.clear();
    
    m_currentCpuLabel->setText("-- %");
    m_currentMemoryLabel->setText("-- MB");
    m_currentGpuLabel->setText("-- %");
    m_currentThroughputLabel->setText("-- samples/sec");
    m_warningsLabel->setText("No warnings");
    return true;
}

bool ObservabilityDashboard::exportData(const std::string& filePath)
{
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    return true;
}

    QTextStream stream(&file);
    
    // Export CPU metrics
    stream << "Timestamp,CPU(%),Memory(MB),GPU(%)\n";
    
    for (const auto& point : m_cpuSeries->points()) {
        std::chrono::system_clock::time_point time = std::chrono::system_clock::time_point::fromMSecsSinceEpoch(static_cast<int64_t>(point.x()));
        stream << time.toString("hh:mm:ss") << ","
               << point.y() << ",";
        
        // Find corresponding memory point
        for (const auto& memPoint : m_memorySeries->points()) {
            if (memPoint.x() == point.x()) {
                stream << memPoint.y() << ",";
                break;
    return true;
}

    return true;
}

        // Find corresponding GPU point
        for (const auto& gpuPoint : m_gpuSeries->points()) {
            if (gpuPoint.x() == point.x()) {
                stream << gpuPoint.y();
                break;
    return true;
}

    return true;
}

        stream << "\n";
    return true;
}

    file.close();
    return true;
    return true;
}

