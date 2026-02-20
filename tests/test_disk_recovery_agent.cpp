// =============================================================================
// test_disk_recovery_agent.cpp — Unit tests for DiskRecoveryAgent
// =============================================================================
// Tests the C++ wrapper bindings to ASM hardware recovery kernel
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// =============================================================================

#include <cassert>
#include <cstring>
#include <iostream>
#include <windows.h>

#include "logging/logger.h"
static Logger s_logger("test_disk_recovery_agent");

// Include the DiskRecoveryAgent header
#include "../src/agent/DiskRecoveryAgent.h"

using namespace RawrXD::Recovery;

// =============================================================================
// Test 1: Basic instantiation
// =============================================================================
static void test_construction() {
    s_logger.info("[TEST 1] Construction and destruction...\n");
    
    DiskRecoveryAgent agent;
    assert(agent.GetState() == RecoveryState::Idle);
    assert(agent.GetBridgeType() == BridgeType::Unknown);
    
    s_logger.info("  ✓ Agent constructed in Idle state\n");
}

// =============================================================================
// Test 2: RecoveryStats reset (the fix we made)
// =============================================================================
static void test_stats_reset() {
    s_logger.info("[TEST 2] RecoveryStats atomic member resets...\n");
    
    RecoveryStats stats;
    
    // Verify atomics initialize to 0
    assert(stats.sectorsProcessed.load() == 0);
    assert(stats.goodSectors.load() == 0);
    assert(stats.badSectors.load() == 0);
    assert(stats.retriesTotal.load() == 0);
    assert(stats.bytesWritten.load() == 0);
    assert(stats.currentLBA.load() == 0);
    assert(stats.percentComplete.load() == 0);
    
    // Verify we can reset them (which was causing the copy-assignment error)
    stats.sectorsProcessed.store(100);
    assert(stats.sectorsProcessed.load() == 100);
    stats.sectorsProcessed.store(0);
    assert(stats.sectorsProcessed.load() == 0);
    
    s_logger.info("  ✓ RecoveryStats atomics work correctly\n");
}

// =============================================================================
// Test 3: DriveInfo initialization
// =============================================================================
static void test_drive_info() {
    s_logger.info("[TEST 3] DriveInfo structure...\n");
    
    DriveInfo info;
    std::memset(&info, 0, sizeof(info));
    
    info.driveNumber = 0;
    info.totalSectors = 1000000;
    info.sectorSize = 4096;
    info.bridgeType = BridgeType::JMS567;
    std::strcpy_s(info.vendorId, sizeof(info.vendorId), "WDC");
    std::strcpy_s(info.productId, sizeof(info.productId), "My Book");
    
    assert(info.driveNumber == 0);
    assert(info.totalSectors == 1000000);
    assert(info.bridgeType == BridgeType::JMS567);
    
    s_logger.info("  ✓ DriveInfo metadata populated correctly\n");
}

// =============================================================================
// Test 4: Configuration defaults
// =============================================================================
static void test_config_defaults() {
    s_logger.info("[TEST 4] RecoveryConfig defaults...\n");
    
    RecoveryConfig config;
    
    assert(config.maxRetries == 100);
    assert(config.timeoutMs == 2000);
    assert(config.sectorSize == 4096);
    assert(config.checkpointInterval == 1000);
    assert(config.extractKey == true);
    assert(config.sparseImage == true);
    assert(!config.outputDir.empty());
    
    s_logger.info("  ✓ RecoveryConfig defaults are reasonable\n");
}

// =============================================================================
// Test 5: PatchResult factory methods
// =============================================================================
static void test_patch_result() {
    s_logger.info("[TEST 5] PatchResult factory methods...\n");
    
    auto ok = PatchResult::ok("Test success");
    assert(ok.success == true);
    assert(ok.errorCode == 0);
    assert(std::string(ok.detail) == "Test success");
    
    auto err = PatchResult::error("Test failure", 42);
    assert(err.success == false);
    assert(err.errorCode == 42);
    assert(std::string(err.detail) == "Test failure");
    
    s_logger.info("  ✓ PatchResult factory pattern works\n");
}

// =============================================================================
// Test 6: RecoveryEvent structure
// =============================================================================
static void test_recovery_event() {
    s_logger.info("[TEST 6] RecoveryEvent creation...\n");
    
    RecoveryEvent event;
    event.type = RecoveryEvent::Type::SectorGood;
    event.lba = 12345;
    event.message = "Sector read successfully";
    event.timestamp = 1.5;
    
    assert(event.type == RecoveryEvent::Type::SectorGood);
    assert(event.lba == 12345);
    assert(event.message == "Sector read successfully");
    
    s_logger.info("  ✓ RecoveryEvent structure valid\n");
}

// =============================================================================
// Test 7: Bridge type enumeration
// =============================================================================
static void test_bridge_types() {
    s_logger.info("[TEST 7] Bridge type enumeration...\n");
    
    assert(static_cast<uint32_t>(BridgeType::Unknown) == 0);
    assert(static_cast<uint32_t>(BridgeType::JMS567) == 1);
    assert(static_cast<uint32_t>(BridgeType::NS1066) == 2);
    assert(static_cast<uint32_t>(BridgeType::ASM1153E) == 3);
    assert(static_cast<uint32_t>(BridgeType::VL716) == 4);
    
    s_logger.info("  ✓ Bridge type constants are correct\n");
}

// =============================================================================
// Main test harness
// =============================================================================
int main() {
    s_logger.info("\n================================================\n");
    s_logger.info("  RawrXD DiskRecoveryAgent Unit Tests\n");
    s_logger.info("  Build: Release (x64)\n");
    s_logger.info("================================================\n\n");
    
    try {
        test_construction();
        test_stats_reset();
        test_drive_info();
        test_config_defaults();
        test_patch_result();
        test_recovery_event();
        test_bridge_types();
        
        s_logger.info("\n================================================\n");
        s_logger.info("  ✓ All tests PASSED (7/7)\n");
        s_logger.info("================================================\n\n");
        
        return 0;
    } catch (const std::exception& ex) {
        s_logger.error( "\n✗ Test failed: " << ex.what() << "\n";
        return 1;
    }
}
