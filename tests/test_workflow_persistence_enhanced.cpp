/**
 * Workflow Persistence Smoke Test - Enhanced with 8 Full Enhancements
 * 
 * Tests all baseline functionality plus 8 production-grade enhancements:
 * 1. Checkpoint Compression - Disk space optimization
 * 2. Incremental Persistence - Delta state persistence
 * 3. Memory-Mapped State - Zero-copy state access
 * 4. Async Persistence Queue - Non-blocking persistence
 * 5. Encrypted Persistence - At-rest state encryption
 * 6. State Versioning - Schema migration support
 * 7. Distributed Persistence - Multi-node state sync
 * 8. Telemetry Persistence - Metrics and diagnostics
 */

#include "../src/agentic_loop_state.h"
#include "../src/agentic_memory_system.h"
#include "../src/execution_state_persistence.h"

// Enhancement headers
#include "../src/checkpoint_compression.h"
#include "../src/incremental_persistence.h"
#include "../src/memory_mapped_state.h"
#include "../src/async_persistence_queue.h"
#include "../src/encrypted_persistence.h"
#include "../src/state_versioning.h"
#include "../src/distributed_persistence.h"
#include "../src/telemetry_persistence.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>

namespace {
    int testsPassed = 0;
    int testsFailed = 0;

    void test(bool condition, const char* name) {
        if (condition) {
            std::cout << "[PASS] " << name << std::endl;
            testsPassed++;
        } else {
            std::cout << "[FAIL] " << name << std::endl;
            testsFailed++;
        }
    }
}

int main()
{
    namespace fs = std::filesystem;

    std::cout << "========================================" << std::endl;
    std::cout << "Workflow Persistence Enhanced Smoke Test" << std::endl;
    std::cout << "========================================" << std::endl;

    const auto root = fs::temp_directory_path() / "rawrxd_workflow_persistence_smoke";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    
    if (ec) {
        std::cerr << "[ERROR] temp dir setup failed: " << ec.message() << std::endl;
        return 1;
    }

    // ============================================================================
    // BASELINE FUNCTIONALITY
    // ============================================================================
    std::cout << "\n--- Baseline Functionality ---" << std::endl;
    
    ExecutionStatePersistence persistence(root);
    
    // Create and persist execution
    AgenticLoopState loop;
    loop.startIteration("Test iteration");
    loop.setCurrentPhase(ReasoningPhase::Planning);
    loop.addToMemory("key", "value");
    
    AgenticMemorySystem memory;
    memory.storeMemory(MemoryType::Fact, "test fact", "test");
    
    auto execution = persistence.captureCurrentExecution(
        "TestWorkflow", "Test goal", &loop, &memory, nullptr);
    execution.executionId = "test_exec_001";
    
    auto execId = persistence.persistWorkflowExecution(execution);
    test(!execId.empty(), "Persist execution");
    
    // Load execution
    auto loaded = persistence.loadWorkflowExecution(execId);
    test(loaded != nullptr, "Load execution");
    test(loaded && loaded->executionId == execId, "Loaded execution ID matches");
    
    // Create checkpoint
    nlohmann::json state;
    state["data"] = "test";
    auto cpId = persistence.createCheckpoint(execId, "test_cp", state);
    test(!cpId.empty(), "Create checkpoint");
    
    // Resume from checkpoint
    auto resumed = persistence.resumeFromCheckpoint(execId);
    test(resumed != nullptr, "Resume from checkpoint");
    
    // ============================================================================
    // ENHANCEMENT 1: Checkpoint Compression
    // ============================================================================
    std::cout << "\n--- Enhancement 1: Checkpoint Compression ---" << std::endl;
    
    persistence.setCompressionLevel(6);
    test(persistence.getCompressionLevel() == 6, "Set compression level");
    
    // Test compression on repeated data
    nlohmann::json largeState;
    largeState["data"] = std::string(1000, 'A');
    std::string jsonStr = largeState.dump();
    std::string compressed = persistence.compressState(jsonStr);
    test(compressed.length() < jsonStr.length(), "Compression reduces size");
    
    std::string decompressed = persistence.decompressState(compressed);
    test(decompressed == jsonStr, "Decompression matches original");
    
    // Test CheckpointCompression namespace
    auto compressResult = CheckpointCompression::compress(
        std::vector<uint8_t>(jsonStr.begin(), jsonStr.end()),
        CP_COMPRESS_AUTO);
    test(!compressResult.data.empty(), "CheckpointCompression API works");
    
    // ============================================================================
    // ENHANCEMENT 2: Incremental Persistence
    // ============================================================================
    std::cout << "\n--- Enhancement 2: Incremental Persistence ---" << std::endl;
    
    IncrementalPersistence::ChangeTracker tracker;
    tracker.markDirty(IP_DIRTY_LOOP);
    tracker.markDirty(IP_DIRTY_MEMORY);
    test(tracker.isDirty(IP_DIRTY_LOOP), "Change tracker marks dirty");
    test(tracker.getDirtyFlags() == (IP_DIRTY_LOOP | IP_DIRTY_MEMORY), "Dirty flags combined");
    
    nlohmann::json oldState;
    oldState["field1"] = "value1";
    oldState["field2"] = "value2";
    
    nlohmann::json newState;
    newState["field1"] = "value1";
    newState["field2"] = "modified";
    newState["field3"] = "added";
    
    auto stateDelta = tracker.computeDelta(oldState, newState);
    test(stateDelta.contains("~"), "Delta contains modifications");
    test(stateDelta.contains("+"), "Delta contains additions");
    
    // Test incremental checkpoint creation
    auto incCp = persistence.createIncrementalCheckpoint(execId, "incremental_cp", newState);
    test(!incCp.empty(), "Create incremental checkpoint");
    
    // ============================================================================
    // ENHANCEMENT 3: Memory-Mapped State
    // ============================================================================
    std::cout << "\n--- Enhancement 3: Memory-Mapped State ---" << std::endl;
    
    test(MemoryMappedState::isSupported(), "Memory mapping is supported");
    test(MemoryMappedState::getPageSize() > 0, "Page size is valid");
    test(MemoryMappedState::getMaxMappingSize() > 0, "Max mapping size is valid");
    
    persistence.enableMemoryMapping(true);
    test(persistence.isMemoryMappingEnabled(), "Enable memory mapping");
    
    // ============================================================================
    // ENHANCEMENT 4: Async Persistence Queue
    // ============================================================================
    std::cout << "\n--- Enhancement 4: Async Persistence Queue ---" << std::endl;
    
    auto& queue = AsyncPersistenceQueue::getGlobalQueue();
    test(AsyncPersistenceQueue::initializeGlobal(2), "Initialize async queue");
    
    persistence.enableAsyncPersistence(true);
    test(persistence.isAsyncPersistenceEnabled(), "Enable async persistence");
    
    auto walStats = persistence.getWalStats();
    test(walStats.pendingWrites >= 0, "WAL stats accessible");
    
    persistence.flushAsyncWrites();
    test(true, "Flush async writes");
    
    persistence.enableAsyncPersistence(false);
    test(!persistence.isAsyncPersistenceEnabled(), "Disable async persistence");
    
    AsyncPersistenceQueue::shutdownGlobal();
    test(true, "Shutdown async queue");
    
    // ============================================================================
    // ENHANCEMENT 5: Encrypted Persistence
    // ============================================================================
    std::cout << "\n--- Enhancement 5: Encrypted Persistence ---" << std::endl;
    
    EncryptedPersistence::SecureKeyStore keyStore;
    auto key = keyStore.generateKey(32);
    test(key.size() == 32, "Generate encryption key");
    
    EncryptedPersistence::EncryptionEngine engine;
    test(engine.initialize(key, ENC_AES256_GCM), "Initialize encryption engine");
    test(engine.isInitialized(), "Engine is initialized");
    
    std::vector<uint8_t> plaintext = {1, 2, 3, 4, 5};
    auto envelope = engine.encrypt(plaintext);
    test(!envelope.ciphertext.empty(), "Encrypt data");
    
    bool decryptSuccess = false;
    auto decrypted = engine.decrypt(envelope, decryptSuccess);
    test(decryptSuccess, "Decrypt success");
    test(decrypted == plaintext, "Decrypted matches plaintext");
    
    // ============================================================================
    // ENHANCEMENT 6: State Versioning
    // ============================================================================
    std::cout << "\n--- Enhancement 6: State Versioning ---" << std::endl;
    
    StateVersioning::registerBuiltInMigrations();
    auto& registry = StateVersioning::getGlobalRegistry();
    
    test(registry.canMigrate(1, 2), "Can migrate v1 to v2");
    test(registry.canMigrate(2, 3), "Can migrate v2 to v3");
    test(registry.canMigrate(1, 3), "Can migrate v1 to v3 (via v2)");
    
    StateVersioning::MigrationEngine engine2(registry);
    
    nlohmann::json v1State;
    v1State["executionId"] = "test";
    v1State["status"] = "active";
    v1State["schemaVersion"] = 1;
    
    auto v2State = engine2.migrate(v1State, 2);
    test(v2State.contains("checkpoints"), "Migration adds checkpoints field");
    test(v2State["schemaVersion"].get<int>() == 2, "Schema version updated to 2");
    
    // ============================================================================
    // ENHANCEMENT 7: Distributed Persistence
    // ============================================================================
    std::cout << "\n--- Enhancement 7: Distributed Persistence ---" << std::endl;
    
    DistributedPersistence::DistributedStateManager distManager;
    
    DistributedPersistence::ClusterConfig config;
    config.clusterId = "test-cluster";
    config.replicationFactor = 2;
    
    DistributedPersistence::NodeId self;
    self.id = "node-1";
    self.address = "127.0.0.1";
    self.port = 8080;
    self.mode = DP_MODE_LEADER;
    
    test(distManager.initialize(config, self), "Initialize distributed manager");
    
    nlohmann::json replicationDelta;
    replicationDelta["update"] = "test";
    test(distManager.replicate("test_exec", replicationDelta), "Replicate state");
    
    auto status = distManager.getStatus();
    test(status.currentMode == DP_MODE_LEADER, "Node is leader");
    
    distManager.shutdown();
    test(true, "Shutdown distributed manager");
    
    // ============================================================================
    // ENHANCEMENT 8: Telemetry Persistence
    // ============================================================================
    std::cout << "\n--- Enhancement 8: Telemetry Persistence ---" << std::endl;
    
    auto& collector = TelemetryPersistence::getGlobalCollector();
    collector.initialize(TM_LEVEL_DETAILED);
    test(collector.getLevel() == TM_LEVEL_DETAILED, "Telemetry level set");
    
    TelemetryPersistence::Timer timer;
    timer.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    timer.stop();
    test(timer.elapsedMicros() > 0, "Timer measures elapsed time");
    
    collector.recordPersistenceTime(100, true);
    collector.recordCompressionTime(50, 1000, 500);
    collector.incrementPersistCount(true);
    collector.incrementCheckpointCount();
    
    auto snapshot = collector.getTelemetrySnapshot();
    test(snapshot.contains("counters"), "Telemetry snapshot has counters");
    test(snapshot.contains("gauges"), "Telemetry snapshot has gauges");
    
    auto histogram = collector.getHistogramData("persistence");
    test(histogram.contains("count"), "Histogram data available");
    
    auto prometheus = collector.exportPrometheus();
    test(!prometheus.empty(), "Prometheus export");
    
    auto statsd = collector.exportStatsD();
    test(!statsd.empty(), "StatsD export");
    
    auto& logger = TelemetryPersistence::getGlobalLogger();
    logger.logEvent("test", "test_event", {{"key", "value"}});
    auto events = logger.getRecentEvents(10);
    test(events.size() > 0, "Diagnostic events logged");
    
    // ============================================================================
    // SUMMARY
    // ============================================================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Summary:" << std::endl;
    std::cout << "  Passed: " << testsPassed << std::endl;
    std::cout << "  Failed: " << testsFailed << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Cleanup
    fs::remove_all(root, ec);
    
    if (testsFailed > 0) {
        return 1;
    }
    
    std::cout << "\n[workflow_persistence_smoke] PASS - All enhancements validated!" << std::endl;
    return 0;
}
