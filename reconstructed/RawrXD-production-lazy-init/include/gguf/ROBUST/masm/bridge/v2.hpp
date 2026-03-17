// =============================================================================
// RawrXD GGUF Robust Tools - MASM Bridge
// C++ interface to zero-CRT assembly functions
// =============================================================================
#pragma once
#include <windows.h>
#include <cstdint>

extern "C" {
    // Zero-allocation skip operations
    __declspec(dllimport) BOOL StrSafe_SkipChunk(HANDLE hFile, uint64_t cbLength);
    __declspec(dllimport) BOOL GGUF_SkipStringValue(HANDLE hFile);
    __declspec(dllimport) BOOL GGUF_SkipArrayValue(HANDLE hFile, uint8_t elemType);
    
    // Safe I/O primitives
    __declspec(dllimport) uint64_t MemSafe_PeekU64(HANDLE hFile, uint64_t* pResult);
    
    // Buffered stream context
    struct GGUF_STREAM_CTX {
        HANDLE hFile;
        void* pBuffer;
        uint64_t cbBuffer;
        uint64_t cbValid;
        uint64_t cbConsumed;
        uint64_t fileOffset;
        uint8_t fEOF;
        uint8_t fError;
        uint16_t padding;
    };
    
    __declspec(dllimport) BOOL GGUF_StreamInit(HANDLE hFile, GGUF_STREAM_CTX* pCtx);
    __declspec(dllimport) void GGUF_StreamFree(GGUF_STREAM_CTX* pCtx);
}

// C++ RAII wrapper for your existing loader
namespace rawrxd::gguf_masm {

class RobustGGUFParser {
    HANDLE hFile_;
    bool ownsHandle_;
    
public:
    explicit RobustGGUFParser(HANDLE h, bool takeOwnership = false) 
        : hFile_(h), ownsHandle_(takeOwnership) {}
    
    ~RobustGGUFParser() {
        if (ownsHandle_ && hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
        }
    }

    // Non-copyable
    RobustGGUFParser(const RobustGGUFParser&) = delete;
    RobustGGUFParser& operator=(const RobustGGUFParser&) = delete;
    
    // Skip known-problematic metadata keys safely
    bool SkipUnsafeString(const char* keyName) {
        uint64_t len = 0;
        
        // Peek length first (no allocation) using MASM function
        if (!MemSafe_PeekU64(hFile_, &len))
            return false;
            
        if (len > 0xFFFFFF) {  // 16MB limit
            // Corrupted or massive - skip without reading using MASM skip
            return GGUF_SkipStringValue(hFile_);
        }
        
        // Small enough - let normal path handle it
        return false;  // Signal caller to read normally
    }
    
    // Skip array using MASM safe implementation
    bool SkipArray(uint8_t elemType) {
        return GGUF_SkipArrayValue(hFile_, elemType);
    }
    
    // Skip arbitrary data chunk
    bool SkipChunk(uint64_t bytes) {
        return StrSafe_SkipChunk(hFile_, bytes);
    }
    
    HANDLE GetHandle() const { return hFile_; }
};

// Buffered stream wrapper
class BufferedGGUFStream {
    GGUF_STREAM_CTX ctx_;
    bool initialized_;
    
public:
    explicit BufferedGGUFStream(HANDLE hFile) : initialized_(false) {
        if (GGUF_StreamInit(hFile, &ctx_)) {
            initialized_ = true;
        }
    }
    
    ~BufferedGGUFStream() {
        if (initialized_) {
            GGUF_StreamFree(&ctx_);
        }
    }
    
    bool IsValid() const { return initialized_; }
    const GGUF_STREAM_CTX* GetContext() const { return initialized_ ? &ctx_ : nullptr; }
};

} // namespace rawrxd::gguf_masm
