#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <string>
#include <mutex>
#include <vector>
#include <deque>

namespace {
constexpr size_t kMaxThoughtIdBytes = 128;
constexpr size_t kMaxThoughtBytes = 16 * 1024;
constexpr size_t kMaxThoughtRecords = 2048;
}

namespace RawrXD::Cognition {

/**
 * @brief Win32IDE_CBT: Cognitive Behavioral Trace
 * Records a trace of agentic 'thought' processes for human debugging 
 * and sovereign audit (Batch 17).
 */
class CBT {
public:
    static CBT& GetInstance() {
        static CBT instance;
        return instance;
    }

    // Resolves: CBT::RecordThought
    void RecordThought(const std::string& thought_id, const std::string& content) {
        if (thought_id.empty() || thought_id.size() > kMaxThoughtIdBytes) {
            LOG_ERROR("[CBT] Rejecting invalid thought id.");
            return;
        }
        if (content.empty() || content.size() > kMaxThoughtBytes) {
            LOG_ERROR("[CBT] Rejecting invalid thought payload.");
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_thoughts.size() >= kMaxThoughtRecords) {
            m_thoughts.pop_front();
        }
        m_thoughts.push_back(thought_id + ":" + content);
        LOG_INFO("[CBT] Recorded thought: " + thought_id);
    }

    // Resolves: CBT::ExportTrace
    std::string ExportTrace() {
        std::lock_guard<std::mutex> lock(m_mutex);

        LOG_INFO("[CBT] Exporting cognitive trace.");
        std::string trace;
        trace.reserve(m_thoughts.size() * 64);
        for (const auto& t : m_thoughts) {
            trace += t + "\n---\n";
        }
        return trace;
    }

private:
    CBT() = default;
    std::mutex m_mutex;
    std::deque<std::string> m_thoughts;
};

} // namespace RawrXD::Cognition

// Linker symbols
extern "C" void CBT_Record(const char* id, const char* thought) {
    if (!id || !thought) {
        return;
    }
    RawrXD::Cognition::CBT::GetInstance().RecordThought(id, thought);
}

extern "C" const char* CBT_Export() {
    static thread_local std::string result;
    result = RawrXD::Cognition::CBT::GetInstance().ExportTrace();
    return result.c_str();
}
