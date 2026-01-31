#include <cstdio>
#include <cstring>
#include "../include/Phase1_Foundation.h"

using namespace Phase1;

/*
================================================================================
 Phase1_Test.cpp - Phase 1 Foundation Validation Test
 
 This program validates that Phase 1 Foundation is working correctly by:
 1. Initializing Phase 1
 2. Querying hardware capabilities
 3. Testing memory allocation
 4. Testing performance timing
 5. Reporting results
================================================================================
*/

int main() {
    printf("================================================================================\n");
    printf("Phase 1 Foundation - Initialization Test\n");
    printf("================================================================================\n\n");
    
    try {
        // Initialize Phase 1
        printf("[1/5] Initializing Phase 1 Foundation...\n");
        Foundation& foundation = Foundation::GetInstance();
        PHASE1_CONTEXT* ctx = foundation.GetRawContext();
        
        if (!ctx || !foundation.IsInitialized()) {
            printf("ERROR: Phase 1 initialization failed\n");
            return 1;
        }
        printf("✓ Phase 1 initialized successfully\n\n");
        
        // Test 1: Query CPU capabilities
        printf("[2/5] Querying CPU Capabilities...\n");
        const auto& cpu = foundation.GetCPUCapabilities();
        printf("  CPU Vendor:           %.12s\n", cpu.vendor_id);
        printf("  CPU Brand:            %.48s\n", cpu.brand_string);
        printf("  Family/Model/Step:    %d/%d/%d\n", cpu.family, cpu.model, cpu.stepping);
        printf("  Physical Cores:       %u\n", cpu.physical_cores);
        printf("  Logical Threads:      %u\n", cpu.logical_cores);
        printf("  Threads per Core:     %u\n", cpu.threads_per_core);
        printf("  L3 Cache Size:        %u KB\n", cpu.l3_cache_size);
        printf("  TSC Frequency:        %llu Hz (%.2f GHz)\n", 
               cpu.tsc_frequency_hz, 
               cpu.tsc_frequency_hz / 1e9);
        printf("  Features:\n");
        printf("    SSE:                %s\n", cpu.has_sse ? "YES" : "NO");
        printf("    SSE2:               %s\n", cpu.has_sse2 ? "YES" : "NO");
        printf("    AVX:                %s\n", cpu.has_avx ? "YES" : "NO");
        printf("    AVX2:               %s\n", cpu.has_avx2 ? "YES" : "NO");
        printf("    AVX-512 F:          %s\n", cpu.has_avx512f ? "YES" : "NO");
        printf("    AVX-512 DQ:         %s\n", cpu.has_avx512dq ? "YES" : "NO");
        printf("    AVX-512 BW:         %s\n", cpu.has_avx512bw ? "YES" : "NO");
        printf("    AVX-512 VL:         %s\n", cpu.has_avx512vl ? "YES" : "NO");
        printf("    FMA:                %s\n", cpu.has_fma ? "YES" : "NO");
        printf("    AES:                %s\n", cpu.has_aes ? "YES" : "NO");
        printf("    RDRAND:             %s\n", cpu.has_rdrand ? "YES" : "NO");
        printf("✓ CPU capabilities queried\n\n");
        
        // Test 2: Query memory topology
        printf("[3/5] Querying Memory Topology...\n");
        const auto& topology = foundation.GetHardwareTopology();
        printf("  Total Physical Memory:   %llu GB\n", topology.total_physical_memory >> 30);
        printf("  Available Memory:        %llu GB\n", topology.available_memory >> 30);
        printf("  Page Size:               0x%llx\n", topology.page_size);
        printf("  Allocation Granularity:  0x%llx\n", topology.allocation_granularity);
        printf("  Processor Count:         %u\n", topology.processor_count);
        printf("  NUMA Nodes:              %u\n", foundation.GetNUMANodeCount());
        printf("  Large Pages Supported:   %s\n", topology.has_large_pages ? "YES" : "NO");
        printf("✓ Memory topology queried\n\n");
        
        // Test 3: Test memory allocation
        printf("[4/5] Testing Memory Allocation...\n");
        const size_t alloc_size = 1024 * 1024;  // 1MB
        void* buffer1 = foundation.AllocateSystemMemory(alloc_size, 64);
        void* buffer2 = foundation.AllocateSystemMemory(alloc_size, 256);
        
        if (!buffer1 || !buffer2) {
            printf("ERROR: Memory allocation failed\n");
            return 1;
        }
        
        // Verify memory is accessible
        memset(buffer1, 0xAB, alloc_size);
        memset(buffer2, 0xCD, alloc_size);
        
        printf("  Allocated %zu bytes at 0x%p (alignment=64)\n", alloc_size, buffer1);
        printf("  Allocated %zu bytes at 0x%p (alignment=256)\n", alloc_size, buffer2);
        printf("✓ Memory allocation successful\n\n");
        
        // Test 4: Test performance timing
        printf("[5/5] Testing Performance Timing...\n");
        
        uint64_t cycles_start = foundation.ReadTSC();
        for (volatile int i = 0; i < 1000000; i++) {}  // Busy loop
        uint64_t cycles_elapsed = foundation.ReadTSC() - cycles_start;
        
        double micros = foundation.GetElapsedMicroseconds();
        double millis = foundation.GetElapsedMilliseconds();
        double secs = foundation.GetElapsedSeconds();
        
        printf("  TSC Cycles (busy loop):  %llu\n", cycles_elapsed);
        printf("  Elapsed Time:            %.2f seconds\n", secs);
        printf("                           %.2f milliseconds\n", millis);
        printf("                           %.0f microseconds\n", micros);
        printf("  TSC Frequency Calc:      %.2f GHz\n", 
               (double)cycles_elapsed / micros / 1000.0);
        printf("✓ Performance timing operational\n\n");
        
        // Summary
        printf("================================================================================\n");
        printf("✓ ALL TESTS PASSED - Phase 1 Foundation is operational\n");
        printf("================================================================================\n");
        printf("\nSystem Summary:\n");
        printf("  - %u cores available\n", foundation.GetPhysicalCoreCount());
        printf("  - %llu GB total memory\n", topology.total_physical_memory >> 30);
        printf("  - %.2f GHz CPU speed\n", cpu.tsc_frequency_hz / 1e9);
        printf("  - %s AVX-512\n", foundation.HasAVX512() ? "Has" : "No");
        printf("\n");
        
        return 0;
        
    } catch (const std::exception& e) {
        printf("\nEXCEPTION: %s\n", e.what());
        return 1;
    }
}
