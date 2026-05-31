// ============================================================================
// PendingEditReviewGate.cpp — Edit review queue implementation
// ============================================================================
#include "PendingEditReviewGate.hpp"
#include <windows.h>
#include <sstream>
#include <algorithm>

namespace RawrXD {

using namespace Review;

PendingEditQueue& PendingEditQueue::instance() {
    static PendingEditQueue s;
    return s;
}

uint64_t PendingEditQueue::enqueue(PendingEdit edit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    edit.id = m_nextId.fetch_add(1);
    edit.created = std::chrono::steady_clock::now();
    edit.state = EditState::Pending;
    m_edits.push_back(std::move(edit));
    return edit.id;
}

bool PendingEditQueue::approve(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_edits) {
        if (e.id == id && e.state == EditState::Pending) {
            e.approved = true;
            e.state = EditState::Approved;
            return true;
        }
    }
    return false;
}

bool PendingEditQueue::decline(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_edits) {
        if (e.id == id && e.state == EditState::Pending) {
            e.state = EditState::Declined;
            return true;
        }
    }
    return false;
}

bool PendingEditQueue::apply(uint64_t id, void* context) {
    PendingEdit edit;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find_if(m_edits.begin(), m_edits.end(),
            [id](const PendingEdit& e) { return e.id == id && e.state == EditState::Approved; });
        if (it == m_edits.end()) return false;
        edit = *it;
        it->state = EditState::Applied;
    }
    
    // The actual application is handled by the caller via context
    // Return true to indicate the edit was marked as applied
    (void)context;  // Context reserved for future use
    return true;
}

void PendingEditQueue::approveAll(EditSource source) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_edits) {
        if (e.state == EditState::Pending && e.source == source) {
            e.approved = true;
            e.state = EditState::Approved;
        }
    }
}

void PendingEditQueue::declineAllOlderThan(std::chrono::seconds age) {
    auto cutoff = std::chrono::steady_clock::now() - age;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_edits) {
        if (e.state == EditState::Pending && e.created < cutoff) {
            e.state = EditState::Declined;
        }
    }
}

void PendingEditQueue::markStale(const std::string& file) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_edits) {
        if (e.state == EditState::Pending && e.file == file) {
            e.state = EditState::Stale;
        }
    }
}

void PendingEditQueue::markStaleOverlapping(const EditRange& range) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& e : m_edits) {
        if (e.state == EditState::Pending) {
            // Simple overlap check
            if (e.oldRange.startLine <= range.endLine && e.oldRange.endLine >= range.startLine) {
                e.state = EditState::Stale;
            }
        }
    }
}

std::vector<PendingEdit> PendingEditQueue::getPending() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PendingEdit> result;
    for (const auto& e : m_edits) {
        if (e.state == EditState::Pending) {
            result.push_back(e);
        }
    }
    return result;
}

size_t PendingEditQueue::pendingCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::count_if(m_edits.begin(), m_edits.end(),
        [](const PendingEdit& e) { return e.state == EditState::Pending; });
}

} // namespace RawrXD
