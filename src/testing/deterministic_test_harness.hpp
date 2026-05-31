#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "../core/state_subscription_engine.hpp"

namespace RawrXD::Testing {

class DeterministicClock {
public:
    using TimePoint = uint64_t;

    TimePoint now() const noexcept { return currentTime_; }
    void advance(TimePoint delta) noexcept { currentTime_ += delta; }
    void set(TimePoint t) noexcept { currentTime_ = t; }

private:
    TimePoint currentTime_{0};
};

class DeterministicRNG {
public:
    explicit DeterministicRNG(uint64_t seed = 42)
        : gen_(seed) {
    }

    template <typename T>
    T randomInt(T min, T max) {
        std::uniform_int_distribution<T> dist(min, max);
        return dist(gen_);
    }

    bool probability(double p) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(gen_) < p;
    }

private:
    std::mt19937_64 gen_;
};

class DeterministicScheduler {
public:
    using Task = std::function<void()>;

    void scheduleAt(DeterministicClock::TimePoint when, Task task) {
        queue_.push(ScheduledTask{when, std::move(task), nextId_++});
    }

    void scheduleAfter(DeterministicClock::TimePoint delay, Task task, DeterministicClock& clock) {
        scheduleAt(clock.now() + delay, std::move(task));
    }

    void run(DeterministicClock& clock, DeterministicClock::TimePoint maxTicks = 10000) {
        while (!queue_.empty() && clock.now() <= maxTicks) {
            ScheduledTask top = queue_.top();
            queue_.pop();
            if (top.time > clock.now()) {
                clock.advance(top.time - clock.now());
            }
            if (top.task) {
                top.task();
            }
        }
    }

    bool empty() const noexcept { return queue_.empty(); }
    size_t size() const noexcept { return queue_.size(); }

private:
    struct ScheduledTask {
        DeterministicClock::TimePoint time{};
        Task task;
        uint64_t id = 0;

        bool operator>(const ScheduledTask& other) const noexcept {
            if (time != other.time) {
                return time > other.time;
            }
            return id > other.id;
        }
    };

    std::priority_queue<ScheduledTask, std::vector<ScheduledTask>, std::greater<ScheduledTask>> queue_;
    uint64_t nextId_{1};
};

class StateTraceRecorder {
public:
    struct Entry {
        DeterministicClock::TimePoint time = 0;
        std::string key;
        std::string oldVal;
        std::string newVal;
        uint64_t version = 0;
    };

    void record(const Core::StateEvent& event, DeterministicClock::TimePoint logicalTime) {
        trace_.push_back(Entry{logicalTime, event.key, event.oldValue, event.newValue, event.version});
    }

    bool verifyMonotonic() const {
        if (trace_.empty()) {
            return true;
        }
        for (size_t i = 1; i < trace_.size(); ++i) {
            if (trace_[i].version <= trace_[i - 1].version) {
                return false;
            }
            if (trace_[i].time < trace_[i - 1].time) {
                return false;
            }
        }
        return true;
    }

    const std::vector<Entry>& trace() const noexcept { return trace_; }
    void clear() { trace_.clear(); }

private:
    std::vector<Entry> trace_;
};

} // namespace RawrXD::Testing
