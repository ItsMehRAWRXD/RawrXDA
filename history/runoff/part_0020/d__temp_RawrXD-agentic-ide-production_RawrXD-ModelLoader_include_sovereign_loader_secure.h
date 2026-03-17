#ifndef SOVEREIGN_LOADER_SECURE_H
#define SOVEREIGN_LOADER_SECURE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * ============================================================================
 * RawrXD Sovereign Loader - SECURE MODEL LOADING PIPELINE
 * ============================================================================
 * 
 * SECURITY MODEL: Pre-Flight Validation
 * 
 * The following security checklist MUST be followed for safe model loading:
 * 
 * 1. SIGNATURE VALIDATION (Before any resource allocation)
 *    - C layer: Open file, read first 512 bytes
 *    - C layer: Call VerifyBeaconSignature() with file header
 *    - MASM: Check GGUF magic (0x46554747) + version field
 *    - If invalid: Close file immediately, return error
 * 
 * 2. MEMORY MAPPING (Only after signature verified)
 *    - C layer: CreateFileMapping() on validated file
 *    - C layer: MapViewOfFile() to get memory pointer
 *    - C layer: Pass pointer to ManifestVisualIdentity()
 * 
 * 3. SAFE LOADING (Happens in ManifestVisualIdentity)
 *    - MASM: Re-check signature at mapped address (defense in depth)
 *    - MASM: Check minimum file size
 *    - MASM: Only then load weights into ZMM registers
 * 
 * 4. ERROR HANDLING (If any step fails)
 *    - C layer: Call UnmapViewOfFile() if mapping succeeded
 *    - C layer: Call CloseHandle() on file mapping
 *    - C layer: Call CloseHandle() on file handle
 * 
 * ============================================================================
 * ADVANTAGE: If file signature is invalid, we never allocate:
 *   - File mapping objects (system resources)
 *   - Mapped virtual memory (could be GB-scale)
 *   - Quantization buffers
 *   - ZMM register states
 * ============================================================================
 */

/**
 * GGUF Magic Number (Little-Endian)
 * ASCII: "GGUF" = 0x46554747
 */
#define GGUF_MAGIC 0x46554747

/**
 * Minimum valid GGUF file size (header must be at least 4KB)
 */
#define GGUF_MIN_SIZE 0x1000

/**
 * ============================================================================
 * Pre-Flight Validation: Check file signature WITHOUT mapping
 * ============================================================================
 * 
 * Call this BEFORE CreateFileMapping to validate the file format.
 * This prevents allocation of system resources for invalid files.
 * 
 * @param file_header Pointer to first 512 bytes of file (read from disk)
 * @param header_size Number of bytes available (must be >= 4)
 * @return 1 if valid GGUF magic + version, 0 if invalid or too small
 * 
 * Example:
 *   FILE* f = fopen("model.gguf", "rb");
 *   unsigned char header[512];
 *   size_t read = fread(header, 1, sizeof(header), f);
 *   
 *   if (!sovereign_verify_signature(header, read)) {
 *       fprintf(stderr, "Invalid GGUF file\n");
 *       fclose(f);
 *       return NULL;
 *   }
 *   // Safe to proceed with mapping
 */
extern int sovereign_verify_signature(const unsigned char* file_header, 
                                       uint64_t header_size);

/**
 * ============================================================================
 * Safe Model Loading: Load AFTER verification + mapping
 * ============================================================================
 * 
 * Call this ONLY after:
 *   1. sovereign_verify_signature() returned 1
 *   2. File has been memory-mapped (MapViewOfFile succeeded)
 *   3. Mapped view pointer is available
 * 
 * This function will re-check the signature at the mapped address
 * as an additional defense mechanism.
 * 
 * @param mapped_ptr Pointer to memory-mapped file (from MapViewOfFile)
 * @param mapped_size Size of mapped region in bytes
 * @return 1 if model loaded successfully, 0 if corrupt or signature invalid
 */
extern int sovereign_load_model_safe(const unsigned char* mapped_ptr,
                                      uint64_t mapped_size);

/**
 * ============================================================================
 * High-Level Safe Loading (Combines all steps)
 * ============================================================================
 * 
 * Convenience function that implements the full secure loading pipeline.
 * Handles all resource allocation and cleanup internally.
 * 
 * @param file_path Path to GGUF file
 * @param out_handle Output: Model handle (opaque pointer)
 * @param out_size Output: Model size in bytes
 * @return 1 if successful, 0 if file invalid or load failed
 * 
 * On success:
 *   - Model is loaded and ready for inference
 *   - out_handle contains opaque model pointer (for unload later)
 *   - out_size contains total model size
 * 
 * On failure:
 *   - All resources are cleaned up
 *   - out_handle is set to NULL
 *   - out_size is set to 0
 */
extern int sovereign_load_model_file(const char* file_path,
                                      void** out_handle,
                                      uint64_t* out_size);

/**
 * ============================================================================
 * Unload Model (Cleanup)
 * ============================================================================
 * 
 * Unload a model loaded via sovereign_load_model_file or sovereign_load_model_safe.
 * Unmaps memory, closes file handles, and frees associated resources.
 * 
 * @param model_handle Handle returned from load function
 * @return 1 if unload successful, 0 if handle invalid
 */
extern int sovereign_unload_model(void* model_handle);

/**
 * ============================================================================
 * Query Model Status
 * ============================================================================
 */
typedef struct {
    uint64_t total_models_loaded;
    uint64_t total_memory_used;
    uint64_t total_tokens_generated;
    double load_time_ms;
    char last_error[256];
} SovereignLoaderStats;

extern SovereignLoaderStats sovereign_get_stats(void);
extern void sovereign_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // SOVEREIGN_LOADER_SECURE_H
