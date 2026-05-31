#pragma once

#include <string>
#include <vector>
#include <cstdint>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace RawrXD {

/**
 * @brief SHA-256 Checksum Validator for GGUF Tensors.
 * Ensures that vision transformer shards haven't been tampered with or corrupted on disk.
 */
class GGUFChecksumValidator {
public:
    static bool verify_sha256(const void* data, size_t length, const std::string& expected_hex) {
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        BYTE rgbHash[32];
        DWORD cbHash = 32;

        if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
            return false;
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
            CryptReleaseContext(hProv, 0);
            return false;
        }

        if (!CryptHashData(hHash, static_cast<const BYTE*>(data), static_cast<DWORD>(length), 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }

        if (!CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0)) {
            CryptDestroyHash(hHash);
            CryptReleaseContext(hProv, 0);
            return false;
        }

        std::string actual_hex = bytes_to_hex(rgbHash, 32);

        CryptDestroyHash(hHash);
        CryptReleaseContext(hProv, 0);

        return actual_hex == expected_hex;
    }

private:
    static std::string bytes_to_hex(const BYTE* bytes, size_t len) {
        static const char hex_chars[] = "0123456789abcdef";
        std::string res;
        res.reserve(len * 2);
        for (size_t i = 0; i < len; ++i) {
            res.push_back(hex_chars[bytes[i] >> 4]);
            res.push_back(hex_chars[bytes[i] & 0x0F]);
        }
        return res;
    }
};

} // namespace RawrXD
