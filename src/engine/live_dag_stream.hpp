#pragma once

#include "replay_core.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

namespace RawrXD::Replay {

class LiveDagStream {
  public:
    LiveDagStream() = default;
    ~LiveDagStream() { Stop(); }

    void Start()
    {
        bool expected = false;
        if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        worker_ = std::thread([this]() { loop(); });
    }

    void Stop()
    {
        bool expected = true;
        if (!running_.compare_exchange_strong(expected, false, std::memory_order_acq_rel))
        {
            return;
        }

        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    ReplayGraph Snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return graph_;
    }

    std::size_t EventCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_.size();
    }

  private:
    void loop()
    {
        while (running_.load(std::memory_order_acquire))
        {
            RawrXD::Trace::TraceEvent ev{};
            bool consumed_any = false;

            while (RawrXD::Trace::TraceBus::Pop(ev))
            {
                consumed_any = true;
                applyEvent(ev);
            }

            if (!consumed_any)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }
    }

    void applyEvent(const RawrXD::Trace::TraceEvent& e)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.push_back(e);
        graph_ = ReplayCore::Build(events_);
    }

    std::atomic<bool> running_{false};
    mutable std::mutex mutex_;
    std::vector<RawrXD::Trace::TraceEvent> events_;
    ReplayGraph graph_;
    std::thread worker_;
};

}  // namespace RawrXD::Replay
