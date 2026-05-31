#pragma once
#include <vector>
#include <cstdint>
#include <iostream>
#include <chrono>

namespace RawrXD::Memory {

enum class DiagnosticMode {
    QUICK,
    NORMAL,
    THOROUGH
};

struct DiagnosticReport {
    bool passed;
    uint64_t errorsFound;
    std::vector<uint64_t> errorAddresses;
    double throughputMBs;
};

class MemoryDiagnosticEngine {
public:
    DiagnosticReport runTest(DiagnosticMode mode, void* baseAddress, uint64_t size) {
        DiagnosticReport report = {true, 0, {}, 0.0};
        uint32_t iterations = (mode == DiagnosticMode::THOROUGH) ? 10 : (mode == DiagnosticMode::NORMAL) ? 3 : 1;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        volatile uint64_t* ptr = static_cast<volatile uint64_t*>(baseAddress);
        uint64_t count = size / sizeof(uint64_t);

        for (uint32_t i = 0; i < iterations; ++i) {
            // Pattern 1: Walking Bits
            uint64_t pattern = 0xAAAAAAAAAAAAAAAAULL;
            if (i % 2 == 1) pattern = 0x5555555555555555ULL;

            for (uint64_t j = 0; j < count; ++j) {
                ptr[j] = pattern;
            }

            for (uint64_t j = 0; j < count; ++j) {
                if (ptr[j] != pattern) {
                    report.passed = false;
                    report.errorsFound++;
                    if (report.errorAddresses.size() < 100) {
                        report.errorAddresses.push_back(reinterpret_cast<uint64_t>(&ptr[j]));
                    }
                }
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> diff = end - start;
        report.throughputMBs = (static_cast<double>(size) * iterations) / (1024.0 * 1024.0 * diff.count());

        return report;
    }
};

}
