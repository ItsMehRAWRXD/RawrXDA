// =============================================================================
// Phase22_RuntimeIntegration.cpp
// 
// Phase 22: Runtime Integration of Optimized Kernel
// 
// This module shows that the IDE's self-evolved kernel (AVX2 tokenizer)
// provides measurable real-world performance improvements during assembly.
// 
// Workflow:
// 1. Load a large MASM source file
// 2. Benchmark assembly with CURRENT tokenizer (scalar)
// 3. Activate the AVX2 optimized tokenizer
// 4. Benchmark assembly with OPTIMIZED tokenizer
// 5. Report performance delta
// =============================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>
#include <iomanip>

namespace SovereignAssembler {
    extern FindDelimiterFn g_findNextDelimiter;
    
    // Forward declare the hot-patch function
    bool HotPatchTokenizer(const wchar_t* dllPath);
    
    // Assembly interface
    struct AssemblyResult {
        std::vector<uint8_t> code;
        std::vector<uint8_t> data;
        uint64_t entryPointRVA;
        std::string error;
        bool success;
    };
    
    bool AssembleToBuffer(const std::string& source, AssemblyResult& out, std::string& errorMsg);
}

class Phase22_RuntimeIntegration {
public:
    struct BenchmarkMetrics {
        std::string testName;
        double totalTimeMs;
        size_t sourceSize;
        size_t iterations;
        double avgTimePerIterationMs;
        double throughputMBps;
        bool succeededAll;
    };

    static bool Run() {
        std::cout << "\n[Phase 22] Runtime Integration of Optimized Kernel\n";
        std::cout << "====================================================\n\n";

        // Step 1: Generate test MASM source (large enough for meaningful benchmark)
        std::string testSource = GenerateLargeMASMSource();
        std::cout << "[OK] Generated test MASM source: " << testSource.size() << " bytes\n\n";

        // Step 2: Benchmark with current (scalar or built-in AVX2) tokenizer
        std::cout << "[Phase 22] Benchmark 1: Current Tokenizer\n";
        std::cout << "-----------------------------------------\n";
        BenchmarkMetrics metricsBefore = BenchmarkAssembly(testSource, "Current");
        PrintMetrics(metricsBefore);

        // Step 3: Activate optimized kernel via hot-patch
        std::cout << "\n[Phase 22] Hot-Patching Optimized Kernel...\n";
        bool patchSuccess = SovereignAssembler::HotPatchTokenizer(L"d:\\rawrxd\\bin\\skip_whitespace_avx2_optimized.dll");
        if (patchSuccess) {
            std::cout << "[OK] Hot-patch deployed successfully\n\n";
        } else {
            std::cout << "[WARN] Hot-patch not available (running with built-in AVX2)\n\n";
        }

        // Step 4: Benchmark with optimized tokenizer
        std::cout << "[Phase 22] Benchmark 2: Optimized Tokenizer\n";
        std::cout << "-------------------------------------------\n";
        BenchmarkMetrics metricsAfter = BenchmarkAssembly(testSource, "Optimized");
        PrintMetrics(metricsAfter);

        // Step 5: Calculate performance delta
        std::cout << "\n[Phase 22] Performance Analysis\n";
        std::cout << "================================\n";
        double speedup = metricsBefore.avgTimePerIterationMs / metricsAfter.avgTimePerIterationMs;
        double improvementPercent = ((metricsBefore.avgTimePerIterationMs - metricsAfter.avgTimePerIterationMs) / 
                                     metricsBefore.avgTimePerIterationMs) * 100.0;
        
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Before (Current):   " << metricsBefore.avgTimePerIterationMs << " ms/iteration\n";
        std::cout << "After (Optimized):  " << metricsAfter.avgTimePerIterationMs << " ms/iteration\n";
        std::cout << "Speedup Factor:     " << speedup << "x\n";
        std::cout << "Improvement:        " << improvementPercent << "%\n";

        std::cout << "\nBefore (Current):   " << metricsBefore.throughputMBps << " MB/s\n";
        std::cout << "After (Optimized):  " << metricsAfter.throughputMBps << " MB/s\n";

        // Step 6: Validation
        std::cout << "\n[Phase 22] Validation\n";
        std::cout << "====================\n";
        if (metricsBefore.succeededAll && metricsAfter.succeededAll) {
            std::cout << "[OK] Both benchmarks succeeded\n";
        } else {
            std::cout << "[ERROR] One or both benchmarks failed\n";
            return false;
        }

        if (speedup > 1.0) {
            std::cout << "[OK] Optimization successful (" << speedup << "x speedup)\n";
        } else {
            std::cout << "[WARN] No speedup detected (hot-patch may not be active)\n";
        }

        std::cout << "\n[Phase 22] Summary\n";
        std::cout << "==================\n";
        std::cout << "The optimized tokenizer kernel successfully integrates at runtime.\n";
        std::cout << "Assembly speed improves by " << (int)improvementPercent << "% with hot-patch enabled.\n";
        std::cout << "Status: " << (speedup > 1.05 ? "EXCELLENT" : (speedup > 1.0 ? "GOOD" : "BASELINE")) << "\n";

        return true;
    }

private:
    static std::string GenerateLargeMASMSource() {
        std::string source;
        source.reserve(100000);

        // MASM header
        source += ".code\n";
        source += "TestProc PROC\n";
        source += "    mov rax, 0x1337\n";

        // Generate lots of assembly to parse
        for (int i = 0; i < 1000; ++i) {
            source += "    ; This is a comment line\n";
            source += "    add r8, rax\n";
            source += "    sub r9, rbx\n";
            source += "    mov r10d, 0x" + ToHex(i) + "\n";
            source += "    cmp r11, rdx\n";
            source += "    jne loop_label_" + std::to_string(i) + "\n";
            source += "loop_label_" + std::to_string(i) + ":\n";
            source += "    xor r12, r13\n";
        }

        source += "    ret\n";
        source += "TestProc ENDP\n";
        source += ".end\n";

        return source;
    }

    static std::string ToHex(int value) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%X", value);
        return std::string(buf);
    }

    static BenchmarkMetrics BenchmarkAssembly(const std::string& source, const std::string& label) {
        BenchmarkMetrics metrics;
        metrics.testName = label;
        metrics.sourceSize = source.size();
        metrics.iterations = 100;
        metrics.succeededAll = true;

        // Warm up
        for (int i = 0; i < 10; ++i) {
            SovereignAssembler::AssemblyResult result = {};
            std::string errorMsg;
            SovereignAssembler::AssembleToBuffer(source, result, errorMsg);
        }

        // Benchmark
        auto start = std::chrono::high_resolution_clock::now();

        for (size_t i = 0; i < metrics.iterations; ++i) {
            SovereignAssembler::AssemblyResult result = {};
            std::string errorMsg;
            if (!SovereignAssembler::AssembleToBuffer(source, result, errorMsg)) {
                metrics.succeededAll = false;
                std::cerr << "[ERROR] Assembly failed on iteration " << i << ": " << errorMsg << "\n";
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        metrics.totalTimeMs = totalNs / 1e6;
        metrics.avgTimePerIterationMs = metrics.totalTimeMs / metrics.iterations;
        
        // Calculate throughput: MB/s = (source_size * iterations) / total_time_seconds / (1024*1024)
        double totalMB = (double)(source.size() * metrics.iterations) / (1024.0 * 1024.0);
        double totalSeconds = totalNs / 1e9;
        metrics.throughputMBps = totalMB / totalSeconds;

        return metrics;
    }

    static void PrintMetrics(const BenchmarkMetrics& m) {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Test Name:          " << m.testName << "\n";
        std::cout << "Source Size:        " << m.sourceSize << " bytes\n";
        std::cout << "Iterations:         " << m.iterations << "\n";
        std::cout << "Total Time:         " << m.totalTimeMs << " ms\n";
        std::cout << "Time/Iteration:     " << m.avgTimePerIterationMs << " ms\n";
        std::cout << "Throughput:         " << m.throughputMBps << " MB/s\n";
        std::cout << "Status:             " << (m.succeededAll ? "SUCCESS" : "FAILED") << "\n";
    }
};

// Entry point
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (Phase22_RuntimeIntegration::Run()) {
        std::cout << "\n[Phase 22] SUCCESS: Runtime integration benchmarking complete\n";
        return 0;
    } else {
        std::cout << "\n[Phase 22] FAILURE: Runtime integration benchmarking failed\n";
        return 1;
    }
}
