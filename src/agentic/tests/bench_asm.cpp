#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <numeric>
#include <immintrin.h>
#include <intrin.h>

namespace SovereignAssembler {
    using FindDelimiterFn = const char* (*)(const char* start, const char* end);

    const char* FindNextDelimiter_Scalar(const char* start, const char* end)
    {
        while (start < end)
        {
            const char c = *start;
            if (c == ' ' || c == '\t' || c == ',' || c == '\n' || c == '\r' || c == '\0' || c == ';' || c == '+' ||
                c == '-' || c == '*' || c == '[' || c == ']' || c == ':')
            {
                return start;
            }
            ++start;
        }
        return end;
    }

    const char* FindNextDelimiter_AVX2(const char* start, const char* end)
    {
        const size_t len = static_cast<size_t>(end - start);
        if (len < 32) return FindNextDelimiter_Scalar(start, end);

        while (start < end && (reinterpret_cast<uintptr_t>(start) & 31) != 0)
        {
            const char c = *start;
            if (c == ' ' || c == '\t' || c == ',' || c == '\n' || c == '\r' || c == '\0' || c == ';' || c == '+' ||
                c == '-' || c == '*' || c == '[' || c == ']' || c == ':')
                return start;
            ++start;
        }

        const __m256i v_spc = _mm256_set1_epi8(' ');
        const __m256i v_tab = _mm256_set1_epi8('\t');
        const __m256i v_com = _mm256_set1_epi8(',');
        const __m256i v_nl = _mm256_set1_epi8('\n');
        const __m256i v_cr = _mm256_set1_epi8('\r');
        const __m256i v_null = _mm256_set1_epi8('\0');
        const __m256i v_sc = _mm256_set1_epi8(';');
        const __m256i v_pls = _mm256_set1_epi8('+');
        const __m256i v_mns = _mm256_set1_epi8('-');
        const __m256i v_ast = _mm256_set1_epi8('*');
        const __m256i v_lbr = _mm256_set1_epi8('[');
        const __m256i v_rbr = _mm256_set1_epi8(']');
        const __m256i v_col = _mm256_set1_epi8(':');

        while (start + 32 <= end)
        {
            __m256i v = _mm256_load_si256(reinterpret_cast<const __m256i*>(start));
            __m256i m = _mm256_cmpeq_epi8(v, v_spc);
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_tab));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_com));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_nl));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_cr));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_null));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_sc));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_pls));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_mns));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_ast));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_lbr));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_rbr));
            m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_col));

            uint32_t mask = _mm256_movemask_epi8(m);
            if (mask != 0)
            {
                unsigned long bitpos;
                _BitScanForward(&bitpos, mask);
                return start + bitpos;
            }
            start += 32;
        }
        return FindNextDelimiter_Scalar(start, end);
    }
}

int main() {
    const size_t bufferSize = 1 * 1024 * 1024; // 1MB per iteration
    std::string source;
    std::string line = "mov rax, 0";
    line.append(100, ' ');
    line.append("\n"); // delimiter

    source.resize(bufferSize, ' '); 
    for(size_t i = 0; i < bufferSize; i += 128) {
        if (i + line.size() < bufferSize)
            std::memcpy(&source[i], line.data(), line.size());
    }

    const double totalMB = (bufferSize * 50) / (1024.0*1024.0);
    std::cout << "Benchmarking delimiter scanning: " << totalMB << "MB processed" << std::endl;

    const int iterations = 50;

    auto bench = [&](const char* name, SovereignAssembler::FindDelimiterFn fn) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            const char* cur = source.data();
            const char* end = cur + source.size();
            while (cur < end) {
                while(cur < end && (*cur == ' ' || *cur == '\t')) cur++;
                if (cur >= end) break;
                cur = fn(cur, end);
                if (cur < end) cur++; // skip the delimiter itself to avoid infinite loop
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << name << ": " << duration << " ms" << std::endl;
        return duration;
    };

    auto scalar_time = bench("Scalar Scanner", SovereignAssembler::FindNextDelimiter_Scalar);
    auto avx2_time = bench("AVX2 Scanner", SovereignAssembler::FindNextDelimiter_AVX2);

    std::cout << "--- Speedup: " << (double)scalar_time / (avx2_time > 0 ? avx2_time : 1) << "x ---" << std::endl;

    // Write report to file
    {
        std::ofstream rf("bench_asm_report.txt", std::ios::trunc);
        if (rf.is_open()) {
            double speedup = (double)scalar_time / (avx2_time > 0 ? avx2_time : 1);
            rf << "RawrXD AVX2 Tokenizer Benchmark" << std::endl;
            rf << "scalar_ms=" << scalar_time << std::endl;
            rf << "avx2_ms=" << avx2_time << std::endl;
            rf << "speedup=" << speedup << "x" << std::endl;
            rf << "total_mb=" << totalMB << std::endl;
        }
    }

    return 0;
}

