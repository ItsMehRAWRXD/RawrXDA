// ============================================================================
// AI_TokenStream_Test.cpp
// Production validation suite for token streaming fixes
// ============================================================================

#include "AI_TokenStream.hpp"
#include <stdio.h>
#include <string>
#include <cassert>
#include <chrono>
#include <thread>

using namespace rawrxd::aistream;

// ============================================================================
// Test Framework (minimal, no external deps)
// ============================================================================

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    printf("  [TEST] %-40s ", #name); \
    try { test_##name(); printf("PASS\n"); ++g_tests_passed; } \
    catch (const char* e) { printf("FAIL: %s\n", e); ++g_tests_failed; } \
    catch (...) { printf("FAIL: unknown exception\n"); ++g_tests_failed; } \
} while(0)

#define ASSERT_TRUE(cond) do { if (!(cond)) throw #cond " is false"; } while(0)
#define ASSERT_FALSE(cond) do { if (cond) throw #cond " is true"; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) throw #a " != " #b; } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) throw #a " == " #b; } while(0)
#define ASSERT_LT(a, b) do { if ((a) >= (b)) throw #a " >= " #b; } while(0)
#define ASSERT_GT(a, b) do { if ((a) <= (b)) throw #a " <= " #b; } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) throw #a " < " #b; } while(0)

// ============================================================================
// SSE Parser Tests
// ============================================================================

TEST(sse_parser_basic_event) {
    SSEParser parser;
    std::string data = "data: hello world\n\n";
    auto events = parser.feed(reinterpret_cast<const uint8_t*>(data.data()), data.length());

    ASSERT_EQ(events.size(), 1u);
    ASSERT_EQ(events[0].data, "hello world");
    ASSERT_FALSE(events[0].is_done);
}

TEST(sse_parser_done_event) {
    SSEParser parser;
    std::string data = "data: [DONE]\n\n";
    auto events = parser.feed(reinterpret_cast<const uint8_t*>(data.data()), data.length());

    ASSERT_EQ(events.size(), 1u);
    ASSERT_TRUE(events[0].is_done);
}

TEST(sse_parser_multiple_events) {
    SSEParser parser;
    std::string data =
        "data: first\n\n"
        "data: second\n\n";
    auto events = parser.feed(reinterpret_cast<const uint8_t*>(data.data()), data.length());

    ASSERT_EQ(events.size(), 2u);
    ASSERT_EQ(events[0].data, "first");
    ASSERT_EQ(events[1].data, "second");
}

TEST(sse_parser_multiline_data) {
    SSEParser parser;
    std::string data = "data: line1\ndata: line2\n\n";
    auto events = parser.feed(reinterpret_cast<const uint8_t*>(data.data()), data.length());

    ASSERT_EQ(events.size(), 1u);
    ASSERT_EQ(events[0].data, "line1\nline2");
}

// CRITICAL FIX: Partial line preservation across chunks
TEST(sse_parser_partial_line_preserved) {
    SSEParser parser;

    // First chunk: incomplete line
    std::string chunk1 = "data: hel";
    auto events1 = parser.feed(reinterpret_cast<const uint8_t*>(chunk1.data()), chunk1.length());
    ASSERT_EQ(events1.size(), 0u); // No complete event yet

    // Second chunk: completes the line
    std::string chunk2 = "lo world\n\n";
    auto events2 = parser.feed(reinterpret_cast<const uint8_t*>(chunk2.data()), chunk2.length());
    ASSERT_EQ(events2.size(), 1u);
    ASSERT_EQ(events2[0].data, "hello world");
}

TEST(sse_parser_partial_across_multiple_chunks) {
    SSEParser parser;

    // Split "data: hello\n\n" across 3 chunks
    std::string c1 = "data: h";
    std::string c2 = "ello\n";
    std::string c3 = "\n";

    auto e1 = parser.feed(reinterpret_cast<const uint8_t*>(c1.data()), c1.length());
    auto e2 = parser.feed(reinterpret_cast<const uint8_t*>(c2.data()), c2.length());
    auto e3 = parser.feed(reinterpret_cast<const uint8_t*>(c3.data()), c3.length());

    ASSERT_EQ(e1.size(), 0u);
    ASSERT_EQ(e2.size(), 0u);
    ASSERT_EQ(e3.size(), 1u);
    ASSERT_EQ(e3[0].data, "hello");
}

TEST(sse_parser_flush_remaining) {
    SSEParser parser;
    std::string data = "data: hello";
    auto events = parser.feed(reinterpret_cast<const uint8_t*>(data.data()), data.length());
    ASSERT_EQ(events.size(), 0u);

    auto flushed = parser.flush();
    ASSERT_EQ(flushed.size(), 1u);
    ASSERT_EQ(flushed[0].data, "hello");
}

// ============================================================================
// Token Ring Buffer Tests
// ============================================================================

TEST(ring_buffer_basic_push_pop) {
    TokenRingBuffer buf(4);
    TokenInfo t1{"id1", "hello", 0, 0, 0, 5, false, TokenType::Text};
    TokenInfo t2{"id2", "world", 1, 0, 0, 5, false, TokenType::Text};

    ASSERT_TRUE(buf.push(t1));
    ASSERT_TRUE(buf.push(t2));
    ASSERT_EQ(buf.size(), 2u);

    TokenInfo out;
    ASSERT_TRUE(buf.pop(out));
    ASSERT_EQ(out.value, "hello");
    ASSERT_TRUE(buf.pop(out));
    ASSERT_EQ(out.value, "world");
    ASSERT_TRUE(buf.empty());
}

TEST(ring_buffer_overwrite_oldest) {
    TokenRingBuffer buf(2);
    TokenInfo t1{"id1", "a", 0, 0, 0, 1, false, TokenType::Text};
    TokenInfo t2{"id2", "b", 1, 0, 0, 1, false, TokenType::Text};
    TokenInfo t3{"id3", "c", 2, 0, 0, 1, false, TokenType::Text};

    buf.push(t1);
    buf.push(t2);
    buf.push(t3); // Overwrites t1

    ASSERT_EQ(buf.size(), 2u);

    TokenInfo out;
    ASSERT_TRUE(buf.pop(out));
    ASSERT_EQ(out.value, "b"); // t2 is now oldest
    ASSERT_TRUE(buf.pop(out));
    ASSERT_EQ(out.value, "c");
}

TEST(ring_buffer_snapshot) {
    TokenRingBuffer buf(4);
    for (int i = 0; i < 3; ++i) {
        buf.push(TokenInfo{"id", std::to_string(i), (uint32_t)i, 0, 0, 1, false, TokenType::Text});
    }

    auto snap = buf.snapshot();
    ASSERT_EQ(snap.size(), 3u);
    ASSERT_EQ(snap[0].value, "0");
    ASSERT_EQ(snap[1].value, "1");
    ASSERT_EQ(snap[2].value, "2");
}

TEST(ring_buffer_clear) {
    TokenRingBuffer buf(4);
    buf.push(TokenInfo{"id", "x", 0, 0, 0, 1, false, TokenType::Text});
    ASSERT_FALSE(buf.empty());

    buf.clear();
    ASSERT_TRUE(buf.empty());
    ASSERT_EQ(buf.size(), 0u);
}

// ============================================================================
// Token Classification Tests
// ============================================================================

TEST(classify_text) {
    ASSERT_EQ(TokenClassifier::classify("hello"), TokenType::Text);
    ASSERT_EQ(TokenClassifier::classify("HelloWorld"), TokenType::Text);
}

TEST(classify_whitespace) {
    ASSERT_EQ(TokenClassifier::classify(" "), TokenType::Whitespace);
    ASSERT_EQ(TokenClassifier::classify("\t"), TokenType::Whitespace);
    ASSERT_EQ(TokenClassifier::classify("  "), TokenType::Whitespace);
}

TEST(classify_newline) {
    ASSERT_EQ(TokenClassifier::classify("\n"), TokenType::Newline);
    ASSERT_EQ(TokenClassifier::classify("\r\n"), TokenType::Newline);
}

TEST(classify_punctuation) {
    ASSERT_EQ(TokenClassifier::classify(","), TokenType::Punctuation);
    ASSERT_EQ(TokenClassifier::classify("."), TokenType::Punctuation);
    ASSERT_EQ(TokenClassifier::classify("!"), TokenType::Punctuation);
}

TEST(classify_special) {
    ASSERT_EQ(TokenClassifier::classify("<tag>"), TokenType::Special);
    ASSERT_EQ(TokenClassifier::classify("[control]"), TokenType::Special);
}

// ============================================================================
// Token Splitter Tests
// ============================================================================

TEST(split_basic_words) {
    auto tokens = TokenSplitter::split("hello world");
    ASSERT_EQ(tokens.size(), 3u); // "hello", " ", "world"
    ASSERT_EQ(tokens[0], "hello");
    ASSERT_EQ(tokens[1], " ");
    ASSERT_EQ(tokens[2], "world");
}

TEST(split_punctuation) {
    auto tokens = TokenSplitter::split("Hello,world!");
    ASSERT_EQ(tokens.size(), 5u); // "Hello", ",", "world", "!"
    ASSERT_EQ(tokens[0], "Hello");
    ASSERT_EQ(tokens[1], ",");
    ASSERT_EQ(tokens[2], "world");
    ASSERT_EQ(tokens[3], "!");
}

TEST(split_multibyte_utf8) {
    auto tokens = TokenSplitter::split("Hello \xf0\x9f\x8e\x89 world"); // Hello 🎉 world
    ASSERT_GE(tokens.size(), 3u);
    // Find the emoji token
    bool found_emoji = false;
    for (const auto& t : tokens) {
        if (t == "\xf0\x9f\x8e\x89") {
            found_emoji = true;
            break;
        }
    }
    ASSERT_TRUE(found_emoji);
}

TEST(split_newline) {
    auto tokens = TokenSplitter::split("line1\nline2");
    bool found_newline = false;
    for (const auto& t : tokens) {
        if (t == "\n") {
            found_newline = true;
            break;
        }
    }
    ASSERT_TRUE(found_newline);
}

// ============================================================================
// JSON Content Extractor Tests
// ============================================================================

TEST(json_extract_content_basic) {
    std::string json = R"({"choices":[{"delta":{"content":"hello"}}]})";
    std::string content = JSONContentExtractor::extractContent(json);
    ASSERT_EQ(content, "hello");
}

TEST(json_extract_content_with_escapes) {
    std::string json = R"({"choices":[{"delta":{"content":"hello\nworld"}}]})";
    std::string content = JSONContentExtractor::extractContent(json);
    ASSERT_EQ(content, "hello\nworld");
}

TEST(json_extract_content_empty) {
    std::string json = R"({"choices":[{"delta":{}}]})";
    std::string content = JSONContentExtractor::extractContent(json);
    ASSERT_TRUE(content.empty());
}

TEST(json_extract_content_no_field) {
    std::string json = R"({"error":"model not found"})";
    std::string content = JSONContentExtractor::extractContent(json);
    ASSERT_TRUE(content.empty());
}

// ============================================================================
// ProductionTokenStreamHandler Integration Tests
// ============================================================================

TEST(handler_basic_stream) {
    ProductionTokenStreamHandler handler;
    handler.reset("test-msg-1");

    std::vector<TokenInfo> received;
    handler.onToken([&received](const TokenInfo& t, const StreamState&) {
        received.push_back(t);
    });

    handler.start();

    // Simulate SSE stream
    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
        "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n\n"
        "data: [DONE]\n\n";

    handler.feedChunk("test-msg-1",
        reinterpret_cast<const uint8_t*>(sse.data()), sse.length());

    // Give worker thread time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    ASSERT_GT(received.size(), 0u);

    handler.shutdown();
}

TEST(handler_state_tracking) {
    ProductionTokenStreamHandler handler;
    handler.reset("test-msg-2");
    handler.start();

    std::string sse =
        "data: {\"choices\":[{\"delta\":{\"content\":\"A\"}}]}\n\n";

    handler.feedChunk("test-msg-2",
        reinterpret_cast<const uint8_t*>(sse.data()), sse.length());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto state = handler.getState();
    ASSERT_EQ(state.message_id, "test-msg-2");
    ASSERT_EQ(state.mode, StreamMode::Single); // Default

    handler.shutdown();
}

// ============================================================================
// Performance Tests (validate no artificial delays)
// ============================================================================

TEST(performance_no_artificial_delay) {
    ProductionTokenStreamHandler handler;
    handler.reset("perf-test");

    std::atomic<uint32_t> token_count{0};
    handler.onToken([&token_count](const TokenInfo&, const StreamState&) {
        ++token_count;
    });

    handler.start();

    // Feed 100 tokens rapidly
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 100; ++i) {
        std::string sse = "data: {\"choices\":[{\"delta\":{\"content\":\"x\"}}]}\n\n";
        handler.feedChunk("perf-test",
            reinterpret_cast<const uint8_t*>(sse.data()), sse.length());
    }

    // Wait for processing
    while (token_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Should complete in < 2 seconds (no artificial 5-30ms delays per token)
    ASSERT_LT(ms, 2000);

    handler.shutdown();
}

TEST(performance_ring_buffer_o1) {
    // Validate O(1) push/pop by timing large operations
    TokenRingBuffer buf(10000);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 100000; ++i) {
        buf.push(TokenInfo{"id", "x", (uint32_t)i, 0, 0, 1, false, TokenType::Text});
    }

    auto elapsed = std::chrono::steady_clock::now() - start;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // 100K pushes should complete in < 1 second
    ASSERT_LT(ms, 1000);
    ASSERT_EQ(buf.size(), 10000u); // Capped at capacity
}

// ============================================================================
// Main
// ============================================================================

int main() {
    printf("========================================\n");
    printf("AI_TokenStream Production Test Suite\n");
    printf("========================================\n\n");

    printf("SSE Parser Tests:\n");
    RUN_TEST(sse_parser_basic_event);
    RUN_TEST(sse_parser_done_event);
    RUN_TEST(sse_parser_multiple_events);
    RUN_TEST(sse_parser_multiline_data);
    RUN_TEST(sse_parser_partial_line_preserved);
    RUN_TEST(sse_parser_partial_across_multiple_chunks);
    RUN_TEST(sse_parser_flush_remaining);

    printf("\nToken Ring Buffer Tests:\n");
    RUN_TEST(ring_buffer_basic_push_pop);
    RUN_TEST(ring_buffer_overwrite_oldest);
    RUN_TEST(ring_buffer_snapshot);
    RUN_TEST(ring_buffer_clear);

    printf("\nToken Classification Tests:\n");
    RUN_TEST(classify_text);
    RUN_TEST(classify_whitespace);
    RUN_TEST(classify_newline);
    RUN_TEST(classify_punctuation);
    RUN_TEST(classify_special);

    printf("\nToken Splitter Tests:\n");
    RUN_TEST(split_basic_words);
    RUN_TEST(split_punctuation);
    RUN_TEST(split_multibyte_utf8);
    RUN_TEST(split_newline);

    printf("\nJSON Content Extractor Tests:\n");
    RUN_TEST(json_extract_content_basic);
    RUN_TEST(json_extract_content_with_escapes);
    RUN_TEST(json_extract_content_empty);
    RUN_TEST(json_extract_content_no_field);

    printf("\nIntegration Tests:\n");
    RUN_TEST(handler_basic_stream);
    RUN_TEST(handler_state_tracking);

    printf("\nPerformance Tests:\n");
    RUN_TEST(performance_no_artificial_delay);
    RUN_TEST(performance_ring_buffer_o1);

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_tests_passed, g_tests_failed);
    printf("========================================\n");

    return g_tests_failed > 0 ? 1 : 0;
}
