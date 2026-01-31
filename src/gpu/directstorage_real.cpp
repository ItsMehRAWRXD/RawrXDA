// directstorage_real.cpp - PRODUCTION DIRECTSTORAGE INITIALIZATION
// Replaces stub with actual DS factory/queue setup
// Implements complete DirectStorage pipeline with error handling and logging

#include <dstorage.h>
#include <windows.h>
#include <stdio.h>
#include <cstring>
#include <vector>

#pragma comment(lib, "dstorage.lib")

// ============================================================
// STRUCTURED LOGGING
// ============================================================
enum LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

static void LogMessage(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    const char* level_str[] = { "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]" };

    v

    va_end(args);
}

// ============================================================
// GLOBAL STATE
// ============================================================
static IDStorageFactoryX2* g_ds_factory = nullptr;
static IDStorageQueueX2* g_ds_queue = nullptr;
static IDStorageStatusArrayX* g_ds_status = nullptr;
static const UINT64 STAGING_BUFFER_SIZE = 64 * 1024 * 1024; // 64MB
static bool g_directstorage_initialized = false;
static UINT32 g_pending_requests = 0;

// ============================================================
// ERROR HANDLING UTILITIES
// ============================================================

static const char* HResultString(HRESULT hr) {
    switch (hr) {
        case S_OK: return "S_OK";
        case E_DSTORAGE_INVALID_FILE: return "Invalid file";
        case E_DSTORAGE_INVALID_BCPACK_MODE: return "Invalid BCPack mode";
        case E_DSTORAGE_INVALID_SWIZZLE_MODE: return "Invalid swizzle mode";
        case E_DSTORAGE_INVALID_DESTINATION_SIZE: return "Invalid destination size";
        case E_DSTORAGE_INVALID_DESTINATION_OFFSET: return "Invalid destination offset";
        case E_DSTORAGE_INVALID_DESTINATION_TYPE: return "Invalid destination type";
        case E_DSTORAGE_INVALID_SOURCE_TYPE: return "Invalid source type";
        case E_DSTORAGE_INVALID_SOURCE_SIZE: return "Invalid source size";
        case E_DSTORAGE_INVALID_SOURCE_OFFSET: return "Invalid source offset";
        case E_DSTORAGE_INVALID_QUEUE_CAPACITY: return "Invalid queue capacity";
        case E_DSTORAGE_TOO_MANY_FILES: return "Too many files";
        case E_DSTORAGE_TOO_MANY_QUEUES: return "Too many queues";
        case E_OUTOFMEMORY: return "Out of memory";
        default: return "Unknown DirectStorage error";
    }
}

static bool IsHResultSuccess(HRESULT hr) {
    return SUCCEEDED(hr);
}

// ============================================================
// REAL DIRECTSTORAGE INITIALIZATION
// ============================================================
HRESULT Titan_DirectStorage_Init_Real() {
    auto start_time = GetTickCount();
    LogMessage(INFO, "=== Starting DirectStorage Initialization ===");
    
    if (g_directstorage_initialized) {
        LogMessage(WARN, "DirectStorage already initialized, skipping");
        return S_OK;
    }
    
    // 1. Get DirectStorage factory
    LogMessage(INFO, "Getting DirectStorage factory");
    HRESULT hr = DStorageGetFactory(__uuidof(IDStorageFactoryX2), (void**)&g_ds_factory);
    
    if (!IsHResultSuccess(hr)) {
        LogMessage(ERROR, "DStorageGetFactory failed: 0x%08X (%s)", hr, HResultString(hr));
        LogMessage(WARN, "Ensure DirectStorage SDK is installed and configured");
        return hr;
    }
    
    if (!g_ds_factory) {
        LogMessage(ERROR, "Factory pointer is NULL");
        return E_POINTER;
    }
    
    LogMessage(DEBUG, "DirectStorage factory obtained successfully");
    
    // 2. Configure staging buffer size
    LogMessage(INFO, "Configuring staging buffer (%.0f MB)", STAGING_BUFFER_SIZE / (1024.0f * 1024.0f));
    hr = g_ds_factory->SetStagingBufferSize(STAGING_BUFFER_SIZE);
    
    if (!IsHResultSuccess(hr)) {
        LogMessage(ERROR, "SetStagingBufferSize failed: 0x%08X (%s)", hr, HResultString(hr));
        g_ds_factory->Release();
        g_ds_factory = nullptr;
        return hr;
    }
    
    LogMessage(DEBUG, "Staging buffer configured: %.0f MB", STAGING_BUFFER_SIZE / (1024.0f * 1024.0f));
    
    // 3. Create request queue
    LogMessage(INFO, "Creating DirectStorage request queue");
    DSTORAGE_QUEUE_DESC queueDesc = {};
    queueDesc.Capacity = 32;  // Max concurrent requests
    queueDesc.Priority = DSTORAGE_PRIORITY_NORMAL;
    queueDesc.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    queueDesc.Device = nullptr;  // Use default GPU device
    queueDesc.Name = L"RawrXD_ModelLoader";
    
    LogMessage(DEBUG, "Queue config: capacity=32, priority=normal, source=file");
    
    hr = g_ds_factory->CreateQueue(&queueDesc, __uuidof(IDStorageQueueX2), (void**)&g_ds_queue);
    
    if (!IsHResultSuccess(hr)) {
        LogMessage(ERROR, "CreateQueue failed: 0x%08X (%s)", hr, HResultString(hr));
        g_ds_factory->Release();
        g_ds_factory = nullptr;
        return hr;
    }
    
    if (!g_ds_queue) {
        LogMessage(ERROR, "Queue pointer is NULL");
        g_ds_factory->Release();
        g_ds_factory = nullptr;
        return E_POINTER;
    }
    
    LogMessage(DEBUG, "DirectStorage queue created successfully");
    
    // 4. Create status array for async completion tracking
    LogMessage(INFO, "Creating status array for completion tracking");
    hr = g_ds_factory->CreateStatusArray(32, L"RawrXD_StatusArray",
        __uuidof(IDStorageStatusArrayX), (void**)&g_ds_status);
    
    if (!IsHResultSuccess(hr)) {
        LogMessage(ERROR, "CreateStatusArray failed: 0x%08X (%s)", hr, HResultString(hr));
        Titan_DirectStorage_Cleanup();
        return hr;
    }
    
    if (!g_ds_status) {
        LogMessage(ERROR, "Status array pointer is NULL");
        Titan_DirectStorage_Cleanup();
        return E_POINTER;
    }
    
    LogMessage(DEBUG, "Status array created with capacity 32");
    
    g_directstorage_initialized = true;
    g_pending_requests = 0;
    
    LogMessage(INFO, "=== DirectStorage Initialization Complete (%.0f ms) ===",
        (float)(GetTickCount() - start_time));
    
    return S_OK;
}

// ============================================================
// FILE OPERATIONS
// ============================================================

// Open file for DirectStorage access
HANDLE Titan_DirectStorage_OpenFile(const wchar_t* path) {
    LogMessage(INFO, "Opening file for DirectStorage: %ls", path);
    
    if (!g_ds_factory) {
        LogMessage(ERROR, "DirectStorage factory not initialized");
        return INVALID_HANDLE_VALUE;
    }
    
    if (!path) {
        LogMessage(ERROR, "Invalid file path (NULL)");
        return INVALID_HANDLE_VALUE;
    }
    
    IDStorageFileX* dsFile = nullptr;
    HRESULT hr = g_ds_factory->OpenFile(path, __uuidof(IDStorageFileX), (void**)&dsFile);
    
    if (!IsHResultSuccess(hr)) {
        LogMessage(ERROR, "OpenFile failed: 0x%08X (%s)", hr, HResultString(hr));
        return INVALID_HANDLE_VALUE;
    }
    
    if (!dsFile) {
        LogMessage(ERROR, "DirectStorage file pointer is NULL");
        return INVALID_HANDLE_VALUE;
    }
    
    // Get Win32 file handle
    HANDLE hFile = INVALID_HANDLE_VALUE;
    hr = dsFile->GetFileHandle(&hFile);
    
    // Release DirectStorage file handle
    dsFile->Release();
    dsFile = nullptr;
    
    if (!IsHResultSuccess(hr) || hFile == INVALID_HANDLE_VALUE) {
        LogMessage(ERROR, "GetFileHandle failed: 0x%08X", hr);
        return INVALID_HANDLE_VALUE;
    }
    
    LogMessage(DEBUG, "File opened successfully");
    return hFile;
}

// Close file opened by DirectStorage
bool Titan_DirectStorage_CloseFile(HANDLE hFile) {
    LogMessage(DEBUG, "Closing DirectStorage file handle");
    
    if (hFile == INVALID_HANDLE_VALUE || hFile == nullptr) {
        LogMessage(WARN, "Invalid file handle, skipping close");
        return false;
    }
    
    if (!CloseHandle(hFile)) {
        LogMessage(ERROR, "CloseHandle failed: 0x%08X", GetLastError());
        return false;
    }
    
    LogMessage(DEBUG, "File closed successfully");
    return true;
}

// ============================================================
// REQUEST SUBMISSION
// ============================================================

// Submit read request with proper cleanup
bool Titan_DirectStorage_SubmitRequest(void* dstBuffer, UINT64 dstOffset,
    HANDLE hFile, UINT64 fileOffset, UINT32 size) {
    
    LogMessage(DEBUG, "Submitting read request: fileOffset=%llu, size=%u, dstOffset=%llu",
        fileOffset, size, dstOffset);
    
    if (!g_ds_queue || !g_ds_status) {
        LogMessage(ERROR, "DirectStorage queue or status array not initialized");
        return false;
    }
    
    if (!dstBuffer) {
        LogMessage(ERROR, "Invalid destination buffer (NULL)");
        return false;
    }
    
    if (hFile == INVALID_HANDLE_VALUE) {
        LogMessage(ERROR, "Invalid file handle");
        return false;
    }
    
    if (size == 0) {
        LogMessage(WARN, "Zero-size request, skipping");
        return false;
    }
    
    // Allocate request
    DSTORAGE_REQUEST* request = new DSTORAGE_REQUEST();
    if (!request) {
        LogMessage(ERROR, "Failed to allocate DSTORAGE_REQUEST");
        return false;
    }
    
    ZeroMemory(request, sizeof(DSTORAGE_REQUEST));
    
    // Configure request
    request->Options.SourceType = DSTORAGE_REQUEST_SOURCE_FILE;
    request->Options.DestinationType = DSTORAGE_REQUEST_DESTINATION_MEMORY;
    request->Options.CompressionFormat = DSTORAGE_COMPRESSION_FORMAT_NONE;
    request->Options.SourceMemory.Layout = DSTORAGE_REQUEST_SOURCE_MEMORY_LAYOUT_ROW_MAJOR;
    
    request->Source.File.Handle = hFile;
    request->Source.File.Offset = fileOffset;
    request->Source.File.Size = size;
    
    request->Destination.Memory.Buffer = (char*)dstBuffer + dstOffset;
    request->Destination.Memory.Size = size;
    
    LogMessage(DEBUG, "Request configured: file_offset=%llu, size=%u, dst_buffer=0x%p",
        fileOffset, size, request->Destination.Memory.Buffer);
    
    // Enqueue request
    UINT32 status_index = g_pending_requests % 32;
    g_ds_queue->EnqueueRequest(request, g_ds_status, status_index);
    g_pending_requests++;
    
    LogMessage(DEBUG, "Request enqueued (status_index=%d)", status_index);
    
    // Submit to GPU/Storage stack
    g_ds_queue->Submit();
    LogMessage(DEBUG, "Request submitted");
    
    // Wait for completion (polling)
    LogMessage(DEBUG, "Waiting for completion (status_index=%d)", status_index);
    
    UINT32 max_wait_ms = 30000;  // 30 second timeout
    auto timeout_start = GetTickCount();
    
    while (true) {
        UINT32 num_completed = g_ds_status->GetNumCompleted();
        if (num_completed > status_index) {
            LogMessage(DEBUG, "Request completed");
            break;
        }
        
        DWORD elapsed = GetTickCount() - timeout_start;
        if (elapsed > max_wait_ms) {
            LogMessage(ERROR, "Request timeout after %d ms", elapsed);
            delete request;
            return false;
        }
        
        Sleep(1);  // Yield to other threads
    }
    
    // Check status
    HRESULT completion_status = g_ds_status->GetHResult(status_index);
    if (!IsHResultSuccess(completion_status)) {
        LogMessage(ERROR, "Request completion with error: 0x%08X (%s)",
            completion_status, HResultString(completion_status));
        delete request;
        return false;
    }
    
    LogMessage(DEBUG, "Request completed successfully");
    
    // ===== MEMORY LEAK FIX: FREE THE REQUEST =====
    delete request;
    g_pending_requests--;
    
    return true;
}

// Wait for all pending requests
bool Titan_DirectStorage_WaitAll() {
    LogMessage(INFO, "Waiting for all DirectStorage requests (pending=%d)", g_pending_requests);
    
    if (!g_ds_queue || !g_ds_status) {
        LogMessage(ERROR, "DirectStorage not initialized");
        return false;
    }
    
    if (g_pending_requests == 0) {
        LogMessage(DEBUG, "No pending requests");
        return true;
    }
    
    // Submit any pending work
    g_ds_queue->Submit();
    
    // Wait for all completions
    auto timeout_start = GetTickCount();
    UINT32 max_wait_ms = 60000;  // 60 second timeout
    
    while (g_ds_status->GetNumCompleted() < g_pending_requests) {
        DWORD elapsed = GetTickCount() - timeout_start;
        if (elapsed > max_wait_ms) {
            LogMessage(ERROR, "WaitAll timeout after %d ms", elapsed);
            return false;
        }
        
        Sleep(1);
    }
    
    LogMessage(INFO, "All DirectStorage requests completed");
    return true;
}

// ============================================================
// CLEANUP FUNCTION (MEMORY LEAK FIX)
// ============================================================
void Titan_DirectStorage_Cleanup() {
    LogMessage(INFO, "=== Starting DirectStorage Cleanup ===");
    
    // Wait for pending requests
    if (g_pending_requests > 0) {
        LogMessage(INFO, "Waiting for %d pending requests", g_pending_requests);
        Titan_DirectStorage_WaitAll();
    }
    
    // Destroy status array
    if (g_ds_status) {
        LogMessage(DEBUG, "Destroying status array");
        g_ds_status->Release();
        g_ds_status = nullptr;
    }
    
    // Destroy queue
    if (g_ds_queue) {
        LogMessage(DEBUG, "Destroying request queue");
        g_ds_queue->Release();
        g_ds_queue = nullptr;
    }
    
    // Release factory
    if (g_ds_factory) {
        LogMessage(DEBUG, "Releasing factory");
        g_ds_factory->Release();
        g_ds_factory = nullptr;
    }
    
    g_directstorage_initialized = false;
    g_pending_requests = 0;
    
    LogMessage(INFO, "=== DirectStorage Cleanup Complete ===");
}

// ============================================================
// GETTER FUNCTIONS
// ============================================================
IDStorageQueueX2* Titan_DirectStorage_GetQueue() {
    return g_ds_queue;
}

IDStorageFactoryX2* Titan_DirectStorage_GetFactory() {
    return g_ds_factory;
}

IDStorageStatusArrayX* Titan_DirectStorage_GetStatusArray() {
    return g_ds_status;
}

bool Titan_DirectStorage_IsInitialized() {
    return g_directstorage_initialized;
}

UINT32 Titan_DirectStorage_GetPendingRequests() {
    return g_pending_requests;
}

// ============================================================
// SAFE WRAPPER
// ============================================================
HRESULT Titan_DirectStorage_Init_Safe() {
    try {
        return Titan_DirectStorage_Init_Real();
    }
    catch (const std::exception& e) {
        LogMessage(ERROR, "Exception in DirectStorage initialization: %s", e.what());
        Titan_DirectStorage_Cleanup();
        return E_UNEXPECTED;
    }
    catch (...) {
        LogMessage(ERROR, "Unknown exception in DirectStorage initialization");
        Titan_DirectStorage_Cleanup();
        return E_UNEXPECTED;
    }
}
