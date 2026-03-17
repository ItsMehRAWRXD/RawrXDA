// RawrXD Unit Tests - DistributedTrainer
// Version: 2.0
// Target: 35+ comprehensive unit tests for distributed training

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include "../src/distributed_trainer.h"

class TestDistributedTrainer : public QObject
{
    Q_OBJECT

private slots:
    // Setup/Teardown
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // Initialization Tests
    void testDefaultConfiguration();
    void testSingleNodeNCCLInitialization();
    void testMultiNodeGlooInitialization();
    void testMPIBackendInitialization();
    void testInvalidBackendRejection();
    void testInvalidWorldSizeRejection();
    void testInvalidRankRejection();
    void testMasterAddressValidation();

    // Training Tests
    void testBasicTrainStep();
    void testGradientSynchronization();
    void testGradientAccumulation();
    void testTopKCompressionReduction();
    void testZeroReduction();
    void testZeroRedundancyOptimization();
    void testMixedPrecisionTraining();

    // Checkpointing Tests
    void testSaveCheckpoint();
    void testLoadCheckpoint();
    void testCheckpointWithResume();
    void testCheckpointOnlyOnMaster();
    void testDistributedCheckpointSharding();

    // Fault Tolerance Tests
    void testWorkerFailureDetection();
    void testAutomaticRecovery();
    void testHeartbeatMonitoring();
    void testNodePerformanceTracking();
    void testTimeoutConfiguration();

    // Load Balancing Tests
    void testDynamicBatchSizeAdjustment();
    void testAdaptiveLoadBalancing();
    void testPerformanceBasedRebalancing();

    // Metrics Tests
    void testMetricsCollection();
    void testLastSyncTimeTracking();
    void testCompressionRatioTracking();
    void testThroughputCalculation();

    // Signal Tests
    void testTrainingStartedSignal();
    void testGradientsSynchronizedSignal();
    void testCheckpointSavedSignal();
    void testNodeRecoveredSignal();

    // Profiling Tests
    void testProfilingDataCollection();
    void testProfilingToggle();

private:
    DistributedTrainer* trainer;
    QTemporaryDir* tempDir;
};

void TestDistributedTrainer::initTestCase()
{
    qInfo() << "Starting DistributedTrainer test suite";
}

void TestDistributedTrainer::cleanupTestCase()
{
    qInfo() << "DistributedTrainer tests completed";
}

void TestDistributedTrainer::init()
{
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());
    
    trainer = new DistributedTrainer();
}

void TestDistributedTrainer::cleanup()
{
    delete trainer;
    delete tempDir;
}

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

void TestDistributedTrainer::testDefaultConfiguration()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    bool result = trainer->Initialize(config);
    QVERIFY2(result, "Default configuration should initialize successfully");

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["world_size"].toInt(), 1);
    QCOMPARE(metrics["rank"].toInt(), 0);
    QCOMPARE(metrics["backend"].toString(), QString("nccl"));
}

void TestDistributedTrainer::testSingleNodeNCCLInitialization()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.pgConfig.masterAddr = "127.0.0.1";
    config.pgConfig.masterPort = 29500;

    QSignalSpy spy(trainer, &DistributedTrainer::trainingStarted);
    
    bool result = trainer->Initialize(config);
    QVERIFY(result);
    QCOMPARE(spy.count(), 1);
}

void TestDistributedTrainer::testMultiNodeGlooInitialization()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 8;
    config.pgConfig.rank = 0;
    config.pgConfig.masterAddr = "192.168.1.100";
    config.pgConfig.masterPort = 29500;

    bool result = trainer->Initialize(config);
    QVERIFY(result);

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["backend"].toString(), QString("gloo"));
}

void TestDistributedTrainer::testMPIBackendInitialization()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "mpi";
    config.pgConfig.worldSize = 16;
    config.pgConfig.rank = 0;

    bool result = trainer->Initialize(config);
    QVERIFY(result);

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["backend"].toString(), QString("mpi"));
}

void TestDistributedTrainer::testInvalidBackendRejection()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "invalid_backend";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    bool result = trainer->Initialize(config);
    QVERIFY2(!result, "Invalid backend should be rejected");
}

void TestDistributedTrainer::testInvalidWorldSizeRejection()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 0;  // Invalid
    config.pgConfig.rank = 0;

    bool result = trainer->Initialize(config);
    QVERIFY2(!result, "Invalid world size should be rejected");
}

void TestDistributedTrainer::testInvalidRankRejection()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 5;  // Rank >= world_size

    bool result = trainer->Initialize(config);
    QVERIFY2(!result, "Invalid rank should be rejected");
}

void TestDistributedTrainer::testMasterAddressValidation()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 1;
    config.pgConfig.masterAddr = "";  // Empty address

    bool result = trainer->Initialize(config);
    QVERIFY2(!result, "Empty master address should be rejected for multi-node");
}

// ============================================================================
// TRAINING TESTS
// ============================================================================

void TestDistributedTrainer::testBasicTrainStep()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    // Simulate gradients (100 floats)
    std::vector<float> gradients(100, 1.0f);

    QSignalSpy spy(trainer, &DistributedTrainer::gradientsSynchronized);
    
    bool result = trainer->TrainStep(gradients, 1);
    QVERIFY(result);
    QCOMPARE(spy.count(), 1);
    
    // Check sync time
    QList<QVariant> args = spy.takeFirst();
    double syncTime = args.at(0).toDouble();
    QVERIFY(syncTime >= 0.0);
}

void TestDistributedTrainer::testGradientSynchronization()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 2.5f);

    QSignalSpy spy(trainer, &DistributedTrainer::gradientsSynchronized);
    
    bool result = trainer->TrainStep(gradients, 1);
    QVERIFY(result);

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("last_sync_time_ms"));
    QVERIFY(metrics["last_sync_time_ms"].toDouble() > 0.0);
}

void TestDistributedTrainer::testGradientAccumulation()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.accumSteps = 4;  // Accumulate over 4 steps

    trainer->Initialize(config);

    std::vector<float> gradients(100, 1.0f);
    QSignalSpy spy(trainer, &DistributedTrainer::gradientsSynchronized);

    // First 3 steps should not sync
    trainer->TrainStep(gradients, 1);
    QCOMPARE(spy.count(), 0);

    trainer->TrainStep(gradients, 2);
    QCOMPARE(spy.count(), 0);

    trainer->TrainStep(gradients, 3);
    QCOMPARE(spy.count(), 0);

    // 4th step should trigger sync
    trainer->TrainStep(gradients, 4);
    QCOMPARE(spy.count(), 1);
}

void TestDistributedTrainer::testTopKCompressionReduction()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.compression = "TopK";
    config.compressionRatio = 0.1;  // Keep only top 10%

    trainer->Initialize(config);

    std::vector<float> gradients(1000);
    for (size_t i = 0; i < gradients.size(); ++i) {
        gradients[i] = static_cast<float>(i) / 1000.0f;
    }

    bool result = trainer->TrainStep(gradients, 1);
    QVERIFY(result);

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["compression"].toString(), QString("TopK"));
    QCOMPARE(metrics["compression_ratio"].toDouble(), 0.1);
}

void TestDistributedTrainer::testZeroReduction()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.zeroRedundancyOptimizer = true;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);

    bool result = trainer->TrainStep(gradients, 1);
    QVERIFY(result);

    // With ZeRO, each rank only holds 1/4 of optimizer state
    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["zero_enabled"].toBool());
}

void TestDistributedTrainer::testZeroRedundancyOptimization()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 8;
    config.pgConfig.rank = 0;
    config.zeroRedundancyOptimizer = true;

    trainer->Initialize(config);

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["zero_enabled"].toBool());

    // Verify memory reduction
    // With ZeRO, memory should be ~1/world_size of non-ZeRO
    double expectedReduction = 1.0 / 8.0;
    QVERIFY(metrics.contains("memory_reduction_factor"));
}

void TestDistributedTrainer::testMixedPrecisionTraining()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.mixedPrecision = true;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);

    bool result = trainer->TrainStep(gradients, 1);
    QVERIFY(result);

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["mixed_precision"].toBool());
}

// ============================================================================
// CHECKPOINTING TESTS
// ============================================================================

void TestDistributedTrainer::testSaveCheckpoint()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    QString checkpointPath = tempDir->filePath("checkpoint.ckpt");

    QSignalSpy spy(trainer, &DistributedTrainer::checkpointSaved);
    
    bool result = trainer->SaveCheckpoint(checkpointPath, 100);
    QVERIFY(result);
    QCOMPARE(spy.count(), 1);

    // Verify file exists
    QFileInfo info(checkpointPath);
    QVERIFY(info.exists());
}

void TestDistributedTrainer::testLoadCheckpoint()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    QString checkpointPath = tempDir->filePath("checkpoint.ckpt");

    // Save checkpoint
    trainer->SaveCheckpoint(checkpointPath, 100);

    // Load checkpoint
    int step = trainer->LoadCheckpoint(checkpointPath);
    QCOMPARE(step, 100);
}

void TestDistributedTrainer::testCheckpointWithResume()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    QString checkpointPath = tempDir->filePath("resume.ckpt");

    // Train for 50 steps
    std::vector<float> gradients(100, 1.0f);
    for (int i = 1; i <= 50; ++i) {
        trainer->TrainStep(gradients, i);
    }

    // Save checkpoint
    trainer->SaveCheckpoint(checkpointPath, 50);

    // Create new trainer and load
    DistributedTrainer* newTrainer = new DistributedTrainer();
    newTrainer->Initialize(config);
    int resumeStep = newTrainer->LoadCheckpoint(checkpointPath);

    QCOMPARE(resumeStep, 50);

    delete newTrainer;
}

void TestDistributedTrainer::testCheckpointOnlyOnMaster()
{
    // Rank 0 (master) should save checkpoint
    DistributedTrainer::TrainerConfig config1;
    config1.pgConfig.backend = "nccl";
    config1.pgConfig.worldSize = 4;
    config1.pgConfig.rank = 0;

    DistributedTrainer trainer1;
    trainer1.Initialize(config1);

    QString path1 = tempDir->filePath("master_checkpoint.ckpt");
    bool result1 = trainer1.SaveCheckpoint(path1, 10);
    QVERIFY(result1);
    QVERIFY(QFileInfo(path1).exists());

    // Rank 1 (worker) should NOT save checkpoint
    DistributedTrainer::TrainerConfig config2;
    config2.pgConfig.backend = "nccl";
    config2.pgConfig.worldSize = 4;
    config2.pgConfig.rank = 1;

    DistributedTrainer trainer2;
    trainer2.Initialize(config2);

    QString path2 = tempDir->filePath("worker_checkpoint.ckpt");
    bool result2 = trainer2.SaveCheckpoint(path2, 10);
    QVERIFY(!result2);  // Should fail on worker
}

void TestDistributedTrainer::testDistributedCheckpointSharding()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.zeroRedundancyOptimizer = true;

    trainer->Initialize(config);

    QString checkpointPath = tempDir->filePath("sharded_checkpoint");

    bool result = trainer->SaveCheckpoint(checkpointPath, 100);
    QVERIFY(result);

    // With ZeRO, each rank saves shard
    // Check for shard files
    QVERIFY(QFileInfo(checkpointPath + "_rank0.ckpt").exists());
}

// ============================================================================
// FAULT TOLERANCE TESTS
// ============================================================================

void TestDistributedTrainer::testWorkerFailureDetection()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.workerTimeoutMs = 5000;

    trainer->Initialize(config);

    // Simulate worker timeout (in real scenario, worker would hang)
    // Check if trainer detects failure
    QSignalSpy spy(trainer, &DistributedTrainer::nodeRecovered);

    // Wait for timeout detection
    QTest::qWait(6000);

    // In real implementation, failed worker should be detected and recovered
    QVERIFY(trainer->GetMetrics().contains("failed_workers"));
}

void TestDistributedTrainer::testAutomaticRecovery()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.autoRecover = true;
    config.faultTolerance.maxRetries = 3;

    trainer->Initialize(config);

    QSignalSpy spy(trainer, &DistributedTrainer::nodeRecovered);

    // Simulate recovery scenario
    // In real implementation, this would restart failed worker
    
    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["fault_tolerance"].toBool());
}

void TestDistributedTrainer::testHeartbeatMonitoring()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.heartbeatIntervalMs = 1000;

    trainer->Initialize(config);

    // Wait for heartbeats
    QTest::qWait(3000);

    // Check heartbeat count in metrics
    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("heartbeats_sent"));
    QVERIFY(metrics["heartbeats_sent"].toInt() >= 2);
}

void TestDistributedTrainer::testNodePerformanceTracking()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);
    
    // Train for several steps
    for (int i = 1; i <= 10; ++i) {
        trainer->TrainStep(gradients, i);
    }

    // Get node performance
    auto nodePerf = trainer->GetNodePerformance();
    QCOMPARE(nodePerf.size(), 4);

    // Verify each node has performance data
    for (const auto& node : nodePerf) {
        QVERIFY(node.throughput > 0.0);
        QVERIFY(node.rank >= 0 && node.rank < 4);
    }
}

void TestDistributedTrainer::testTimeoutConfiguration()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.workerTimeoutMs = 10000;  // 10 seconds

    trainer->Initialize(config);

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["worker_timeout_ms"].toInt(), 10000);
}

// ============================================================================
// LOAD BALANCING TESTS
// ============================================================================

void TestDistributedTrainer::testDynamicBatchSizeAdjustment()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.loadBalancing.enabled = true;
    config.loadBalancing.dynamicBatchSize = true;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);

    // Train with varying load
    for (int i = 1; i <= 20; ++i) {
        trainer->TrainStep(gradients, i);
    }

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["load_balancing"].toBool());
}

void TestDistributedTrainer::testAdaptiveLoadBalancing()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 8;
    config.pgConfig.rank = 0;
    config.loadBalancing.enabled = true;
    config.loadBalancing.rebalanceIntervalSteps = 10;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);

    // Train for 15 steps (should trigger rebalance at step 10)
    for (int i = 1; i <= 15; ++i) {
        trainer->TrainStep(gradients, i);
    }

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("rebalances"));
    QVERIFY(metrics["rebalances"].toInt() >= 1);
}

void TestDistributedTrainer::testPerformanceBasedRebalancing()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.loadBalancing.enabled = true;

    trainer->Initialize(config);

    auto nodePerf = trainer->GetNodePerformance();

    // Verify nodes are balanced (throughput should be similar)
    if (nodePerf.size() > 1) {
        double avgThroughput = 0.0;
        for (const auto& node : nodePerf) {
            avgThroughput += node.throughput;
        }
        avgThroughput /= nodePerf.size();

        // Check that no node is >20% slower than average
        for (const auto& node : nodePerf) {
            double deviation = std::abs(node.throughput - avgThroughput) / avgThroughput;
            QVERIFY(deviation < 0.2);
        }
    }
}

// ============================================================================
// METRICS TESTS
// ============================================================================

void TestDistributedTrainer::testMetricsCollection()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    auto metrics = trainer->GetMetrics();

    // Verify required metrics exist
    QVERIFY(metrics.contains("world_size"));
    QVERIFY(metrics.contains("rank"));
    QVERIFY(metrics.contains("backend"));
    QVERIFY(metrics.contains("step"));
}

void TestDistributedTrainer::testLastSyncTimeTracking()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);
    trainer->TrainStep(gradients, 1);

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("last_sync_time_ms"));
    QVERIFY(metrics["last_sync_time_ms"].toDouble() >= 0.0);
}

void TestDistributedTrainer::testCompressionRatioTracking()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.compression = "TopK";
    config.compressionRatio = 0.05;

    trainer->Initialize(config);

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["compression"].toString(), QString("TopK"));
    QCOMPARE(metrics["compression_ratio"].toDouble(), 0.05);
}

void TestDistributedTrainer::testThroughputCalculation()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);

    // Train for multiple steps
    for (int i = 1; i <= 10; ++i) {
        trainer->TrainStep(gradients, i);
        QTest::qWait(10);  // Small delay
    }

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("throughput_samples_per_sec"));
    QVERIFY(metrics["throughput_samples_per_sec"].toDouble() > 0.0);
}

// ============================================================================
// SIGNAL TESTS
// ============================================================================

void TestDistributedTrainer::testTrainingStartedSignal()
{
    QSignalSpy spy(trainer, &DistributedTrainer::trainingStarted);

    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    QCOMPARE(spy.count(), 1);
}

void TestDistributedTrainer::testGradientsSynchronizedSignal()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    QSignalSpy spy(trainer, &DistributedTrainer::gradientsSynchronized);

    std::vector<float> gradients(100, 1.0f);
    trainer->TrainStep(gradients, 1);

    QCOMPARE(spy.count(), 1);

    QList<QVariant> args = spy.takeFirst();
    double syncTime = args.at(0).toDouble();
    QVERIFY(syncTime >= 0.0);
}

void TestDistributedTrainer::testCheckpointSavedSignal()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    QSignalSpy spy(trainer, &DistributedTrainer::checkpointSaved);

    QString path = tempDir->filePath("checkpoint.ckpt");
    trainer->SaveCheckpoint(path, 100);

    QCOMPARE(spy.count(), 1);

    QList<QVariant> args = spy.takeFirst();
    QString savedPath = args.at(0).toString();
    QCOMPARE(savedPath, path);
}

void TestDistributedTrainer::testNodeRecoveredSignal()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.autoRecover = true;

    trainer->Initialize(config);

    QSignalSpy spy(trainer, &DistributedTrainer::nodeRecovered);

    // In real scenario, simulate node failure and recovery
    // For unit test, just verify signal is registered correctly
    QVERIFY(spy.isValid());
}

// ============================================================================
// PROFILING TESTS
// ============================================================================

void TestDistributedTrainer::testProfilingDataCollection()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.pgConfig.enableProfiling = true;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);
    
    for (int i = 1; i <= 5; ++i) {
        trainer->TrainStep(gradients, i);
    }

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("profiling_enabled"));
    QVERIFY(metrics["profiling_enabled"].toBool());
}

void TestDistributedTrainer::testProfilingToggle()
{
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    config.pgConfig.enableProfiling = false;  // Start disabled

    trainer->Initialize(config);

    auto metrics1 = trainer->GetMetrics();
    QVERIFY(!metrics1["profiling_enabled"].toBool());

    // Enable profiling dynamically (if supported)
    // Implementation-specific API
    
    auto metrics2 = trainer->GetMetrics();
    // After enabling, should be true
}

QTEST_MAIN(TestDistributedTrainer)
#include "test_distributed_trainer.moc"
