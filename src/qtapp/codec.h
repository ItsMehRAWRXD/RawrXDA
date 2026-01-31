#pragma once

// Deflate (compress) using MASM-accelerated brutal algorithm
std::vector<uint8_t> deflate_brutal_masm(const std::vector<uint8_t>& data);

// Inflate (decompress) using MASM-accelerated brutal algorithm
std::vector<uint8_t> inflate_brutal_masm(const std::vector<uint8_t>& data);

