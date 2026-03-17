#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace codec {

/**
 * @brief Compress data using deflate algorithm
 * @param data Input data to compress
 * @param success Optional pointer to receive success status
 * @return Compressed data as std::vector<uint8_t>
 */
std::vector<uint8_t> deflate(const std::vector<uint8_t>& data, bool* success = nullptr);

/**
 * @brief Decompress data using inflate algorithm
 * @param data Compressed data to decompress
 * @param success Optional pointer to receive success status
 * @return Decompressed data as std::vector<uint8_t>
 */
std::vector<uint8_t> inflate(const std::vector<uint8_t>& data, bool* success = nullptr);


/**
 * @brief Compress data using MASM-optimized deflate
 * @param data Input data to compress
 * @param success Optional pointer to receive success status
 * @return Compressed data as QByteArray
 */
QByteArray deflate_brutal_masm(const QByteArray& data, bool* success = nullptr);

} // namespace codec