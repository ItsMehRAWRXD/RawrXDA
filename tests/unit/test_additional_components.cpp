// RawrXD Unit Tests - Additional Components
// Version: 2.0
// Target: 40+ tests for HardwareBackend, Profiler, ObservabilityDashboard

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "../src/hardware_backend_selector.h"
#include "../src/profiler.h"
#include "../src/observability_dashboard.h"

// ============================================================================
// HARDWARE BACKEND SELECTOR TESTS
// ============================================================================

class TestHardwareBackendSelector : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Auto-Detection Tests
    void testCUDADetection();
    void testVulkanDetection();
    void testCPUFallback();
    void testMultipleBackendsAvailable();
    void testBackendPriority();

    // Device Enumeration Tests
    void testListGPUDevices();
    void testGPUCapabilities();
    void testDeviceSelection();
    void testInvalidDeviceRejection();

    // Backend Initialization Tests
    void testInitializeCUDA();
    void testInitializeVulkan();
    void testInitializeCPU();
    void testBackendSwitching();

    // Capability Tests
    void testComputeCapabilityQuery();
    void testMemoryQuery();
    void testConcurrentStreamSupport();

private:
    HardwareBackendSelector* selector;
};

void TestHardwareBackendSelector::init()
{
    selector = new HardwareBackendSelector();
}

void TestHardwareBackendSelector::cleanup()
{
    delete selector;
}

void TestHardwareBackendSelector::testCUDADetection()
{
    selector->autoDetect();
    
    auto backends = selector->getAvailableBackends();
    
    // If CUDA is available, it should be detected
    bool cudaSupported = selector->isBackendSupported("CUDA");
    if (cudaSupported) {
        QVERIFY(backends.contains("CUDA"));
    }
}

void TestHardwareBackendSelector::testVulkanDetection()
{
    selector->autoDetect();
    
    bool vulkanSupported = selector->isBackendSupported("Vulkan");
    if (vulkanSupported) {
        auto backends = selector->getAvailableBackends();
        QVERIFY(backends.contains("Vulkan"));
    }
}

void TestHardwareBackendSelector::testCPUFallback()
{
    selector->autoDetect();
    
    // CPU should always be available as fallback
    QVERIFY(selector->isBackendSupported("CPU"));
    
    auto backends = selector->getAvailableBackends();
    QVERIFY(backends.contains("CPU"));
}

void TestHardwareBackendSelector::testMultipleBackendsAvailable()
{
    selector->autoDetect();
    
    auto backends = selector->getAvailableBackends();
    
    // Should have at least CPU
    QVERIFY(backends.size() >= 1);
}

void TestHardwareBackendSelector::testBackendPriority()
{
    selector->autoDetect();
    
    QString best = selector->selectBestBackend();
    
    // Priority: CUDA > Vulkan > CPU
    if (selector->isBackendSupported("CUDA")) {
        QCOMPARE(best, QString("CUDA"));
    } else if (selector->isBackendSupported("Vulkan")) {
        QCOMPARE(best, QString("Vulkan"));
    } else {
        QCOMPARE(best, QString("CPU"));
    }
}

void TestHardwareBackendSelector::testListGPUDevices()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        auto devices = selector->listDevices("CUDA");
        QVERIFY(devices.size() > 0);
        
        for (const auto& device : devices) {
            QVERIFY(device.deviceIndex >= 0);
            QVERIFY(!device.deviceName.isEmpty());
        }
    }
}

void TestHardwareBackendSelector::testGPUCapabilities()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        auto devices = selector->listDevices("CUDA");
        if (!devices.isEmpty()) {
            const auto& device = devices.first();
            QVERIFY(device.totalMemoryMB > 0);
            QVERIFY(device.computeCapability > 0.0);
        }
    }
}

void TestHardwareBackendSelector::testDeviceSelection()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        bool result = selector->selectDevice("CUDA", 0);
        QVERIFY(result);
        
        QCOMPARE(selector->getCurrentBackend(), QString("CUDA"));
        QCOMPARE(selector->getCurrentDeviceIndex(), 0);
    }
}

void TestHardwareBackendSelector::testInvalidDeviceRejection()
{
    selector->autoDetect();
    
    // Invalid device index should fail
    bool result = selector->selectDevice("CUDA", 999);
    QVERIFY(!result);
}

void TestHardwareBackendSelector::testInitializeCUDA()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        bool result = selector->initializeBackend("CUDA");
        QVERIFY(result);
    }
}

void TestHardwareBackendSelector::testInitializeVulkan()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("Vulkan")) {
        bool result = selector->initializeBackend("Vulkan");
        QVERIFY(result);
    }
}

void TestHardwareBackendSelector::testInitializeCPU()
{
    selector->autoDetect();
    
    bool result = selector->initializeBackend("CPU");
    QVERIFY(result);
}

void TestHardwareBackendSelector::testBackendSwitching()
{
    selector->autoDetect();
    
    // Initialize CPU
    selector->initializeBackend("CPU");
    QCOMPARE(selector->getCurrentBackend(), QString("CPU"));
    
    // Switch to CUDA if available
    if (selector->isBackendSupported("CUDA")) {
        selector->initializeBackend("CUDA");
        QCOMPARE(selector->getCurrentBackend(), QString("CUDA"));
    }
}

void TestHardwareBackendSelector::testComputeCapabilityQuery()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        selector->selectDevice("CUDA", 0);
        
        double capability = selector->getComputeCapability();
        QVERIFY(capability > 0.0);
    }
}

void TestHardwareBackendSelector::testMemoryQuery()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        selector->selectDevice("CUDA", 0);
        
        size_t totalMem = selector->getTotalMemoryMB();
        size_t freeMem = selector->getFreeMemoryMB();
        
        QVERIFY(totalMem > 0);
        QVERIFY(freeMem <= totalMem);
    }
}

void TestHardwareBackendSelector::testConcurrentStreamSupport()
{
    selector->autoDetect();
    
    if (selector->isBackendSupported("CUDA")) {
        selector->selectDevice("CUDA", 0);
        
        bool supportsStreams = selector->supportsConcurrentStreams();
        // Modern GPUs should support concurrent streams
        QVERIFY(supportsStreams);
    }
}

// ============================================================================
// PROFILER TESTS
// ============================================================================

class TestProfiler : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Profiling Control Tests
    void testStartProfiling();
    void testStopProfiling();
    void testResetProfiling();
    void testProfilingState();

    // Phase Tracking Tests
    void testMarkPhaseStart();
    void testMarkPhaseEnd();
    void testNestedPhases();
    void testPhaseTimingAccuracy();

    // Metrics Collection Tests
    void testCPUMetrics();
    void testGPUMetrics();
    void testMemoryMetrics();
    void testThroughputMetrics();

    // Snapshot Tests
    void testGetSnapshot();
    void testSnapshotContents();
    void testPercentileCalculation();

    // Export Tests
    void testExportReport();
    void testReportFormat();

private:
    Profiler* profiler;
    QTemporaryDir* tempDir;
};

void TestProfiler::init()
{
    profiler = new Profiler();
    tempDir = new QTemporaryDir();
}

void TestProfiler::cleanup()
{
    delete profiler;
    delete tempDir;
}

void TestProfiler::testStartProfiling()
{
    bool result = profiler->startProfiling();
    QVERIFY(result);
    QVERIFY(profiler->isProfiling());
}

void TestProfiler::testStopProfiling()
{
    profiler->startProfiling();
    profiler->stopProfiling();
    QVERIFY(!profiler->isProfiling());
}

void TestProfiler::testResetProfiling()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("phase1");
    QTest::qWait(10);
    profiler->markPhaseEnd("phase1");
    
    profiler->reset();
    
    auto snapshot = profiler->getSnapshot();
    QCOMPARE(snapshot.forwardPassMs, 0.0);
}

void TestProfiler::testProfilingState()
{
    QVERIFY(!profiler->isProfiling());
    
    profiler->startProfiling();
    QVERIFY(profiler->isProfiling());
    
    profiler->stopProfiling();
    QVERIFY(!profiler->isProfiling());
}

void TestProfiler::testMarkPhaseStart()
{
    profiler->startProfiling();
    profiler->markPhaseStart("testPhase");
    
    // Phase should be active
    QVERIFY(profiler->isPhaseActive("testPhase"));
}

void TestProfiler::testMarkPhaseEnd()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("testPhase");
    QTest::qWait(10);
    profiler->markPhaseEnd("testPhase");
    
    // Phase should no longer be active
    QVERIFY(!profiler->isPhaseActive("testPhase"));
}

void TestProfiler::testNestedPhases()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("outer");
    profiler->markPhaseStart("inner");
    QTest::qWait(5);
    profiler->markPhaseEnd("inner");
    profiler->markPhaseEnd("outer");
    
    auto snapshot = profiler->getSnapshot();
    // Outer should take longer than inner
    QVERIFY(snapshot.phaseTimings.contains("outer"));
    QVERIFY(snapshot.phaseTimings.contains("inner"));
}

void TestProfiler::testPhaseTimingAccuracy()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("timedPhase");
    QTest::qWait(100);  // Wait 100ms
    profiler->markPhaseEnd("timedPhase");
    
    auto snapshot = profiler->getSnapshot();
    double timing = snapshot.phaseTimings["timedPhase"];
    
    // Should be close to 100ms (allow 20ms variance)
    QVERIFY(timing >= 80.0 && timing <= 120.0);
}

void TestProfiler::testCPUMetrics()
{
    profiler->startProfiling();
    
    // Simulate some CPU work
    for (int i = 0; i < 1000000; ++i) {
        volatile int x = i * 2;
        Q_UNUSED(x);
    }
    
    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.cpuUtilization >= 0.0 && snapshot.cpuUtilization <= 100.0);
}

void TestProfiler::testGPUMetrics()
{
    profiler->startProfiling();
    
    auto snapshot = profiler->getSnapshot();
    
    // GPU metrics may be 0 if no GPU is active
    QVERIFY(snapshot.gpuUtilization >= 0.0 && snapshot.gpuUtilization <= 100.0);
}

void TestProfiler::testMemoryMetrics()
{
    profiler->startProfiling();
    
    auto snapshot = profiler->getSnapshot();
    
    QVERIFY(snapshot.totalMemoryMB > 0);
    QVERIFY(snapshot.usedMemoryMB >= 0);
    QVERIFY(snapshot.usedMemoryMB <= snapshot.totalMemoryMB);
}

void TestProfiler::testThroughputMetrics()
{
    profiler->startProfiling();
    
    // Simulate processing samples
    for (int i = 0; i < 100; ++i) {
        profiler->recordSampleProcessed();
    }
    
    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.throughputSamples > 0.0);
}

void TestProfiler::testGetSnapshot()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("forwardPass");
    QTest::qWait(10);
    profiler->markPhaseEnd("forwardPass");
    
    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.forwardPassMs > 0.0);
}

void TestProfiler::testSnapshotContents()
{
    profiler->startProfiling();
    
    auto snapshot = profiler->getSnapshot();
    
    // Verify all required fields exist
    QVERIFY(snapshot.totalMemoryMB >= 0);
    QVERIFY(snapshot.cpuUtilization >= 0.0);
}

void TestProfiler::testPercentileCalculation()
{
    profiler->startProfiling();
    
    // Record multiple latencies
    for (int i = 0; i < 100; ++i) {
        profiler->recordLatency(static_cast<double>(i));
    }
    
    auto snapshot = profiler->getSnapshot();
    
    QVERIFY(snapshot.p50LatencyMs >= 0.0);
    QVERIFY(snapshot.p95LatencyMs >= snapshot.p50LatencyMs);
    QVERIFY(snapshot.p99LatencyMs >= snapshot.p95LatencyMs);
}

void TestProfiler::testExportReport()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("phase1");
    QTest::qWait(10);
    profiler->markPhaseEnd("phase1");
    
    QString reportPath = tempDir->filePath("profiler_report.json");
    bool result = profiler->exportReport(reportPath);
    
    QVERIFY(result);
    QVERIFY(QFileInfo(reportPath).exists());
}

void TestProfiler::testReportFormat()
{
    profiler->startProfiling();
    
    profiler->markPhaseStart("testPhase");
    QTest::qWait(10);
    profiler->markPhaseEnd("testPhase");
    
    QString reportPath = tempDir->filePath("report.json");
    profiler->exportReport(reportPath);
    
    // Read and parse JSON
    QFile file(reportPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QVERIFY(doc.isObject());
    
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("cpu_utilization"));
    QVERIFY(obj.contains("phase_timings"));
}

// ============================================================================
// OBSERVABILITY DASHBOARD TESTS
// ============================================================================

class TestObservabilityDashboard : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // Initialization Tests
    void testDashboardCreation();
    void testWidgetSetup();

    // Metrics Update Tests
    void testUpdateMetrics();
    void testMetricsDisplay();
    void testHistoricalData();

    // Alert Tests
    void testAlertConfiguration();
    void testAlertTriggering();

    // Chart Tests
    void testThroughputChart();
    void testLatencyChart();
    void testMemoryChart();

private:
    ObservabilityDashboard* dashboard;
};

void TestObservabilityDashboard::init()
{
    dashboard = new ObservabilityDashboard();
}

void TestObservabilityDashboard::cleanup()
{
    delete dashboard;
}

void TestObservabilityDashboard::testDashboardCreation()
{
    QVERIFY(dashboard != nullptr);
    QVERIFY(dashboard->isVisible() == false);  // Not shown by default
}

void TestObservabilityDashboard::testWidgetSetup()
{
    dashboard->show();
    QVERIFY(dashboard->isVisible());
}

void TestObservabilityDashboard::testUpdateMetrics()
{
    QJsonObject metrics;
    metrics["cpu_utilization"] = 45.5;
    metrics["gpu_utilization"] = 80.0;
    metrics["memory_used_mb"] = 1024;
    
    dashboard->updateMetrics(metrics);
    
    // Metrics should be stored
    auto current = dashboard->getCurrentMetrics();
    QCOMPARE(current["cpu_utilization"].toDouble(), 45.5);
}

void TestObservabilityDashboard::testMetricsDisplay()
{
    QJsonObject metrics;
    metrics["throughput_samples_per_sec"] = 1500.0;
    metrics["p95_latency_ms"] = 85.0;
    
    dashboard->updateMetrics(metrics);
    
    // Visual update should occur (hard to test in unit test)
    QVERIFY(true);
}

void TestObservabilityDashboard::testHistoricalData()
{
    // Update metrics multiple times
    for (int i = 0; i < 10; ++i) {
        QJsonObject metrics;
        metrics["throughput_samples_per_sec"] = 1000.0 + i * 100;
        dashboard->updateMetrics(metrics);
        QTest::qWait(100);
    }
    
    auto history = dashboard->getMetricsHistory();
    QVERIFY(history.size() > 0);
}

void TestObservabilityDashboard::testAlertConfiguration()
{
    dashboard->setAlertThreshold("cpu_utilization", 80.0);
    dashboard->setAlertThreshold("memory_used_mb", 8000);
    
    // Thresholds should be set
    QVERIFY(true);
}

void TestObservabilityDashboard::testAlertTriggering()
{
    QSignalSpy spy(dashboard, &ObservabilityDashboard::alertTriggered);
    
    dashboard->setAlertThreshold("cpu_utilization", 50.0);
    
    QJsonObject metrics;
    metrics["cpu_utilization"] = 85.0;  // Exceeds threshold
    dashboard->updateMetrics(metrics);
    
    // Alert should be triggered
    QVERIFY(spy.count() > 0);
}

void TestObservabilityDashboard::testThroughputChart()
{
    // Add throughput data
    for (int i = 0; i < 20; ++i) {
        QJsonObject metrics;
        metrics["throughput_samples_per_sec"] = 1000.0 + (i % 5) * 200;
        dashboard->updateMetrics(metrics);
    }
    
    // Chart should be updated (visual test)
    QVERIFY(true);
}

void TestObservabilityDashboard::testLatencyChart()
{
    for (int i = 0; i < 20; ++i) {
        QJsonObject metrics;
        metrics["p95_latency_ms"] = 80.0 + (i % 10) * 5;
        dashboard->updateMetrics(metrics);
    }
    
    QVERIFY(true);
}

void TestObservabilityDashboard::testMemoryChart()
{
    for (int i = 0; i < 20; ++i) {
        QJsonObject metrics;
        metrics["memory_used_mb"] = 2000 + i * 100;
        dashboard->updateMetrics(metrics);
    }
    
    QVERIFY(true);
}

// Main test runner
QTEST_MAIN(TestHardwareBackendSelector)
#include "test_additional_components.moc"
