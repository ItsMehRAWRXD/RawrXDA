#include "jwt_validator.h"
#include <windows.h>
#include <wincrypt.h>
#include <vector>

namespace RawrXD::Security {
    std::vector<uint8_t> base64UrlDecode(const std::string& in);
    std::string base64UrlEncode(const std::vector<uint8_t>& in);

    bool JWTValidator::validate(const std::string& token, const std::string& secret) {
        size_t firstDot = token.find('.');
        size_t secondDot = token.find('.', firstDot + 1);
        if (firstDot == std::string::npos || secondDot == std::string::npos) return false;

        std::string headerB64 = token.substr(0, firstDot);
        std::string payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
        std::string signatureB64 = token.substr(secondDot + 1);

        std::string dataToSign = headerB64 + "." + payloadB64;

        // HMAC-SHA256 via Windows CAPI
        HCRYPTPROV hProv = 0;
        HCRYPTHASH hHash = 0;
        HCRYPTKEY hKey = 0;

        if (!CryptAcquireContext(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
            return false;

        // Import plain key as HMAC key
        struct HmacKeyBlob { BLOBHEADER hdr; DWORD len; BYTE key[64]; } blob;
        blob.hdr.bType = PLAINTEXTKEYBLOB;
        blob.hdr.bVersion = CUR_BLOB_VERSION;
        blob.hdr.reserved = 0;
        blob.hdr.aiKeyAlg = CALG_RC2;
        blob.len = static_cast<DWORD>(secret.size());
        memcpy(blob.key, secret.data(), secret.size());

        if (!CryptImportKey(hProv, reinterpret_cast<BYTE*>(&blob), sizeof(BLOBHEADER) + sizeof(DWORD) + blob.len, 0, CRYPT_IPSEC_HMAC_KEY, &hKey)) {
            CryptReleaseContext(hProv, 0);
            return false;
        }

        if (!CryptCreateHash(hProv, CALG_SHA_256, hKey, 0, &hHash)) {
            CryptDestroyKey(hKey);
            CryptReleaseContext(hProv, 0);
            return false;
        }

        CryptHashData(hHash, reinterpret_cast<const BYTE*>(dataToSign.data()), static_cast<DWORD>(dataToSign.size()), 0);

        BYTE hash[32];
        DWORD hashLen = 32;
        CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0);

        CryptDestroyHash(hHash);
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);

        auto expectedSig = base64UrlDecode(signatureB64);
        return (expectedSig.size() == hashLen && memcmp(expectedSig.data(), hash, hashLen) == 0);
    }
}
