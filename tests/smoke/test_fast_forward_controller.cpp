// ============================================================================
// test_fast_forward_controller.cpp — Smoke Test for Fast Forward + TLS
// ============================================================================

#include "ui/fast_forward_controller.h"
#include "ui/async_message_queue.h"
#include <iostream>
#include <assert>
#include <chrono>
#include <thread>

using namespace RawrXD::UI;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "  [TEST] " #name "... "; \
    try { test_##name(); g_testsPassed++; std::cout << "PASS\n"; } \
    catch (const std::exception& e) { g_testsFailed++; std::cout << "FAIL: " << e.what() << "\n"; } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        throw std::runtime_error("Assertion failed: " #cond); \
    } \
} while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))

// ============================================================================
// Helper: Create a mock QueuedMessage
// ============================================================================

QueuedMessage makeMockMessage(const std::string& id, float progress, uint32_t tokens) {
    QueuedMessage msg;
    msg.id = id;
    msg.progress = progress;
    msg.completionTokens = tokens;
    msg.status = MessageStatus::STREAMING;
    return msg;
}

// ============================================================================
// Tests
// ============================================================================

TEST(initiate_ff_creates_state) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 5000;
    config.minProgressThreshold = 5;

    FastForwardController controller(config);

    auto msg = makeMockMessage("msg-1", 50.0f, 100);
    auto state = controller.initiateFF(msg, "manual");

    ASSERT_EQ(state.originalMessageId, "msg-1");
    ASSERT_TRUE(state.deadline > state.startTime);
    ASSERT_FALSE(state.forcedCompletion);
    ASSERT_TRUE(controller.isFFActive("msg-1"));
}

TEST(initiate_ff_throws_on_low_progress) {
    FastForwardConfig config;
    config.minProgressThreshold = 50;

    FastForwardController controller(config);

    auto msg = makeMockMessage("msg-2", 10.0f, 0);

    bool threw = false;
    try {
        controller.initiateFF(msg);
    } catch (const std::runtime_error&) {
        threw = true;
    }

    ASSERT_TRUE(threw);
}

TEST(cancel_ff_removes_active) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    auto msg = makeMockMessage("msg-3", 50.0f, 100);
    controller.initiateFF(msg);

    ASSERT_TRUE(controller.isFFActive("msg-3"));

    bool cancelled = controller.cancelFF("msg-3", "user_cancel");
    ASSERT_TRUE(cancelled);
    ASSERT_FALSE(controller.isFFActive("msg-3"));
}

TEST(complete_ff_removes_active) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    auto msg = makeMockMessage("msg-4", 50.0f, 100);
    controller.initiateFF(msg);

    ASSERT_TRUE(controller.isFFActive("msg-4"));

    bool completed = controller.completeFF("msg-4");
    ASSERT_TRUE(completed);
    ASSERT_FALSE(controller.isFFActive("msg-4"));
}

TEST(should_keep_token_respects_acceleration) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;
    config.accelerationFactor = 3;

    FastForwardController controller(config);

    auto msg = makeMockMessage("msg-5", 50.0f, 100);
    controller.initiateFF(msg);

    // With factor=3, keep tokens 0, 3, 6, 9...
    ASSERT_TRUE(controller.shouldKeepToken("msg-5", 0));
    ASSERT_FALSE(controller.shouldKeepToken("msg-5", 1));
    ASSERT_FALSE(controller.shouldKeepToken("msg-5", 2));
    ASSERT_TRUE(controller.shouldKeepToken("msg-5", 3));
    ASSERT_TRUE(controller.shouldKeepToken("msg-5", 6));
    ASSERT_TRUE(controller.shouldKeepToken("msg-5", 9));
}

TEST(should_keep_token_all_when_no_ff) {
    FastForwardConfig config;
    config.accelerationFactor = 3;

    FastForwardController controller(config);

    // No FF active for "msg-6"
    ASSERT_TRUE(controller.shouldKeepToken("msg-6", 0));
    ASSERT_TRUE(controller.shouldKeepToken("msg-6", 1));
    ASSERT_TRUE(controller.shouldKeepToken("msg-6", 2));
    ASSERT_TRUE(controller.shouldKeepToken("msg-6", 3));
}

TEST(update_progress_calculates_tps) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    auto msg = makeMockMessage("msg-7", 50.0f, 0);
    controller.initiateFF(msg);

    // Simulate tokens over time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    controller.updateProgress("msg-7", 10);

    auto* state = controller.getFFState("msg-7");
    ASSERT_TRUE(state != nullptr);
    ASSERT_TRUE(state->tokensGenerated == 10);
    ASSERT_TRUE(state->currentTPS > 0.0f);
}

TEST(get_active_ffs_returns_all) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    controller.initiateFF(makeMockMessage("msg-8a", 50.0f, 100));
    controller.initiateFF(makeMockMessage("msg-8b", 60.0f, 200));
    controller.initiateFF(makeMockMessage("msg-8c", 70.0f, 300));

    auto active = controller.getActiveFFs();
    ASSERT_EQ(active.size(), 3u);
}

TEST(tls_exceeded_triggers_timeout) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 100;  // Very short for testing
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    bool timeoutFired = false;
    controller.onFFTimeout([&](const FastForwardState& state) {
        timeoutFired = true;
    });

    auto msg = makeMockMessage("msg-9", 50.0f, 100);
    controller.initiateFF(msg);

    // Wait for TLS to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ASSERT_TRUE(timeoutFired);
    ASSERT_FALSE(controller.isFFActive("msg-9"));
}

TEST(tls_warning_fires_at_threshold) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 500;
    config.minProgressThreshold = 0;
    config.warningAtRemainingMs = 200;

    FastForwardController controller(config);

    bool warningFired = false;
    controller.onTLSWarning([&](const FastForwardState& state, float remaining) {
        warningFired = true;
    });

    auto msg = makeMockMessage("msg-10", 50.0f, 100);
    controller.initiateFF(msg);

    // Wait for warning to fire
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    ASSERT_TRUE(warningFired);

    controller.shutdown();
}

TEST(event_callbacks_fire) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    bool started = false;
    bool completed = false;

    controller.onFFStarted([&](const FastForwardState&) { started = true; });
    controller.onFFCompleted([&](const FastForwardState&) { completed = true; });

    auto msg = makeMockMessage("msg-11", 50.0f, 100);
    controller.initiateFF(msg);
    ASSERT_TRUE(started);

    controller.completeFF("msg-11");
    ASSERT_TRUE(completed);
}

TEST(shutdown_cancels_all) {
    FastForwardConfig config;
    config.tlsTimeoutMs = 10000;
    config.minProgressThreshold = 0;

    FastForwardController controller(config);

    controller.initiateFF(makeMockMessage("msg-12a", 50.0f, 100));
    controller.initiateFF(makeMockMessage("msg-12b", 60.0f, 200));
    controller.initiateFF(makeMockMessage("msg-12c", 70.0f, 300));

    ASSERT_EQ(controller.getActiveFFs().size(), 3u);

    controller.shutdown();

    ASSERT_EQ(controller.getActiveFFs().size(), 0u);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=== Fast Forward Controller Smoke Tests ===\n\n";

    RUN_TEST(initiate_ff_creates_state);
    RUN_TEST(initiate_ff_throws_on_low_progress);
    RUN_TEST(cancel_ff_removes_active);
    RUN_TEST(complete_ff_removes_active);
    RUN_TEST(should_keep_token_respects_acceleration);
    RUN_TEST(should_keep_token_all_when_no_ff);
    RUN_TEST(update_progress_calculates_tps);
    RUN_TEST(get_active_ffs_returns_all);
    RUN_TEST(tls_exceeded_triggers_timeout);
    RUN_TEST(tls_warning_fires_at_threshold);
    RUN_TEST(event_callbacks_fire);
    RUN_TEST(shutdown_cancels_all);

    std::cout << "\n=== Results: " << g_testsPassed << "/"
              << (g_testsPassed + g_testsFailed) << " passed ===\n";

    return (g_testsFailed == 0) ? 0 : 1;
}
