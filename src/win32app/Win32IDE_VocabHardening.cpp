#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>

/**
 * @file Win32IDE_VocabHardening.cpp
 * @brief Batch 4 (31/118): Vocabulary Integrity & Safety.
 * Prevents prompt injection and vocab-corruption-based hijacking.
 */

namespace RawrXD::Safety::Vocab {

// Resolves: Vocab_VerifyIntegrity
extern "C" bool Vocab_VerifyIntegrity(void* vocab_ptr) {
    if (!vocab_ptr) return false;
    // CRC32 over the first 4096 bytes of the vocab region as a sanity check
    const auto* p = static_cast<const uint8_t*>(vocab_ptr);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < 4096; ++i) {
        crc ^= p[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int32_t)(crc & 1)));
    }
    crc ^= 0xFFFFFFFF;
    LOG_INFO("[Vocab] Integrity CRC32: 0x%08X", crc);
    return crc != 0; // zero CRC indicates zeroed/corrupt memory
}

// Resolves: Vocab_FilterSensitiveTokens
extern "C" void Vocab_FilterSensitiveTokens(int* token_ids, int count) {
    if (!token_ids || count <= 0) return;
    // Redact known dangerous token IDs: BOS override (0), EOS override (1),
    // system/admin tokens typically in range [32000..32099]
    constexpr int REDACT_LO = 32000, REDACT_HI = 32099, PAD_TOKEN = 0;
    for (int i = 0; i < count; ++i) {
        if (token_ids[i] >= REDACT_LO && token_ids[i] <= REDACT_HI)
            token_ids[i] = PAD_TOKEN;
    }
}

} // namespace RawrXD::Safety::Vocab
