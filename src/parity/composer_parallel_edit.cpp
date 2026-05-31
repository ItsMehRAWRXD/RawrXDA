// composer_parallel_edit.cpp - Implementation
#include "composer_parallel_edit.h"

#include <algorithm>
#include <chrono>

namespace rawrxd::parity {

void ComposerParallelEdit::set_worker(FileEditWorkerFn fn) {
    std::lock_guard lk(mu_);
    worker_ = std::move(fn);
}

void ComposerParallelEdit::set_concurrency(std::uint32_t n) {
    std::lock_guard lk(mu_);
    concurrency_ = (n == 0) ? 1u : std::min<std::uint32_t>(n, 64);
}

std::vector<FileEditOutcome> ComposerParallelEdit::run(
    const std::vector<FileEditSpec>& specs) {
    FileEditWorkerFn w;
    std::uint32_t conc;
    {
        std::lock_guard lk(mu_);
        w = worker_;
        conc = concurrency_;
    }
    std::vector<FileEditOutcome> results(specs.size());
    if (!w) {
        for (std::size_t i = 0; i < specs.size(); ++i) {
            results[i].path = specs[i].path;
            results[i].error = "no worker attached";
        }
        return results;
    }

    std::atomic<std::size_t> next{0};
    auto launcher = [&]() {
        while (!cancel_.load(std::memory_order_relaxed)) {
            std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= specs.size()) return;
            auto t0 = std::chrono::steady_clock::now();
            FileEditOutcome o;
            try {
                o = w(specs[i]);
            } catch (const std::exception& e) {
                o.ok = false;
                o.error = e.what();
            } catch (...) {
                o.ok = false;
                o.error = "unknown exception";
            }
            if (o.path.empty()) o.path = specs[i].path;
            auto t1 = std::chrono::steady_clock::now();
            o.latency_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
            results[i] = std::move(o);
        }
    };

    std::vector<std::future<void>> pool;
    pool.reserve(conc);
    for (std::uint32_t i = 0; i < conc; ++i)
        pool.emplace_back(std::async(std::launch::async, launcher));
    for (auto& f : pool) f.get();
    return results;
}

void ComposerParallelEdit::cancel() { cancel_.store(true, std::memory_order_release); }
bool ComposerParallelEdit::cancelled() const { return cancel_.load(std::memory_order_acquire); }
void ComposerParallelEdit::reset_cancel() { cancel_.store(false, std::memory_order_release); }

} // namespace rawrxd::parity
