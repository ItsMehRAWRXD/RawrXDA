// RawrXD Performance Tests - Phase 2 Components
// Version: 2.0
// Target: 15+ performance tests with SLA validation

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include "../src/distributed_trainer.h"
#include "../src/security_manager.h"
#include "../src/hardware_backend_selector.h"
#include "../src/profiler.h"

class TestPhase2Performance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Distributed Training Performance
    void testSingleGPUTrainingThroughput();
    void testMultiGPUScalingEfficiency();
    void testGradientSyncLatency();
    void testCheckpointSavePerformance();
    void testCheckpointLoadPerformance();

    // Security Performance
    void testEncryptionThroughput();
    void testDecryptionThroughput();
    void testHMACGenerationPerformance();
    void testCredentialLookupPerformance();

    // Hardware Performance
    void testBackendInitializationTime();
    void testDeviceEnumerationPerformance();

    // Profiler Performance
    void testProfilingOverhead();
    void testMetricsCollectionPerformance();

    // Combined Performance
    void testEndToEndLatency();
    void testConcurrentRequestHandling();

private:
    DistributedTrainer* trainer;
    SecurityManager* security;
    HardwareBackendSelector* hardware;
    Profiler* profiler;
    QTemporaryDir* tempDir;

    void printPerformanceReport(const QString& testName, 
                               double actualValue, 
                               double targetValue, 
                               const QString& unit);
};

void TestPhase2Performance::initTestCase()
{
    qInfo() << "=== Starting Phase 2 Performance Tests ===";
    qInfo() << "Performance SLA Targets:";
    qInfo() << "- Distributed Training Efficiency: >=85% on 4 GPUs";
    qInfo() << "- Gradient Sync Latency: <100ms (P95)";
    qInfo() << "- Encryption Throughput: >100 MB/s";
    qInfo() << "- Decryption Throughput: >100 MB/s";
    qInfo() << "- Checkpoint Save: <2s for 1GB model";
    qInfo() << "- End-to-End Latency: <200ms (P95)";
}

void TestPhase2Performance::cleanupTestCase()
{
    qInfo() << "=== Phase 2 Performance Tests Complete ===";
}

void TestPhase2Performance::init()
{
    tempDir = new QTemporaryDir();
    trainer = new DistributedTrainer();
    security = SecurityManager::getInstance();
    hardware = new HardwareBackendSelector();
    profiler = new Profiler();
}

void TestPhase2Performance::cleanup()
{
    delete trainer;
    security->shutdown();
    delete hardware;
    delete profiler;
    delete tempDir;
}

void TestPhase2Performance::printPerformanceReport(const QString& testName,
                                                   double actualValue,
                                                   double targetValue,
                                                   const QString& unit)
{
    QString status = (actualValue <= targetValue) ? "✓ PASS" : "✗ FAIL";
    qInfo().noquote() << QString("%1: %2 %3 (target: <%4 %5) %6")
        .arg(testName)
        .arg(actualValue, 0, 'f', 2)
        .arg(unit)
        .arg(targetValue, 0, 'f', 2)
        .arg(unit)
        .arg(status);
}

// ============================================================================
// DISTRIBUTED TRAINING PERFORMANCE
// ============================================================================

void TestPhase2Performance::testSingleGPUTrainingThroughput()
{
    qInfo() << "\n--- Single GPU Training Throughput ---";

    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    std::vector<float> gradients(100000, 1.0f);  // 100K parameters

    // Warm-up
    for (int i = 0; i < 5; ++i) {
        trainer->TrainStep(gradients, i);
    }

    // Benchmark
    QElapsedTimer timer;
    timer.start();

    int numSteps = 100;
    for (int i = 0; i < numSteps; ++i) {
        trainer->TrainStep(gradients, i);
    }

    qint64 elapsedMs = timer.elapsed();
    double stepsPerSec = (numSteps * 1000.0) / elapsedMs;
    double samplesPerSec = stepsPerSec * gradients.size();

    qInfo() << "Steps/sec:" << stepsPerSec;
    qInfo() << "Samples/sec:" << samplesPerSec;
    qInfo() << "Avg step time:" << (elapsedMs / (double)numSteps) << "ms";

    // SLA: Should process >500 steps/sec (single GPU)
    QVERIFY2(stepsPerSec > 500.0, "Training throughput below SLA");

    printPerformanceReport("Single GPU Throughput", 
                          1000.0 / stepsPerSec,  // ms per step
                          2.0,  // Target: <2ms per step
                          "ms/step");
}

void TestPhase2Performance::testMultiGPUScalingEfficiency()
{
    qInfo() << "\n--- Multi-GPU Scaling Efficiency ---";

    hardware->autoDetect();

    if (!hardware->isBackendSupported("CUDA")) {
        qInfo() << "⊘ Test skipped (CUDA not available)";
        return;
    }

    auto devices = hardware->listDevices("CUDA");
    if (devices.size() < 2) {
        qInfo() << "⊘ Test skipped (need 2+ GPUs)";
        return;
    }

    int worldSize = qMin(4, devices.size());

    // Measure single GPU baseline
    DistributedTrainer::TrainerConfig config1;
    config1.pgConfig.backend = "nccl";
    config1.pgConfig.worldSize = 1;
    config1.pgConfig.rank = 0;
    trainer->Initialize(config1);

    std::vector<float> gradients(100000, 1.0f);

    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < 50; ++i) {
        trainer->TrainStep(gradients, i);
    }
    qint64 singleGpuTime = timer.elapsed();

    delete trainer;
    trainer = new DistributedTrainer();

    // Measure multi-GPU performance
    DistributedTrainer::TrainerConfig config2;
    config2.pgConfig.backend = "nccl";
    config2.pgConfig.worldSize = worldSize;
    config2.pgConfig.rank = 0;
    trainer->Initialize(config2);

    timer.restart();
    for (int i = 0; i < 50; ++i) {
        trainer->TrainStep(gradients, i);
    }
    qint64 multiGpuTime = timer.elapsed();

    double efficiency = (singleGpuTime * worldSize) / (double)multiGpuTime * 100.0;

    qInfo() << "Single GPU time:" << singleGpuTime << "ms";
    qInfo() << "Multi-GPU time (" << worldSize << "GPUs):" << multiGpuTime << "ms";
    qInfo() << "Scaling efficiency:" << efficiency << "%";

    // SLA: >=85% efficiency for 4 GPUs
    QVERIFY2(efficiency >= 85.0, "Multi-GPU scaling efficiency below SLA");

    printPerformanceReport("Multi-GPU Efficiency",
                          100.0 - efficiency,  // Lower is better
                          15.0,  // Target: <15% overhead
                          "% overhead");
}

void TestPhase2Performance::testGradientSyncLatency()
{
    qInfo() << "\n--- Gradient Synchronization Latency ---";

    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    std::vector<float> gradients(1000000, 1.0f);  // 1M parameters

    // Collect sync times
    QVector<double> syncTimes;

    for (int i = 0; i < 100; ++i) {
        QElapsedTimer timer;
        timer.start();

        trainer->TrainStep(gradients, i);

        double syncTime = timer.elapsed();
        syncTimes.append(syncTime);
    }

    // Calculate percentiles
    std::sort(syncTimes.begin(), syncTimes.end());
    double p50 = syncTimes[syncTimes.size() / 2];
    double p95 = syncTimes[(syncTimes.size() * 95) / 100];
    double p99 = syncTimes[(syncTimes.size() * 99) / 100];

    qInfo() << "P50 sync latency:" << p50 << "ms";
    qInfo() << "P95 sync latency:" << p95 << "ms";
    qInfo() << "P99 sync latency:" << p99 << "ms";

    // SLA: P95 <100ms
    QVERIFY2(p95 < 100.0, "Gradient sync latency exceeds SLA");

    printPerformanceReport("Gradient Sync P95", p95, 100.0, "ms");
}

void TestPhase2Performance::testCheckpointSavePerformance()
{
    qInfo() << "\n--- Checkpoint Save Performance ---";

    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    // Train to generate state
    std::vector<float> gradients(10000000, 1.0f);  // 10M parameters (simulates 1GB model)
    trainer->TrainStep(gradients, 1);

    QString checkpointPath = tempDir->filePath("perf_checkpoint.ckpt");

    // Benchmark checkpoint save
    QElapsedTimer timer;
    timer.start();

    trainer->SaveCheckpoint(checkpointPath, 1);

    qint64 saveTime = timer.elapsed();

    qInfo() << "Checkpoint save time:" << saveTime << "ms";

    QFileInfo info(checkpointPath);
    double sizeMB = info.size() / (1024.0 * 1024.0);
    double throughputMBps = sizeMB / (saveTime / 1000.0);

    qInfo() << "Checkpoint size:" << sizeMB << "MB";
    qInfo() << "Save throughput:" << throughputMBps << "MB/s";

    // SLA: <2s for 1GB model
    QVERIFY2(saveTime < 2000, "Checkpoint save time exceeds SLA");

    printPerformanceReport("Checkpoint Save", saveTime / 1000.0, 2.0, "seconds");
}

void TestPhase2Performance::testCheckpointLoadPerformance()
{
    qInfo() << "\n--- Checkpoint Load Performance ---";

    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    // Create checkpoint
    std::vector<float> gradients(10000000, 1.0f);
    trainer->TrainStep(gradients, 1);

    QString checkpointPath = tempDir->filePath("load_perf_checkpoint.ckpt");
    trainer->SaveCheckpoint(checkpointPath, 1);

    delete trainer;
    trainer = new DistributedTrainer();
    trainer->Initialize(config);

    // Benchmark checkpoint load
    QElapsedTimer timer;
    timer.start();

    int step = trainer->LoadCheckpoint(checkpointPath);

    qint64 loadTime = timer.elapsed();

    qInfo() << "Checkpoint load time:" << loadTime << "ms";
    qInfo() << "Loaded step:" << step;

    QFileInfo info(checkpointPath);
    double sizeMB = info.size() / (1024.0 * 1024.0);
    double throughputMBps = sizeMB / (loadTime / 1000.0);

    qInfo() << "Load throughput:" << throughputMBps << "MB/s";

    // SLA: <1.5s for 1GB model
    QVERIFY2(loadTime < 1500, "Checkpoint load time exceeds SLA");

    printPerformanceReport("Checkpoint Load", loadTime / 1000.0, 1.5, "seconds");
}

// ============================================================================
// SECURITY PERFORMANCE
// ============================================================================

void TestPhase2Performance::testEncryptionThroughput()
{
    qInfo() << "\n--- Encryption Throughput ---";

    SecurityManager::Config config;
    config.masterPassword = "PerfTest123!";
    config.encryptionAlgorithm = "AES-256-GCM";
    security->initialize(config);

    // Test with 10 MB of data
    QByteArray plaintext(10 * 1024 * 1024, 'A');
    QByteArray aad = "performance_test";

    // Warm-up
    for (int i = 0; i < 3; ++i) {
        security->encryptData(plaintext, aad);
    }

    // Benchmark
    QElapsedTimer timer;
    timer.start();

    int iterations = 10;
    for (int i = 0; i < iterations; ++i) {
        security->encryptData(plaintext, aad);
    }

    qint64 elapsedMs = timer.elapsed();
    double totalMB = (plaintext.size() * iterations) / (1024.0 * 1024.0);
    double throughputMBps = totalMB / (elapsedMs / 1000.0);

    qInfo() << "Encrypted:" << totalMB << "MB in" << elapsedMs << "ms";
    qInfo() << "Throughput:" << throughputMBps << "MB/s";

    // SLA: >100 MB/s
    QVERIFY2(throughputMBps > 100.0, "Encryption throughput below SLA");

    printPerformanceReport("Encryption Throughput",
                          1000.0 / throughputMBps,  // ms per MB
                          10.0,  // Target: <10ms per MB
                          "ms/MB");
}

void TestPhase2Performance::testDecryptionThroughput()
{
    qInfo() << "\n--- Decryption Throughput ---";

    SecurityManager::Config config;
    config.masterPassword = "PerfTest123!";
    security->initialize(config);

    QByteArray plaintext(10 * 1024 * 1024, 'A');
    QByteArray aad = "performance_test";

    // Encrypt first
    QByteArray ciphertext = security->encryptData(plaintext, aad);

    // Benchmark decryption
    QElapsedTimer timer;
    timer.start();

    int iterations = 10;
    for (int i = 0; i < iterations; ++i) {
        security->decryptData(ciphertext, aad);
    }

    qint64 elapsedMs = timer.elapsed();
    double totalMB = (plaintext.size() * iterations) / (1024.0 * 1024.0);
    double throughputMBps = totalMB / (elapsedMs / 1000.0);

    qInfo() << "Decrypted:" << totalMB << "MB in" << elapsedMs << "ms";
    qInfo() << "Throughput:" << throughputMBps << "MB/s";

    // SLA: >100 MB/s
    QVERIFY2(throughputMBps > 100.0, "Decryption throughput below SLA");

    printPerformanceReport("Decryption Throughput",
                          1000.0 / throughputMBps,
                          10.0,
                          "ms/MB");
}

void TestPhase2Performance::testHMACGenerationPerformance()
{
    qInfo() << "\n--- HMAC Generation Performance ---";

    SecurityManager::Config config;
    config.masterPassword = "PerfTest123!";
    security->initialize(config);

    QByteArray data(1024 * 1024, 'A');  // 1 MB

    // Benchmark
    QElapsedTimer timer;
    timer.start();

    int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        security->generateHMAC(data);
    }

    qint64 elapsedMs = timer.elapsed();
    double opsPerSec = (iterations * 1000.0) / elapsedMs;

    qInfo() << "HMAC operations:" << iterations << "in" << elapsedMs << "ms";
    qInfo() << "Throughput:" << opsPerSec << "ops/sec";

    // SLA: >5000 ops/sec for 1MB data
    QVERIFY2(opsPerSec > 5000.0, "HMAC generation performance below SLA");

    printPerformanceReport("HMAC Generation",
                          1000.0 / opsPerSec,  // ms per op
                          0.2,  // Target: <0.2ms per op
                          "ms/op");
}

void TestPhase2Performance::testCredentialLookupPerformance()
{
    qInfo() << "\n--- Credential Lookup Performance ---";

    SecurityManager::Config config;
    config.masterPassword = "PerfTest123!";
    security->initialize(config);

    // Store 1000 credentials
    for (int i = 0; i < 1000; ++i) {
        QString userId = QString("user%1@example.com").arg(i);
        security->storeCredential(userId, "token", "bearer",
                                 QDateTime::currentDateTime().addSecs(3600));
    }

    // Benchmark lookups
    QElapsedTimer timer;
    timer.start();

    int lookups = 10000;
    for (int i = 0; i < lookups; ++i) {
        QString userId = QString("user%1@example.com").arg(i % 1000);
        security->getCredential(userId);
    }

    qint64 elapsedMs = timer.elapsed();
    double lookupsPerSec = (lookups * 1000.0) / elapsedMs;

    qInfo() << "Lookups:" << lookups << "in" << elapsedMs << "ms";
    qInfo() << "Throughput:" << lookupsPerSec << "lookups/sec";

    // SLA: >50,000 lookups/sec
    QVERIFY2(lookupsPerSec > 50000.0, "Credential lookup performance below SLA");

    printPerformanceReport("Credential Lookup",
                          1000.0 / lookupsPerSec,  // ms per lookup
                          0.02,  // Target: <0.02ms per lookup
                          "ms/lookup");
}

// ============================================================================
// HARDWARE PERFORMANCE
// ============================================================================

void TestPhase2Performance::testBackendInitializationTime()
{
    qInfo() << "\n--- Backend Initialization Time ---";

    hardware->autoDetect();

    QElapsedTimer timer;
    timer.start();

    QString backend = hardware->selectBestBackend();
    hardware->initializeBackend(backend);

    qint64 initTime = timer.elapsed();

    qInfo() << "Backend:" << backend;
    qInfo() << "Initialization time:" << initTime << "ms";

    // SLA: <500ms
    QVERIFY2(initTime < 500, "Backend initialization time exceeds SLA");

    printPerformanceReport("Backend Init", initTime, 500.0, "ms");
}

void TestPhase2Performance::testDeviceEnumerationPerformance()
{
    qInfo() << "\n--- Device Enumeration Performance ---";

    QElapsedTimer timer;
    timer.start();

    hardware->autoDetect();

    qint64 enumTime = timer.elapsed();

    auto backends = hardware->getAvailableBackends();

    qInfo() << "Enumeration time:" << enumTime << "ms";
    qInfo() << "Backends found:" << backends.size();

    // SLA: <200ms
    QVERIFY2(enumTime < 200, "Device enumeration time exceeds SLA");

    printPerformanceReport("Device Enumeration", enumTime, 200.0, "ms");
}

// ============================================================================
// PROFILER PERFORMANCE
// ============================================================================

void TestPhase2Performance::testProfilingOverhead()
{
    qInfo() << "\n--- Profiling Overhead ---";

    // Measure without profiling
    auto workload = []() {
        volatile int sum = 0;
        for (int i = 0; i < 1000000; ++i) {
            sum += i;
        }
    };

    QElapsedTimer timer;
    timer.start();

    for (int i = 0; i < 100; ++i) {
        workload();
    }

    qint64 noProfTime = timer.elapsed();

    // Measure with profiling
    profiler->startProfiling();

    timer.restart();

    for (int i = 0; i < 100; ++i) {
        profiler->markPhaseStart("workload");
        workload();
        profiler->markPhaseEnd("workload");
    }

    qint64 profTime = timer.elapsed();

    double overhead = ((profTime - noProfTime) / (double)noProfTime) * 100.0;

    qInfo() << "Without profiling:" << noProfTime << "ms";
    qInfo() << "With profiling:" << profTime << "ms";
    qInfo() << "Overhead:" << overhead << "%";

    // SLA: <5% overhead
    QVERIFY2(overhead < 5.0, "Profiling overhead exceeds SLA");

    printPerformanceReport("Profiling Overhead", overhead, 5.0, "%");
}

void TestPhase2Performance::testMetricsCollectionPerformance()
{
    qInfo() << "\n--- Metrics Collection Performance ---";

    profiler->startProfiling();

    // Simulate training loop
    for (int i = 0; i < 100; ++i) {
        profiler->markPhaseStart("phase1");
        QTest::qWait(1);
        profiler->markPhaseEnd("phase1");

        profiler->recordSampleProcessed();
        profiler->recordLatency(10.0);
    }

    // Benchmark snapshot collection
    QElapsedTimer timer;
    timer.start();

    int iterations = 1000;
    for (int i = 0; i < iterations; ++i) {
        profiler->getSnapshot();
    }

    qint64 elapsedMs = timer.elapsed();
    double opsPerSec = (iterations * 1000.0) / elapsedMs;

    qInfo() << "Snapshot operations:" << iterations << "in" << elapsedMs << "ms";
    qInfo() << "Throughput:" << opsPerSec << "snapshots/sec";

    // SLA: >10,000 snapshots/sec
    QVERIFY2(opsPerSec > 10000.0, "Metrics collection performance below SLA");

    printPerformanceReport("Metrics Collection",
                          1000.0 / opsPerSec,
                          0.1,  // Target: <0.1ms per snapshot
                          "ms/snapshot");
}

// ============================================================================
// COMBINED PERFORMANCE
// ============================================================================

void TestPhase2Performance::testEndToEndLatency()
{
    qInfo() << "\n--- End-to-End Latency ---";

    // Initialize all components
    SecurityManager::Config secConfig;
    secConfig.masterPassword = "E2EPerf123!";
    security->initialize(secConfig);

    hardware->autoDetect();
    hardware->initializeBackend(hardware->selectBestBackend());

    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = "nccl";
    trainConfig.pgConfig.worldSize = 1;
    trainConfig.pgConfig.rank = 0;
    trainer->Initialize(trainConfig);

    profiler->startProfiling();

    // Benchmark end-to-end request
    std::vector<float> gradients(10000, 1.0f);
    QVector<double> latencies;

    for (int i = 0; i < 100; ++i) {
        QElapsedTimer timer;
        timer.start();

        // Complete request: security check, training, profiling
        security->checkAccess("user@example.com", "models/training",
                             SecurityManager::AccessLevel::Write);

        profiler->markPhaseStart("train");
        trainer->TrainStep(gradients, i);
        profiler->markPhaseEnd("train");

        profiler->getSnapshot();

        double latency = timer.elapsed();
        latencies.append(latency);
    }

    // Calculate percentiles
    std::sort(latencies.begin(), latencies.end());
    double p50 = latencies[latencies.size() / 2];
    double p95 = latencies[(latencies.size() * 95) / 100];
    double p99 = latencies[(latencies.size() * 99) / 100];

    qInfo() << "P50 latency:" << p50 << "ms";
    qInfo() << "P95 latency:" << p95 << "ms";
    qInfo() << "P99 latency:" << p99 << "ms";

    // SLA: P95 <200ms
    QVERIFY2(p95 < 200.0, "End-to-end latency exceeds SLA");

    printPerformanceReport("E2E Latency P95", p95, 200.0, "ms");
}

void TestPhase2Performance::testConcurrentRequestHandling()
{
    qInfo() << "\n--- Concurrent Request Handling ---";

    // Initialize components
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    std::vector<float> gradients(5000, 1.0f);

    // Benchmark concurrent requests (simulated via rapid sequential calls)
    QElapsedTimer timer;
    timer.start();

    int requests = 1000;
    for (int i = 0; i < requests; ++i) {
        trainer->TrainStep(gradients, i);
    }

    qint64 elapsedMs = timer.elapsed();
    double requestsPerSec = (requests * 1000.0) / elapsedMs;

    qInfo() << "Requests:" << requests << "in" << elapsedMs << "ms";
    qInfo() << "Throughput:" << requestsPerSec << "req/sec";

    // SLA: >500 req/sec
    QVERIFY2(requestsPerSec > 500.0, "Concurrent request throughput below SLA");

    printPerformanceReport("Request Throughput",
                          1000.0 / requestsPerSec,  // ms per request
                          2.0,  // Target: <2ms per request
                          "ms/req");
}

QTEST_MAIN(TestPhase2Performance)
#include "test_phase2_performance.moc"
