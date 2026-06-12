/**
 * API Server Stress Test — Concurrent Bearer Auth + Rate Limit Validation
 * Tests: deadlock-free concurrent access, token validation, rate limiting
 */
#include "../include/api_server.h"
#include "../src/AppState.h"
#include "../src/overclock_governor.h"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cassert>
#include <cstring>

// White-box test harness: friend of APIServer for accessing private members
class APIServerStressTest {
public:
    static bool ValidateRequest(APIServer& server, const HttpRequest& req) {
        return server.ValidateRequest(req);
    }
    static bool CheckRateLimit(APIServer& server, const std::string& client_id) {
        return server.CheckRateLimit(client_id);
    }
    static void UpdateRateLimit(APIServer& server, const std::string& client_id) {
        server.UpdateRateLimit(client_id);
    }
    static JsonValue ParseJsonRequest(APIServer& server, const std::string& req) {
        return server.ParseJsonRequest(req);
    }
};

// Minimal HTTP client for stress testing
static bool HttpPost(const std::string& host, uint16_t port,
                     const std::string& path,
                     const std::string& body,
                     const std::string& auth_token,
                     int& out_status,
                     std::string& out_body,
                     std::chrono::milliseconds timeout) {
    (void)host; (void)port; (void)path; (void)body; (void)auth_token;
    (void)timeout;
    // Stub: in a real test this would use WinHTTP/curl
    // For unit-test purposes we validate the APIServer logic directly
    out_status = 200;
    out_body = "{\"response\":\"ok\"}";
    return true;
}

// Validate APIServer::ValidateRequest directly
static bool TestBearerAuth() {
    AppState state;
    APIServer server(state);

    // Valid request with correct Bearer token
    HttpRequest req1;
    req1.method = "POST";
    req1.path = "/api/generate";
    req1.body = "{\"prompt\":\"hello\"}";
    req1.headers["Authorization"] = "Bearer rawrxd-dev-key-2026";
    req1.headers["Content-Type"] = "application/json";

    // Invalid: missing auth
    HttpRequest req2 = req1;
    req2.headers.erase("Authorization");

    // Invalid: wrong token
    HttpRequest req3 = req1;
    req3.headers["Authorization"] = "Bearer wrong-token";

    // Invalid: malformed prefix
    HttpRequest req4 = req1;
    req4.headers["Authorization"] = "Basic rawrxd-dev-key-2026";

    bool ok1 = APIServerStressTest::ValidateRequest(server, req1);
    bool ok2 = APIServerStressTest::ValidateRequest(server, req2);
    bool ok3 = APIServerStressTest::ValidateRequest(server, req3);
    bool ok4 = APIServerStressTest::ValidateRequest(server, req4);

    std::cout << "  Bearer auth valid token:   " << (ok1 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Bearer auth missing:       " << (!ok2 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Bearer auth wrong token:   " << (!ok3 ? "PASS" : "FAIL") << "\n";
    std::cout << "  Bearer auth bad prefix:    " << (!ok4 ? "PASS" : "FAIL") << "\n";

    return ok1 && !ok2 && !ok3 && !ok4;
}

// Stress test: rapid concurrent validation calls
static bool TestConcurrentValidation() {
    AppState state;
    APIServer server(state);

    const int kThreads = 16;
    const int kIterations = 1000;
    std::atomic<int> passed{0};
    std::atomic<int> failed{0};
    std::atomic<bool> deadlock{false};

    auto worker = [&](int tid) {
        HttpRequest req;
        req.method = "POST";
        req.path = "/api/generate";
        req.body = "{\"prompt\":\"test\"}";
        req.headers["Authorization"] = (tid % 2 == 0)
            ? "Bearer rawrxd-dev-key-2026"
            : "Bearer wrong-token";

        for (int i = 0; i < kIterations; ++i) {
            auto start = std::chrono::steady_clock::now();
            bool ok = APIServerStressTest::ValidateRequest(server, req);
            auto elapsed = std::chrono::steady_clock::now() - start;

            // Deadlock detection: any call taking > 5s is a deadlock
            if (elapsed > std::chrono::seconds(5)) {
                deadlock.store(true);
            }

            if (tid % 2 == 0) {
                if (ok) ++passed; else ++failed;
            } else {
                if (!ok) ++passed; else ++failed;
            }
        }
    };

    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back(worker, t);
    }
    for (auto& th : threads) {
        th.join();
    }
    auto t1 = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double ops_per_sec = (kThreads * kIterations) / (ms / 1000.0);

    std::cout << "  Threads: " << kThreads << ", Iterations: " << kIterations << "\n";
    std::cout << "  Total time: " << ms << " ms\n";
    std::cout << "  Throughput: " << ops_per_sec << " validations/sec\n";
    std::cout << "  Passed: " << passed.load() << ", Failed: " << failed.load() << "\n";
    std::cout << "  Deadlock: " << (deadlock.load() ? "DETECTED (FAIL)" : "none (PASS)") << "\n";

    return !deadlock.load() && failed.load() == 0;
}

// Rate limit stress: hammer from same client_id
static bool TestRateLimitStress() {
    AppState state;
    APIServer server(state);

    const int kBurst = 200;
    const std::string client_id = "stress-client-1";
    int allowed = 0;
    int blocked = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kBurst; ++i) {
        if (APIServerStressTest::CheckRateLimit(server, client_id)) {
            ++allowed;
            APIServerStressTest::UpdateRateLimit(server, client_id);
        } else {
            ++blocked;
        }
    }
    auto t1 = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::cout << "  Burst requests: " << kBurst << "\n";
    std::cout << "  Allowed: " << allowed << ", Blocked: " << blocked << "\n";
    std::cout << "  Time: " << ms << " ms\n";
    std::cout << "  Rate limit active: " << (blocked > 0 ? "PASS" : "FAIL (no blocking)") << "\n";

    return blocked > 0 && allowed > 0;
}

// JSON parsing stress: malformed + valid payloads
static bool TestJsonParsingStress() {
    AppState state;
    APIServer server(state);

    std::vector<std::string> payloads = {
        "{\"prompt\":\"hello\"}",
        "{\"prompt\":\"hello\",\"temperature\":0.7}",
        "{\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}",
        "not json at all",
        "{\"nested\":{\"deep\":{\"value\":123}}}",
        "",
        "{\"array\":[1,2,3],\"bool\":true,\"null\":null}",
    };

    int parsed = 0;
    int rejected = 0;
    for (const auto& p : payloads) {
        auto jv = APIServerStressTest::ParseJsonRequest(server, p);
        if (jv.is_object) {
            ++parsed;
        } else {
            ++rejected;
        }
    }

    std::cout << "  Payloads: " << payloads.size() << "\n";
    std::cout << "  Parsed: " << parsed << ", Rejected: " << rejected << "\n";
    std::cout << "  Malformed rejection: " << (rejected >= 2 ? "PASS" : "FAIL") << "\n";

    return parsed >= 4 && rejected >= 2;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    std::cout << "=== RawrXD API Server Stress Test ===\n\n";

    bool all_pass = true;

    std::cout << "[Test 1] Bearer Token Authentication\n";
    all_pass &= TestBearerAuth();
    std::cout << "\n";

    std::cout << "[Test 2] Concurrent Validation (16 threads x 1000 iter)\n";
    all_pass &= TestConcurrentValidation();
    std::cout << "\n";

    std::cout << "[Test 3] Rate Limit Stress (200 burst)\n";
    all_pass &= TestRateLimitStress();
    std::cout << "\n";

    std::cout << "[Test 4] JSON Parsing Stress (malformed + valid)\n";
    all_pass &= TestJsonParsingStress();
    std::cout << "\n";

    if (all_pass) {
        std::cout << "=== ALL TESTS PASSED ===\n";
        return 0;
    } else {
        std::cout << "=== SOME TESTS FAILED ===\n";
        return 1;
    }
}
