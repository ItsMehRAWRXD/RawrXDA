#pragma once
#include <cstdint>
#include <cstdlib>

#ifdef _WIN64
#  define HAS_BRUTAL_GZIP_MASM
extern "C" std::uint8_t* __fastcall deflate_brutal_masm(const std::uint8_t* src,
                                                         std::uint64_t       len,
                                                         std::uint64_t*      out_len);
#endif

#ifdef __ARM_NEON
#  define HAS_BRUTAL_GZIP_NEON
extern "C" std::uint8_t* deflate_brutal_neon(const std::uint8_t* src,
                                              size_t              len,
                                              size_t*             out_len);
#endif
