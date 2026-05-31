#pragma once

#include <chrono>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>

namespace RawrXD::Testing::Chaos {

enum class FailureMode {
    None = 0,
    Delay,
    Drop,
    Exception,
    Corruption,
    OOM,
    Partition,
};

class FailureInjector {
public:
    struct Policy {
        FailureMode mode = FailureMode::None;
        double probability = 0.0;
        std::chrono::milliseconds delay{0};
    };

    explicit FailureInjector(uint64_t seed = 42)
        : rng_(seed) {
    }

    void setPolicy(const Policy& p) { policy_ = p; }
    Policy policy() const { return policy_; }

    bool shouldDrop() {
        return shouldFail() && policy_.mode == FailureMode::Drop;
    }

    template <typename F>
    auto execute(F&& func) -> decltype(func()) {
        if (shouldFail()) {
            injectFailure();
        }

        if (policy_.mode == FailureMode::Delay && policy_.delay.count() > 0) {
            std::this_thread::sleep_for(policy_.delay);
        }

        return func();
    }

    std::string corruptString(const std::string& input) {
        if (input.empty()) {
            return input;
        }
        std::string out = input;
        size_t idx = static_cast<size_t>(rng_() % out.size());
        out[idx] = static_cast<char>(rng_() & 0x7F);
        return out;
    }

private:
    bool shouldFail() {
        if (policy_.mode == FailureMode::None) {
            return false;
        }
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng_) < policy_.probability;
    }

    void injectFailure() {
        switch (policy_.mode) {
            case FailureMode::Exception:
                throw std::runtime_error("Injected failure");
            case FailureMode::OOM:
                throw std::bad_alloc();
            case FailureMode::Partition:
                throw std::runtime_error("Injected partition");
            default:
                break;
        }
    }

private:
    Policy policy_;
    std::mt19937_64 rng_;
};

class ScopedFailure {
public:
    ScopedFailure(FailureInjector& injector, const FailureInjector::Policy& policy)
        : injector_(injector), prevPolicy_(injector.policy()) {
        injector_.setPolicy(policy);
    }

    ~ScopedFailure() {
        injector_.setPolicy(prevPolicy_);
    }

    ScopedFailure(const ScopedFailure&) = delete;
    ScopedFailure& operator=(const ScopedFailure&) = delete;

private:
    FailureInjector& injector_;
    FailureInjector::Policy prevPolicy_;
};

} // namespace RawrXD::Testing::Chaos
