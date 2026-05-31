#include "../src/ai/inference_retry_shim.h"
#include <cassert>
#include <iostream>

using namespace rxd::ai;

int main() {
    RetryPolicy p;
    p.max_retries = 3;
    p.base_ms = 10;
    p.circuit_threshold = 2;
    InferenceRetryShim shim(p);

    int calls = 0;

    // Test 1: success on first try
    auto s1 = shim.Execute([&]() -> InferenceStatus {
        calls++; return InferenceStatus::OK;
    }, "ep_ok");
    assert(s1 == InferenceStatus::OK);
    assert(calls == 1);

    // Test 2: retryable -> success on 3rd attempt (use a fresh tag)
    calls = 0;
    auto s2 = shim.Execute([&]() -> InferenceStatus {
        calls++;
        return (calls < 3) ? InferenceStatus::Retryable : InferenceStatus::OK;
    }, "ep_retry");
    assert(s2 == InferenceStatus::OK);
    assert(calls == 3);

    // Test 3: circuit breaker on a dedicated tag
    //   max_retries=3, threshold=2 -> first Execute records 4 failures (>= 2)
    //   so the circuit opens during the first call's loop. Second Execute
    //   sees the open circuit and returns CircuitOpen.
    calls = 0;
    auto s3a = shim.Execute([&]() -> InferenceStatus {
        calls++; return InferenceStatus::Retryable;
    }, "ep_break");
    assert(s3a == InferenceStatus::Retryable);
    auto s3b = shim.Execute([&]() -> InferenceStatus {
        calls++; return InferenceStatus::OK;
    }, "ep_break");
    assert(s3b == InferenceStatus::CircuitOpen);
    assert(shim.IsCircuitOpen("ep_break"));

    // Reset and verify recovery
    shim.ResetCircuit("ep_break");
    assert(!shim.IsCircuitOpen("ep_break"));
    auto s3c = shim.Execute([&]() -> InferenceStatus {
        return InferenceStatus::OK;
    }, "ep_break");
    assert(s3c == InferenceStatus::OK);

    // Test 4: NonRetryable fails fast (no retries)
    calls = 0;
    auto s4 = shim.Execute([&]() -> InferenceStatus {
        calls++; return InferenceStatus::NonRetryable;
    }, "ep_4xx");
    assert(s4 == InferenceStatus::NonRetryable);
    assert(calls == 1);

    auto m = shim.GetMetrics();
    assert(m.total_calls >= 5);
    assert(m.successes >= 3);
    assert(m.circuit_opens >= 1);
    assert(m.retries >= 2); // at least the 2 retries from Test 2

    std::cout << "retry_shim: ALL OK"
              << " total=" << m.total_calls
              << " succ=" << m.successes
              << " retries=" << m.retries
              << " opens=" << m.circuit_opens
              << "\n";
    return 0;
}
