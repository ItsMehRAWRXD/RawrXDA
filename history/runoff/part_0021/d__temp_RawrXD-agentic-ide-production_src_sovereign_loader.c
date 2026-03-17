// sovereign_loader.c
// RawrXD Sovereign Loader - C Kernel Orchestrator
// Bridges MASM AVX-512 kernels with Qt/C++ infrastructure
// SECURITY-HARDENED: Pre-flight validation with memory-mapped read-only checking

// Workaround for Windows SDK SAL annotation issues
#ifndef _SAL_VERSION
#define _SAL_VERSION 20
#endif
#ifndef _USE_DECLSPECS_FOR_SAL
#define _USE_DECLSPECS_FOR_SAL 0
#endif
#define _In_
#define _Out_
#define _Inout_
#define _In_opt_
#define _Out_opt_
#define _Inout_opt_

// Minimal Windows includes - avoid spec strings
#include <windef.h>
#include <winbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#define SOVEREIGN_LOADER_EXPORTS
#include "sovereign_loader.h"

// ============================================================================
// STATIC LINKING MODE - All symbols resolved at compile/link time
// No runtime GetProcAddress - direct calls to MASM kernels
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// === Universal Quantization (AVX-512) ===
// From universal_quant_kernel.asm
extern void __stdcall EncodeToPoints(void* tensor, size_t size, float scale);
extern void __stdcall DecodeFromPoints(void* tensor, size_t size, float scale);

// === Beaconism Dispatcher (Trusted Kernel) ===
// From beaconism_dispatcher.asm - CRITICAL: Security-sensitive
extern void* __stdcall ManifestVisualIdentity(const void* model_data, uint64_t file_size, uint64_t* out_size);
extern void __stdcall UnloadModelManifest(void* model_handle);
extern int __stdcall VerifyBeaconSignature(const void* model_data, size_t buffer_size);

// === Dimensional Pooling (1:11 Compression) ===
// From dimensional_pool.asm
extern void __stdcall CreateWeightPool(void* source, void* pool_out, void* spice_map, size_t element_count);
extern void __stdcall AllocateTensor(size_t bytes, void** tensor_ptr);
extern void __stdcall FreeTensor(void* tensor);

#ifdef __cplusplus
}
#endif

// NO FUNCTION POINTERS - Static linking enforces single trusted kernel
// Symbol resolution at compile/link time, not runtime
// Prevents hot-swapping of security-critical beaconism dispatcher

// Production observability
static LoaderMetrics g_loader_metrics = {0};

// Critical section for thread-safe model loading
static CRITICAL_SECTION loader_lock;

// ============================================================================
// SECURITY: Internal helper for read-only file mapping
// ============================================================================
// Purpose: Map file into memory with PAGE_READONLY before ANY validation
// This prevents TOCTOU attacks where file is modified between validation and use
// Returns mapped pointer, caller must UnmapViewOfFile() the result
// ============================================================================
static void* MapModelFileReadOnly(const char* path, uint64_t* out_size) {
    if (!path || !out_size) {
        return NULL;
    }
    
    *out_size = 0;
    
    // Step 1: Open file in read-only mode
    HANDLE hFile = CreateFileA(
        path,
        GENERIC_READ,                          // Read-only access
        FILE_SHARE_READ,                       // Allow other readers
        NULL,
        OPEN_EXISTING,                         // Must exist
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
        NULL
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "MapModelFileReadOnly: Failed to open %s (error: %lu)\n",
                path, GetLastError());
        return NULL;
    }
    
    // Step 2: Get file size
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        fprintf(stderr, "MapModelFileReadOnly: Failed to get size of %s (error: %lu)\n",
                path, GetLastError());
        CloseHandle(hFile);
        return NULL;
    }
    
    // Step 3: Validate file is not absurdly large (> 128 GB)
    if (fileSize.QuadPart > 0x2000000000LL) {
        fprintf(stderr, "MapModelFileReadOnly: File too large %s (%.2f GB)\n",
                path, (double)fileSize.QuadPart / 1024.0 / 1024.0 / 1024.0);
        CloseHandle(hFile);
        return NULL;
    }
    
    *out_size = (uint64_t)fileSize.QuadPart;
    
    // Step 4: Create file mapping with PAGE_READONLY (no write/execute)
    HANDLE hMapping = CreateFileMappingA(
        hFile,
        NULL,
        PAGE_READONLY,                         // Read-only mapping (NO write/execute rights)
        0, 0,                                  // Use default size
        NULL
    );
    
    CloseHandle(hFile);  // File handle no longer needed after mapping created
    
    if (!hMapping) {
        fprintf(stderr, "MapModelFileReadOnly: Failed to create mapping for %s (error: %lu)\n",
                path, GetLastError());
        return NULL;
    }
    
    // Step 5: Map view with FILE_MAP_READ only
    void* mapping = MapViewOfFile(
        hMapping,
        FILE_MAP_READ,                         // Read-only view
        0, 0,                                  // Map entire file
        0
    );
    
    CloseHandle(hMapping);  // Mapping handle no longer needed after view created
    
    if (!mapping) {
        fprintf(stderr, "MapModelFileReadOnly: Failed to map view of %s (error: %lu)\n",
                path, GetLastError());
        return NULL;
    }
    
    fprintf(stderr, "[SECURITY] Mapped file read-only: %s (%.2f MB)\n",
            path, (double)*out_size / 1024.0 / 1024.0);
    
    return mapping;  // Caller MUST UnmapViewOfFile() this pointer
}

// Initialize the sovereign loader
// With static linking, all symbols verified at compile/link time
int sovereign_loader_init(size_t max_ram_gb, size_t vram_mb) {
    InitializeCriticalSection(&loader_lock);

    printf("Sovereign Loader: AVX-512 kernels ready (RAM: %zuGB, VRAM: %zuMB)\n",
           max_ram_gb, vram_mb);
    printf("  ✓ EncodeToPoints (quantization)\n");
    printf("  ✓ DecodeFromPoints (dequantization)\n");
    printf("  ✓ ManifestVisualIdentity (model loading)\n");
    printf("  ✓ VerifyBeaconSignature (security)\n");
    printf("  ✓ CreateWeightPool (dimensional pooling)\n");
    printf("  ✓ Static linking: compile-time verified\n");
    printf("  ✓ Pre-flight validation: ENABLED\n");
    printf("  ✓ Security: PAGE_READONLY mapping\n");
    
    return 0;
}

// Production-grade model loading via security-hardened beaconism dispatcher
// SECURITY: Implements pre-flight validation pattern (check before load)
void* sovereign_loader_load_model(const char* model_path, uint64_t* out_size) {
    if (!model_path || !out_size) {
        fprintf(stderr, "sovereign_loader_load_model: NULL path or size pointer\n");
        return NULL;
    }

    *out_size = 0;
    EnterCriticalSection(&loader_lock);

    // High-resolution timing for AI inference metrics
    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    // ========================================================================
    // SECURITY STEP 1: Map file with PAGE_READONLY (no allocation yet)
    // ========================================================================
    uint64_t file_size = 0;
    void* mapped = MapModelFileReadOnly(model_path, &file_size);
    if (!mapped) {
        fprintf(stderr, "Sovereign Loader: SECURITY: Failed to map file %s\n", model_path);
        LeaveCriticalSection(&loader_lock);
        return NULL;
    }
    
    // ========================================================================
    // SECURITY STEP 2: PRE-FLIGHT VALIDATION (before any allocation)
    // ========================================================================
    // First check: Quick validation of GGUF magic + version
    // This MUST pass before we proceed to full loading
    if (!VerifyBeaconSignature(mapped, file_size)) {
        fprintf(stderr, "Sovereign Loader: SECURITY CHECKPOINT FAILED: Invalid GGUF signature in %s\n", 
                model_path);
        UnmapViewOfFile(mapped);
        LeaveCriticalSection(&loader_lock);
        return NULL;
    }
    
    fprintf(stderr, "[SECURITY] Pre-flight validation PASSED: %s\n", model_path);

    // ========================================================================
    // SECURITY STEP 3: Only NOW call full loader (which may allocate)
    // ========================================================================
    // BEACONISM PATH: Static-linked ManifestVisualIdentity call
    // No runtime symbol resolution - directly call trusted kernel
    // ManifestVisualIdentity receives memory-mapped, validated file
    void* model_handle = ManifestVisualIdentity(mapped, file_size, out_size);
    
    // Clean up read-only mapping (actual model handle uses different memory)
    UnmapViewOfFile(mapped);
    
    if (!model_handle) {
        fprintf(stderr, "Sovereign Loader: SECURITY: Failed to load validated model %s\n", model_path);
        LeaveCriticalSection(&loader_lock);
        return NULL;
    }

    QueryPerformanceCounter(&end);
    double load_time = (double)(end.QuadPart - start.QuadPart) / (double)freq.QuadPart * 1000.0;

    // Update metrics
    g_loader_metrics.models_loaded++;
    g_loader_metrics.total_bytes_loaded += *out_size;
    g_loader_metrics.load_time_ms += load_time;

    printf("Sovereign Loader: Loaded %s (%.2f MB, %.2f ms) [PRE-FLIGHT VALIDATED]\n",
           model_path, *out_size / 1024.0 / 1024.0, load_time);

    LeaveCriticalSection(&loader_lock);
    return model_handle;
}

// Quantize model weights (in-place) via static-linked EncodeToPoints
int sovereign_loader_quantize_weights(void* model_data, size_t tensor_count, float scale) {
    if (!model_data) return -1;

    // Process each tensor in parallel (optional OpenMP)
    if (tensor_count > (size_t)INT_MAX) return -1;
    int n = (int)tensor_count;
#ifdef _OPENMP
    #pragma omp parallel for schedule(dynamic)
#endif
    for (int i = 0; i < n; i++) {
        GGUF_Tensor* tensor = ((GGUF_Tensor**)model_data)[i];
        if (!tensor) continue;
        if (tensor->type == GGUF_TYPE_F32 || tensor->type == GGUF_TYPE_F16) {
            // Direct static call to AVX-512 quantization kernel
            EncodeToPoints(tensor->data, tensor->size, scale);
            g_loader_metrics.quantization_ops++;
        }
    }

    return 0;
}

// Cleanup via static-linked UnloadModelManifest
void sovereign_loader_unload_model(void* model_handle) {
    if (!model_handle) return;

    EnterCriticalSection(&loader_lock);
    // Direct call to beaconism dispatcher unload routine
    UnloadModelManifest(model_handle);
    LeaveCriticalSection(&loader_lock);
}

// Get metrics for observability
void sovereign_loader_get_metrics(LoaderMetrics* metrics) {
    if (metrics) memcpy(metrics, &g_loader_metrics, sizeof(LoaderMetrics));
}

// Shutdown loader
void sovereign_loader_shutdown(void) {
    DeleteCriticalSection(&loader_lock);
    printf("Sovereign Loader: Shutdown complete\n");
}
