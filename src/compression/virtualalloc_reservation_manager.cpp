// ============================================================================
// virtualalloc_reservation_manager.cpp
// Win32 VirtualAlloc-backed reservation layer implementation
// ============================================================================

#include "compression/virtualalloc_reservation_manager.h"

#include <windows.h>
#include <algorithm>
#include <cstdlib>

namespace RawrXD {
namespace Compression {

VirtualAllocReservationManager& VirtualAllocReservationManager::Instance() {
    static VirtualAllocReservationManager instance;
    return instance;
}

VirtualAllocReservationManager::VirtualAllocReservationManager() = default;

VirtualAllocReservationManager::~VirtualAllocReservationManager() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& res : reservations_) {
        if (res && res->baseAddress) {
            VirtualFree(res->baseAddress, 0, MEM_RELEASE);
            res->baseAddress = nullptr;
            res->committedBytes = 0;
        }
    }
    reservations_.clear();
}

std::shared_ptr<TensorReservation> VirtualAllocReservationManager::ReserveBlock(
    uint64_t bytes, uint32_t tensorId, bool useLargePages) {
    if (bytes == 0) return nullptr;

    // Large page alignment (typically 2 MB)
    uint64_t alignedBytes = bytes;
    if (useLargePages) {
        // Query large page minimum size
        SIZE_T largePageMin = GetLargePageMinimum();
        if (largePageMin > 0) {
            alignedBytes = ((bytes + largePageMin - 1) / largePageMin) * largePageMin;
        }
    }

    // Reserve without committing
    DWORD allocFlags = MEM_RESERVE;
    if (useLargePages) {
        allocFlags |= MEM_LARGE_PAGES;
    }

    void* addr = VirtualAlloc(nullptr, alignedBytes, allocFlags, PAGE_NOACCESS);
    if (!addr) {
        return nullptr;
    }

    auto reservation = std::make_shared<TensorReservation>();
    reservation->baseAddress = static_cast<uint8_t*>(addr);
    reservation->reservedBytes = alignedBytes;
    reservation->committedBytes = 0;
    reservation->tensorId = tensorId;
    reservation->useLargePages = useLargePages;
    reservation->state.store(ReservationState::Reserved, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        reservations_.push_back(reservation);
        totalReservedBytes_.fetch_add(alignedBytes, std::memory_order_acq_rel);
    }

    return reservation;
}

bool VirtualAllocReservationManager::CommitBlock(
    std::shared_ptr<TensorReservation> reservation, uint64_t offset, uint64_t bytes) {
    if (!reservation || !reservation->baseAddress || offset + bytes > reservation->reservedBytes) {
        return false;
    }

    ReservationState expected = ReservationState::Reserved;
    if (!reservation->state.compare_exchange_strong(expected, ReservationState::Committing,
                                                    std::memory_order_acq_rel)) {
        return reservation->state.load(std::memory_order_acquire) == ReservationState::Committed;
    }

    // Commit the range with PAGE_READWRITE
    void* commitAddr = VirtualAlloc(
        reservation->baseAddress + offset,
        bytes,
        MEM_COMMIT,
        PAGE_READWRITE);

    if (!commitAddr) {
        reservation->state.store(ReservationState::Failed, std::memory_order_release);
        return false;
    }

    uint64_t prevCommitted = reservation->committedBytes;
    reservation->committedBytes = std::max(prevCommitted, offset + bytes);
    totalCommittedBytes_.fetch_add(bytes, std::memory_order_acq_rel);

    reservation->state.store(ReservationState::Committed, std::memory_order_release);
    return true;
}

void VirtualAllocReservationManager::ReleaseBlock(std::shared_ptr<TensorReservation> reservation) {
    if (!reservation || !reservation->baseAddress) {
        return;
    }

    uint64_t committedToFree = reservation->committedBytes;
    if (committedToFree > 0) {
        totalCommittedBytes_.fetch_sub(committedToFree, std::memory_order_acq_rel);
    }

    uint64_t reservedToFree = reservation->reservedBytes;
    if (VirtualFree(reservation->baseAddress, 0, MEM_RELEASE)) {
        totalReservedBytes_.fetch_sub(reservedToFree, std::memory_order_acq_rel);
        reservation->baseAddress = nullptr;
        reservation->committedBytes = 0;
        reservation->reservedBytes = 0;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find(reservations_.begin(), reservations_.end(), reservation);
        if (it != reservations_.end()) {
            reservations_.erase(it);
        }
    }
}

bool VirtualAllocReservationManager::IsCommitted(
    const std::shared_ptr<TensorReservation>& reservation, uint64_t offset, uint64_t bytes) const {
    if (!reservation) return false;
    return reservation->state.load(std::memory_order_acquire) == ReservationState::Committed &&
           offset + bytes <= reservation->committedBytes;
}

uint64_t VirtualAllocReservationManager::GetTotalCommittedBytes() const {
    return totalCommittedBytes_.load(std::memory_order_acquire);
}

uint64_t VirtualAllocReservationManager::GetTotalReservedBytes() const {
    return totalReservedBytes_.load(std::memory_order_acquire);
}

uint64_t VirtualAllocReservationManager::EvictLRU() {
    // Simplified: evict first releasable reservation
    // In production, track access times and evict least-recently-used
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = reservations_.begin(); it != reservations_.end(); ++it) {
        auto& res = *it;
        if (res && res->committedBytes > 0) {
            uint64_t freed = res->committedBytes;
            ReleaseBlock(res);
            return freed;
        }
    }
    return 0;
}

} // namespace Compression
} // namespace RawrXD
