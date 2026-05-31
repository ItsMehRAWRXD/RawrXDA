// ============================================================================
// virtualalloc_reservation_manager.h
// Win32 VirtualAlloc-backed reservation layer for lazy tensor paging
// ============================================================================
// Purpose: Reserve 95GB logical address space without committing RAM
//          Commit only on access (via guardpage faults or explicit paging)
//          Enables ~26.5 GB physical resident set for 2T model inference
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <atomic>
#include <mutex>

namespace RawrXD {
namespace Compression {

// Reservation state tracking
enum class ReservationState : uint8_t {
    Reserved = 0,   // MEM_RESERVE only, not yet committed
    Committing = 1, // In process of MEM_COMMIT
    Committed = 2,  // Fully committed and ready to use
    Failed = 3,     // Commitment failed
};

// Reservation descriptor: tracks a single tensor's address space
struct TensorReservation {
    uint8_t* baseAddress = nullptr;
    uint64_t reservedBytes = 0;
    uint64_t committedBytes = 0;
    std::atomic<ReservationState> state{ReservationState::Reserved};
    uint32_t tensorId = 0;
    bool useLargePages = false;
};

// Global reservation manager: coordinate all tensor mappings
class VirtualAllocReservationManager {
public:
    static VirtualAllocReservationManager& Instance();

    // Reserve a contiguous block (MEM_RESERVE, no actual RAM)
    // Returns descriptor if successful, nullptr if failed
    std::shared_ptr<TensorReservation> ReserveBlock(uint64_t bytes, uint32_t tensorId, bool useLargePages = false);

    // Commit a reserved block on-demand (allocates physical RAM)
    // Returns true if successful
    bool CommitBlock(std::shared_ptr<TensorReservation> reservation, uint64_t offset, uint64_t bytes);

    // Release reservation and free underlying address space
    void ReleaseBlock(std::shared_ptr<TensorReservation> reservation);

    // Check if reservation is accessible (committed)
    bool IsCommitted(const std::shared_ptr<TensorReservation>& reservation, uint64_t offset, uint64_t bytes) const;

    // Get current physical committed size across all reservations
    uint64_t GetTotalCommittedBytes() const;

    // Get total reserved (logical) size
    uint64_t GetTotalReservedBytes() const;

    // Evict least-recently-used tensor to free physical RAM
    // Returns bytes freed, 0 if none available
    uint64_t EvictLRU();

private:
    VirtualAllocReservationManager();
    ~VirtualAllocReservationManager();

    VirtualAllocReservationManager(const VirtualAllocReservationManager&) = delete;
    VirtualAllocReservationManager& operator=(const VirtualAllocReservationManager&) = delete;

    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<TensorReservation>> reservations_;
    std::atomic<uint64_t> totalCommittedBytes_{0};
    std::atomic<uint64_t> totalReservedBytes_{0};
};

// RAII wrapper for automatic cleanup
class ReservationGuard {
public:
    explicit ReservationGuard(std::shared_ptr<TensorReservation> res)
        : reservation_(std::move(res)) {}

    ~ReservationGuard() {
        if (reservation_) {
            VirtualAllocReservationManager::Instance().ReleaseBlock(reservation_);
        }
    }

    TensorReservation* operator->() { return reservation_.get(); }
    const TensorReservation* operator->() const { return reservation_.get(); }
    TensorReservation& operator*() { return *reservation_; }

private:
    std::shared_ptr<TensorReservation> reservation_;
};

} // namespace Compression
} // namespace RawrXD
