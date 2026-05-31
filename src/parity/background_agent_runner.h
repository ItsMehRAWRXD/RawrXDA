// background_agent_runner.h - Long-running background agent orchestrator
// Feature 8/15 (Cursor parity).
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace rawrxd::parity {

enum class BgAgentStatus { Queued, Running, Completed, Failed, Cancelled };

struct BgAgentJob {
    std::string id;
    std::string name;
    std::string workspace_root;
    std::string prompt;
    std::uint32_t max_iterations{32};
    std::uint32_t priority{5};        // lower value = higher priority
};

struct BgAgentProgress {
    std::string  id;
    BgAgentStatus status{BgAgentStatus::Queued};
    std::uint32_t iterations{0};
    std::string  last_log;
    std::string  error;
    std::chrono::steady_clock::time_point queued_at{};
    std::chrono::steady_clock::time_point started_at{};
    std::chrono::steady_clock::time_point finished_at{};
};

using BgAgentRunFn = std::function<void(const BgAgentJob&, std::atomic<bool>&,
                                        std::function<void(const std::string&)>)>;

class BackgroundAgentRunner {
public:
    explicit BackgroundAgentRunner(std::uint32_t workers = 2);
    ~BackgroundAgentRunner();

    BackgroundAgentRunner(const BackgroundAgentRunner&)            = delete;
    BackgroundAgentRunner& operator=(const BackgroundAgentRunner&) = delete;

    void set_runner(BgAgentRunFn fn);

    // Enqueue; returns the assigned job id (uses job.id if non-empty).
    std::string submit(BgAgentJob job);

    // Signal cancellation to a running job.
    bool cancel(const std::string& id);

    // Snapshot of progress.
    BgAgentProgress status(const std::string& id) const;
    std::vector<BgAgentProgress> all_status() const;

    // Block until job finishes or timeout. Returns final status.
    BgAgentProgress wait(const std::string& id, std::chrono::milliseconds timeout);

    void shutdown();

private:
    void worker_loop();

    struct InternalJob {
        BgAgentJob      job;
        BgAgentProgress prog;
        std::shared_ptr<std::atomic<bool>> cancel;
    };

    mutable std::mutex              mu_;
    std::condition_variable         cv_;
    std::deque<std::string>         queue_;
    std::unordered_map<std::string, InternalJob> jobs_;
    std::vector<std::thread>        workers_;
    BgAgentRunFn                    runner_;
    std::atomic<bool>               stop_{false};
    std::uint64_t                   seq_{1};
};

} // namespace rawrxd::parity
