/**
 * @file secure_settings_api.h
 * @brief Unified C API for production-hardened settings and persistence
 * @version 1.0.0
 * 
 * Replaces unsafe patterns with validated, encrypted alternatives
 */

#pragma once

#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SECURE API KEY STORAGE (Replaces plaintext ai_settings.ini)
// ============================================================================

/**
 * Save API key encrypted with DPAPI (current user only)
 * @param keyName Identifier for the key (e.g., "openai", "anthropic")
 * @param apiKey The key to encrypt (null-terminated)
 * @return true on success
 */
bool SecureStorage_SaveApiKey(const char* keyName, const char* apiKey);

/**
 * Load decrypted API key
 * @param keyName Identifier for the key
 * @param buffer Output buffer (must be at least 4096 bytes)
 * @param bufferSize Size of output buffer
 * @return true on success (buffer contains null-terminated key)
 */
bool SecureStorage_LoadApiKey(const char* keyName, char* buffer, size_t bufferSize);

/**
 * Delete stored API key
 * @param keyName Identifier for the key
 */
void SecureStorage_DeleteApiKey(const char* keyName);

/**
 * Check if key exists without loading it
 * @param keyName Identifier for the key
 */
bool SecureStorage_HasKey(const char* keyName);

// ============================================================================
// SAFE SETTINGS DIALOG (Replaces broken MonacoSettingsDialog)
// ============================================================================

typedef struct {
    char theme[128];
    int fontSize;
    float lineHeight;
    COLORREF backgroundColor;
    COLORREF foregroundColor;
    bool wordWrap;
} MonacoSettings;

/**
 * Show settings dialog with proper modal lifecycle
 * @param hwndParent Parent window handle
 * @param settings In: current settings, Out: new settings if returns true
 * @return true if user clicked OK and settings validated, false if cancelled
 */
bool MonacoSettingsDialog_ShowModal(HWND hwndParent, MonacoSettings* settings);

/**
 * Load settings from JSON file with validation
 * @param path File path
 * @param settings Output settings (only modified on success)
 * @return true if file loaded and validated successfully
 */
bool MonacoSettingsDialog_LoadFromFile(const char* path, MonacoSettings* settings);

/**
 * Save settings to JSON file
 * @param path File path
 * @param settings Settings to save
 * @return true on success
 */
bool MonacoSettingsDialog_SaveToFile(const char* path, const MonacoSettings* settings);

// ============================================================================
// SAFE PARSING UTILITIES (Use these instead of raw std::stoi)
// ============================================================================

typedef struct {
    bool success;
    int value;
    char error[256];
} SafeParseIntResult;

typedef struct {
    bool success;
    float value;
    char error[256];
} SafeParseFloatResult;

/**
 * Parse integer with bounds checking
 * @param str Input string
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @return Result with success flag and error message
 */
SafeParseIntResult SafeParse_Int(const char* str, int min, int max);

/**
 * Parse float with bounds checking
 * @param str Input string
 * @param min Minimum allowed value
 * @param max Maximum allowed value
 * @return Result with success flag
 */
SafeParseFloatResult SafeParse_Float(const char* str, float min, float max);

// ============================================================================
// VECTOR INDEX PERSISTENCE (Replaces unsafe persistence)
// ============================================================================

typedef void* RawrXDVectorIndexHandle;

/**
 * Create new vector index instance
 */
RawrXDVectorIndexHandle VectorIndex_Create(void);

/**
 * Destroy vector index and free all resources
 */
void VectorIndex_Destroy(RawrXDVectorIndexHandle handle);

/**
 * Attach GGUF tensor with 64-bit overflow checking
 * @param handle Index handle
 * @param filePath Wide char file path
 * @param tensorName Tensor identifier
 * @param tensorOffset Byte offset in file
 * @param tensorSize Byte size of tensor
 * @param dimensions Number of dimensions per vector
 * @param rows Number of vectors
 * @return true if safely attached, false on overflow or bounds error
 */
bool VectorIndex_AttachGGUFTensor(RawrXDVectorIndexHandle handle,
                                  const wchar_t* filePath,
                                  const char* tensorName,
                                  uint64_t tensorOffset,
                                  uint64_t tensorSize,
                                  uint64_t dimensions,
                                  uint64_t rows);

/**
 * Save index to file (only in-memory entries, mapped tensors not included)
 * @param handle Index handle
 * @param path Output file path
 * @return true on success
 */
bool VectorIndex_SaveToFile(RawrXDVectorIndexHandle handle, const char* path);

/**
 * Load index from file (clears existing state)
 * @param handle Index handle
 * @param path Input file path
 * @return true on success (index is in clean state)
 */
bool VectorIndex_LoadFromFile(RawrXDVectorIndexHandle handle, const char* path);

// ============================================================================
// REPOSITORY INDEXER (Safe UTF-8 conversion)
// ============================================================================

/**
 * Export repository index with safe encoding handling
 * @param rootPath Wide char root directory
 * @param outputPath UTF-8 output file path
 * @return true on success
 */
bool RepositoryIndexer_Export(const wchar_t* rootPath, const char* outputPath);

#ifdef __cplusplus
}
#endif
