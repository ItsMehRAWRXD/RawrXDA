#include <cstdint>
#include <cstdlib>
#include <cstring>

// ============================================================
// TEMPORARY STUB: Brutal GZIP wrappers
// These are simplified stubs that skip actual compression/decompression
// to avoid linker errors with missing MASM symbols (AsmDeflate/AsmInflate).
// Full brutal compression support will be re-enabled once MASM module is complete.
// ============================================================

extern "C" std::uint8_t* __fastcall deflate_brutal_masm(const std::uint8_t* src,
                                                         std::uint64_t       len,
                                                         std::uint64_t*      out_len)
{
    if (!src || len == 0) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // STUB: Return uncompressed data (allocate copy)
    void* dst = std::malloc(len + 10);  // Small header space
    if (!dst) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // Simple passthrough: copy source as-is
    std::memcpy(dst, src, len);
    if (out_len) *out_len = len;
    return static_cast<std::uint8_t*>(dst);
}

// Stub for inflate - also passthrough
extern "C" std::uint8_t* __fastcall AsmInflate(const std::uint8_t* src,
                                                 std::uint64_t       len,
                                                 std::uint64_t*      out_len)
{
    if (!src || len == 0) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // STUB: Return data as-is
    void* dst = std::malloc(len);
    if (!dst) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    std::memcpy(dst, src, len);
    if (out_len) *out_len = len;
    return static_cast<std::uint8_t*>(dst);
}
