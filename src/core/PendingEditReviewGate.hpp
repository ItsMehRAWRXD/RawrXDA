// ============================================================================
// PendingEditReviewGate.hpp — Tool-driven edit review queue
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

// Include the shared Review namespace definitions
#include "../win32app/PendingEditReview.h"

namespace RawrXD {

namespace Review {

struct EditRange {
    uint32_t startLine = 0;
    uint32_t startCol = 0;
    uint32_t endLine = 0;
    uint32_t endCol = 0;
};

struct PendingEdit {
    uint64_t id = 0;
    EditType type = EditType::Replace;
    std::string file;
    EditRange oldRange;
    std::string oldText;
    std::string newText;
    std::string diffPreview;
    EditSource source = EditSource::Unknown;
    std::chrono::steady_clock::time_point created;
    EditState state = EditState::Pending;
    bool approved = false;
};

class PendingEditQueue {
public:
    static PendingEditQueue& instance();
    
    uint64_t enqueue(PendingEdit edit);
    bool approve(uint64_t id);
    bool decline(uint64_t id);
    bool apply(uint64_t id, void* context = nullptr);
    
    void approveAll(EditSource source);
    void declineAllOlderThan(std::chrono::seconds age);
    void markStale(const std::string& file);
    void markStaleOverlapping(const EditRange& range);
    
    std::vector<PendingEdit> getPending() const;
    size_t pendingCount() const;
    
private:
    mutable std::mutex m_mutex;
    std::vector<PendingEdit> m_edits;
    std::atomic<uint64_t> m_nextId{1};
};

} // namespace Review

} // namespace RawrXD
