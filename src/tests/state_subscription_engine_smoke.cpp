#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "../core/state_subscription_engine.hpp"
#include "../core/rollback_engine.hpp"
#include "../testing/deterministic_test_harness.hpp"
#include "../testing/failure_injector.hpp"

using namespace RawrXD::Core;
using namespace RawrXD::Testing;
using namespace RawrXD::Testing::Chaos;

namespace {

int gPass = 0;
int gFail = 0;

void check(const char* name, bool ok) {
    if (ok) {
        ++gPass;
        std::cout << "[PASS] " << name << "\n";
    } else {
        ++gFail;
        std::cout << "[FAIL] " << name << "\n";
    }
}

void testLockFreeReadWriteCoW() {
    StateSubscriptionEngine::Config cfg;
    cfg.maxAsyncQueue = 64;
    cfg.workerThreads = 2;

    StateSubscriptionEngine engine(cfg);

    auto set1 = engine.set("mode", "idle");
    check("set initial state", set1.ok());

    auto before = engine.getConsistent("mode");
    check("read returns value", before.first.has_value() && *before.first == "idle");

    auto set2 = engine.set("mode", "running");
    check("set changed state", set2.ok());

    auto after = engine.getConsistent("mode");
    check("read returns updated value", after.first.has_value() && *after.first == "running");
    check("version monotonic", after.second >= before.second);

    engine.shutdown();
}

void testBoundedQueueBackpressure() {
    StateSubscriptionEngine::Config cfg;
    cfg.maxAsyncQueue = 1;
    cfg.workerThreads = 1;

    StateSubscriptionEngine engine(cfg);

    std::atomic<int> processed{0};
    auto sub = engine.subscribe("k", StateSubscriptionEngine::Subscriber{
        [&](const StateEvent&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            ++processed;
        },
        true,
        std::nullopt,
    });

    check("subscribe async", sub.ok());

    for (int i = 0; i < 20; ++i) {
        (void)engine.set("k", std::to_string(i));
    }

    engine.waitForIdle(std::chrono::milliseconds(400));
    auto failures = engine.recentFailures();
    check("backpressure recorded failures", !failures.empty());

    engine.shutdown();
}

void testExceptionIsolation() {
    StateSubscriptionEngine engine;

    auto bad = engine.subscribe("x", StateSubscriptionEngine::Subscriber{
        [](const StateEvent&) {
            throw std::runtime_error("boom");
        },
        false,
        std::nullopt,
    });

    check("subscribe sync throwing callback", bad.ok());

    auto goodCount = std::make_shared<std::atomic<int>>(0);
    auto good = engine.subscribe("x", StateSubscriptionEngine::Subscriber{
        [goodCount](const StateEvent&) {
            ++(*goodCount);
        },
        false,
        std::nullopt,
    });

    check("subscribe sync good callback", good.ok());

    auto setRes = engine.set("x", "1");
    check("set with one throwing callback does not fail", setRes.ok());
    check("good callback still executed", goodCount->load() == 1);

    auto failures = engine.recentFailures();
    check("failure log captures callback exception", !failures.empty());

    engine.shutdown();
}

void testTimeoutIsolation() {
    StateSubscriptionEngine engine;

    auto sub = engine.subscribe("t", StateSubscriptionEngine::Subscriber{
        [](const StateEvent&) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        },
        false,
        std::chrono::milliseconds(1),
    });
    check("subscribe timeout callback", sub.ok());

    auto res = engine.set("t", "slow");
    check("set with timeout callback", res.ok());

    auto failures = engine.recentFailures();
    check("timeout recorded", !failures.empty());

    engine.shutdown();
}

void testDeterministicHarnessAndTrace() {
    DeterministicClock clock;
    DeterministicScheduler scheduler;
    DeterministicRNG rng(7);
    StateTraceRecorder trace;

    StateEvent e1{"a", "0", "1", 1, std::chrono::steady_clock::now()};
    StateEvent e2{"a", "1", "2", 2, std::chrono::steady_clock::now()};

    scheduler.scheduleAt(5, [&]() { trace.record(e1, clock.now()); });
    scheduler.scheduleAt(10, [&]() { trace.record(e2, clock.now()); });

    scheduler.run(clock, 100);

    check("trace size == 2", trace.trace().size() == 2);
    check("trace monotonic", trace.verifyMonotonic());
    check("rng deterministic range", rng.randomInt<int>(1, 3) >= 1);
}

void testFailureInjector() {
    FailureInjector fi(123);
    fi.setPolicy({FailureMode::Exception, 1.0, std::chrono::milliseconds(0)});

    bool threw = false;
    try {
        fi.execute([]() { return 1; });
    } catch (...) {
        threw = true;
    }
    check("failure injector throws in exception mode", threw);

    fi.setPolicy({FailureMode::Corruption, 1.0, std::chrono::milliseconds(0)});
    std::string src = "abcdef";
    std::string dst = fi.corruptString(src);
    check("corruption changes string length stable", dst.size() == src.size());
}

void testRollbackEngine() {
    StateSubscriptionEngine engine;
    RollbackEngine rollback(engine);

    int state = 0;

    RollbackEngine::Transaction tx;
    tx.txId = "tx-1";
    tx.commands.push_back(RollbackEngine::Command{
        [&]() { state = 1; return true; },
        [&]() { state = 0; },
        "set-1",
    });
    tx.commands.push_back(RollbackEngine::Command{
        [&]() { state = 2; return false; },
        [&]() { state = 1; },
        "fail-2",
    });

    bool ok = rollback.executeTransaction(tx);
    check("rollback transaction fails", !ok);
    check("rollback restored state", state == 0);
    check("transaction not committed", !tx.committed);

    engine.shutdown();
}

} // namespace

int main() {
    std::cout << "StateSubscriptionEngine Smoke\n";

    testLockFreeReadWriteCoW();
    testBoundedQueueBackpressure();
    testExceptionIsolation();
    testTimeoutIsolation();
    testDeterministicHarnessAndTrace();
    testFailureInjector();
    testRollbackEngine();

    std::cout << "PASS=" << gPass << " FAIL=" << gFail << "\n";
    return gFail == 0 ? 0 : 1;
}
