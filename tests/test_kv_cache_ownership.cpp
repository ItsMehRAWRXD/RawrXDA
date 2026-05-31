// ============================================================================
// test_kv_cache_ownership.cpp — Unit tests for KVCacheOwnershipTracker
// ============================================================================
// Uses RawrXD's existing GoogleTest v1.14.0 infrastructure
// Build: cmake --build . --target test_kv_cache_ownership
// Run:   ctest -R test_kv_cache_ownership -V
// ============================================================================

#include <gtest/gtest.h>
#include "inference/kv_cache_ownership.h"

using namespace RawrXD::Inference;

class KVCacheOwnershipTest : public ::testing::Test {
protected:
    static constexpr uint32_t TOTAL_BLOCKS = 128;
    static constexpr uint64_t SEQ_A = 1001;
    static constexpr uint64_t SEQ_B = 1002;
    static constexpr uint64_t SEQ_C = 1003;

    void SetUp() override {
        tracker = std::make_unique<KVCacheOwnershipTracker>(TOTAL_BLOCKS);
    }

    void TearDown() override {
        tracker.reset();
    }

    std::unique_ptr<KVCacheOwnershipTracker> tracker;
};

// ---------------------------------------------------------------------------
// Basic allocation
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, AllocateSingleBlock) {
    EXPECT_TRUE(tracker->allocateBlock(0, SEQ_A));
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_A));
    EXPECT_EQ(tracker->getOwner(0), SEQ_A);
    EXPECT_EQ(tracker->getRefCount(0), 1u);
}

TEST_F(KVCacheOwnershipTest, AllocateMultipleBlocks) {
    auto blocks = tracker->allocateBlocks(5, SEQ_A);
    EXPECT_EQ(blocks.size(), 5u);
    for (auto idx : blocks) {
        EXPECT_TRUE(tracker->isOwnedBy(idx, SEQ_A));
    }
}

TEST_F(KVCacheOwnershipTest, AllocateFailsForInvalidBlock) {
    EXPECT_FALSE(tracker->allocateBlock(TOTAL_BLOCKS, SEQ_A)); // out of range
}

TEST_F(KVCacheOwnershipTest, DoubleAllocateFails) {
    EXPECT_TRUE(tracker->allocateBlock(0, SEQ_A));
    EXPECT_FALSE(tracker->allocateBlock(0, SEQ_B)); // already owned
}

// ---------------------------------------------------------------------------
// Sharing
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, ShareBlockIncrementsRefCount) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_TRUE(tracker->shareBlock(0, SEQ_B));
    EXPECT_EQ(tracker->getRefCount(0), 2u);
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_A));
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_B));
}

TEST_F(KVCacheOwnershipTest, ShareNonExistentBlockFails) {
    EXPECT_FALSE(tracker->shareBlock(0, SEQ_A)); // not allocated
}

// ---------------------------------------------------------------------------
// Release
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, ReleaseDecrementsRefCount) {
    tracker->allocateBlock(0, SEQ_A);
    tracker->shareBlock(0, SEQ_B);
    EXPECT_TRUE(tracker->releaseBlock(0, SEQ_A));
    EXPECT_EQ(tracker->getRefCount(0), 1u);
    EXPECT_FALSE(tracker->isOwnedBy(0, SEQ_A));
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_B));
}

TEST_F(KVCacheOwnershipTest, ReleaseLastOwnerFreesBlock) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_TRUE(tracker->releaseBlock(0, SEQ_A));
    EXPECT_EQ(tracker->getRefCount(0), 0u);
    EXPECT_FALSE(tracker->isOwnedBy(0, SEQ_A));
}

TEST_F(KVCacheOwnershipTest, ReleaseWrongOwnerFails) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_FALSE(tracker->releaseBlock(0, SEQ_B)); // wrong owner
}

// ---------------------------------------------------------------------------
// Transfer
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, TransferBlockOwnership) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_TRUE(tracker->transferBlock(0, SEQ_A, SEQ_B));
    EXPECT_FALSE(tracker->isOwnedBy(0, SEQ_A));
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_B));
    EXPECT_EQ(tracker->getOwner(0), SEQ_B);
}

TEST_F(KVCacheOwnershipTest, TransferSharedBlock) {
    tracker->allocateBlock(0, SEQ_A);
    tracker->shareBlock(0, SEQ_B);
    EXPECT_TRUE(tracker->transferBlock(0, SEQ_A, SEQ_C));
    EXPECT_EQ(tracker->getRefCount(0), 2u); // B and C
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_B));
    EXPECT_TRUE(tracker->isOwnedBy(0, SEQ_C));
}

// ---------------------------------------------------------------------------
// Bulk operations
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, ReleaseAllForSequence) {
    auto blocks = tracker->allocateBlocks(5, SEQ_A);
    tracker->releaseAllForSequence(SEQ_A);
    for (auto idx : blocks) {
        EXPECT_EQ(tracker->getRefCount(idx), 0u);
    }
}

TEST_F(KVCacheOwnershipTest, TransferAllForSequence) {
    auto blocks = tracker->allocateBlocks(5, SEQ_A);
    tracker->transferAllForSequence(SEQ_A, SEQ_B);
    for (auto idx : blocks) {
        EXPECT_TRUE(tracker->isOwnedBy(idx, SEQ_B));
        EXPECT_FALSE(tracker->isOwnedBy(idx, SEQ_A));
    }
}

// ---------------------------------------------------------------------------
// Pinning
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, PinBlock) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_TRUE(tracker->pinBlock(0));
    EXPECT_TRUE(tracker->isPinned(0));
}

TEST_F(KVCacheOwnershipTest, EvictRespectsPinnedBlocks) {
    tracker->allocateBlock(0, SEQ_A);
    tracker->pinBlock(0);
    EXPECT_FALSE(tracker->evictBlock(0)); // pinned, can't evict
}

TEST_F(KVCacheOwnershipTest, EvictUnpinnedBlock) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_TRUE(tracker->evictBlock(0));
    EXPECT_EQ(tracker->getRefCount(0), 0u);
}

// ---------------------------------------------------------------------------
// Query operations
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, GetBlocksForSequence) {
    auto blocks = tracker->allocateBlocks(5, SEQ_A);
    auto retrieved = tracker->getBlocksForSequence(SEQ_A);
    EXPECT_EQ(retrieved.size(), 5u);
    for (size_t i = 0; i < blocks.size(); ++i) {
        EXPECT_EQ(retrieved[i], blocks[i]);
    }
}

TEST_F(KVCacheOwnershipTest, GetSequencesForBlock) {
    tracker->allocateBlock(0, SEQ_A);
    tracker->shareBlock(0, SEQ_B);
    auto seqs = tracker->getSequencesForBlock(0);
    EXPECT_EQ(seqs.size(), 2u);
}

TEST_F(KVCacheOwnershipTest, IsShared) {
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_FALSE(tracker->isShared(0));
    tracker->shareBlock(0, SEQ_B);
    EXPECT_TRUE(tracker->isShared(0));
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, CallbackFiresOnAllocate) {
    bool fired = false;
    tracker->setCallback([&](OwnershipEvent event, uint32_t blockIdx, uint64_t seqId) {
        if (event == OwnershipEvent::Allocated && blockIdx == 0 && seqId == SEQ_A) {
            fired = true;
        }
    });
    tracker->allocateBlock(0, SEQ_A);
    EXPECT_TRUE(fired);
}

TEST_F(KVCacheOwnershipTest, CallbackFiresOnRelease) {
    bool fired = false;
    tracker->setCallback([&](OwnershipEvent event, uint32_t, uint64_t) {
        if (event == OwnershipEvent::Released) fired = true;
    });
    tracker->allocateBlock(0, SEQ_A);
    tracker->releaseBlock(0, SEQ_A);
    EXPECT_TRUE(fired);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
TEST_F(KVCacheOwnershipTest, StatsTrackAllocations) {
    tracker->allocateBlocks(10, SEQ_A);
    auto stats = tracker->getStats();
    EXPECT_EQ(stats.totalBlocks, TOTAL_BLOCKS);
    EXPECT_EQ(stats.allocatedBlocks, 10u);
    EXPECT_EQ(stats.freeBlocks, TOTAL_BLOCKS - 10);
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
