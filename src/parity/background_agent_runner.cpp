// background_agent_runner.cpp - Implementation
#include "background_agent_runner.h"

#include <algorithm>
#include <sstream>

namespace rawrxd::parity {

BackgroundAgentRunner::BackgroundAgentRunner(std::uint32_t workers) {
    if (workers == 0) workers = 1;
    if (workers > 16) workers = 16;
    workers_.reserve(workers);
    for (std::uint32_t i = 0; i < workers; ++i)
        workers_.emplace_back(&BackgroundAgentRunner::worker_loop, this);
}

BackgroundAgentRunner::~BackgroundAgentRunner() { shutdown(); }

void BackgroundAgentRunner::set_runner(BgAgentRunFn fn) {
    std::lock_guard lk(mu_);
    runner_ = std::move(fn);
}

std::string BackgroundAgentRunner::submit(BgAgentJob job) {
    std::unique_lock lk(mu_);
    if (job.id.empty()) {
        std::ostringstream os; os << "bg-" << seq_++;
        job.id = os.str();
    }
    InternalJob ij;
    ij.job = std::move(job);
    ij.prog.id = ij.job.id;
    ij.prog.status = BgAgentStatus::Queued;
    ij.prog.queued_at = std::chrono::steady_clock::now();
    ij.cancel = std::make_shared<std::atomic<bool>>(false);
    std::string id = ij.prog.id;

    // Priority insert.
    auto it = std::find_if(queue_.begin(), queue_.end(), [&](const std::string& qid) {
        auto qit = jobs_.find(qid);
        return qit != jobs_.end() && qit->second.job.priority > ij.job.priority;
    });
    queue_.insert(it, id);
    jobs_.emplace(id, std::move(ij));
    lk.unlock();
    cv_.notify_one();
    return id;
}

bool BackgroundAgentRunner::cancel(const std::string& id) {
    std::lock_guard lk(mu_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) return false;
    if (it->second.prog.status == BgAgentStatus::Completed ||
        it->second.prog.status == BgAgentStatus::Failed    ||
        it->second.prog.status == BgAgentStatus::Cancelled) return false;
    it->second.cancel->store(true, std::memory_order_release);
    if (it->second.prog.status == BgAgentStatus::Queued) {
        it->second.prog.status = BgAgentStatus::Cancelled;
        it->second.prog.finished_at = std::chrono::steady_clock::now();
        auto qit = std::find(queue_.begin(), queue_.end(), id);
        if (qit != queue_.end()) queue_.erase(qit);
    }
    cv_.notify_all();
    return true;
}

BgAgentProgress BackgroundAgentRunner::status(const std::string& id) const {
    std::lock_guard lk(mu_);
    auto it = jobs_.find(id);
    if (it == jobs_.end()) {
        BgAgentProgress p; p.id = id; p.status = BgAgentStatus::Failed; p.error = "not found";
        return p;
    }
    return it->second.prog;
}

std::vector<BgAgentProgress> BackgroundAgentRunner::all_status() const {
    std::lock_guard lk(mu_);
    std::vector<BgAgentProgress> out;
    out.reserve(jobs_.size());
    for (const auto& [_, v] : jobs_) out.push_back(v.prog);
    return out;
}

BgAgentProgress BackgroundAgentRunner::wait(const std::string& id,
                                            std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock lk(mu_);
    cv_.wait_until(lk, deadline, [&] {
        auto it = jobs_.find(id);
        if (it == jobs_.end()) return true;
        auto s = it->second.prog.status;
        return s == BgAgentStatus::Completed || s == BgAgentStatus::Failed ||
               s == BgAgentStatus::Cancelled;
    });
    auto it = jobs_.find(id);
    if (it == jobs_.end()) { BgAgentProgress p; p.id = id; p.status = BgAgentStatus::Failed; p.error = "not found"; return p; }
    return it->second.prog;
}

void BackgroundAgentRunner::shutdown() {
    {
        std::lock_guard lk(mu_);
        if (stop_.exchange(true)) return;
    }
    cv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    workers_.clear();
}

void BackgroundAgentRunner::worker_loop() {
    while (true) {
        std::string id;
        BgAgentJob job;
        std::shared_ptr<std::atomic<bool>> cancel_flag;
        BgAgentRunFn runner;

        {
            std::unique_lock lk(mu_);
            cv_.wait(lk, [&] { return stop_.load() || !queue_.empty(); });
            if (stop_.load() && queue_.empty()) return;
            id = queue_.front(); queue_.pop_front();
            auto it = jobs_.find(id);
            if (it == jobs_.end()) continue;
            if (it->second.prog.status != BgAgentStatus::Queued) continue;
            it->second.prog.status = BgAgentStatus::Running;
            it->second.prog.started_at = std::chrono::steady_clock::now();
            job = it->second.job;
            cancel_flag = it->second.cancel;
            runner = runner_;
        }

        if (!runner) {
            std::lock_guard lk(mu_);
            auto it = jobs_.find(id);
            if (it != jobs_.end()) {
                it->second.prog.status = BgAgentStatus::Failed;
                it->second.prog.error = "no runner attached";
                it->second.prog.finished_at = std::chrono::steady_clock::now();
            }
            cv_.notify_all();
            continue;
        }

        auto log_cb = [this, id](const std::string& line) {
            std::lock_guard lk(mu_);
            auto it = jobs_.find(id);
            if (it == jobs_.end()) return;
            it->second.prog.iterations += 1;
            it->second.prog.last_log = line;
        };

        BgAgentStatus final_status = BgAgentStatus::Completed;
        std::string err;
        try {
            runner(job, *cancel_flag, log_cb);
            if (cancel_flag->load()) final_status = BgAgentStatus::Cancelled;
        } catch (const std::exception& e) {
            final_status = BgAgentStatus::Failed; err = e.what();
        } catch (...) {
            final_status = BgAgentStatus::Failed; err = "unknown exception";
        }

        {
            std::lock_guard lk(mu_);
            auto it = jobs_.find(id);
            if (it != jobs_.end()) {
                it->second.prog.status = final_status;
                it->second.prog.error = err;
                it->second.prog.finished_at = std::chrono::steady_clock::now();
            }
        }
        cv_.notify_all();
    }
}

} // namespace rawrxd::parity
