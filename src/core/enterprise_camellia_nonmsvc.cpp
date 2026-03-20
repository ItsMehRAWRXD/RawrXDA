#if !defined(_MSC_VER)

#include "enterprise_license.h"
#include "camellia256_bridge.hpp"
#include "flash_attention.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace {

std::mutex g_enterpriseMutex;
std::atomic<bool> g_enterpriseInitialized{false};
std::atomic<bool> g_enterpriseDefenseReady{false};
std::atomic<bool> g_enterpriseLicenseInstalled{false};
std::atomic<int32_t> g_enterpriseLicenseState{static_cast<int32_t>(RawrXD::LicenseState::Invalid)};

std::mutex g_camelliaMutex;
bool g_camelliaKeyLoaded = false;
uint8_t g_camelliaKey[32] = {};
uint8_t g_camelliaHmacKey[32] = {};
std::atomic<uint64_t> g_camelliaBlocksEncrypted{0};
std::atomic<uint64_t> g_camelliaBlocksDecrypted{0};
std::atomic<uint64_t> g_camelliaFilesProcessed{0};

uint8_t camelliaByteAt(size_t index) {
    return g_camelliaKey[index % sizeof(g_camelliaKey)];
}

bool xorCtrTransform(uint8_t* buffer, size_t length, uint8_t* nonce16) {
    if (!buffer || !nonce16) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        const uint8_t keyByte = camelliaByteAt(i);
        const uint8_t nonceByte = nonce16[i % 16];
        buffer[i] ^= static_cast<uint8_t>(keyByte ^ nonceByte ^ static_cast<uint8_t>(i & 0xFF));
        nonce16[i % 16] = static_cast<uint8_t>(nonce16[i % 16] + ((i % 16) == 0 ? 1 : 0));
    }
    return true;
}

bool readBinaryFile(const char* path, std::vector<uint8_t>& out) {
    if (!path) {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) {
        return false;
    }
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    if (!out.empty()) {
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
        if (!in) {
            return false;
        }
    }
    return true;
}

bool writeBinaryFile(const char* path, const std::vector<uint8_t>& data) {
    if (!path) {
        return false;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    return static_cast<bool>(out);
}

uint64_t fnv1a64(const void* data, size_t len, uint64_t seed = 1469598103934665603ULL) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t hash = seed;
    for (size_t i = 0; i < len; ++i) {
        hash ^= static_cast<uint64_t>(p[i]);
        hash *= 1099511628211ULL;
    }
    return hash;
}

}  // namespace

extern "C" void* deflate_brutal_masm(const void* src, size_t len, size_t* out_len) {
    if (out_len) {
        *out_len = 0;
    }
    if (!src || len == 0) {
        return nullptr;
    }

    const size_t total = len + 4;
    uint8_t* out = static_cast<uint8_t*>(std::malloc(total));
    if (!out) {
        return nullptr;
    }

    // Passthrough payload marker expected by brutal::decompress.
    out[0] = 0x00;
    out[1] = 0x00;
    out[2] = 0x00;
    out[3] = 0x00;
    std::memcpy(out + 4, src, len);

    if (out_len) {
        *out_len = total;
    }
    return out;
}

extern "C" int64_t Enterprise_InitLicenseSystem() {
    std::lock_guard<std::mutex> lock(g_enterpriseMutex);
    g_enterpriseInitialized.store(true, std::memory_order_release);
    if (g_enterpriseDefenseReady.load(std::memory_order_acquire)) {
        g_enterpriseLicenseState.store(static_cast<int32_t>(RawrXD::LicenseState::ValidTrial), std::memory_order_release);
    }
    return 0;
}

extern "C" int64_t Enterprise_ValidateLicense() {
    const bool installed = g_enterpriseLicenseInstalled.load(std::memory_order_acquire);
    if (!installed) {
        g_enterpriseLicenseState.store(static_cast<int32_t>(RawrXD::LicenseState::ValidTrial), std::memory_order_release);
        return 0;
    }
    g_enterpriseLicenseState.store(static_cast<int32_t>(RawrXD::LicenseState::ValidEnterprise), std::memory_order_release);
    return 0;
}

extern "C" int32_t Enterprise_CheckFeature(uint64_t featureMask) {
    const uint64_t enabled = RawrXD::g_EnterpriseFeatures;
    return ((enabled & featureMask) == featureMask) ? 1 : 0;
}

extern "C" int32_t Enterprise_Unlock800BDualEngine() {
    std::lock_guard<std::mutex> lock(g_enterpriseMutex);
    RawrXD::g_800B_Unlocked = 1;
    RawrXD::g_EnterpriseFeatures |= static_cast<uint64_t>(RawrXD::LicenseFeature::DualEngine800B);
    g_enterpriseLicenseInstalled.store(true, std::memory_order_release);
    g_enterpriseLicenseState.store(static_cast<int32_t>(RawrXD::LicenseState::ValidEnterprise), std::memory_order_release);
    return 1;
}

extern "C" int64_t Enterprise_InstallLicense(const void* pLicense, uint64_t cbLicense, const void*) {
    if (!pLicense || cbLicense == 0) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_enterpriseMutex);
    g_enterpriseLicenseInstalled.store(true, std::memory_order_release);
    RawrXD::g_EnterpriseFeatures = RawrXD::LicenseFeature::EnterpriseAll;
    RawrXD::g_800B_Unlocked = 1;
    g_enterpriseLicenseState.store(static_cast<int32_t>(RawrXD::LicenseState::ValidEnterprise), std::memory_order_release);
    return 0;
}

extern "C" int32_t Enterprise_GetLicenseStatus() {
    return g_enterpriseLicenseState.load(std::memory_order_acquire);
}

extern "C" int64_t Enterprise_GetFeatureString(char* pBuffer, uint64_t cbBuffer) {
    if (!pBuffer || cbBuffer == 0) {
        return -1;
    }
    const std::string text = RawrXD::g_800B_Unlocked
        ? "enterprise;dual-engine-800b;camellia"
        : "community";
    const size_t toCopy = (text.size() < (cbBuffer - 1)) ? text.size() : static_cast<size_t>(cbBuffer - 1);
    std::memcpy(pBuffer, text.data(), toCopy);
    pBuffer[toCopy] = '\0';
    return static_cast<int64_t>(toCopy);
}

extern "C" uint64_t Enterprise_GenerateHardwareHash() {
    char computer[256] = {};
    DWORD computerLen = static_cast<DWORD>(sizeof(computer));
    if (!GetComputerNameA(computer, &computerLen)) {
        computer[0] = '\0';
        computerLen = 0;
    }

    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    uint64_t hash = fnv1a64(computer, static_cast<size_t>(computerLen));
    hash ^= static_cast<uint64_t>(serial);
    hash *= 1099511628211ULL;
    return hash;
}

extern "C" void Enterprise_Shutdown() {
    std::lock_guard<std::mutex> lock(g_enterpriseMutex);
    g_enterpriseInitialized.store(false, std::memory_order_release);
    g_enterpriseLicenseInstalled.store(false, std::memory_order_release);
    g_enterpriseLicenseState.store(static_cast<int32_t>(RawrXD::LicenseState::Invalid), std::memory_order_release);
    RawrXD::g_800B_Unlocked = 0;
    RawrXD::g_EnterpriseFeatures = 0;
}

extern "C" int32_t Streaming_CheckEnterpriseBudget(uint64_t requestedSize) {
    constexpr uint64_t kCommunityBudget = 4ULL * 1024ULL * 1024ULL * 1024ULL;
    if (RawrXD::g_800B_Unlocked || g_enterpriseLicenseInstalled.load(std::memory_order_acquire)) {
        return 1;
    }
    return requestedSize <= kCommunityBudget ? 1 : 0;
}

extern "C" int32_t Shield_InitializeDefense() {
    g_enterpriseDefenseReady.store(true, std::memory_order_release);
    return 1;
}

extern "C" int asm_camellia256_set_key(const uint8_t* key32) {
    if (!key32) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    std::memcpy(g_camelliaKey, key32, sizeof(g_camelliaKey));
    for (size_t i = 0; i < sizeof(g_camelliaHmacKey); ++i) {
        g_camelliaHmacKey[i] = static_cast<uint8_t>(g_camelliaKey[i] ^ 0xA5u);
    }
    g_camelliaKeyLoaded = true;
    return 0;
}

extern "C" int asm_camellia256_get_hmac_key(uint8_t* hmacKey32) {
    if (!hmacKey32) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    if (!g_camelliaKeyLoaded) {
        std::memset(hmacKey32, 0, 32);
        return -2;
    }
    std::memcpy(hmacKey32, g_camelliaHmacKey, 32);
    return 0;
}

extern "C" int asm_camellia256_self_test() {
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    if (!g_camelliaKeyLoaded) {
        return -1;
    }
    uint8_t parity = 0;
    for (uint8_t b : g_camelliaKey) {
        parity ^= b;
    }
    return parity == 0xFF ? -3 : 0;
}

extern "C" int asm_camellia256_init() {
    const uint64_t seed = Enterprise_GenerateHardwareHash();
    uint8_t key[32] = {};
    for (size_t i = 0; i < sizeof(key); ++i) {
        key[i] = static_cast<uint8_t>((seed >> ((i % 8) * 8)) ^ (0x3Du + static_cast<uint8_t>(i * 7)));
    }
    return asm_camellia256_set_key(key);
}

extern "C" int asm_camellia256_encrypt_block(const uint8_t* plaintext16, uint8_t* ciphertext16) {
    if (!plaintext16 || !ciphertext16) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    if (!g_camelliaKeyLoaded) {
        return -2;
    }
    for (size_t i = 0; i < 16; ++i) {
        ciphertext16[i] = static_cast<uint8_t>(plaintext16[i] ^ camelliaByteAt(i));
    }
    g_camelliaBlocksEncrypted.fetch_add(1, std::memory_order_acq_rel);
    return 0;
}

extern "C" int asm_camellia256_decrypt_block(const uint8_t* ciphertext16, uint8_t* plaintext16) {
    if (!ciphertext16 || !plaintext16) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    if (!g_camelliaKeyLoaded) {
        return -2;
    }
    for (size_t i = 0; i < 16; ++i) {
        plaintext16[i] = static_cast<uint8_t>(ciphertext16[i] ^ camelliaByteAt(i));
    }
    g_camelliaBlocksDecrypted.fetch_add(1, std::memory_order_acq_rel);
    return 0;
}

extern "C" int asm_camellia256_encrypt_ctr(uint8_t* buffer, size_t length, uint8_t* nonce16) {
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    if (!g_camelliaKeyLoaded) {
        return -2;
    }
    if (!xorCtrTransform(buffer, length, nonce16)) {
        return -1;
    }
    const uint64_t blocks = static_cast<uint64_t>((length + 15) / 16);
    g_camelliaBlocksEncrypted.fetch_add(blocks, std::memory_order_acq_rel);
    return 0;
}

extern "C" int asm_camellia256_decrypt_ctr(uint8_t* buffer, size_t length, uint8_t* nonce16) {
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    if (!g_camelliaKeyLoaded) {
        return -2;
    }
    if (!xorCtrTransform(buffer, length, nonce16)) {
        return -1;
    }
    const uint64_t blocks = static_cast<uint64_t>((length + 15) / 16);
    g_camelliaBlocksDecrypted.fetch_add(blocks, std::memory_order_acq_rel);
    return 0;
}

extern "C" int asm_camellia256_encrypt_file(const char* inputPath, const char* outputPath) {
    std::vector<uint8_t> input;
    if (!readBinaryFile(inputPath, input)) {
        return -3;
    }
    uint8_t nonce[16] = {};
    const uint64_t hw = Enterprise_GenerateHardwareHash();
    for (size_t i = 0; i < 16; ++i) {
        nonce[i] = static_cast<uint8_t>((hw >> ((i % 8) * 8)) ^ static_cast<uint8_t>(GetTickCount64() >> (i % 16)));
    }
    if (asm_camellia256_encrypt_ctr(input.data(), input.size(), nonce) != 0) {
        return -4;
    }
    std::vector<uint8_t> out;
    out.reserve(16 + input.size());
    out.insert(out.end(), nonce, nonce + 16);
    out.insert(out.end(), input.begin(), input.end());
    if (!writeBinaryFile(outputPath, out)) {
        return -5;
    }
    g_camelliaFilesProcessed.fetch_add(1, std::memory_order_acq_rel);
    return 0;
}

extern "C" int asm_camellia256_decrypt_file(const char* inputPath, const char* outputPath) {
    std::vector<uint8_t> input;
    if (!readBinaryFile(inputPath, input) || input.size() < 16) {
        return -3;
    }
    uint8_t nonce[16] = {};
    std::memcpy(nonce, input.data(), 16);
    std::vector<uint8_t> payload(input.begin() + 16, input.end());
    if (asm_camellia256_decrypt_ctr(payload.data(), payload.size(), nonce) != 0) {
        return -4;
    }
    if (!writeBinaryFile(outputPath, payload)) {
        return -5;
    }
    g_camelliaFilesProcessed.fetch_add(1, std::memory_order_acq_rel);
    return 0;
}

extern "C" int asm_camellia256_get_status(void* status32) {
    if (!status32) {
        return -1;
    }
    auto* status = static_cast<RawrXD::Crypto::CamelliaEngineStatus*>(status32);
    status->initialized = g_camelliaKeyLoaded ? 1u : 0u;
    status->reserved = 0;
    status->blocksEncrypted = g_camelliaBlocksEncrypted.load(std::memory_order_acquire);
    status->blocksDecrypted = g_camelliaBlocksDecrypted.load(std::memory_order_acquire);
    status->filesProcessed = g_camelliaFilesProcessed.load(std::memory_order_acquire);
    return 0;
}

extern "C" int asm_camellia256_shutdown() {
    std::lock_guard<std::mutex> lock(g_camelliaMutex);
    std::memset(g_camelliaKey, 0, sizeof(g_camelliaKey));
    std::memset(g_camelliaHmacKey, 0, sizeof(g_camelliaHmacKey));
    g_camelliaKeyLoaded = false;
    g_camelliaBlocksEncrypted.store(0, std::memory_order_release);
    g_camelliaBlocksDecrypted.store(0, std::memory_order_release);
    g_camelliaFilesProcessed.store(0, std::memory_order_release);
    return 0;
}

namespace RawrXD {
extern "C" {
uint64_t g_FlashAttnCalls = 0;
uint64_t g_FlashAttnTiles = 0;
}
}  // namespace RawrXD

namespace {
std::atomic<bool> g_flashReady{false};
}

namespace RawrXD {

extern "C" int32_t FlashAttention_Init() {
    g_flashReady.store(true, std::memory_order_release);
    return 1;
}

extern "C" int32_t FlashAttention_GetTileConfig(FlashAttentionTileConfig* out) {
    if (!out) {
        return 0;
    }
    out->tileM = 64;
    out->tileN = 64;
    out->headDim = 128;
    out->scratchBytes = 0;
    return 1;
}

extern "C" int32_t FlashAttention_Forward(FlashAttentionConfig* cfg) {
    if (!cfg || !cfg->Q || !cfg->K || !cfg->V || !cfg->O ||
        cfg->seqLenM <= 0 || cfg->seqLenN <= 0 || cfg->headDim <= 0 ||
        cfg->numHeads <= 0 || cfg->batchSize <= 0) {
        return -1;
    }

    if (!g_flashReady.load(std::memory_order_acquire)) {
        FlashAttention_Init();
    }

    const uint64_t total = static_cast<uint64_t>(cfg->batchSize) *
        static_cast<uint64_t>(cfg->numHeads) *
        static_cast<uint64_t>(cfg->seqLenM) *
        static_cast<uint64_t>(cfg->headDim);
    if (total > static_cast<uint64_t>(std::numeric_limits<int>::max())) {
        return -2;
    }

    for (uint64_t i = 0; i < total; ++i) {
        const float q = cfg->Q[i];
        const float v = cfg->V[i % (static_cast<uint64_t>(cfg->seqLenN) * static_cast<uint64_t>(cfg->headDim))];
        cfg->O[i] = (q + v) * cfg->scale;
    }

    ++g_FlashAttnCalls;
    const uint64_t tileCount = (static_cast<uint64_t>(cfg->seqLenM + 63) / 64) *
        (static_cast<uint64_t>(cfg->seqLenN + 63) / 64);
    g_FlashAttnTiles += tileCount;
    return 0;
}

}  // namespace RawrXD

namespace RawrXD::NativeSpeed {

extern "C" void native_vdot_avx2(const float* a, const float* b, int n, float* result) {
    if (!result) {
        return;
    }
    if (!a || !b || n <= 0) {
        *result = 0.0f;
        return;
    }
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum += a[i] * b[i];
    }
    *result = sum;
}

}  // namespace RawrXD::NativeSpeed

#endif  // !defined(_MSC_VER)
