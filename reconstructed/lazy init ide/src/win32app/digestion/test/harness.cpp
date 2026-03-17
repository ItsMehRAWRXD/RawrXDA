// digestion_test_harness.cpp
// Standalone test for RawrXD_DigestionEngine_Avx512
// No Qt dependencies—pure Win32 console

#include <windows.h>
#include <stdio.h>
#include <cstdlib>
#include <atomic>

// Forward declare the engine
extern "C" DWORD __stdcall RawrXD_DigestionEngine_Avx512(
    LPCWSTR wszSource,
    LPCWSTR wszOutput,
    DWORD   dwChunkSize,
    DWORD   dwThreads,
    DWORD   dwFlags,
    void (__stdcall *pfnProgress)(DWORD percent, DWORD taskId)
);

static std::atomic<int> g_lastPercent{-1};

// Progress callback
static void __stdcall ProgressCallback(DWORD percent, DWORD taskId)
{
    int prev = g_lastPercent.exchange(percent, std::memory_order_relaxed);
    if (percent != prev) {
        printf("[Task %lu] Progress: %lu%%\n", taskId, percent);
        fflush(stdout);
    }
}

int wmain(int argc, wchar_t* argv[])
{
    printf("=== RawrXD Digestion Engine Test Harness ===\n\n");

    if (argc < 3) {
        printf("Usage: digestion_test_harness.exe <source_file> <output_json>\n");
        printf("Example: digestion_test_harness.exe Win32IDE.cpp digestion_report.json\n");
        return 1;
    }

    LPCWSTR wszSource = argv[1];
    LPCWSTR wszOutput = argv[2];

    printf("Source:  %ws\n", wszSource);
    printf("Output:  %ws\n", wszOutput);
    printf("\nStarting digestion...\n\n");

    // Call the engine
    DWORD result = RawrXD_DigestionEngine_Avx512(
        wszSource,
        wszOutput,
        65536,      // dwChunkSize
        0,          // dwThreads (auto-detect)
        0x1 | 0x2,  // dwFlags
        ProgressCallback
    );

    printf("\n");
    if (result == 0) {
        printf("✓ Digestion completed successfully (result = %lu)\n", result);
        
        // Verify output file exists
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(wszOutput, GetFileExInfoStandard, &fad)) {
            printf("✓ Output file created: %llu bytes\n", 
                   ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow);
            
            // Read and display the JSON
            FILE* fp = nullptr;
            if (_wfopen_s(&fp, wszOutput, L"r") == 0 && fp) {
                printf("\n=== digestion_report.json ===\n");
                char buf[4096];
                while (fgets(buf, sizeof(buf), fp)) {
                    printf("%s", buf);
                }
                fclose(fp);
                printf("=== End Report ===\n");
            }
        } else {
            printf("✗ Output file not found (GetFileAttributesEx failed)\n");
            return 2;
        }
    } else {
        printf("✗ Digestion failed with error code: %lu (0x%08lx)\n", result, result);
        return result;
    }

    printf("\n✓ Validation PASSED\n");
    return 0;
}
