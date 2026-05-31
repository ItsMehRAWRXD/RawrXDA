// ============================================================================
// test_stream_transformer.cpp — Smoke test for corrected Stream Transformer
// ============================================================================
// Validates all 7 critical fixes from the corrected architecture:
//   1. Buffer remainder — unparsed fragments kept across calls
//   2. No artificial latency — only delays when tokens are TOO fast
//   3. Robust SSE parsing — incremental buffer with \n\n boundaries
//   4. Semantic chunking — words/phrases, not pseudo-tokens
//   5. Dual timing model — arrival / processing / effective latency
//   6. Backpressure — pause reader when UI saturated
//   7. Stall detection — detect when stream goes quiet
// ============================================================================

#include "../src/streaming/stream_transformer.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>
#include <cmath>
#include <chrono>
#include <thread>

using namespace RawrXD::Streaming;

// ============================================================================
// Test Helpers
// ============================================================================

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) run_test(#name, test_##name)

void run_test(const char* name, void (*test)()) {
    std::cout << "  [TEST] " << name << " ... ";
    try {
        test();
        std::cout << "PASS\n";
        g_testsPassed++;
    } catch (const std::exception& e) {
        std::cout << "FAIL: " << e.what() << "\n";
        g_testsFailed++;
    } catch (...) {
        std::cout << "FAIL: unknown exception\n";
        g_testsFailed++;
    }
}

#define ASSERT_TRUE(cond) \
    if (!(cond)) { \
        throw std::runtime_error("ASSERT_TRUE failed: " #cond); \
    }

#define ASSERT_FALSE(cond) \
    if (cond) { \
        throw std::runtime_error("ASSERT_FALSE failed: " #cond); \
    }

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) { \
        std::ostringstream oss; \
        oss << "ASSERT_EQ failed: " << (a) << " != " << (b); \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) { \
        std::ostringstream oss; \
        oss << "ASSERT_GT failed: " << (a) << " <= " << (b); \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_LT(a, b) \
    if (!((a) < (b))) { \
        std::ostringstream oss; \
        oss << "ASSERT_LT failed: " << (a) << " >= " << (b); \
        throw std::runtime_error(oss.str()); \
    }

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) { \
        std::ostringstream oss; \
        oss << "ASSERT_NEAR failed: |" << (a) << " - " << (b) << "| > " << (eps); \
        throw std::runtime_error(oss.str()); \
    }

// ============================================================================
// FIX 1: Buffer Remainder — Unparsed fragments kept across calls
// ============================================================================

TEST(buffer_remainder_kept) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false; // Raw mode for simplicity
    StreamTransformer transformer(config);

    // Send first half of an SSE event
    std::string part1 = "data: {\"choices\":[{\"delta\":{\"content\":\"Hel";
    auto chunks1 = transformer.pushString(part1);
    ASSERT_EQ(chunks1.size(), 0u); // Incomplete — should buffer

    // Send second half
    std::string part2 = "lo\"}}]}\n\n";
    auto chunks2 = transformer.pushString(part2);
    ASSERT_GT(chunks2.size(), 0u); // Now complete

    // Verify content
    ASSERT_EQ(chunks2[0].text, "Hello");
}

TEST(buffer_remainder_multiple_fragments) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    // Split across 3 calls
    transformer.pushString("data: {\"choices\":[{\"delta\":{\"content\":\"A");
    transformer.pushString("B");
    auto chunks = transformer.pushString("C\"}}]}\n\n");

    ASSERT_GT(chunks.size(), 0u);
    ASSERT_EQ(chunks[0].text, "ABC");
}

// ============================================================================
// FIX 2: No Artificial Latency — Only delay when tokens are TOO fast
// ============================================================================

TEST(no_delay_for_slow_tokens) {
    StreamTransformerConfig config;
    config.targetMinInterTokenMs = 50.0f; // 50ms minimum
    StreamTransformer transformer(config);

    // First token
    transformer.pushString("data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");

    // Wait 100ms (slower than 50ms minimum)
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Second token — should NOT need delay
    auto chunks = transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");

    ASSERT_GT(chunks.size(), 0u);
    float delay = transformer.getRecommendedDelayMs();
    ASSERT_EQ(delay, 0.0f); // No delay needed — tokens are slow enough
}

TEST(delay_only_for_fast_tokens) {
    StreamTransformerConfig config;
    config.targetMinInterTokenMs = 50.0f;
    StreamTransformer transformer(config);

    // Send two tokens rapidly
    transformer.pushString("data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");
    auto chunks = transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");

    ASSERT_GT(chunks.size(), 0u);

    // Check if delay is recommended (depends on timing, may be 0 in fast CI)
    float delay = transformer.getRecommendedDelayMs();
    // We can't assert exact value due to timing, but we can verify it's not
    // unconditionally large
    ASSERT_LT(delay, 100.0f); // Should never recommend >100ms
}

// ============================================================================
// FIX 3: Robust SSE Parsing — Incremental buffer with \n\n boundaries
// ============================================================================

TEST(sse_parsing_complete_event) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);
    ASSERT_EQ(chunks[0].text, "Hello");
}

TEST(sse_parsing_multiple_events) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n"
        "\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n"
        "\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\"C\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_EQ(chunks.size(), 3u);
    ASSERT_EQ(chunks[0].text, "A");
    ASSERT_EQ(chunks[1].text, "B");
    ASSERT_EQ(chunks[2].text, "C");
}

TEST(sse_parsing_done_marker) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}}]}\n"
        "\n"
        "data: [DONE]\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_EQ(chunks.size(), 1u);
    ASSERT_EQ(chunks[0].text, "Hello");
    ASSERT_FALSE(transformer.isStreamActive());
}

TEST(sse_parsing_multiline_data) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    // Multi-line data field
    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Line1\\nLine2\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);
    // Should contain newline
    ASSERT_TRUE(chunks[0].text.find('\n') != std::string::npos ||
                chunks[0].text == "Line1\nLine2");
}

// ============================================================================
// FIX 4: Semantic Chunking — Words/phrases, not pseudo-tokens
// ============================================================================

TEST(semantic_chunking_words) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello world test\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);

    // Should be split into words, not characters
    bool foundHello = false;
    bool foundWorld = false;
    for (const auto& chunk : chunks) {
        if (chunk.text == "Hello") foundHello = true;
        if (chunk.text == "world") foundWorld = true;
    }
    ASSERT_TRUE(foundHello);
    ASSERT_TRUE(foundWorld);
}

TEST(semantic_chunking_whitespace) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"A B\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);

    // Should have whitespace as separate chunk
    bool foundSpace = false;
    for (const auto& chunk : chunks) {
        if (chunk.type == ChunkType::WHITESPACE && chunk.text == " ") {
            foundSpace = true;
        }
    }
    ASSERT_TRUE(foundSpace);
}

TEST(semantic_chunking_punctuation) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello, world!\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);

    // Should have punctuation as separate chunks
    bool foundComma = false;
    bool foundBang = false;
    for (const auto& chunk : chunks) {
        if (chunk.type == ChunkType::PUNCTUATION) {
            if (chunk.text == ",") foundComma = true;
            if (chunk.text == "!") foundBang = true;
        }
    }
    ASSERT_TRUE(foundComma);
    ASSERT_TRUE(foundBang);
}

TEST(semantic_chunking_newline) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\\nB\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);

    bool foundNewline = false;
    for (const auto& chunk : chunks) {
        if (chunk.type == ChunkType::NEWLINE) {
            foundNewline = true;
        }
    }
    ASSERT_TRUE(foundNewline);
}

TEST(semantic_chunking_code_block) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    config.preserveCodeBlocks = true;
    StreamTransformer transformer(config);

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"```cpp\\nint main() {\\n    return 0;\\n}\\n```\"}}}]}\n"
        "\n";

    auto chunks = transformer.pushString(sse);
    ASSERT_GT(chunks.size(), 0u);

    // Should have code block as single chunk
    bool foundCodeBlock = false;
    for (const auto& chunk : chunks) {
        if (chunk.type == ChunkType::CODE_BLOCK) {
            foundCodeBlock = true;
            ASSERT_TRUE(chunk.text.find("```") != std::string::npos);
        }
    }
    ASSERT_TRUE(foundCodeBlock);
}

// ============================================================================
// FIX 5: Dual Timing Model — arrival / processing / effective latency
// ============================================================================

TEST(dual_timing_tracks_arrival) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    auto before = std::chrono::steady_clock::now();
    auto chunks = transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"Test\"}}}]}\n\n");
    auto after = std::chrono::steady_clock::now();

    ASSERT_GT(chunks.size(), 0u);
    ASSERT_GT(chunks[0].timing.arrivalTime.time_since_epoch().count(), 0);

    // Arrival time should be between before and after
    ASSERT_TRUE(chunks[0].timing.arrivalTime >= before);
    ASSERT_TRUE(chunks[0].timing.arrivalTime <= after);
}

TEST(dual_timing_processing_delay_zero_for_slow) {
    StreamTransformerConfig config;
    config.targetMinInterTokenMs = 100.0f;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    // First token
    transformer.pushString("data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");

    // Wait (slow)
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Second token
    auto chunks = transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");

    ASSERT_GT(chunks.size(), 0u);
    // Processing delay should be 0 for slow tokens
    ASSERT_EQ(chunks[0].timing.processingDelay.count(), 0);
}

TEST(dual_timing_effective_latency) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    auto chunks = transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"Test\"}}}]}\n\n");

    ASSERT_GT(chunks.size(), 0u);
    // Effective latency = inter-arrival + processing delay
    auto expected = chunks[0].timing.interArrival + chunks[0].timing.processingDelay;
    ASSERT_EQ(chunks[0].timing.effectiveLatency.count(), expected.count());
}

// ============================================================================
// FIX 6: Backpressure — Pause reader when UI saturated
// ============================================================================

TEST(backpressure_activates_when_saturated) {
    StreamTransformerConfig config;
    config.enableBackpressure = true;
    config.maxPendingChunks = 2; // Very low for testing
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    ASSERT_FALSE(transformer.isBackpressureActive());

    // Push chunks without consuming
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"C\"}}}]}\n\n");

    // Should be saturated now
    ASSERT_TRUE(transformer.isBackpressureActive());
}

TEST(backpressure_releases_when_consumed) {
    StreamTransformerConfig config;
    config.enableBackpressure = true;
    config.maxPendingChunks = 2;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    // Saturate
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"C\"}}}]}\n\n");

    ASSERT_TRUE(transformer.isBackpressureActive());

    // Consume chunks
    transformer.markChunkConsumed();
    transformer.markChunkConsumed();
    transformer.markChunkConsumed();

    ASSERT_FALSE(transformer.isBackpressureActive());
}

TEST(backpressure_callback_fires) {
    StreamTransformerConfig config;
    config.enableBackpressure = true;
    config.maxPendingChunks = 1;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    bool callbackFired = false;
    bool backpressureState = false;

    transformer.onBackpressure([&](bool active) {
        callbackFired = true;
        backpressureState = active;
    });

    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");

    ASSERT_TRUE(callbackFired);
    ASSERT_TRUE(backpressureState);
}

// ============================================================================
// FIX 7: Stall Detection — Detect when stream goes quiet
// ============================================================================

TEST(stall_detection_triggers) {
    StreamTransformerConfig config;
    config.targetMaxInterTokenMs = 50.0f; // Very short for testing
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    bool stallDetected = false;
    transformer.onStall([&](const StreamState& state) {
        stallDetected = true;
    });

    // Send one token
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");

    ASSERT_FALSE(stallDetected);

    // Wait longer than threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    // Trigger check by sending another token (or we could check state directly)
    // The stall is detected on the next push or we can check state
    auto state = transformer.getState();
    // Note: stall detection happens in pushBytes, so we need another call
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");

    ASSERT_TRUE(stallDetected);
}

TEST(stall_detection_resumes) {
    StreamTransformerConfig config;
    config.targetMaxInterTokenMs = 50.0f;
    config.emitSemanticChunks = false;
    StreamTransformer transformer(config);

    bool resumeDetected = false;
    transformer.onResume([&](const StreamState& state) {
        resumeDetected = true;
    });

    // Trigger stall
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}}]}\n\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"B\"}}}]}\n\n");

    // Now send quickly to resume
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"C\"}}}]}\n\n");

    ASSERT_TRUE(resumeDetected);
}

// ============================================================================
// Integration: End-to-end streaming simulation
// ============================================================================

TEST(integration_full_stream) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    config.targetMinInterTokenMs = 10.0f;
    config.targetMaxInterTokenMs = 500.0f;
    config.enableBackpressure = true;
    config.maxPendingChunks = 50;
    StreamTransformer transformer(config);

    std::vector<SemanticChunk> allChunks;
    transformer.onChunk([&](const SemanticChunk& chunk) {
        allChunks.push_back(chunk);
    });

    // Simulate a realistic SSE stream
    std::vector<std::string> sseEvents = {
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\"!\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\"\\n\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\"How\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\" are\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\" you\"}}}]}\n\n",
        "data: {\"choices\":[{\"delta\":{\"content\":\"?\"}}}]}\n\n",
        "data: [DONE]\n\n"
    };

    for (const auto& event : sseEvents) {
        auto chunks = transformer.pushString(event);
        for (const auto& chunk : chunks) {
            transformer.markChunkConsumed();
        }
    }

    // Verify we got meaningful chunks
    ASSERT_GT(allChunks.size(), 0u);

    // Check state
    auto state = transformer.getState();
    ASSERT_GT(state.totalChunks, 0u);
    ASSERT_GT(state.totalBytes, 0u);
}

TEST(integration_state_tracking) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    StreamTransformer transformer(config);

    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello world\"}}}]}\n\n");

    auto state = transformer.getState();
    ASSERT_GT(state.totalChunks, 0u);
    ASSERT_GT(state.totalBytes, 0u);
    ASSERT_GT(state.chunkTypeCounts.size(), 0u);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(edge_empty_input) {
    StreamTransformer transformer;
    auto chunks = transformer.pushString("");
    ASSERT_EQ(chunks.size(), 0u);
}

TEST(edge_null_bytes) {
    StreamTransformer transformer;
    auto chunks = transformer.pushBytes(nullptr, 0);
    ASSERT_EQ(chunks.size(), 0u);
}

TEST(edge_reset_clears_state) {
    StreamTransformer transformer;
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}}]}\n\n");

    transformer.reset();

    auto state = transformer.getState();
    ASSERT_EQ(state.totalChunks, 0u);
    ASSERT_EQ(state.totalBytes, 0u);
}

TEST(edge_shutdown_stops_processing) {
    StreamTransformer transformer;
    transformer.shutdown();

    auto chunks = transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}}]}\n\n");
    ASSERT_EQ(chunks.size(), 0u);
}

TEST(edge_end_stream_flushes) {
    StreamTransformerConfig config;
    config.emitSemanticChunks = true;
    config.preserveCodeBlocks = true;
    StreamTransformer transformer(config);

    // Send a partial code block (opening ``` but no closing)
    // chunkText will keep this in textBuffer_ since it's incomplete
    transformer.pushString(
        "data: {\"choices\":[{\"delta\":{\"content\":\"```cpp\\nint main() {\\n    return 0;\"}}}]}\n\n");

    // The code block is incomplete (no closing ```), so it stays in textBuffer_
    // endStream() should flush it as a regular text chunk
    auto chunks = transformer.endStream();
    ASSERT_GT(chunks.size(), 0u);
    // Should contain the code content
    ASSERT_TRUE(chunks[0].text.find("int main()") != std::string::npos ||
                chunks[0].text.find("return 0") != std::string::npos);
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     Stream Transformer Smoke Test — 7 Critical Fixes        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    std::cout << "FIX 1: Buffer Remainder (unparsed fragments kept)\n";
    RUN_TEST(buffer_remainder_kept);
    RUN_TEST(buffer_remainder_multiple_fragments);

    std::cout << "\nFIX 2: No Artificial Latency (only delay when too fast)\n";
    RUN_TEST(no_delay_for_slow_tokens);
    RUN_TEST(delay_only_for_fast_tokens);

    std::cout << "\nFIX 3: Robust SSE Parsing (incremental \n\n boundaries)\n";
    RUN_TEST(sse_parsing_complete_event);
    RUN_TEST(sse_parsing_multiple_events);
    RUN_TEST(sse_parsing_done_marker);
    RUN_TEST(sse_parsing_multiline_data);

    std::cout << "\nFIX 4: Semantic Chunking (words/phrases, not pseudo-tokens)\n";
    RUN_TEST(semantic_chunking_words);
    RUN_TEST(semantic_chunking_whitespace);
    RUN_TEST(semantic_chunking_punctuation);
    RUN_TEST(semantic_chunking_newline);
    RUN_TEST(semantic_chunking_code_block);

    std::cout << "\nFIX 5: Dual Timing Model (arrival/processing/effective)\n";
    RUN_TEST(dual_timing_tracks_arrival);
    RUN_TEST(dual_timing_processing_delay_zero_for_slow);
    RUN_TEST(dual_timing_effective_latency);

    std::cout << "\nFIX 6: Backpressure (pause reader when UI saturated)\n";
    RUN_TEST(backpressure_activates_when_saturated);
    RUN_TEST(backpressure_releases_when_consumed);
    RUN_TEST(backpressure_callback_fires);

    std::cout << "\nFIX 7: Stall Detection (detect when stream goes quiet)\n";
    RUN_TEST(stall_detection_triggers);
    RUN_TEST(stall_detection_resumes);

    std::cout << "\nIntegration Tests\n";
    RUN_TEST(integration_full_stream);
    RUN_TEST(integration_state_tracking);

    std::cout << "\nEdge Cases\n";
    RUN_TEST(edge_empty_input);
    RUN_TEST(edge_null_bytes);
    RUN_TEST(edge_reset_clears_state);
    RUN_TEST(edge_shutdown_stops_processing);
    RUN_TEST(edge_end_stream_flushes);

    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "  Results: " << g_testsPassed << "/"
              << (g_testsPassed + g_testsFailed) << " passed\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    return g_testsFailed > 0 ? 1 : 0;
}
