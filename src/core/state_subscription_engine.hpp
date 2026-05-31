#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RawrXD::Core {

using StateKey = std::string;
using StateValue = std::string;
using Version = uint64_t;
using SubscriberId = uint64_t;

struct StateEvent {
    StateKey key;
    StateValue oldValue;
    StateValue newValue;
    Version version = 0;
    std::chrono::steady_clock::time_point timestamp{};

    bool isNoOp() const noexcept { return oldValue == newValue; }
};

enum class SubscriptionError {
    None = 0,
    QueueFull,
    InvalidCallback,
    SubscriberNotFound,
    ShutdownInProgress,
    LimitExceeded,
};

template <typename T>
struct Result {
    T value{};
    SubscriptionError error = SubscriptionError::None;
    bool ok() const noexcept { return error == SubscriptionError::None; }
};

struct CallbackFailure {
    SubscriberId id = 0;
    StateKey key;
    std::string reason;
    std::chrono::steady_clock::time_point timestamp{};
};

class AtomicStateStore {
public:
    using StateMap = std::unordered_map<StateKey, StateValue>;

    explicit AtomicStateStore(size_t maxHistory = 256)
        : maxHistory_(maxHistory) {
        auto init = std::make_shared<const StateMap>();
        current_.store(init, std::memory_order_release);
    }

    std::optional<StateValue> get(const StateKey& key) const {
        auto snapshot = current_.load(std::memory_order_acquire);
        auto it = snapshot->find(key);
        if (it == snapshot->end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::shared_ptr<const StateMap> snapshot() const {
        return current_.load(std::memory_order_acquire);
    }

    std::pair<bool, StateEvent> set(const StateKey& key, const StateValue& value) {
        std::lock_guard<std::mutex> lock(writeMutex_);

        auto oldSnapshot = current_.load(std::memory_order_acquire);
        auto oldIt = oldSnapshot->find(key);
        StateValue oldValue = oldIt == oldSnapshot->end() ? StateValue{} : oldIt->second;

        if (oldValue == value) {
            return {false, StateEvent{}};
        }

        auto newMutable = std::make_shared<StateMap>(*oldSnapshot);
        (*newMutable)[key] = value;

        auto newConst = std::static_pointer_cast<const StateMap>(newMutable);
        current_.store(newConst, std::memory_order_release);

        Version nextVersion = version_.fetch_add(1, std::memory_order_acq_rel) + 1;

        if (history_.size() >= maxHistory_) {
            history_.pop_front();
        }
        history_.push_back(newConst);

        StateEvent ev;
        ev.key = key;
        ev.oldValue = std::move(oldValue);
        ev.newValue = value;
        ev.version = nextVersion;
        ev.timestamp = std::chrono::steady_clock::now();

        return {true, std::move(ev)};
    }

    Version currentVersion() const noexcept {
        return version_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::shared_ptr<const StateMap>> current_;
    std::atomic<Version> version_{0};
    std::deque<std::shared_ptr<const StateMap>> history_;
    std::mutex writeMutex_;
    size_t maxHistory_;
};

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t capacity)
        : capacity_(capacity == 0 ? 1 : capacity) {
    }

    bool tryPush(T&& item, std::chrono::milliseconds timeout = std::chrono::milliseconds(0)) {
        std::unique_lock<std::mutex> lock(mutex_);
        auto canPush = [this]() {
            return shutdown_ || queue_.size() < capacity_;
        };

        if (!cv_.wait_for(lock, timeout, canPush)) {
            return false;
        }
        if (shutdown_) {
            return false;
        }

        queue_.push(std::move(item));
        cv_.notify_all();
        return true;
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return shutdown_ || !queue_.empty(); });

        if (queue_.empty()) {
            return false;
        }

        item = std::move(queue_.front());
        queue_.pop();
        cv_.notify_all();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
        cv_.notify_all();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t capacity_;
    bool shutdown_{false};
};

class StateSubscriptionEngine {
public:
    struct Config {
        size_t maxAsyncQueue = 10000;
        size_t maxSubscribersPerKey = 1000;
        size_t workerThreads = 1;
        std::chrono::milliseconds shutdownTimeout{5000};
        size_t maxFailureLog = 2048;
    };

    struct Subscriber {
        std::function<void(const StateEvent&)> callback;
        bool async = true;
        std::optional<std::chrono::milliseconds> timeout = std::nullopt;
    };

    explicit StateSubscriptionEngine(const Config& config = Config{})
        : config_(config),
          eventQueue_(config.maxAsyncQueue == 0 ? 1 : config.maxAsyncQueue),
          shutdownFlag_(false) {
        const size_t workers = config_.workerThreads == 0 ? 1 : config_.workerThreads;
        workers_.reserve(workers);
        for (size_t i = 0; i < workers; ++i) {
            workers_.emplace_back([this]() { workerLoop(); });
        }
    }

    ~StateSubscriptionEngine() {
        shutdown();
    }

    StateSubscriptionEngine(const StateSubscriptionEngine&) = delete;
    StateSubscriptionEngine& operator=(const StateSubscriptionEngine&) = delete;

    Result<SubscriberId> subscribe(const StateKey& key, const Subscriber& sub) {
        if (!sub.callback) {
            return Result<SubscriberId>{0, SubscriptionError::InvalidCallback};
        }
        if (shutdownFlag_.load(std::memory_order_acquire)) {
            return Result<SubscriberId>{0, SubscriptionError::ShutdownInProgress};
        }

        std::unique_lock<std::shared_mutex> lock(subsMutex_);
        auto& slot = subscribers_[key];
        if (slot.size() >= config_.maxSubscribersPerKey) {
            return Result<SubscriberId>{0, SubscriptionError::LimitExceeded};
        }
        SubscriberId id = nextId_.fetch_add(1, std::memory_order_acq_rel);
        slot.emplace(id, sub);
        idToKey_[id] = key;
        return Result<SubscriberId>{id, SubscriptionError::None};
    }

    SubscriptionError unsubscribe(SubscriberId id) {
        std::unique_lock<std::shared_mutex> lock(subsMutex_);
        auto keyIt = idToKey_.find(id);
        if (keyIt == idToKey_.end()) {
            return SubscriptionError::SubscriberNotFound;
        }

        auto subsIt = subscribers_.find(keyIt->second);
        if (subsIt == subscribers_.end()) {
            idToKey_.erase(keyIt);
            return SubscriptionError::SubscriberNotFound;
        }

        auto erased = subsIt->second.erase(id);
        idToKey_.erase(keyIt);

        if (subsIt->second.empty()) {
            subscribers_.erase(subsIt);
        }

        return erased > 0 ? SubscriptionError::None : SubscriptionError::SubscriberNotFound;
    }

    Result<Version> set(const StateKey& key, const StateValue& value) {
        if (shutdownFlag_.load(std::memory_order_acquire)) {
            return Result<Version>{store_.currentVersion(), SubscriptionError::ShutdownInProgress};
        }

        auto [changed, ev] = store_.set(key, value);
        if (!changed) {
            return Result<Version>{store_.currentVersion(), SubscriptionError::None};
        }

        dispatchEvent(ev);
        return Result<Version>{ev.version, SubscriptionError::None};
    }

    std::optional<StateValue> get(const StateKey& key) const {
        return store_.get(key);
    }

    std::pair<std::optional<StateValue>, Version> getConsistent(const StateKey& key) const {
        return {store_.get(key), store_.currentVersion()};
    }

    bool waitForIdle(std::chrono::milliseconds timeout = std::chrono::milliseconds(1000)) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (eventQueue_.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                if (eventQueue_.empty()) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return false;
    }

    void shutdown() {
        bool expected = false;
        if (!shutdownFlag_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
            return;
        }

        eventQueue_.shutdown();

        for (auto& t : workers_) {
            if (t.joinable()) {
                t.join();
            }
        }
        workers_.clear();

        std::unique_lock<std::shared_mutex> lock(subsMutex_);
        subscribers_.clear();
        idToKey_.clear();
    }

    std::vector<CallbackFailure> recentFailures() const {
        std::lock_guard<std::mutex> lock(failureMutex_);
        return std::vector<CallbackFailure>(failures_.begin(), failures_.end());
    }

    size_t subscriberCount(const StateKey& key) const {
        std::shared_lock<std::shared_mutex> lock(subsMutex_);
        auto it = subscribers_.find(key);
        if (it == subscribers_.end()) {
            return 0;
        }
        return it->second.size();
    }

private:
    struct QueueItem {
        SubscriberId id = 0;
        StateKey key;
        Subscriber sub;
        StateEvent event;
    };

    void dispatchEvent(const StateEvent& event) {
        std::vector<QueueItem> items;
        {
            std::shared_lock<std::shared_mutex> lock(subsMutex_);
            auto it = subscribers_.find(event.key);
            if (it == subscribers_.end()) {
                return;
            }
            items.reserve(it->second.size());
            for (const auto& entry : it->second) {
                QueueItem qi;
                qi.id = entry.first;
                qi.key = event.key;
                qi.sub = entry.second;
                qi.event = event;
                items.push_back(std::move(qi));
            }
        }

        for (auto& item : items) {
            if (item.sub.async) {
                bool pushed = eventQueue_.tryPush(std::move(item), std::chrono::milliseconds(0));
                if (!pushed) {
                    recordFailure(item.id, item.key, "async queue full");
                }
            } else {
                executeWithIsolation(item.id, item.key, item.sub, item.event);
            }
        }
    }

    void executeWithIsolation(SubscriberId id, const StateKey& key, const Subscriber& sub, const StateEvent& event) {
        try {
            if (sub.timeout.has_value()) {
                auto fut = std::async(std::launch::async, [&sub, &event]() {
                    sub.callback(event);
                });
                if (fut.wait_for(*sub.timeout) == std::future_status::timeout) {
                    recordFailure(id, key, "callback timeout");
                    return;
                }
                fut.get();
            } else {
                sub.callback(event);
            }
        } catch (const std::exception& ex) {
            recordFailure(id, key, std::string("callback exception: ") + ex.what());
        } catch (...) {
            recordFailure(id, key, "callback exception: unknown");
        }
    }

    void workerLoop() {
        while (!shutdownFlag_.load(std::memory_order_acquire)) {
            QueueItem item;
            if (!eventQueue_.pop(item)) {
                break;
            }
            executeWithIsolation(item.id, item.key, item.sub, item.event);
        }
    }

    void recordFailure(SubscriberId id, const StateKey& key, std::string reason) {
        std::lock_guard<std::mutex> lock(failureMutex_);
        if (failures_.size() >= config_.maxFailureLog) {
            failures_.pop_front();
        }
        CallbackFailure f;
        f.id = id;
        f.key = key;
        f.reason = std::move(reason);
        f.timestamp = std::chrono::steady_clock::now();
        failures_.push_back(std::move(f));
    }

private:
    Config config_;
    AtomicStateStore store_;

    std::unordered_map<StateKey, std::unordered_map<SubscriberId, Subscriber>> subscribers_;
    std::unordered_map<SubscriberId, StateKey> idToKey_;
    mutable std::shared_mutex subsMutex_;

    std::atomic<SubscriberId> nextId_{1};
    BoundedQueue<QueueItem> eventQueue_;
    std::vector<std::thread> workers_;
    std::atomic<bool> shutdownFlag_;

    mutable std::mutex failureMutex_;
    std::deque<CallbackFailure> failures_;
};

} // namespace RawrXD::Core
