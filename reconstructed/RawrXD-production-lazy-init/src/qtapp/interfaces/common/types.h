/**
 * \file common_types.h
 * \brief Common type definitions and enums for file operations
 * \author RawrXD Team
 * \date 2025-12-05
 */

#ifndef RAWRXD_COMMON_TYPES_H
#define RAWRXD_COMMON_TYPES_H

namespace RawrXD {

/**
 * \enum Encoding
 * \brief Supported file encodings for automatic detection and conversion
 * 
 * This enum is used across all file I/O interfaces and implementations
 * to specify and detect file encoding.
 */
enum class Encoding {
    UTF8,           ///< UTF-8 encoding (most common for text files)
    UTF16_LE,       ///< UTF-16 Little Endian
    UTF16_BE,       ///< UTF-16 Big Endian
    ASCII,          ///< ASCII encoding (7-bit clean)
    Unknown         ///< Could not detect encoding (fallback)
};

} // namespace RawrXD

#endif // RAWRXD_COMMON_TYPES_H
