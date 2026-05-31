#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>
#include <mutex>
#include <string>
#include <queue>
#include <vector>
#include <unordered_map>

namespace {
constexpr size_t kMaxAddressBytes = 128;
constexpr size_t kMaxMessageBytes = 64 * 1024;
constexpr size_t kMaxQueueDepth = 512;
}

namespace RawrXD::Messaging {

/**
 * @brief Win32IDE_Mailbox: Secure Cross-Agent Messaging
 * This provides the shared memory 'Mailbox' for the swarm agents.
 * It's part of the 'Nervous System' for agent coordination.
 */
class Mailbox {
public:
    static Mailbox& GetInstance() {
        static Mailbox instance;
        return instance;
    }

    // Resolves: Mailbox::Push
    void Push(const std::string& address, const std::string& message) {
        if (address.empty() || address.size() > kMaxAddressBytes) {
            LOG_ERROR("[Mailbox] Rejecting invalid address.");
            return;
        }
        if (message.size() > kMaxMessageBytes) {
            LOG_ERROR("[Mailbox] Rejecting oversized message.");
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto& q = m_messages[address];
        if (q.size() >= kMaxQueueDepth) {
            q.pop();
        }
        q.push(message);
        LOG_INFO("[Mailbox] Pushed message into mailbox " + address);
    }

    // Resolves: Mailbox::PopSecure
    std::string PopSecure(const std::string& address, const uint8_t* pqc_sig) {
        if (address.empty() || address.size() > kMaxAddressBytes) {
            return "";
        }

        // Minimal fail-closed gate: null signatures are treated as unauthorized.
        if (!pqc_sig) {
            LOG_ERROR("[Mailbox] Rejecting pop request without signature.");
            return "";
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_messages.find(address);
        if (it == m_messages.end() || it->second.empty()) {
            return "";
        }

        std::string msg = it->second.front();
        it->second.pop();
        LOG_INFO("[Mailbox] Popped message from mailbox " + address);
        return msg;
    }

private:
    Mailbox() = default;
    std::mutex m_mutex;
    std::unordered_map<std::string, std::queue<std::string>> m_messages;
};

} // namespace RawrXD::Messaging

// Linker symbols
extern "C" void Mailbox_Push(const char* addr, const char* msg) {
    if (!addr || !msg) {
        return;
    }
    RawrXD::Messaging::Mailbox::GetInstance().Push(addr, msg);
}

extern "C" const char* Mailbox_Pop(const char* addr, const uint8_t* sig) {
    static thread_local std::string result;
    if (!addr) {
        result.clear();
        return result.c_str();
    }
    result = RawrXD::Messaging::Mailbox::GetInstance().PopSecure(addr, sig);
    return result.c_str();
}
