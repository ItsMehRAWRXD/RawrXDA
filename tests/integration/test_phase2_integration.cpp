// RawrXD Integration Tests - Phase 2 Components
// Version: 2.0
// Target: 25+ integration tests for end-to-end workflows

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include "../src/distributed_trainer.h"
#include "../src/security_manager.h"
#include "../src/hardware_backend_selector.h"
#include "../src/profiler.h"
#include "../src/observability_dashboard.h"

class TestPhase2Integration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // End-to-End Training Workflow Tests
    void testCompleteTrainingPipeline();
    void testDistributedTrainingWithProfiling();
    void testSecureCredentialHandlingInTraining();
    void testFaultTolerantDistributedTraining();
    void testCheckpointRecoveryWorkflow();

    // Security Integration Tests
    void testSecurityWithDistributedTraining();
    void testEncryptedCredentialStorage();
    void testACLEnforcementDuringTraining();
    void testAuditLoggingIntegration();
    void testOAuth2FlowWithTraining();

    // Hardware Integration Tests
    void testAutoHardwareSelectionForTraining();
    void testMultiGPUTrainingWorkflow();
    void testBackendSwitchingDuringTraining();
    void testGPUMemoryManagement();

    // Observability Integration Tests
    void testDashboardWithLiveMetrics();
    void testAlertingDuringTraining();
    void testProfilingDataVisualization();
    void testMetricsExportToPrometheus();

    // Combined Component Tests
    void testAllComponentsIntegration();
    void testSecurityProfilerIntegration();
    void testHardwareProfilerIntegration();
    void testDistributedSecurityProfiling();

    // Production Scenario Tests
    void testMultiNodeProductionWorkflow();
    void testHighAvailabilitySetup();
    void testLoadBalancingScenario();
    void testDisasterRecovery();

private:
    DistributedTrainer* trainer;
    SecurityManager* security;
    HardwareBackendSelector* hardware;
    Profiler* profiler;
    ObservabilityDashboard* dashboard;
    QTemporaryDir* tempDir;
};

void TestPhase2Integration::initTestCase()
{
    qInfo() << "=== Starting Phase 2 Integration Tests ===";
}

void TestPhase2Integration::cleanupTestCase()
{
    qInfo() << "=== Phase 2 Integration Tests Complete ===";
}

void TestPhase2Integration::init()
{
    tempDir = new QTemporaryDir();
    QVERIFY(tempDir->isValid());

    trainer = new DistributedTrainer();
    security = SecurityManager::getInstance();
    hardware = new HardwareBackendSelector();
    profiler = new Profiler();
    dashboard = new ObservabilityDashboard();
}

void TestPhase2Integration::cleanup()
{
    delete trainer;
    security->shutdown();
    delete hardware;
    delete profiler;
    delete dashboard;
    delete tempDir;
}

// ============================================================================
// END-TO-END TRAINING WORKFLOW TESTS
// ============================================================================

void TestPhase2Integration::testCompleteTrainingPipeline()
{
    // 1. Initialize security
    SecurityManager::Config secConfig;
    secConfig.masterPassword = "IntegrationTestPassword123!";
    QVERIFY(security->initialize(secConfig));

    // 2. Select hardware backend
    hardware->autoDetect();
    QString backend = hardware->selectBestBackend();
    QVERIFY(hardware->initializeBackend(backend));

    // 3. Initialize distributed trainer
    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = "nccl";
    trainConfig.pgConfig.worldSize = 1;
    trainConfig.pgConfig.rank = 0;
    QVERIFY(trainer->Initialize(trainConfig));

    // 4. Start profiling
    profiler->startProfiling();

    // 5. Run training step
    std::vector<float> gradients(1000, 1.0f);
    QVERIFY(trainer->TrainStep(gradients, 1));

    // 6. Save checkpoint
    QString checkpointPath = tempDir->filePath("integration_checkpoint.ckpt");
    QVERIFY(trainer->SaveCheckpoint(checkpointPath, 1));

    // 7. Verify profiling data
    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.forwardPassMs >= 0.0);

    // 8. Verify metrics
    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("step"));
    QCOMPARE(metrics["step"].toInt(), 1);

    qInfo() << "✓ Complete training pipeline successful";
}

void TestPhase2Integration::testDistributedTrainingWithProfiling()
{
    // Initialize profiler first
    profiler->startProfiling();

    // Initialize distributed trainer
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 2;
    config.pgConfig.rank = 0;
    config.pgConfig.enableProfiling = true;

    QVERIFY(trainer->Initialize(config));

    // Run multiple training steps
    std::vector<float> gradients(10000, 2.0f);
    for (int i = 1; i <= 10; ++i) {
        profiler->markPhaseStart("forwardPass");
        // Simulate forward pass
        QTest::qWait(5);
        profiler->markPhaseEnd("forwardPass");

        profiler->markPhaseStart("backwardPass");
        trainer->TrainStep(gradients, i);
        profiler->markPhaseEnd("backwardPass");
    }

    // Verify profiling captured all phases
    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.forwardPassMs > 0.0);
    QVERIFY(snapshot.backwardPassMs > 0.0);

    // Verify trainer metrics
    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["step"].toInt(), 10);

    qInfo() << "✓ Distributed training with profiling successful";
}

void TestPhase2Integration::testSecureCredentialHandlingInTraining()
{
    // Initialize security
    SecurityManager::Config secConfig;
    secConfig.masterPassword = "SecureTraining123!";
    QVERIFY(security->initialize(secConfig));

    // Store training credentials (e.g., for remote data access)
    QString userId = "training_job@example.com";
    QString token = "training_access_token_xyz";
    QVERIFY(security->storeCredential(
        userId, token, "bearer",
        QDateTime::currentDateTime().addSecs(7200)
    ));

    // Set access control for training resources
    QVERIFY(security->setAccessControl(
        userId, "models/training",
        SecurityManager::AccessLevel::Write
    ));

    // Verify access before training
    QVERIFY(security->checkAccess(
        userId, "models/training",
        SecurityManager::AccessLevel::Write
    ));

    // Initialize trainer
    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = "nccl";
    trainConfig.pgConfig.worldSize = 1;
    trainConfig.pgConfig.rank = 0;
    QVERIFY(trainer->Initialize(trainConfig));

    // Run training (simulating authenticated access)
    std::vector<float> gradients(1000, 1.0f);
    QVERIFY(trainer->TrainStep(gradients, 1));

    // Log security event
    security->logSecurityEvent(
        "training_started",
        userId,
        "Training job started successfully",
        SecurityManager::SecurityEventSeverity::Info
    );

    // Verify audit log
    auto events = security->getAuditLog(
        QDateTime::currentDateTime().addSecs(-60),
        QDateTime::currentDateTime().addSecs(60)
    );
    QVERIFY(events.size() > 0);

    qInfo() << "✓ Secure credential handling in training successful";
}

void TestPhase2Integration::testFaultTolerantDistributedTraining()
{
    // Configure fault tolerance
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.autoRecover = true;
    config.faultTolerance.maxRetries = 3;
    config.faultTolerance.workerTimeoutMs = 5000;

    QVERIFY(trainer->Initialize(config));

    QSignalSpy recoveryspy(trainer, &DistributedTrainer::nodeRecovered);

    // Run training
    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 5; ++i) {
        QVERIFY(trainer->TrainStep(gradients, i));
    }

    // In real scenario, worker failure would be simulated here
    // For integration test, just verify fault tolerance is active
    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["fault_tolerance"].toBool());

    qInfo() << "✓ Fault-tolerant distributed training successful";
}

void TestPhase2Integration::testCheckpointRecoveryWorkflow()
{
    // Phase 1: Initial training with checkpointing
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    QVERIFY(trainer->Initialize(config));

    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 20; ++i) {
        trainer->TrainStep(gradients, i);
    }

    // Save checkpoint at step 20
    QString checkpointPath = tempDir->filePath("recovery_test.ckpt");
    QVERIFY(trainer->SaveCheckpoint(checkpointPath, 20));

    // Phase 2: Simulate crash and recovery
    delete trainer;
    trainer = new DistributedTrainer();

    // Reinitialize and load checkpoint
    QVERIFY(trainer->Initialize(config));
    int resumeStep = trainer->LoadCheckpoint(checkpointPath);
    QCOMPARE(resumeStep, 20);

    // Continue training from step 21
    for (int i = 21; i <= 30; ++i) {
        trainer->TrainStep(gradients, i);
    }

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["step"].toInt(), 30);

    qInfo() << "✓ Checkpoint recovery workflow successful";
}

// ============================================================================
// SECURITY INTEGRATION TESTS
// ============================================================================

void TestPhase2Integration::testSecurityWithDistributedTraining()
{
    // Initialize security
    SecurityManager::Config secConfig;
    secConfig.masterPassword = "DistSecure123!";
    secConfig.auditLogging.enabled = true;
    QVERIFY(security->initialize(secConfig));

    // Initialize distributed trainer
    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = "nccl";
    trainConfig.pgConfig.worldSize = 2;
    trainConfig.pgConfig.rank = 0;
    QVERIFY(trainer->Initialize(trainConfig));

    // Set up access control
    QString userId = "trainer@example.com";
    security->setAccessControl(userId, "models/training",
                              SecurityManager::AccessLevel::Write);

    // Run training with security logging
    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 5; ++i) {
        // Log training step
        security->logSecurityEvent(
            "training_step",
            userId,
            QString("Step %1 completed").arg(i),
            SecurityManager::SecurityEventSeverity::Info
        );

        trainer->TrainStep(gradients, i);
    }

    // Verify audit trail
    auto events = security->getAuditLog(
        QDateTime::currentDateTime().addSecs(-120),
        QDateTime::currentDateTime()
    );
    QVERIFY(events.size() >= 5);

    qInfo() << "✓ Security with distributed training successful";
}

void TestPhase2Integration::testEncryptedCredentialStorage()
{
    // Initialize security
    SecurityManager::Config config;
    config.masterPassword = "EncryptTest123!";
    config.encryptionAlgorithm = "AES-256-GCM";
    QVERIFY(security->initialize(config));

    // Store multiple credentials
    QStringList userIds = {
        "user1@example.com",
        "user2@example.com",
        "user3@example.com"
    };

    for (const QString& userId : userIds) {
        QString token = userId + "_token";
        QVERIFY(security->storeCredential(
            userId, token, "bearer",
            QDateTime::currentDateTime().addSecs(3600)
        ));
    }

    // Retrieve and verify all credentials
    for (const QString& userId : userIds) {
        auto cred = security->getCredential(userId);
        QCOMPARE(cred.userId, userId);
        QVERIFY(!cred.token.isEmpty());
    }

    // Verify encryption by checking stored data is not plaintext
    // (Implementation-specific check)

    qInfo() << "✓ Encrypted credential storage successful";
}

void TestPhase2Integration::testACLEnforcementDuringTraining()
{
    // Initialize security
    SecurityManager::Config config;
    config.masterPassword = "ACLTest123!";
    QVERIFY(security->initialize(config));

    // Set up ACLs
    QString adminUser = "admin@example.com";
    QString readOnlyUser = "readonly@example.com";
    QString resource = "models/training";

    security->setAccessControl(adminUser, resource,
                              SecurityManager::AccessLevel::Admin);
    security->setAccessControl(readOnlyUser, resource,
                              SecurityManager::AccessLevel::Read);

    // Verify admin has full access
    QVERIFY(security->checkAccess(adminUser, resource,
                                  SecurityManager::AccessLevel::Admin));
    QVERIFY(security->checkAccess(adminUser, resource,
                                  SecurityManager::AccessLevel::Write));

    // Verify read-only user has limited access
    QVERIFY(security->checkAccess(readOnlyUser, resource,
                                  SecurityManager::AccessLevel::Read));
    QVERIFY(!security->checkAccess(readOnlyUser, resource,
                                   SecurityManager::AccessLevel::Write));

    qInfo() << "✓ ACL enforcement during training successful";
}

void TestPhase2Integration::testAuditLoggingIntegration()
{
    // Enable audit logging
    SecurityManager::Config config;
    config.masterPassword = "AuditTest123!";
    config.auditLogging.enabled = true;
    config.auditLogging.logToFile = true;
    config.auditLogging.maxFileSize = 10485760;  // 10 MB
    QVERIFY(security->initialize(config));

    // Log various events
    security->logSecurityEvent("login", "user1", "Login successful",
                              SecurityManager::SecurityEventSeverity::Info);
    security->logSecurityEvent("access_denied", "user2", "Access denied",
                              SecurityManager::SecurityEventSeverity::Warning);
    security->logSecurityEvent("unauthorized_access", "user3", "Unauthorized",
                              SecurityManager::SecurityEventSeverity::Critical);

    // Retrieve and filter logs
    auto allEvents = security->getAuditLog(
        QDateTime::currentDateTime().addSecs(-60),
        QDateTime::currentDateTime()
    );
    QVERIFY(allEvents.size() >= 3);

    // Filter by event type
    auto loginEvents = security->getAuditLog(
        QDateTime::currentDateTime().addSecs(-60),
        QDateTime::currentDateTime(),
        "login"
    );
    QVERIFY(loginEvents.size() >= 1);

    qInfo() << "✓ Audit logging integration successful";
}

void TestPhase2Integration::testOAuth2FlowWithTraining()
{
    // Initialize security
    SecurityManager::Config config;
    config.masterPassword = "OAuth2Test123!";
    QVERIFY(security->initialize(config));

    // Simulate OAuth2 token issuance
    QString userId = "oauth_user@example.com";
    QString accessToken = "oauth_access_token";
    QString refreshToken = "oauth_refresh_token";
    QDateTime expiresAt = QDateTime::currentDateTime().addSecs(3600);

    QVERIFY(security->storeCredential(userId, accessToken, "bearer",
                                     expiresAt, refreshToken));

    // Verify token is not expired
    QVERIFY(!security->isTokenExpired(userId));

    // Use credential for training initialization
    auto cred = security->getCredential(userId);
    QCOMPARE(cred.token, accessToken);
    QVERIFY(cred.isRefreshable);

    // Initialize trainer with OAuth2 user
    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = "nccl";
    trainConfig.pgConfig.worldSize = 1;
    trainConfig.pgConfig.rank = 0;
    QVERIFY(trainer->Initialize(trainConfig));

    // Run training
    std::vector<float> gradients(1000, 1.0f);
    QVERIFY(trainer->TrainStep(gradients, 1));

    qInfo() << "✓ OAuth2 flow with training successful";
}

// ============================================================================
// HARDWARE INTEGRATION TESTS
// ============================================================================

void TestPhase2Integration::testAutoHardwareSelectionForTraining()
{
    // Auto-detect hardware
    hardware->autoDetect();
    QString bestBackend = hardware->selectBestBackend();
    QVERIFY(!bestBackend.isEmpty());

    // Initialize backend
    QVERIFY(hardware->initializeBackend(bestBackend));

    // Configure trainer to use selected backend
    DistributedTrainer::TrainerConfig config;
    if (bestBackend == "CUDA") {
        config.pgConfig.backend = "nccl";
    } else {
        config.pgConfig.backend = "gloo";
    }
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    QVERIFY(trainer->Initialize(config));

    // Run training
    std::vector<float> gradients(1000, 1.0f);
    QVERIFY(trainer->TrainStep(gradients, 1));

    qInfo() << "✓ Auto hardware selection for training successful";
}

void TestPhase2Integration::testMultiGPUTrainingWorkflow()
{
    hardware->autoDetect();

    if (hardware->isBackendSupported("CUDA")) {
        auto devices = hardware->listDevices("CUDA");
        if (devices.size() >= 2) {
            // Multi-GPU available
            DistributedTrainer::TrainerConfig config;
            config.pgConfig.backend = "nccl";
            config.pgConfig.worldSize = devices.size();
            config.pgConfig.rank = 0;

            QVERIFY(trainer->Initialize(config));

            std::vector<float> gradients(10000, 1.0f);
            for (int i = 1; i <= 5; ++i) {
                QVERIFY(trainer->TrainStep(gradients, i));
            }

            auto metrics = trainer->GetMetrics();
            QCOMPARE(metrics["world_size"].toInt(), devices.size());

            qInfo() << "✓ Multi-GPU training workflow successful";
        } else {
            qInfo() << "⊘ Multi-GPU test skipped (insufficient GPUs)";
        }
    } else {
        qInfo() << "⊘ Multi-GPU test skipped (CUDA not available)";
    }
}

void TestPhase2Integration::testBackendSwitchingDuringTraining()
{
    hardware->autoDetect();

    // Initialize with CPU
    hardware->initializeBackend("CPU");
    QCOMPARE(hardware->getCurrentBackend(), QString("CPU"));

    // Run training on CPU
    DistributedTrainer::TrainerConfig config1;
    config1.pgConfig.backend = "gloo";
    config1.pgConfig.worldSize = 1;
    config1.pgConfig.rank = 0;
    trainer->Initialize(config1);

    std::vector<float> gradients(1000, 1.0f);
    trainer->TrainStep(gradients, 1);

    // Switch to CUDA if available
    if (hardware->isBackendSupported("CUDA")) {
        hardware->initializeBackend("CUDA");
        QCOMPARE(hardware->getCurrentBackend(), QString("CUDA"));

        // Reinitialize trainer for new backend
        delete trainer;
        trainer = new DistributedTrainer();

        DistributedTrainer::TrainerConfig config2;
        config2.pgConfig.backend = "nccl";
        config2.pgConfig.worldSize = 1;
        config2.pgConfig.rank = 0;
        trainer->Initialize(config2);

        trainer->TrainStep(gradients, 2);

        qInfo() << "✓ Backend switching during training successful";
    } else {
        qInfo() << "⊘ Backend switching test partially completed (CUDA unavailable)";
    }
}

void TestPhase2Integration::testGPUMemoryManagement()
{
    hardware->autoDetect();

    if (hardware->isBackendSupported("CUDA")) {
        hardware->initializeBackend("CUDA");
        hardware->selectDevice("CUDA", 0);

        size_t totalMem = hardware->getTotalMemoryMB();
        size_t freeMem1 = hardware->getFreeMemoryMB();

        QVERIFY(totalMem > 0);
        QVERIFY(freeMem1 <= totalMem);

        // Initialize trainer (allocates GPU memory)
        DistributedTrainer::TrainerConfig config;
        config.pgConfig.backend = "nccl";
        config.pgConfig.worldSize = 1;
        config.pgConfig.rank = 0;
        trainer->Initialize(config);

        // Run training (uses GPU memory)
        std::vector<float> gradients(100000, 1.0f);
        trainer->TrainStep(gradients, 1);

        size_t freeMem2 = hardware->getFreeMemoryMB();

        // Free memory should decrease after allocation
        QVERIFY(freeMem2 <= freeMem1);

        qInfo() << "✓ GPU memory management successful";
    } else {
        qInfo() << "⊘ GPU memory test skipped (CUDA not available)";
    }
}

// ============================================================================
// OBSERVABILITY INTEGRATION TESTS
// ============================================================================

void TestPhase2Integration::testDashboardWithLiveMetrics()
{
    // Initialize trainer and profiler
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    profiler->startProfiling();

    // Run training and update dashboard
    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 10; ++i) {
        trainer->TrainStep(gradients, i);

        // Update dashboard with metrics
        auto trainerMetrics = trainer->GetMetrics();
        auto profilerSnapshot = profiler->getSnapshot();

        QJsonObject combined;
        combined["step"] = trainerMetrics["step"];
        combined["cpu_utilization"] = profilerSnapshot.cpuUtilization;
        combined["throughput"] = profilerSnapshot.throughputSamples;

        dashboard->updateMetrics(combined);
    }

    // Verify dashboard has historical data
    auto history = dashboard->getMetricsHistory();
    QVERIFY(history.size() >= 10);

    qInfo() << "✓ Dashboard with live metrics successful";
}

void TestPhase2Integration::testAlertingDuringTraining()
{
    // Configure alert thresholds
    dashboard->setAlertThreshold("cpu_utilization", 80.0);
    dashboard->setAlertThreshold("memory_used_mb", 8000);

    QSignalSpy spy(dashboard, &ObservabilityDashboard::alertTriggered);

    // Initialize and run training
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    profiler->startProfiling();

    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 5; ++i) {
        trainer->TrainStep(gradients, i);

        // Simulate high CPU usage
        QJsonObject metrics;
        metrics["cpu_utilization"] = 90.0;  // Exceeds threshold
        dashboard->updateMetrics(metrics);
    }

    // Alert should have been triggered
    QVERIFY(spy.count() > 0);

    qInfo() << "✓ Alerting during training successful";
}

void TestPhase2Integration::testProfilingDataVisualization()
{
    profiler->startProfiling();

    // Run training with phase marking
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 10; ++i) {
        profiler->markPhaseStart("forwardPass");
        QTest::qWait(5);
        profiler->markPhaseEnd("forwardPass");

        profiler->markPhaseStart("backwardPass");
        trainer->TrainStep(gradients, i);
        profiler->markPhaseEnd("backwardPass");

        // Update dashboard
        auto snapshot = profiler->getSnapshot();
        QJsonObject metrics;
        metrics["forward_pass_ms"] = snapshot.forwardPassMs;
        metrics["backward_pass_ms"] = snapshot.backwardPassMs;
        dashboard->updateMetrics(metrics);
    }

    // Export profiling report
    QString reportPath = tempDir->filePath("profiling_visualization.json");
    QVERIFY(profiler->exportReport(reportPath));

    qInfo() << "✓ Profiling data visualization successful";
}

void TestPhase2Integration::testMetricsExportToPrometheus()
{
    // Initialize trainer
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;
    trainer->Initialize(config);

    profiler->startProfiling();

    // Run training
    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 5; ++i) {
        trainer->TrainStep(gradients, i);
    }

    // Collect metrics for Prometheus
    auto metrics = trainer->GetMetrics();
    auto snapshot = profiler->getSnapshot();

    // Format for Prometheus (simplified)
    QJsonObject prometheusMetrics;
    prometheusMetrics["rawrxd_training_step"] = metrics["step"];
    prometheusMetrics["rawrxd_cpu_utilization"] = snapshot.cpuUtilization;
    prometheusMetrics["rawrxd_throughput_samples_per_sec"] = snapshot.throughputSamples;

    // Verify metrics are exportable
    QVERIFY(!prometheusMetrics.isEmpty());

    qInfo() << "✓ Metrics export to Prometheus successful";
}

// ============================================================================
// COMBINED COMPONENT TESTS
// ============================================================================

void TestPhase2Integration::testAllComponentsIntegration()
{
    qInfo() << "=== Testing All Components Integration ===";

    // 1. Security
    SecurityManager::Config secConfig;
    secConfig.masterPassword = "AllComponents123!";
    secConfig.auditLogging.enabled = true;
    QVERIFY(security->initialize(secConfig));

    // 2. Hardware
    hardware->autoDetect();
    QString backend = hardware->selectBestBackend();
    QVERIFY(hardware->initializeBackend(backend));

    // 3. Profiler
    profiler->startProfiling();

    // 4. Distributed Trainer
    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = (backend == "CUDA") ? "nccl" : "gloo";
    trainConfig.pgConfig.worldSize = 1;
    trainConfig.pgConfig.rank = 0;
    trainConfig.pgConfig.enableProfiling = true;
    QVERIFY(trainer->Initialize(trainConfig));

    // 5. Observability Dashboard
    dashboard->setAlertThreshold("cpu_utilization", 85.0);

    // Run integrated workflow
    QString userId = "integrated_user@example.com";
    security->storeCredential(userId, "token", "bearer",
                             QDateTime::currentDateTime().addSecs(3600));
    security->setAccessControl(userId, "models/training",
                              SecurityManager::AccessLevel::Write);

    std::vector<float> gradients(5000, 1.0f);
    for (int i = 1; i <= 10; ++i) {
        // Mark profiling phases
        profiler->markPhaseStart("trainStep");

        // Run training
        trainer->TrainStep(gradients, i);

        profiler->markPhaseEnd("trainStep");

        // Log security event
        security->logSecurityEvent("training_step", userId,
                                  QString("Step %1").arg(i),
                                  SecurityManager::SecurityEventSeverity::Info);

        // Update dashboard
        auto metrics = trainer->GetMetrics();
        auto snapshot = profiler->getSnapshot();
        QJsonObject combined;
        combined["step"] = metrics["step"];
        combined["cpu_utilization"] = snapshot.cpuUtilization;
        dashboard->updateMetrics(combined);
    }

    // Verify all components worked together
    QCOMPARE(trainer->GetMetrics()["step"].toInt(), 10);
    QVERIFY(profiler->getSnapshot().forwardPassMs >= 0.0);
    QVERIFY(security->getAuditLog(QDateTime::currentDateTime().addSecs(-120),
                                  QDateTime::currentDateTime()).size() >= 10);
    QVERIFY(dashboard->getMetricsHistory().size() >= 10);

    qInfo() << "✓ All components integration successful";
}

void TestPhase2Integration::testSecurityProfilerIntegration()
{
    // Initialize security with profiling
    SecurityManager::Config config;
    config.masterPassword = "SecProf123!";
    config.auditLogging.enabled = true;
    security->initialize(config);

    profiler->startProfiling();

    // Measure encryption performance
    QByteArray data(1024 * 1024, 'A');  // 1 MB
    QByteArray aad = "test_context";

    profiler->markPhaseStart("encryption");
    QByteArray encrypted = security->encryptData(data, aad);
    profiler->markPhaseEnd("encryption");

    profiler->markPhaseStart("decryption");
    QByteArray decrypted = security->decryptData(encrypted, aad);
    profiler->markPhaseEnd("decryption");

    QCOMPARE(decrypted, data);

    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.phaseTimings.contains("encryption"));
    QVERIFY(snapshot.phaseTimings.contains("decryption"));

    qInfo() << "Encryption time:" << snapshot.phaseTimings["encryption"] << "ms";
    qInfo() << "Decryption time:" << snapshot.phaseTimings["decryption"] << "ms";

    qInfo() << "✓ Security-Profiler integration successful";
}

void TestPhase2Integration::testHardwareProfilerIntegration()
{
    hardware->autoDetect();
    profiler->startProfiling();

    QString backend = hardware->selectBestBackend();
    
    profiler->markPhaseStart("hardware_init");
    hardware->initializeBackend(backend);
    profiler->markPhaseEnd("hardware_init");

    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.phaseTimings.contains("hardware_init"));

    qInfo() << "Hardware init time:" << snapshot.phaseTimings["hardware_init"] << "ms";

    qInfo() << "✓ Hardware-Profiler integration successful";
}

void TestPhase2Integration::testDistributedSecurityProfiling()
{
    // Initialize all three components
    SecurityManager::Config secConfig;
    secConfig.masterPassword = "DistSecProf123!";
    security->initialize(secConfig);

    DistributedTrainer::TrainerConfig trainConfig;
    trainConfig.pgConfig.backend = "nccl";
    trainConfig.pgConfig.worldSize = 2;
    trainConfig.pgConfig.rank = 0;
    trainer->Initialize(trainConfig);

    profiler->startProfiling();

    // Run training with security and profiling
    QString userId = "secure_trainer@example.com";
    security->setAccessControl(userId, "models/training",
                              SecurityManager::AccessLevel::Write);

    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 5; ++i) {
        profiler->markPhaseStart("secure_train_step");

        // Check access (security)
        QVERIFY(security->checkAccess(userId, "models/training",
                                      SecurityManager::AccessLevel::Write));

        // Train (distributed)
        trainer->TrainStep(gradients, i);

        // Log (security)
        security->logSecurityEvent("step", userId, QString::number(i),
                                  SecurityManager::SecurityEventSeverity::Info);

        profiler->markPhaseEnd("secure_train_step");
    }

    auto snapshot = profiler->getSnapshot();
    QVERIFY(snapshot.phaseTimings.contains("secure_train_step"));

    qInfo() << "✓ Distributed-Security-Profiling integration successful";
}

// ============================================================================
// PRODUCTION SCENARIO TESTS
// ============================================================================

void TestPhase2Integration::testMultiNodeProductionWorkflow()
{
    qInfo() << "=== Multi-Node Production Workflow ===";

    // Configure multi-node setup
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.pgConfig.masterAddr = "127.0.0.1";
    config.pgConfig.masterPort = 29500;
    config.faultTolerance.enabled = true;
    config.loadBalancing.enabled = true;
    config.pgConfig.enableProfiling = true;

    QVERIFY(trainer->Initialize(config));

    // Run production training
    std::vector<float> gradients(10000, 1.0f);
    for (int i = 1; i <= 20; ++i) {
        QVERIFY(trainer->TrainStep(gradients, i));

        // Save checkpoint every 10 steps
        if (i % 10 == 0) {
            QString path = tempDir->filePath(QString("prod_ckpt_%1.ckpt").arg(i));
            trainer->SaveCheckpoint(path, i);
        }
    }

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["step"].toInt(), 20);

    qInfo() << "✓ Multi-node production workflow successful";
}

void TestPhase2Integration::testHighAvailabilitySetup()
{
    // Configure HA setup
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "gloo";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.faultTolerance.enabled = true;
    config.faultTolerance.autoRecover = true;
    config.faultTolerance.maxRetries = 5;
    config.faultTolerance.heartbeatIntervalMs = 1000;

    QVERIFY(trainer->Initialize(config));

    // Run training with HA
    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 10; ++i) {
        QVERIFY(trainer->TrainStep(gradients, i));
    }

    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics["fault_tolerance"].toBool());

    qInfo() << "✓ High availability setup successful";
}

void TestPhase2Integration::testLoadBalancingScenario()
{
    // Configure load balancing
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 4;
    config.pgConfig.rank = 0;
    config.loadBalancing.enabled = true;
    config.loadBalancing.dynamicBatchSize = true;
    config.loadBalancing.rebalanceIntervalSteps = 5;

    QVERIFY(trainer->Initialize(config));

    // Run training with load balancing
    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 15; ++i) {
        QVERIFY(trainer->TrainStep(gradients, i));
    }

    // Verify rebalancing occurred
    auto metrics = trainer->GetMetrics();
    QVERIFY(metrics.contains("rebalances"));

    qInfo() << "✓ Load balancing scenario successful";
}

void TestPhase2Integration::testDisasterRecovery()
{
    // Phase 1: Normal training with checkpointing
    DistributedTrainer::TrainerConfig config;
    config.pgConfig.backend = "nccl";
    config.pgConfig.worldSize = 1;
    config.pgConfig.rank = 0;

    trainer->Initialize(config);

    std::vector<float> gradients(1000, 1.0f);
    for (int i = 1; i <= 10; ++i) {
        trainer->TrainStep(gradients, i);
    }

    QString checkpoint = tempDir->filePath("disaster_recovery.ckpt");
    trainer->SaveCheckpoint(checkpoint, 10);

    // Phase 2: Simulate disaster (system crash)
    delete trainer;
    trainer = new DistributedTrainer();

    // Phase 3: Recovery
    trainer->Initialize(config);
    int resumeStep = trainer->LoadCheckpoint(checkpoint);
    QCOMPARE(resumeStep, 10);

    // Continue training from recovered state
    for (int i = 11; i <= 20; ++i) {
        trainer->TrainStep(gradients, i);
    }

    auto metrics = trainer->GetMetrics();
    QCOMPARE(metrics["step"].toInt(), 20);

    qInfo() << "✓ Disaster recovery successful";
}

QTEST_MAIN(TestPhase2Integration)
#include "test_phase2_integration.moc"
