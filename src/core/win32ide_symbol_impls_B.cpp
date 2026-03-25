// win32ide_symbol_impls_B.cpp — RawrXD IDE debug agentic symbol implementations

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>

struct GGUF_LOADER_CTX {
    wchar_t path[512];
    uint32_t parse_done;
    uint64_t gpu_threshold;
    uint32_t tensor_count;
    uint64_t tensor_offsets[256];
    char tensor_names[256][64];
    uint64_t file_size;
};

struct GGUF_INFO_OUT {
    uint64_t file_size;
    uint32_t tensor_count;
    uint32_t reserved;
    uint64_t gpu_threshold;
};

struct GGUF_STATS_OUT {
    uint32_t tensor_count;
    uint32_t parse_done;
    uint64_t gpu_threshold;
    uint64_t file_size;
};

extern "C" {

// 1. asm_gguf_loader_get_info
void asm_gguf_loader_get_info(void* ctx, void* infoOut) {
    if (!ctx || !infoOut) return;
    GGUF_LOADER_CTX* c = static_cast<GGUF_LOADER_CTX*>(ctx);
    GGUF_INFO_OUT*   o = static_cast<GGUF_INFO_OUT*>(infoOut);
    o->file_size     = c->file_size;
    o->tensor_count  = c->tensor_count;
    o->reserved      = 0;
    o->gpu_threshold = c->gpu_threshold;
}

// 2. asm_gguf_loader_get_stats
void asm_gguf_loader_get_stats(void* ctx, void* statsOut) {
    if (!ctx || !statsOut) return;
    GGUF_LOADER_CTX* c = static_cast<GGUF_LOADER_CTX*>(ctx);
    GGUF_STATS_OUT*  o = static_cast<GGUF_STATS_OUT*>(statsOut);
    o->tensor_count  = c->tensor_count;
    o->parse_done    = c->parse_done;
    o->gpu_threshold = c->gpu_threshold;
    o->file_size     = c->file_size;
}

// 3. asm_gguf_loader_init
int asm_gguf_loader_init(void* ctxPtr, const wchar_t* path, uint32_t pathLen) {
    if (!ctxPtr || !path) return -1;

    GGUF_LOADER_CTX* c = static_cast<GGUF_LOADER_CTX*>(malloc(sizeof(GGUF_LOADER_CTX)));
    if (!c) return -2;

    memset(c, 0, sizeof(GGUF_LOADER_CTX));

    uint32_t copyLen = pathLen < 511u ? pathLen : 511u;
    memcpy(c->path, path, copyLen * sizeof(wchar_t));
    c->path[copyLen] = L'\0';

    static_cast<void**>(ctxPtr)[0] = static_cast<void*>(c);
    return 0;
}

// 4. asm_gguf_loader_lookup
int asm_gguf_loader_lookup(void* ctx, const char* name, uint32_t nameLen) {
    if (!ctx || !name || nameLen == 0) return -1;
    GGUF_LOADER_CTX* c = static_cast<GGUF_LOADER_CTX*>(ctx);
    uint32_t limit = nameLen < 63u ? nameLen : 63u;
    for (uint32_t i = 0; i < c->tensor_count && i < 256u; ++i) {
        if (strncmp(c->tensor_names[i], name, limit) == 0 &&
            (c->tensor_names[i][limit] == '\0' || limit == 63u)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// 5. asm_gguf_loader_parse
int asm_gguf_loader_parse(void* ctx) {
    if (!ctx) return -1;
    GGUF_LOADER_CTX* c = static_cast<GGUF_LOADER_CTX*>(ctx);

    HANDLE hFile = CreateFileW(
        c->path,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (hFile == INVALID_HANDLE_VALUE) return -2;

    // Read magic (4 bytes) and verify GGUF: 0x46554747 ('GGUF' little-endian)
    uint32_t magic = 0;
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, &magic, sizeof(magic), &bytesRead, nullptr) || bytesRead != 4) {
        CloseHandle(hFile);
        return -3;
    }
    if (magic != 0x46554747u) {
        CloseHandle(hFile);
        return -4;
    }

    // Get file size
    LARGE_INTEGER fileSize = {};
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return -5;
    }
    c->file_size = static_cast<uint64_t>(fileSize.QuadPart);

    // Read up to 256 tensor records; each record occupies 64 bytes (name only)
    // Skip version (4 bytes) + tensor_count field (8 bytes) in the GGUF header
    // GGUF v3 header layout: magic(4) + version(4) + tensor_count(8) + kv_count(8)
    uint32_t version = 0;
    if (!ReadFile(hFile, &version, 4, &bytesRead, nullptr) || bytesRead != 4) {
        CloseHandle(hFile);
        return -6;
    }

    uint64_t tensorCountFile = 0;
    if (!ReadFile(hFile, &tensorCountFile, 8, &bytesRead, nullptr) || bytesRead != 8) {
        CloseHandle(hFile);
        return -7;
    }

    uint32_t limit = static_cast<uint32_t>(tensorCountFile < 256ull ? tensorCountFile : 256ull);
    c->tensor_count = 0;

    // For each tensor: read a 64-byte name block (simplified — real GGUF uses length-prefixed strings,
    // but we read fixed 64-byte slots here for the symbol stub contract)
    for (uint32_t i = 0; i < limit; ++i) {
        char nameBuf[64] = {};
        if (!ReadFile(hFile, nameBuf, 64, &bytesRead, nullptr) || bytesRead == 0) break;
        memcpy(c->tensor_names[i], nameBuf, 64);
        c->tensor_names[i][63] = '\0';

        LARGE_INTEGER pos = {};
        if (!GetFileSizeEx(hFile, &pos)) break; // reuse handle trick not reliable; track offset instead
        // Store current file offset as tensor offset approximation
        LARGE_INTEGER cur = {};
        LARGE_INTEGER zero = {};
        zero.QuadPart = 0;
        SetFilePointerEx(hFile, zero, &cur, FILE_CURRENT);
        c->tensor_offsets[i] = static_cast<uint64_t>(cur.QuadPart);
        c->tensor_count = i + 1;
    }

    CloseHandle(hFile);
    c->parse_done = 1;
    return 0;
}

// 6. asm_hotpatch_alloc_shadow
void* asm_hotpatch_alloc_shadow(size_t size) {
    if (size == 0) size = 4096;
    void* mem = VirtualAlloc(
        nullptr,
        size,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE
    );
    return mem; // nullptr on failure — caller checks
}

// 7. asm_hotpatch_atomic_swap
// x64 absolute indirect JMP: FF 25 00 00 00 00 [8-byte address] = 14 bytes
int asm_hotpatch_atomic_swap(void* targetFn, void* newFn) {
    if (!targetFn || !newFn) return -1;

    // Build the 14-byte patch in a local buffer
    unsigned char patch[14];
    patch[0] = 0xFF;
    patch[1] = 0x25;
    patch[2] = 0x00;
    patch[3] = 0x00;
    patch[4] = 0x00;
    patch[5] = 0x00;
    uint64_t addr = reinterpret_cast<uint64_t>(newFn);
    memcpy(&patch[6], &addr, 8);

    // Make target page writable
    DWORD oldProtect = 0;
    if (!VirtualProtect(targetFn, 14, PAGE_EXECUTE_READWRITE, &oldProtect)) return -2;

    // Write patch bytes atomically enough for a hot-patch scenario
    memcpy(targetFn, patch, 14);

    // Flush instruction cache so CPUs see the new bytes
    FlushInstructionCache(GetCurrentProcess(), targetFn, 14);

    // Restore original protection
    DWORD dummy = 0;
    VirtualProtect(targetFn, 14, oldProtect, &dummy);

    return 0;
}

} // extern "C"
