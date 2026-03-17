/**
 * RawrXD Cryptographic Library Implementation
 * Production-ready cryptographic primitives
 */

#include "rawrxd_crypto.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <unistd.h>
#include <fcntl.h>
#endif

namespace RawrXD {
namespace Crypto {

// ============================================================
// BASE64 URL-SAFE ENCODING IMPLEMENTATION
// ============================================================

static const char BASE64URL_ENCODE_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static const int8_t BASE64URL_DECODE_TABLE[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

std::string Base64Url::encode(const uint8_t* data, size_t len) {
    if (len == 0) return "";
    
    size_t outlen = ((len + 2) / 3) * 4;
    std::string result;
    result.reserve(outlen);
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        
        result += BASE64URL_ENCODE_TABLE[(n >> 18) & 63];
        result += BASE64URL_ENCODE_TABLE[(n >> 12) & 63];
        if (i + 1 < len) result += BASE64URL_ENCODE_TABLE[(n >> 6) & 63];
        if (i + 2 < len) result += BASE64URL_ENCODE_TABLE[n & 63];
    }
    
    return result;
}

std::string Base64Url::encode(const std::vector<uint8_t>& data) {
    return encode(data.data(), data.size());
}

std::string Base64Url::encode(const std::string& data) {
    return encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> Base64Url::decode(const std::string& encoded) {
    std::vector<uint8_t> result;
    decode(encoded, result);
    return result;
}

bool Base64Url::decode(const std::string& encoded, std::vector<uint8_t>& output) {
    output.clear();
    if (encoded.empty()) return true;
    
    // Add padding if needed
    std::string padded = encoded;
    while (padded.size() % 4) padded += '=';
    
    size_t len = padded.size();
    output.reserve((len / 4) * 3);
    
    for (size_t i = 0; i < len; i += 4) {
        int8_t v[4];
        for (int j = 0; j < 4; j++) {
            if (padded[i + j] == '=') {
                v[j] = 0;
            } else {
                v[j] = BASE64URL_DECODE_TABLE[(uint8_t)padded[i + j]];
                if (v[j] < 0) return false; // Invalid character
            }
        }
        
        uint32_t n = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) |
                     ((uint32_t)v[2] << 6) | (uint32_t)v[3];
        
        output.push_back((n >> 16) & 0xFF);
        if (padded[i + 2] != '=') output.push_back((n >> 8) & 0xFF);
        if (padded[i + 3] != '=') output.push_back(n & 0xFF);
    }
    
    return true;
}

// ============================================================
// SECURE RANDOM NUMBER GENERATOR
// ============================================================

bool SecureRandom::generate(uint8_t* buffer, size_t size) {
#ifdef _WIN32
    return BCryptGenRandom(NULL, buffer, (ULONG)size,
                          BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return false;
    
    size_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, buffer + total, size - total);
        if (n <= 0) {
            close(fd);
            return false;
        }
        total += n;
    }
    close(fd);
    return true;
#endif
}

std::vector<uint8_t> SecureRandom::generate(size_t size) {
    std::vector<uint8_t> result(size);
    if (!generate(result.data(), size)) {
        throw std::runtime_error("Failed to generate secure random bytes");
    }
    return result;
}

uint32_t SecureRandom::generateUInt32() {
    uint32_t result;
    generate(reinterpret_cast<uint8_t*>(&result), sizeof(result));
    return result;
}

uint64_t SecureRandom::generateUInt64() {
    uint64_t result;
    generate(reinterpret_cast<uint8_t*>(&result), sizeof(result));
    return result;
}

// ============================================================
// SHA-256 IMPLEMENTATION
// ============================================================

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x, n) ((x) >> (n))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR32(x, 2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x) (ROTR32(x, 6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x) (ROTR32(x, 7) ^ ROTR32(x, 18) ^ SHR(x, 3))
#define SIG1(x) (ROTR32(x, 17) ^ ROTR32(x, 19) ^ SHR(x, 10))

constexpr uint32_t SHA256::K[64];

SHA256::SHA256() {
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
    bitlen_ = 0;
    buflen_ = 0;
}

void SHA256::update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buffer_[buflen_++] = data[i];
        if (buflen_ == BLOCK_SIZE) {
            transform();
            bitlen_ += 512;
            buflen_ = 0;
        }
    }
}

void SHA256::update(const std::vector<uint8_t>& data) {
    update(data.data(), data.size());
}

void SHA256::update(const std::string& data) {
    update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void SHA256::transform() {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    
    // Prepare message schedule
    for (int i = 0; i < 16; i++) {
        w[i] = (uint32_t)buffer_[i * 4] << 24 |
               (uint32_t)buffer_[i * 4 + 1] << 16 |
               (uint32_t)buffer_[i * 4 + 2] << 8 |
               (uint32_t)buffer_[i * 4 + 3];
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SIG1(w[i - 2]) + w[i - 7] + SIG0(w[i - 15]) + w[i - 16];
    }
    
    // Initialize working variables
    a = state_[0];
    b = state_[1];
    c = state_[2];
    d = state_[3];
    e = state_[4];
    f = state_[5];
    g = state_[6];
    h = state_[7];
    
    // Main loop
    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + w[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // Update state
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void SHA256::finalize(uint8_t digest[DIGEST_SIZE]) {
    size_t i = buflen_;
    
    // Pad
    buffer_[i++] = 0x80;
    if (buflen_ < 56) {
        while (i < 56) buffer_[i++] = 0x00;
    } else {
        while (i < 64) buffer_[i++] = 0x00;
        transform();
        memset(buffer_, 0, 56);
    }
    
    // Append length
    uint64_t totalBits = bitlen_ + buflen_ * 8;
    buffer_[63] = totalBits;
    buffer_[62] = totalBits >> 8;
    buffer_[61] = totalBits >> 16;
    buffer_[60] = totalBits >> 24;
    buffer_[59] = totalBits >> 32;
    buffer_[58] = totalBits >> 40;
    buffer_[57] = totalBits >> 48;
    buffer_[56] = totalBits >> 56;
    transform();
    
    // Produce final hash
    for (i = 0; i < 8; i++) {
        digest[i * 4] = (state_[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (state_[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (state_[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = state_[i] & 0xFF;
    }
}

std::vector<uint8_t> SHA256::finalize() {
    std::vector<uint8_t> result(DIGEST_SIZE);
    finalize(result.data());
    return result;
}

void SHA256::hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_SIZE]) {
    SHA256 sha;
    sha.update(data, len);
    sha.finalize(digest);
}

std::vector<uint8_t> SHA256::hash(const uint8_t* data, size_t len) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    hash(data, len, result.data());
    return result;
}

std::vector<uint8_t> SHA256::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

std::vector<uint8_t> SHA256::hash(const std::string& data) {
    return hash(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

// ============================================================
// SHA-384/512 IMPLEMENTATION
// ============================================================

#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
#define SHR64(x, n) ((x) >> (n))
#define CH64(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ64(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0_512(x) (ROTR64(x, 28) ^ ROTR64(x, 34) ^ ROTR64(x, 39))
#define EP1_512(x) (ROTR64(x, 14) ^ ROTR64(x, 18) ^ ROTR64(x, 41))
#define SIG0_512(x) (ROTR64(x, 1) ^ ROTR64(x, 8) ^ SHR64(x, 7))
#define SIG1_512(x) (ROTR64(x, 19) ^ ROTR64(x, 61) ^ SHR64(x, 6))

static const uint64_t K512[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

SHA512::SHA512() {
    state_[0] = 0x6a09e667f3bcc908ULL;
    state_[1] = 0xbb67ae8584caa73bULL;
    state_[2] = 0x3c6ef372fe94f82bULL;
    state_[3] = 0xa54ff53a5f1d36f1ULL;
    state_[4] = 0x510e527fade682d1ULL;
    state_[5] = 0x9b05688c2b3e6c1fULL;
    state_[6] = 0x1f83d9abfb41bd6bULL;
    state_[7] = 0x5be0cd19137e2179ULL;
    bitlen_[0] = 0;
    bitlen_[1] = 0;
    buflen_ = 0;
}

void SHA512::update(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buffer_[buflen_++] = data[i];
        if (buflen_ == BLOCK_SIZE) {
            transform();
            // Increment 128-bit counter
            bitlen_[0] += 1024;
            if (bitlen_[0] < 1024) bitlen_[1]++;
            buflen_ = 0;
        }
    }
}

void SHA512::update(const std::vector<uint8_t>& data) {
    update(data.data(), data.size());
}

void SHA512::update(const std::string& data) {
    update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void SHA512::transform() {
    uint64_t w[80];
    uint64_t a, b, c, d, e, f, g, h, t1, t2;
    
    // Prepare message schedule
    for (int i = 0; i < 16; i++) {
        w[i] = (uint64_t)buffer_[i * 8] << 56 |
               (uint64_t)buffer_[i * 8 + 1] << 48 |
               (uint64_t)buffer_[i * 8 + 2] << 40 |
               (uint64_t)buffer_[i * 8 + 3] << 32 |
               (uint64_t)buffer_[i * 8 + 4] << 24 |
               (uint64_t)buffer_[i * 8 + 5] << 16 |
               (uint64_t)buffer_[i * 8 + 6] << 8 |
               (uint64_t)buffer_[i * 8 + 7];
    }
    for (int i = 16; i < 80; i++) {
        w[i] = SIG1_512(w[i - 2]) + w[i - 7] + SIG0_512(w[i - 15]) + w[i - 16];
    }
    
    // Initialize working variables
    a = state_[0]; b = state_[1]; c = state_[2]; d = state_[3];
    e = state_[4]; f = state_[5]; g = state_[6]; h = state_[7];
    
    // Main loop
    for (int i = 0; i < 80; i++) {
        t1 = h + EP1_512(e) + CH64(e, f, g) + K512[i] + w[i];
        t2 = EP0_512(a) + MAJ64(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    
    // Update state
    state_[0] += a; state_[1] += b; state_[2] += c; state_[3] += d;
    state_[4] += e; state_[5] += f; state_[6] += g; state_[7] += h;
}

void SHA512::finalize(uint8_t digest[DIGEST_SIZE]) {
    size_t i = buflen_;
    
    // Pad
    buffer_[i++] = 0x80;
    if (buflen_ < 112) {
        while (i < 112) buffer_[i++] = 0x00;
    } else {
        while (i < 128) buffer_[i++] = 0x00;
        transform();
        memset(buffer_, 0, 112);
    }
    
    // Append length (128-bit)
    uint64_t totalBits = bitlen_[0] + buflen_ * 8;
    uint64_t totalBitsHi = bitlen_[1];
    if (totalBits < buflen_ * 8) totalBitsHi++;
    
    for (int j = 0; j < 8; j++) {
        buffer_[120 + j] = (totalBits >> (56 - j * 8)) & 0xFF;
        buffer_[112 + j] = (totalBitsHi >> (56 - j * 8)) & 0xFF;
    }
    transform();
    
    // Produce final hash
    for (i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            digest[i * 8 + j] = (state_[i] >> (56 - j * 8)) & 0xFF;
        }
    }
}

std::vector<uint8_t> SHA512::finalize() {
    std::vector<uint8_t> result(DIGEST_SIZE);
    finalize(result.data());
    return result;
}

void SHA512::hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_SIZE]) {
    SHA512 sha;
    sha.update(data, len);
    sha.finalize(digest);
}

std::vector<uint8_t> SHA512::hash(const uint8_t* data, size_t len) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    hash(data, len, result.data());
    return result;
}

std::vector<uint8_t> SHA512::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

std::vector<uint8_t> SHA512::hash(const std::string& data) {
    return hash(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

// SHA-384 (truncated SHA-512 with different IV)
SHA384::SHA384() {
    state_[0] = 0xcbbb9d5dc1059ed8ULL;
    state_[1] = 0x629a292a367cd507ULL;
    state_[2] = 0x9159015a3070dd17ULL;
    state_[3] = 0x152fecd8f70e5939ULL;
    state_[4] = 0x67332667ffc00b31ULL;
    state_[5] = 0x8eb44a8768581511ULL;
    state_[6] = 0xdb0c2e0d64f98fa7ULL;
    state_[7] = 0x47b5481dbefa4fa4ULL;
    bitlen_[0] = 0;
    bitlen_[1] = 0;
    buflen_ = 0;
}

void SHA384::update(const uint8_t* data, size_t len) {
    SHA512 sha;
    memcpy(sha.state_, this->state_, sizeof(state_));
    sha.bitlen_[0] = this->bitlen_[0];
    sha.bitlen_[1] = this->bitlen_[1];
    sha.buflen_ = this->buflen_;
    memcpy(sha.buffer_, this->buffer_, buflen_);
    
    sha.update(data, len);
    
    memcpy(this->state_, sha.state_, sizeof(state_));
    this->bitlen_[0] = sha.bitlen_[0];
    this->bitlen_[1] = sha.bitlen_[1];
    this->buflen_ = sha.buflen_;
    memcpy(this->buffer_, sha.buffer_, buflen_);
}

void SHA384::update(const std::vector<uint8_t>& data) {
    update(data.data(), data.size());
}

void SHA384::update(const std::string& data) {
    update(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

void SHA384::transform() {}

void SHA384::finalize(uint8_t digest[DIGEST_SIZE]) {
    uint8_t sha512_digest[64];
    SHA512 sha;
    memcpy(sha.state_, this->state_, sizeof(state_));
    sha.bitlen_[0] = this->bitlen_[0];
    sha.bitlen_[1] = this->bitlen_[1];
    sha.buflen_ = this->buflen_;
    memcpy(sha.buffer_, this->buffer_, buflen_);
    sha.finalize(sha512_digest);
    memcpy(digest, sha512_digest, DIGEST_SIZE);
}

std::vector<uint8_t> SHA384::finalize() {
    std::vector<uint8_t> result(DIGEST_SIZE);
    finalize(result.data());
    return result;
}

void SHA384::hash(const uint8_t* data, size_t len, uint8_t digest[DIGEST_SIZE]) {
    SHA384 sha;
    sha.update(data, len);
    sha.finalize(digest);
}

std::vector<uint8_t> SHA384::hash(const uint8_t* data, size_t len) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    hash(data, len, result.data());
    return result;
}

std::vector<uint8_t> SHA384::hash(const std::vector<uint8_t>& data) {
    return hash(data.data(), data.size());
}

std::vector<uint8_t> SHA384::hash(const std::string& data) {
    return hash(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

// ============================================================
// HMAC IMPLEMENTATION
// ============================================================

void HMAC_SHA256::compute(const uint8_t* key, size_t keylen,
                          const uint8_t* message, size_t msglen,
                          uint8_t mac[DIGEST_SIZE]) {
    uint8_t k[SHA256::BLOCK_SIZE];
    uint8_t k_ipad[SHA256::BLOCK_SIZE];
    uint8_t k_opad[SHA256::BLOCK_SIZE];
    
    // If key is longer than block size, hash it
    if (keylen > SHA256::BLOCK_SIZE) {
        SHA256::hash(key, keylen, k);
        memset(k + SHA256::DIGEST_SIZE, 0, SHA256::BLOCK_SIZE - SHA256::DIGEST_SIZE);
    } else {
        memcpy(k, key, keylen);
        memset(k + keylen, 0, SHA256::BLOCK_SIZE - keylen);
    }
    
    // Compute k_ipad and k_opad
    for (size_t i = 0; i < SHA256::BLOCK_SIZE; i++) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }
    
    // Inner hash: H(K ⊕ ipad || message)
    SHA256 inner;
    inner.update(k_ipad, SHA256::BLOCK_SIZE);
    inner.update(message, msglen);
    uint8_t inner_hash[SHA256::DIGEST_SIZE];
    inner.finalize(inner_hash);
    
    // Outer hash: H(K ⊕ opad || inner_hash)
    SHA256 outer;
    outer.update(k_opad, SHA256::BLOCK_SIZE);
    outer.update(inner_hash, SHA256::DIGEST_SIZE);
    outer.finalize(mac);
    
    // Zero sensitive data
    SecureMemory::secureZero(k, sizeof(k));
    SecureMemory::secureZero(k_ipad, sizeof(k_ipad));
    SecureMemory::secureZero(k_opad, sizeof(k_opad));
    SecureMemory::secureZero(inner_hash, sizeof(inner_hash));
}

std::vector<uint8_t> HMAC_SHA256::compute(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& message) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    compute(key.data(), key.size(), message.data(), message.size(), result.data());
    return result;
}

std::vector<uint8_t> HMAC_SHA256::compute(const std::string& key,
                                          const std::string& message) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    compute(reinterpret_cast<const uint8_t*>(key.data()), key.size(),
           reinterpret_cast<const uint8_t*>(message.data()), message.size(),
           result.data());
    return result;
}

bool HMAC_SHA256::verify(const uint8_t* mac1, const uint8_t* mac2, size_t len) {
    return SecureMemory::constantTimeCompare(mac1, mac2, len) == 0;
}

// Similar implementations for HMAC_SHA384 and HMAC_SHA512
void HMAC_SHA384::compute(const uint8_t* key, size_t keylen,
                          const uint8_t* message, size_t msglen,
                          uint8_t mac[DIGEST_SIZE]) {
    uint8_t k[SHA384::BLOCK_SIZE];
    uint8_t k_ipad[SHA384::BLOCK_SIZE];
    uint8_t k_opad[SHA384::BLOCK_SIZE];
    
    if (keylen > SHA384::BLOCK_SIZE) {
        SHA384::hash(key, keylen, k);
        memset(k + SHA384::DIGEST_SIZE, 0, SHA384::BLOCK_SIZE - SHA384::DIGEST_SIZE);
    } else {
        memcpy(k, key, keylen);
        memset(k + keylen, 0, SHA384::BLOCK_SIZE - keylen);
    }
    
    for (size_t i = 0; i < SHA384::BLOCK_SIZE; i++) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }
    
    SHA384 inner;
    inner.update(k_ipad, SHA384::BLOCK_SIZE);
    inner.update(message, msglen);
    uint8_t inner_hash[SHA384::DIGEST_SIZE];
    inner.finalize(inner_hash);
    
    SHA384 outer;
    outer.update(k_opad, SHA384::BLOCK_SIZE);
    outer.update(inner_hash, SHA384::DIGEST_SIZE);
    outer.finalize(mac);
    
    SecureMemory::secureZero(k, sizeof(k));
    SecureMemory::secureZero(k_ipad, sizeof(k_ipad));
    SecureMemory::secureZero(k_opad, sizeof(k_opad));
    SecureMemory::secureZero(inner_hash, sizeof(inner_hash));
}

std::vector<uint8_t> HMAC_SHA384::compute(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& message) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    compute(key.data(), key.size(), message.data(), message.size(), result.data());
    return result;
}

bool HMAC_SHA384::verify(const uint8_t* mac1, const uint8_t* mac2, size_t len) {
    return SecureMemory::constantTimeCompare(mac1, mac2, len) == 0;
}

void HMAC_SHA512::compute(const uint8_t* key, size_t keylen,
                          const uint8_t* message, size_t msglen,
                          uint8_t mac[DIGEST_SIZE]) {
    uint8_t k[SHA512::BLOCK_SIZE];
    uint8_t k_ipad[SHA512::BLOCK_SIZE];
    uint8_t k_opad[SHA512::BLOCK_SIZE];
    
    if (keylen > SHA512::BLOCK_SIZE) {
        SHA512::hash(key, keylen, k);
        memset(k + SHA512::DIGEST_SIZE, 0, SHA512::BLOCK_SIZE - SHA512::DIGEST_SIZE);
    } else {
        memcpy(k, key, keylen);
        memset(k + keylen, 0, SHA512::BLOCK_SIZE - keylen);
    }
    
    for (size_t i = 0; i < SHA512::BLOCK_SIZE; i++) {
        k_ipad[i] = k[i] ^ 0x36;
        k_opad[i] = k[i] ^ 0x5c;
    }
    
    SHA512 inner;
    inner.update(k_ipad, SHA512::BLOCK_SIZE);
    inner.update(message, msglen);
    uint8_t inner_hash[SHA512::DIGEST_SIZE];
    inner.finalize(inner_hash);
    
    SHA512 outer;
    outer.update(k_opad, SHA512::BLOCK_SIZE);
    outer.update(inner_hash, SHA512::DIGEST_SIZE);
    outer.finalize(mac);
    
    SecureMemory::secureZero(k, sizeof(k));
    SecureMemory::secureZero(k_ipad, sizeof(k_ipad));
    SecureMemory::secureZero(k_opad, sizeof(k_opad));
    SecureMemory::secureZero(inner_hash, sizeof(inner_hash));
}

std::vector<uint8_t> HMAC_SHA512::compute(const std::vector<uint8_t>& key,
                                          const std::vector<uint8_t>& message) {
    std::vector<uint8_t> result(DIGEST_SIZE);
    compute(key.data(), key.size(), message.data(), message.size(), result.data());
    return result;
}

bool HMAC_SHA512::verify(const uint8_t* mac1, const uint8_t* mac2, size_t len) {
    return SecureMemory::constantTimeCompare(mac1, mac2, len) == 0;
}

// ============================================================
// SECURE MEMORY UTILITIES
// ============================================================

int SecureMemory::constantTimeCompare(const void* a, const void* b, size_t len) {
    const uint8_t* aa = static_cast<const uint8_t*>(a);
    const uint8_t* bb = static_cast<const uint8_t*>(b);
    uint8_t result = 0;
    
    for (size_t i = 0; i < len; i++) {
        result |= aa[i] ^ bb[i];
    }
    
    return result;
}

void SecureMemory::secureZero(void* ptr, size_t len) {
#ifdef _WIN32
    SecureZeroMemory(ptr, len);
#else
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    while (len--) *p++ = 0;
#endif
}

void SecureMemory::constantTimeCopy(uint8_t* dest, const uint8_t* src,
                                   size_t len, int condition) {
    uint8_t mask = (uint8_t)(-(int8_t)(condition & 1));
    for (size_t i = 0; i < len; i++) {
        dest[i] ^= (dest[i] ^ src[i]) & mask;
    }
}

} // namespace Crypto
} // namespace RawrXD
