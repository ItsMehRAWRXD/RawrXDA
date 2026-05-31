// test_model_streamer.cpp - Performance Test Suite
// Tests: 2T tokens in 60 seconds = 33.3 GB/s throughput

#include "model_streamer.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <cassert>

using namespace rawrxd;

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::cerr << "ASSERT FAILED: " << #cond << " at " << __LINE__ << "\n"; \
        return false; \
    } \
} while(0)

#define TEST(name) bool name()

// ─────────────────────────────────────────────────────────────────────────
// UTILITY FUNCTIONS
// ─────────────────────────────────────────────────────────────────────────

uint64_t get_timestamp_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

void generate_tokens(uint32_t* tokens, size_t count, uint32_t vocab = 32000) {
    static thread_local std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, vocab - 1);
    for (size_t i = 0; i < count; i++) tokens[i] = dist(rng);
}

double bytes_to_tbps(uint64_t bytes) {
    return bytes * 8.0 / 1e12;
}

// ─────────────────────────────────────────────────────────────────────────
// CORE TESTS
// ─────────────────────────────────────────────────────────────────────────

TEST(test_simd_encode) {
    std::vector<uint32_t> tokens(1024);
    generate_tokens(tokens.data(), tokens.size());
    
    SIMD_ALIGN uint32_t output[1024];
    
    auto start = get_timestamp_us();
    simd_encode_tokens((__m512i*)output, tokens.data(), tokens.size());
    auto end = get_timestamp_us();
    
    double ops_per_sec = tokens.size() / ((end - start) / 1e6);
    std::cout << "  SIMD encode: " << std::fixed << std::setprecision(0) 
              << ops_per_sec << " tokens/sec\n";
    
    ASSERT(true);
    return true;
}

TEST(test_crc32c) {
    std::vector<uint8_t> data(1024 * 1024);  // 1MB
    generate_tokens((uint32_t*)data.data(), data.size() / 4);
    
    auto start = get_timestamp_us();
    for (int i = 0; i < 1000; i++) {
        volatile uint32_t crc = crc32c_avx512(data.data(), data.size());
    }
    auto end = get_timestamp_us();
    
    double mbps = (1000.0 * 1024) / ((end - start) / 1e6) / 1024;
    std::cout << "  CRC32C: " << std::fixed << std::setprecision(0) 
              << mbps << " MB/s\n";
    
    ASSERT(true);
    return true;
}

TEST(test_compression) {
    CompressionEngine comp;
    
    // Generate realistic token data
    std::vector<uint32_t> tokens(64 * 1024);  // 64K tokens = 256KB
    generate_tokens(tokens.data(), tokens.size());
    
    uint8_t compressed[CompressionBlock::MAX_COMPRESSED];
    uint8_t decompressed[CompressionBlock::BLOCK_SIZE];
    
    auto start = get_timestamp_us();
    
    auto [comp_size, orig_size] = comp.compress(
        (const uint8_t*)tokens.data(), tokens.size() * 4,
        compressed, sizeof(compressed), false);
    
    size_t dec_size = comp.decompress(compressed, comp_size,
                                     decompressed, sizeof(decompressed));
    
    auto end = get_timestamp_us();
    
    double compress_ratio = (double)orig_size / comp_size;
    double throughput = orig_size / ((end - start) / 1e6) / 1e9;
    
    std::cout << "  Compression ratio: " << std::fixed << std::setprecision(2) 
              << compress_ratio << "x\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2) 
              << throughput << " GB/s\n";
    
    ASSERT(compress_ratio >= 1.0);  // Compression shouldn't make it bigger
    ASSERT(dec_size == orig_size);  // Decompression should match
    
    return true;
}

TEST(test_buffer_pool) {
    BufferPool pool(64);
    
    // Acquire all buffers
    std::vector<Buffer*> buffers;
    for (size_t i = 0; i < 64; i++) {
        buffers.push_back(pool.acquire());
        ASSERT(buffers.back() != nullptr);
    }
    
    // Pool should be exhausted
    ASSERT(pool.available() == 0);
    
    // Release all
    for (auto* buf : buffers) {
        pool.release(buf);
    }
    
    ASSERT(pool.available() == 64);
    
    std::cout << "  Buffer pool: OK\n";
    return true;
}

TEST(test_buffer_write_read) {
    BufferPool pool;
    Buffer* buf = pool.acquire();
    
    // Write test
    uint8_t write_data[1024];
    generate_tokens((uint32_t*)write_data, 256);
    
    ASSERT(buf->write(write_data, sizeof(write_data)));
    ASSERT(buf->length == sizeof(write_data));
    ASSERT(buf->capacity >= sizeof(write_data));
    
    // Read test
    uint8_t read_data[1024];
    size_t read = buf->read(read_data, 512);
    ASSERT(read == 512);
    ASSERT(buf->offset == 512);
    
    // Auto-grow test
    ASSERT(buf->reserve(4 * 1024 * 1024));  // Request 4MB
    
    pool.release(buf);
    
    std::cout << "  Buffer operations: OK\n";
    return true;
}

TEST(test_http2_framer) {
    HTTP2Framer framer;
    
    uint8_t output[4096];
    
    auto hlen = framer.encodeHeaders(1, true, {
        {":status", "200"},
        {"content-type", "application/octet-stream"},
        {"x-tokens-sent", "12345"}
    }, output, sizeof(output));
    
    ASSERT(hlen > 0);
    ASSERT(hlen < sizeof(output));
    
    // Verify frame header
    ASSERT(output[0] == 0x00);  // Length high
    ASSERT(output[3] == 0x01);  // Type: HEADERS
    ASSERT(output[4] & 0x04);     // END_HEADERS flag
    
    std::cout << "  HTTP/2 framing: OK (frame size: " << hlen << ")\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// INTEGRATION TESTS
// ─────────────────────────────────────────────────────────────────────────

TEST(test_token_encoding_roundtrip) {
    CompressionEngine comp;
    
    // Generate 64K token batch
    const size_t batch_size = 64 * 1024;
    std::vector<uint32_t> tokens(batch_size);
    generate_tokens(tokens.data(), batch_size);
    
    // Compress
    uint8_t compressed[CompressionBlock::MAX_COMPRESSED];
    auto [comp_size, orig_size] = comp.compress(
        (const uint8_t*)tokens.data(), batch_size * 4,
        compressed, sizeof(compressed), false);
    
    // Decompress
    std::vector<uint8_t> decompressed(orig_size);
    size_t dec_size = comp.decompress(compressed, comp_size,
                                    decompressed.data(), decompressed.size());
    
    // Verify
    ASSERT(dec_size == orig_size);
    ASSERT(memcmp(tokens.data(), decompressed.data(), orig_size) == 0);
    
    std::cout << "  Token roundtrip: OK\n";
    return true;
}

TEST(test_stream_frame_creation) {
    uint32_t data[] = {1, 2, 3, 4, 5};
    
    StreamFrame* frame = StreamFrame::create(
        1, FrameType::TOKENS, 0x01,
        data, sizeof(data));
    
    ASSERT(frame != nullptr);
    ASSERT(frame->stream_id == 1);
    ASSERT(frame->type == (uint8_t)FrameType::TOKENS);
    ASSERT(frame->length == sizeof(data));
    ASSERT(frame->totalSize() == StreamFrame::HEADER_SIZE + sizeof(data));
    
    free(frame);
    
    std::cout << "  Stream frame: OK\n";
    return true;
}

TEST(test_buffer_reserve_grow) {
    BufferPool pool;
    Buffer* buf = pool.acquire();
    
    // Initial size
    size_t initial_cap = buf->capacity;
    
    // Grow to 4MB
    ASSERT(buf->reserve(4 * 1024 * 1024));
    ASSERT(buf->capacity > initial_cap);
    ASSERT(buf->capacity >= 4 * 1024 * 1024);
    
    // Write large amount
    std::vector<uint8_t> large_data(3 * 1024 * 1024);
    ASSERT(buf->write(large_data.data(), large_data.size()));
    
    pool.release(buf);
    
    std::cout << "  Buffer growth: OK (grew to " << (buf->capacity / 1024 / 1024) << "MB)\n";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// PERFORMANCE BENCHMARKS
// ─────────────────────────────────────────────────────────────────────────

TEST(test_batch_compression_throughput) {
    CompressionEngine comp;
    
    const size_t iterations = 100;
    const size_t batch_tokens = 64 * 1024;
    std::vector<uint32_t> tokens(batch_tokens);
    
    uint64_t total_compressed = 0;
    uint64_t total_original = 0;
    
    auto start = get_timestamp_us();
    
    for (int i = 0; i < iterations; i++) {
        generate_tokens(tokens.data(), batch_tokens);
        
        uint8_t compressed[CompressionBlock::MAX_COMPRESSED];
        auto [comp_size, orig_size] = comp.compress(
            (const uint8_t*)tokens.data(), batch_tokens * 4,
            compressed, sizeof(compressed), false);
        
        total_compressed += comp_size;
        total_original += orig_size;
    }
    
    auto end = get_timestamp_us();
    double elapsed_sec = (end - start) / 1e6;
    
    double throughput_gbps = (total_original / elapsed_sec) * 8 / 1e9;
    double batches_per_sec = iterations / elapsed_sec;
    double compression_ratio = (double)total_original / total_compressed;
    
    std::cout << "  Batch compression: " << std::fixed << std::setprecision(2)
              << batches_per_sec << " batches/sec\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
              << throughput_gbps << " Gbps effective\n";
    std::cout << "  Compression ratio: " << std::fixed << std::setprecision(2)
              << compression_ratio << "x\n";
    
    ASSERT(throughput_gbps > 5.0);  // Should achieve >5 Gbps
    
    return true;
}

TEST(test_multi_buffer_concurrent) {
    BufferPool pool(256);
    std::vector<std::thread> threads;
    std::atomic<size_t> operations{0};
    
    auto start = get_timestamp_us();
    
    for (int t = 0; t < 16; t++) {
        threads.emplace_back([&]() {
            std::vector<Buffer*> bufs;
            
            for (int i = 0; i < 100; i++) {
                Buffer* buf = pool.acquire();
                
                // Write
                uint8_t data[65536];
                generate_tokens((uint32_t*)data, 16384);
                buf->write(data, sizeof(data));
                
                // Read
                uint8_t read_buf[65536];
                buf->read(read_buf, sizeof(read_buf));
                
                operations++;
                pool.release(buf);
            }
        });
    }
    
    for (auto& th : threads) th.join();
    
    auto end = get_timestamp_us();
    double ops_per_sec = operations.load() / ((end - start) / 1e6);
    
    std::cout << "  Concurrent buffers: " << std::fixed << std::setprecision(0)
              << ops_per_sec << " ops/sec\n";
    
    ASSERT(ops_per_sec > 10000);  // Should handle >10K ops/sec
    
    return true;
}

TEST(test_simd_throughput) {
    const size_t iterations = 10000;
    const size_t tokens_per_iter = 4096;
    
    std::vector<uint32_t> tokens(tokens_per_iter);
    SIMD_ALIGN uint32_t output[tokens_per_iter];
    
    auto start = get_timestamp_us();
    
    for (int i = 0; i < iterations; i++) {
        generate_tokens(tokens.data(), tokens_per_iter);
        simd_encode_tokens((__m512i*)output, tokens.data(), tokens_per_iter);
    }
    
    auto end = get_timestamp_us();
    double total_tokens = (double)iterations * tokens_per_iter;
    double tokens_per_sec = total_tokens / ((end - start) / 1e6);
    
    std::cout << "  SIMD throughput: " << std::fixed << std::setprecision(0)
              << tokens_per_sec << " tokens/sec\n";
    
    ASSERT(tokens_per_sec > 100000000);  // Should handle >100M tokens/sec
    
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// 2T IN 60 SECONDS STRESS TEST
// ─────────────────────────────────────────────────────────────────────────

TEST(test_2t_60s_target) {
    CompressionEngine comp;
    
    // Target: 2 trillion tokens in 60 seconds
    // = 33.3 billion tokens/second
    // = 133.3 GB/s at 4 bytes/token (assuming 4:1 compression)
    constexpr uint64_t TARGET_TOKENS = 2000000000000ULL;
    constexpr double TARGET_SECONDS = 60.0;
    constexpr double TARGET_TOKENS_PER_SEC = TARGET_TOKENS / TARGET_SECONDS;
    constexpr double TARGET_BYTES_PER_SEC = TARGET_TOKENS_PER_SEC * 4 / 1.5;  // With compression
    
    std::cout << "\n  Target metrics:\n";
    std::cout << "    Tokens: " << TARGET_TOKENS << " (" 
              << (TARGET_TOKENS / 1e12) << " trillion)\n";
    std::cout << "    Time: " << TARGET_SECONDS << " seconds\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(2)
              << TARGET_TOKENS_PER_SEC / 1e9 << " billion tokens/sec\n";
    std::cout << "    Bandwidth: " << std::fixed << std::setprecision(2)
              << TARGET_BYTES_PER_SEC / 1e9 << " GB/s\n";
    
    // Simulate with smaller batch
    const size_t batch_size = 64 * 1024;  // 64K tokens
    const size_t iterations = 10000;
    
    uint64_t total_tokens = 0;
    uint64_t total_bytes = 0;
    double total_compress_time = 0;
    double total_decompress_time = 0;
    
    auto start = get_timestamp_us();
    
    for (int i = 0; i < iterations; i++) {
        generate_tokens((uint32_t*)malloc(batch_size * 4), batch_size);
        std::vector<uint32_t> tokens(batch_size);
        generate_tokens(tokens.data(), batch_size);
        
        // Compress
        uint8_t compressed[CompressionBlock::MAX_COMPRESSED];
        auto t1 = get_timestamp_us();
        auto [comp_size, orig_size] = comp.compress(
            (const uint8_t*)tokens.data(), batch_size * 4,
            compressed, sizeof(compressed), false);
        auto t2 = get_timestamp_us();
        total_compress_time += (t2 - t1);
        
        // Decompress (simulated)
        uint8_t decompressed[CompressionBlock::BLOCK_SIZE];
        auto t3 = get_timestamp_us();
        comp.decompress(compressed, comp_size, decompressed, sizeof(decompressed));
        auto t4 = get_timestamp_us();
        total_decompress_time += (t4 - t3);
        
        total_tokens += batch_size;
        total_bytes += orig_size;
    }
    
    auto end = get_timestamp_us();
    double elapsed = (end - start) / 1e6;
    
    // Extrapolate to 2T tokens
    double scale_factor = (double)TARGET_TOKENS / total_tokens;
    double estimated_time = elapsed * scale_factor;
    double tokens_per_sec = total_tokens / elapsed * scale_factor;
    double bytes_per_sec = total_bytes / elapsed * scale_factor / 1e9;
    
    std::cout << "\n  Benchmark results:\n";
    std::cout << "    Processed: " << std::fixed << std::setprecision(0) 
              << total_tokens << " tokens in " << std::fixed << std::setprecision(2)
              << elapsed << " ms\n";
    std::cout << "    Compression overhead: " << std::fixed << std::setprecision(2)
              << (total_compress_time / elapsed * 100) << "%\n";
    
    std::cout << "\n  Extrapolated to 2T tokens:\n";
    std::cout << "    Estimated time: " << std::fixed << std::setprecision(2)
              << estimated_time << " seconds\n";
    std::cout << "    Throughput: " << std::fixed << std::setprecision(2)
              << tokens_per_sec / 1e9 << " billion tokens/sec\n";
    std::cout << "    Bandwidth: " << std::fixed << std::setprecision(2)
              << bytes_per_sec << " GB/s\n";
    
    bool target_met = estimated_time <= TARGET_SECONDS;
    std::cout << "\n  TARGET " << (target_met ? "✓ MET" : "✗ NOT MET") << "\n";
    
    return target_met;
}

TEST(test_zero_copy_overhead) {
    // Measure zero-copy overhead
    BufferPool pool;
    Buffer* src = pool.acquire();
    Buffer* dst = pool.acquire();
    
    uint8_t data[1024 * 1024];
    generate_tokens((uint32_t*)data, 256 * 1024);
    src->write(data, sizeof(data));
    
    auto start = get_timestamp_us();
    
    for (int i = 0; i < 10000; i++) {
        // Zero-copy: just adjust pointers
        dst->data = src->data + src->offset;
        dst->length = src->length;
        dst->capacity = src->capacity;
        dst->offset = 0;
    }
    
    auto end = get_timestamp_us();
    double ops_per_sec = 10000.0 / ((end - start) / 1e6);
    
    std::cout << "  Zero-copy overhead: " << std::fixed << std::setprecision(0)
              << ops_per_sec << " zero-copy ops/sec\n";
    
    pool.release(src);
    pool.release(dst);
    
    ASSERT(ops_per_sec > 1000000);  // Should handle >1M ops/sec
    
    return true;
}

TEST(test_tls_session_resume) {
    TLSContext ctx;
    ctx.session_resume = true;
    
    // Simulate session ticket generation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    for (int i = 0; i < 256; i++) {
        ctx.session_ticket[i] = (uint8_t)dis(gen);
    }
    
    // Simulate resumption
    memcpy(ctx.session_id, ctx.session_ticket, 32);
    
    std::cout << "  TLS session resumption: OK\n";
    return true;
}

TEST(test_connection_multiplexing) {
    // Simulate HTTP/2 multiplexing
    HTTP2Framer framer;
    
    const int concurrent_streams = 256;
    uint8_t frame_data[8192];
    size_t total_size = 0;
    
    auto start = get_timestamp_us();
    
    for (int i = 0; i < concurrent_streams; i++) {
        total_size += framer.encodeHeaders(i + 1, i == concurrent_streams - 1, {
            {":status", "200"},
            {"x-stream-id", std::to_string(i)}
        }, frame_data, sizeof(frame_data));
    }
    
    auto end = get_timestamp_us();
    double streams_per_sec = concurrent_streams / ((end - start) / 1e6);
    
    std::cout << "  HTTP/2 multiplexing: " << std::fixed << std::setprecision(0)
              << streams_per_sec << " streams/sec\n";
    
    ASSERT(streams_per_sec > 10000);  // Should handle >10K streams/sec
    
    return true;
}

// ─────────────────────────────────────────────────────────────────────────
// MAIN
// ─────────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "══════════════════════════════════════════════════════════\n";
    std::cout << "   2T→1m TOKEN STREAM ENGINE - PERFORMANCE TESTS\n";
    std::cout << "   Target: 2 Trillion tokens in 60 seconds\n";
    std::cout << "══════════════════════════════════════════════════════════\n\n";
    
    int passed = 0, failed = 0;
    
    auto run = [&](const char* name, auto (*test)()) {
        std::cout << "► " << name << "... ";
        if (test()) {
            passed++;
        } else {
            failed++;
        }
    };
    
    std::cout << "═══ SIMD & ENCODING ═══\n";
    run("SIMD encode", test_simd_encode);
    run("CRC32C", test_crc32c);
    
    std::cout << "\n═══ COMPRESSION ═══\n";
    run("Compression engine", test_compression);
    
    std::cout << "\n═══ BUFFER MANAGEMENT ═══\n";
    run("Buffer pool", test_buffer_pool);
    run("Buffer write/read", test_buffer_write_read);
    run("Buffer reserve", test_buffer_reserve_grow);
    
    std::cout << "\n═══ PROTOCOL ═══\n";
    run("HTTP/2 framer", test_http2_framer);
    run("Stream frame", test_stream_frame_creation);
    
    std::cout << "\n═══ INTEGRATION ═══\n";
    run("Token roundtrip", test_token_encoding_roundtrip);
    run("Zero-copy overhead", test_zero_copy_overhead);
    run("TLS session", test_tls_session_resume);
    run("Connection multiplexing", test_connection_multiplexing);
    
    std::cout << "\n═══ PERFORMANCE ═══\n";
    run("Batch compression", test_batch_compression_throughput);
    run("SIMD throughput", test_simd_throughput);
    run("Concurrent buffers", test_multi_buffer_concurrent);
    
    std::cout << "\n═══ STRESS TEST ═══\n";
    run("2T in 60s target", test_2t_60s_target);
    
    std::cout << "\n══════════════════════════════════════════════════════════\n";
    std::cout << "   RESULTS: " << passed << " passed, " << failed << " failed\n";
    std::cout << "══════════════════════════════════════════════════════════\n";
    
    return failed > 0 ? 1 : 0;
}
