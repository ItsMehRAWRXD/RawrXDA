#include <cstdint>
#include <cstdlib>

// Provide wrapper for MASM symbols exported by inflate_deflate_asm.asm
extern "C" size_t AsmDeflate(const void* src, size_t src_len, void* dst, size_t dst_max_len);

// Matches declaration in include/brutal_gzip.hpp and src/qtapp/brutal_gzip.h
extern "C" std::uint8_t* __fastcall deflate_brutal_masm(const std::uint8_t* src,
                                                         std::uint64_t       len,
                                                         std::uint64_t*      out_len)
{
    if (!src || len == 0) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    // Compute a safe upper bound for stored-block gzip output
    // header(10) + footer(8) + ceil(len/65535)*(5) + len
    std::uint64_t blocks = (len + 65534) / 65535; // ceil
    std::uint64_t max_out = 10 + 8 + (blocks * 5) + len;

    void* dst = std::malloc(static_cast<size_t>(max_out));
    if (!dst) {
        if (out_len) *out_len = 0;
        return nullptr;
    }

    size_t written = AsmDeflate(src, static_cast<size_t>(len), dst, static_cast<size_t>(max_out));
    if (written == 0) {
        std::free(dst);
        if (out_len) *out_len = 0;
        return nullptr;
    }

    if (out_len) *out_len = static_cast<std::uint64_t>(written);
    return static_cast<std::uint8_t*>(dst);
}
