// bench_deflate_masm.cpp — Compare custom gzip (stored block) vs zlib deflate
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <chrono>

#ifdef DEFLATE_NASM
extern "C" void* deflate_nasm(const void* src, size_t len, size_t* out_len, void* hash_buf);
#define HASH_SIZE (1u << 15)
#define deflate_custom deflate_nasm
#elif defined(DEFLATE_GODMODE)
extern "C" void* deflate_godmode(const void* src, size_t len, size_t* out_len, void* hash_buf);
#define HASH_SIZE 8192
#define deflate_custom deflate_godmode
#else
extern "C" void* deflate_masm(const void* src, size_t len, size_t* out_len);
#define deflate_custom deflate_masm
#endif

// zlib reference (optional)
#if !defined(NO_ZLIB_REF)
extern "C" {
    int compress2(unsigned char* dest, unsigned long* destLen, const unsigned char* source, unsigned long sourceLen, int level);
}
#endif

using clk = std::chrono::high_resolution_clock;

static std::string make_json_payload(size_t words, size_t avg_len) {
    std::string s;
    s.reserve(words * (avg_len + 8));
    s += "{\n  \"data\": [\n";
    for (size_t i = 0; i < words; ++i) {
        s += "    {\"id\": ";
        s += std::to_string(i);
        s += ", \"text\": \"";
        for (size_t j = 0; j < avg_len; ++j) {
            char c = static_cast<char>('a' + (j + i) % 26);
            s += c;
        }
        s += "\"}";
        if (i + 1 != words) s += ",";
        s += "\n";
    }
    s += "  ]\n}";
    return s;
}

static void run_case(const char* label, size_t words, size_t avg_len) {
    std::string json = make_json_payload(words, avg_len);
    const unsigned char* src = reinterpret_cast<const unsigned char*>(json.data());
    size_t src_len = json.size();
    
    printf("Benchmarking %s (size=%zu bytes)...\n", label, src_len);

    // Warmup
    size_t out_len = 0;
    void* hash_buf = malloc(HASH_SIZE * 4);
    void* out = deflate_custom(src, src_len, &out_len, hash_buf);
    if (out) free(out);

    // Measure
    auto t0 = clk::now();
    for (int i = 0; i < 10; ++i) {
        out = deflate_custom(src, src_len, &out_len, hash_buf);
        if (out) free(out);
    }
    auto t1 = clk::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / 10.0;
    
    printf("  Time: %.3f ms\n", ms);
    printf("  Throughput: %.2f MB/s\n", (src_len / 1024.0 / 1024.0) / (ms / 1000.0));
    
    free(hash_buf);
}

int main() {
    run_case("Small JSON", 100, 20);
    run_case("Medium JSON", 1000, 50);
    run_case("Large JSON", 10000, 100);
    return 0;
}
