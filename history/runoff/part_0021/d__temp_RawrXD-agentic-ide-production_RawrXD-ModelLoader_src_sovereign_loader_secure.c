#include "sovereign_loader_secure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * Global statistics (in production, would be per-instance with thread-safety)
 */
static SovereignLoaderStats g_stats = {0};

/**
 * ============================================================================
 * MASM External Procedures (imported from compiled .obj files)
 * ============================================================================
 */
extern int VerifyBeaconSignature(const unsigned char* file_header, uint64_t header_size);
extern int ManifestVisualIdentity(const unsigned char* mapped_ptr, uint64_t mapped_size);
extern int UnloadModelManifest(void* model_handle);

/**
 * ============================================================================
 * Implementation: Pre-Flight Validation
 * ============================================================================
 * 
 * This is STEP 1: Validate without allocating resources
 */
int sovereign_verify_signature(const unsigned char* file_header, uint64_t header_size)
{
    if (!file_header || header_size < 4) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "Invalid header pointer or too small");
        return 0;
    }
    
    // Call MASM verification routine
    // This checks:
    // - GGUF magic (0x46554747)
    // - Version field validity
    // - Minimum structure integrity
    int result = VerifyBeaconSignature((unsigned char*)file_header, header_size);
    
    if (!result) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "GGUF signature verification failed - invalid magic or version");
    }
    
    return result;
}

/**
 * ============================================================================
 * Implementation: Safe Model Loading (After Verification + Mapping)
 * ============================================================================
 * 
 * This is STEP 3: Load only after verification + memory mapping succeeded
 */
int sovereign_load_model_safe(const unsigned char* mapped_ptr, uint64_t mapped_size)
{
    if (!mapped_ptr || mapped_size < GGUF_MIN_SIZE) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "Invalid mapped pointer or size too small");
        return 0;
    }
    
    // Call MASM loading routine
    // This performs:
    // - Re-check signature at mapped address (defense in depth)
    // - Load weights into ZMM registers
    // - Set up quantization state
    int result = ManifestVisualIdentity((unsigned char*)mapped_ptr, mapped_size);
    
    if (result) {
        g_stats.total_models_loaded++;
        g_stats.total_memory_used += mapped_size;
    } else {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "Model loading failed in ManifestVisualIdentity");
    }
    
    return result;
}

/**
 * ============================================================================
 * Implementation: Full Secure Pipeline
 * ============================================================================
 * 
 * This combines all steps: verify -> map -> load -> handle errors
 * This is the recommended entry point for production use.
 */
int sovereign_load_model_file(const char* file_path, void** out_handle, uint64_t* out_size)
{
    FILE* file_handle = NULL;
    HANDLE file_mapping = NULL;
    unsigned char* mapped_view = NULL;
    unsigned char file_header[512];
    size_t header_read;
    uint64_t file_size;
    int result = 0;
    
    // Initialize output parameters
    if (out_handle) *out_handle = NULL;
    if (out_size) *out_size = 0;
    
    if (!file_path) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "File path is NULL");
        return 0;
    }
    
    // ========================================================================
    // STEP 1: Open file and read header for verification
    // ========================================================================
    #ifdef _WIN32
    // Windows version
    HANDLE win_file = CreateFileA(
        file_path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (win_file == INVALID_HANDLE_VALUE) {
        sprintf_s(g_stats.last_error, sizeof(g_stats.last_error),
                 "Cannot open file: %s", file_path);
        return 0;
    }
    
    // Get file size
    LARGE_INTEGER file_size_li;
    if (!GetFileSizeEx(win_file, &file_size_li)) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "Cannot get file size");
        CloseHandle(win_file);
        return 0;
    }
    file_size = file_size_li.QuadPart;
    
    // Read first 512 bytes
    DWORD bytes_read = 0;
    if (!ReadFile(win_file, file_header, sizeof(file_header), &bytes_read, NULL)) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "Cannot read file header");
        CloseHandle(win_file);
        return 0;
    }
    
    #else
    // POSIX version
    file_handle = fopen(file_path, "rb");
    if (!file_handle) {
        sprintf_s(g_stats.last_error, sizeof(g_stats.last_error),
                 "Cannot open file: %s", file_path);
        return 0;
    }
    
    header_read = fread(file_header, 1, sizeof(file_header), file_handle);
    fseek(file_handle, 0, SEEK_END);
    file_size = ftell(file_handle);
    fseek(file_handle, 0, SEEK_SET);
    #endif
    
    // ========================================================================
    // STEP 2: PRE-FLIGHT VALIDATION - Check signature BEFORE mapping
    // ========================================================================
    // This is critical: if signature is invalid, we return immediately
    // WITHOUT allocating file mapping objects or virtual memory.
    if (!sovereign_verify_signature(file_header, (uint64_t)bytes_read)) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "File signature validation failed - not a valid GGUF");
        #ifdef _WIN32
        CloseHandle(win_file);
        #else
        fclose(file_handle);
        #endif
        return 0;
    }
    
    // ========================================================================
    // STEP 3: MEMORY MAPPING - Now safe to allocate resources
    // ========================================================================
    #ifdef _WIN32
    file_mapping = CreateFileMappingA(
        win_file,
        NULL,
        PAGE_READONLY,
        (DWORD)(file_size >> 32),
        (DWORD)(file_size & 0xFFFFFFFF),
        NULL
    );
    
    if (!file_mapping) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "CreateFileMapping failed");
        CloseHandle(win_file);
        return 0;
    }
    
    // Map view of file
    mapped_view = (unsigned char*)MapViewOfFile(
        file_mapping,
        FILE_MAP_READ,
        0,
        0,
        0  // Map entire file
    );
    
    if (!mapped_view) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "MapViewOfFile failed");
        CloseHandle(file_mapping);
        CloseHandle(win_file);
        return 0;
    }
    
    #else
    // POSIX: Use mmap instead
    #include <sys/mman.h>
    mapped_view = (unsigned char*)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fileno(file_handle), 0);
    if (mapped_view == MAP_FAILED) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "mmap failed");
        fclose(file_handle);
        return 0;
    }
    #endif
    
    // ========================================================================
    // STEP 4: SAFE LOADING - Model loading with validated + mapped file
    // ========================================================================
    if (sovereign_load_model_safe(mapped_view, file_size)) {
        // Success
        if (out_handle) *out_handle = (void*)mapped_view;
        if (out_size) *out_size = file_size;
        result = 1;
    } else {
        // Failure - unmap and cleanup
        #ifdef _WIN32
        UnmapViewOfFile(mapped_view);
        CloseHandle(file_mapping);
        CloseHandle(win_file);
        #else
        munmap(mapped_view, file_size);
        fclose(file_handle);
        #endif
    }
    
    return result;
}

/**
 * ============================================================================
 * Implementation: Model Unload
 * ============================================================================
 */
int sovereign_unload_model(void* model_handle)
{
    if (!model_handle) {
        strcpy_s(g_stats.last_error, sizeof(g_stats.last_error),
                "Invalid model handle");
        return 0;
    }
    
    // Call MASM unload routine
    int result = UnloadModelManifest(model_handle);
    
    if (result) {
        // Unmap memory (assumes Windows mapping for now)
        #ifdef _WIN32
        UnmapViewOfFile(model_handle);
        #else
        // POSIX: Need to track size separately for munmap
        // In production, would store size in model metadata
        #endif
        
        if (g_stats.total_models_loaded > 0) {
            g_stats.total_models_loaded--;
        }
    }
    
    return result;
}

/**
 * ============================================================================
 * Implementation: Query Statistics
 * ============================================================================
 */
SovereignLoaderStats sovereign_get_stats(void)
{
    return g_stats;
}

void sovereign_reset_stats(void)
{
    memset(&g_stats, 0, sizeof(g_stats));
}
