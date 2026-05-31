// ============================================================================
// test_kv_cache_ownership.cpp — Unit tests for KVCacheOwnershipTracker
// ============================================================================
// Uses: GoogleTest (already configured in tests/CMakeLists.txt)
// Build: cmake --build . --target test_kv_cache_ownership
// Run: ctest -R test_kv_cache_ownership -V
// ============================================================================

#include <gtest/gtest.h>
#include "inference/kv_cache_ownership.h"

using namespace RawrXD::Inference;

class KVCacheOwnershipTest : public ::testing::Test {
protected:
    static constexpr uint32_t kTotalBlocks = 128;
    static constexpr uint64_t kSeqA = 1001;
    static constexpr uint64_t kSeqB = 1002;
    static constexpr uint64_t kSeqC = 1003;

    void SetUp() override {
        tracker = std::make_unique<KVCacheOwnershipTracker>(kTotalBlocks);
    }

    std::unique_ptr<KVCacheOwnershipTracker> tracker;
};

// ---------------------------------------------------------------------------
// Basic allocation
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, AllocateBlock_Success) {
    EXPECT_TRUE(tracker->allocateBlock(0, kSeqA));
    EXPECT_TRUE(tracker->isOwnedBy(0, kSeqA));
    EXPECT_EQ(tracker->getOwner(0), kSeqA);
    EXPECT_EQ(tracker->getRefCount(0), 1u);
}

TEST_F(KVCacheOwnershipTest, AllocateBlock_DoubleAllocateFails) {
    EXPECT_TRUE(tracker->allocateBlock(5, kSeqA));
    EXPECT_FALSE(tracker->allocateBlock(5, kSeqB)); // Already owned
    EXPECT_EQ(tracker->getOwner(5), kSeqA);
}

TEST_F(KVCacheOwnershipTest, AllocateBlock_OutOfRangeFails) {
    EXPECT_FALSE(tracker->allocateBlock(kTotalBlocks, kSeqA));
    EXPECT_FALSE(tracker->allocateBlock(9999, kSeqA));
}

// ---------------------------------------------------------------------------
// Sharing
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, ShareBlock_IncrementsRefCount) {
    ASSERT_TRUE(tracker->allocateBlock(10, kSeqA));
    EXPECT_TRUE(tracker->shareBlock(10, kSeqB));
    EXPECT_EQ(tracker->getRefCount(10), 2u);
    EXPECT_TRUE(tracker->isOwnedBy(10, kSeqA));
    EXPECT_TRUE(tracker->isOwnedBy(10, kSeqB));
    EXPECT_TRUE(tracker->isShared(10));
}

TEST_F(KVCacheOwnershipTest, ShareBlock_UnallocatedFails) {
    EXPECT_FALSE(tracker->shareBlock(20, kSeqB));
}

TEST_F(KVCacheOwnershipTest, ShareBlock_SameSequenceIdempotent) {
    ASSERT_TRUE(tracker->allocateBlock(15, kSeqA));
    EXPECT_TRUE(tracker->shareBlock(15, kSeqA));
    EXPECT_EQ(tracker->getRefCount(15), 2u);
}

// ---------------------------------------------------------------------------
// Release
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, ReleaseBlock_DecrementsRefCount) {
    ASSERT_TRUE(tracker->allocateBlock(30, kSeqA));
    ASSERT_TRUE(tracker->shareBlock(30, kSeqB));
    EXPECT_EQ(tracker->getRefCount(30), 2u);

    EXPECT_TRUE(tracker->releaseBlock(30, kSeqA));
    EXPECT_EQ(tracker->getRefCount(30), 1u);
    EXPECT_FALSE(tracker->isOwnedBy(30, kSeqA));
    EXPECT_TRUE(tracker->isOwnedBy(30, kSeqB));
}

TEST_F(KVCacheOwnershipTest, ReleaseBlock_LastReleaseFreesBlock) {
    ASSERT_TRUE(tracker->allocateBlock(35, kSeqA));
    EXPECT_TRUE(tracker->releaseBlock(35, kSeqA));
    EXPECT_EQ(tracker->getRefCount(35), 0u);
    EXPECT_FALSE(tracker->isOwnedBy(35, kSeqA));
}

TEST_F(KVCacheOwnershipTest, ReleaseBlock_WrongOwnerFails) {
    ASSERT_TRUE(tracker->allocateBlock(40, kSeqA));
    EXPECT_FALSE(tracker->releaseBlock(40, kSeqB));
    EXPECT_EQ(tracker->getRefCount(40), 1u);
}

// ---------------------------------------------------------------------------
// Transfer
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, TransferBlock_ChangesOwner) {
    ASSERT_TRUE(tracker->allocateBlock(50, kSeqA));
    EXPECT_TRUE(tracker->transferBlock(50, kSeqA, kSeqB));
    EXPECT_FALSE(tracker->isOwnedBy(50, kSeqA));
    EXPECT_TRUE(tracker->isOwnedBy(50, kSeqB));
    EXPECT_EQ(tracker->getOwner(50), kSeqB);
}

TEST_F(KVCacheOwnershipTest, TransferBlock_SharedBlockFails) {
    ASSERT_TRUE(tracker->allocateBlock(55, kSeqA));
    ASSERT_TRUE(tracker->shareBlock(55, kSeqB));
    EXPECT_FALSE(tracker->transferBlock(55, kSeqA, kSeqC));
}

TEST_F(KVCacheOwnershipTest, TransferBlock_WrongFromOwnerFails) {
    ASSERT_TRUE(tracker->allocateBlock(60, kSeqA));
    EXPECT_FALSE(tracker->transferBlock(60, kSeqB, kSeqC));
}

// ---------------------------------------------------------------------------
// Bulk operations
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, AllocateBlocks_Multiple) {
    auto blocks = tracker->allocateBlocks(5, kSeqA);
    EXPECT_EQ(blocks.size(), 5u);
    for (auto b : blocks) {
        EXPECT_TRUE(tracker->isOwnedBy(b, kSeqA));
    }
}

TEST_F(KVCacheOwnershipTest, ReleaseAllForSequence_CleansUp) {
    auto blocks = tracker->allocateBlocks(10, kSeqA);
    tracker->releaseAllForSequence(kSeqA);
    for (auto b : blocks) {
        EXPECT_EQ(tracker->getRefCount(b), 0u);
        EXPECT_FALSE(tracker->isOwnedBy(b, kSeqA));
    }
}

TEST_F(KVCacheOwnershipTest, TransferAllForSequence_MovesAllBlocks) {
    auto blocks = tracker->allocateBlocks(8, kSeqA);
    tracker->transferAllForSequence(kSeqA, kSeqB);
    for (auto b : blocks) {
        EXPECT_TRUE(tracker->isOwnedBy(b, kSeqB));
        EXPECT_FALSE(tracker->isOwnedBy(b, kSeqA));
    }
}

// ---------------------------------------------------------------------------
// Pinning
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, PinBlock_PreventsEviction) {
    ASSERT_TRUE(tracker->allocateBlock(70, kSeqA));
    EXPECT_TRUE(tracker->pinBlock(70));
    EXPECT_TRUE(tracker->isPinned(70));
}

TEST_F(KVCacheOwnershipTest, UnpinBlock_AllowsEviction) {
    ASSERT_TRUE(tracker->allocateBlock(75, kSeqA));
    ASSERT_TRUE(tracker->pinBlock(75));
    EXPECT_TRUE(tracker->unpinBlock(75));
    EXPECT_FALSE(tracker->isPinned(75));
}

TEST_F(KVCacheOwnershipTest, PinBlock_UnallocatedFails) {
    EXPECT_FALSE(tracker->pinBlock(80));
}

// ---------------------------------------------------------------------------
// Query operations
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, GetBlocksForSequence_ReturnsCorrectBlocks) {
    auto blocksA = tracker->allocateBlocks(5, kSeqA);
    auto blocksB = tracker->allocateBlocks(3, kSeqB);

    auto retrievedA = tracker->getBlocksForSequence(kSeqA);
    EXPECT_EQ(retrievedA.size(), 5u);

    auto retrievedB = tracker->getBlocksForSequence(kSeqB);
    EXPECT_EQ(retrievedB.size(), 3u);
}

TEST_F(KVCacheOwnershipTest, GetSequencesForBlock_ReturnsSharers) {
    ASSERT_TRUE(tracker->allocateBlock(90, kSeqA));
    ASSERT_TRUE(tracker->shareBlock(90, kSeqB));
    ASSERT_TRUE(tracker->shareBlock(90, kSeqC));

    auto seqs = tracker->getSequencesForBlock(90);
    EXPECT_EQ(seqs.size(), 3u);
}

// ---------------------------------------------------------------------------
// Eviction
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, EvictBlock_UnpinnedSucceeds) {
    ASSERT_TRUE(tracker->allocateBlock(100, kSeqA));
    EXPECT_TRUE(tracker->evictBlock(100));
    EXPECT_EQ(tracker->getRefCount(100), 0u);
}

TEST_F(KVCacheOwnershipTest, EvictBlock_PinnedFails) {
    ASSERT_TRUE(tracker->allocateBlock(105, kSeqA));
    ASSERT_TRUE(tracker->pinBlock(105));
    EXPECT_FALSE(tracker->evictBlock(105));
    EXPECT_EQ(tracker->getRefCount(105), 1u);
}

TEST_F(KVCacheOwnershipTest, EvictBlock_SharedFails) {
    ASSERT_TRUE(tracker->allocateBlock(110, kSeqA));
    ASSERT_TRUE(tracker->shareBlock(110, kSeqB));
    EXPECT_FALSE(tracker->evictBlock(110));
}

// ---------------------------------------------------------------------------
// Statistics
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, Stats_AccurateAfterOperations) {
    tracker->allocateBlocks(10, kSeqA);
    tracker->allocateBlocks(5, kSeqB);
    tracker->shareBlock(0, kSeqB);
    tracker->pinBlock(1);

    auto stats = tracker->getStats();
    EXPECT_EQ(stats.totalBlocks, kTotalBlocks);
    EXPECT_EQ(stats.allocatedBlocks, 15u);
    EXPECT_EQ(stats.sharedBlocks, 1u);
    EXPECT_EQ(stats.pinnedBlocks, 1u);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, Callback_FiresOnEvents) {
    int eventCount = 0;
    tracker->setCallback([&eventCount](OwnershipEvent, uint32_t, uint64_t) {
        ++eventCount;
    });

    tracker->allocateBlock(120, kSeqA);
    tracker->shareBlock(120, kSeqB);
    tracker->releaseBlock(120, kSeqA);

    EXPECT_EQ(eventCount, 3);
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
