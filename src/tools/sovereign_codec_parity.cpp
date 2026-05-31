#include "compression/sovereign_bitstream_codec.h"
#include "compression/sovereign_block_protocol.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

bool runCase(const uint8_t* predicted, const std::vector<uint8_t>& input, const char* name)
{
    std::vector<uint8_t> stream(input.size() * 2 + 32, 0);
    const uint32_t encoded = RawrXD::Compression::SqueezeBitstream(predicted, input.data(),
                                                                    static_cast<uint32_t>(input.size()),
                                                                    stream.data(),
                                                                    static_cast<uint32_t>(stream.size()));
    if (encoded == 0)
    {
        std::printf("FAIL %s: encode returned 0\n", name);
        return false;
    }

    std::vector<uint8_t> restored(input.size(), 0xCC);
    const uint32_t decoded = RawrXD::Compression::ExpandBitstream(predicted, stream.data(), encoded,
                                                                   restored.data(),
                                                                   static_cast<uint32_t>(restored.size()));
    if (decoded != input.size())
    {
        std::printf("FAIL %s: decode size mismatch (%u vs %zu)\n", name, decoded, input.size());
        return false;
    }

    for (size_t i = 0; i < input.size(); ++i)
    {
        if (restored[i] != input[i])
        {
            std::printf("FAIL %s: byte mismatch at %zu\n", name, i);
            return false;
        }
    }

    std::printf("PASS %s: encoded=%u decoded=%u\n", name, encoded, decoded);
    return true;
}

bool runCorruptionCheck(const uint8_t* predicted, const std::vector<uint8_t>& input)
{
    std::vector<uint8_t> stream(input.size() * 2 + 128, 0);
    const uint32_t encoded = RawrXD::Compression::SqueezeBitstream(predicted, input.data(),
                                                                    static_cast<uint32_t>(input.size()),
                                                                    stream.data(),
                                                                    static_cast<uint32_t>(stream.size()));
    if (encoded == 0)
    {
        std::puts("FAIL corruption-check: encode returned 0");
        return false;
    }

    if (encoded <= sizeof(RawrXD::Compression::SovereignBlockHeader))
    {
        std::puts("FAIL corruption-check: encoded payload too small");
        return false;
    }

    // Flip one payload byte and ensure checksum validation rejects decode.
    stream[sizeof(RawrXD::Compression::SovereignBlockHeader)] ^= 0x01;

    std::vector<uint8_t> restored(input.size(), 0);
    const uint32_t decoded = RawrXD::Compression::ExpandBitstream(predicted, stream.data(), encoded,
                                                                   restored.data(),
                                                                   static_cast<uint32_t>(restored.size()));
    if (decoded != 0)
    {
        std::printf("FAIL corruption-check: decode unexpectedly succeeded (%u)\n", decoded);
        return false;
    }

    std::puts("PASS corruption-check: tampered payload rejected");
    return true;
}

uint32_t xorshift32(uint32_t& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

bool runMixedBenchmark()
{
    constexpr uint32_t kBytes = 8 * 1024 * 1024;  // 8 MiB
    constexpr int kIterations = 20;

    std::vector<uint8_t> predicted(kBytes);
    std::vector<uint8_t> actual(kBytes);
    uint32_t rng = 0xC0FFEE42u;

    for (uint32_t i = 0; i < kBytes; ++i)
    {
        predicted[i] = static_cast<uint8_t>(xorshift32(rng) & 0xFFu);
    }

    actual = predicted;

    // Sparse clustered edits (~5%), plus explicit zero islands to exercise ZeroRun.
    for (uint32_t cluster = 0; cluster < 512; ++cluster)
    {
        const uint32_t start = xorshift32(rng) % (kBytes - 2048);
        const uint32_t len = 64 + (xorshift32(rng) % 1024);
        for (uint32_t i = 0; i < len && (start + i) < kBytes; ++i)
        {
            actual[start + i] ^= static_cast<uint8_t>((xorshift32(rng) >> 8) & 0xFFu);
        }
    }

    for (uint32_t z = 0; z < 128; ++z)
    {
        const uint32_t start = xorshift32(rng) % (kBytes - 4096);
        std::memset(actual.data() + start, 0, 256 + (xorshift32(rng) % 2048));
    }

    std::vector<uint8_t> stream(kBytes * 2 + 4096, 0);
    std::vector<uint8_t> restored(kBytes, 0);

    uint64_t totalEncoded = 0;
    double encodeMs = 0.0;
    double decodeMs = 0.0;
    uint64_t literalOps = 0;
    uint64_t deltaOps = 0;
    uint64_t zeroOps = 0;

    for (int it = 0; it < kIterations; ++it)
    {
        const auto t0 = std::chrono::high_resolution_clock::now();
        const uint32_t encoded = RawrXD::Compression::SqueezeBitstream(
            predicted.data(), actual.data(), kBytes, stream.data(), static_cast<uint32_t>(stream.size()));
        const auto t1 = std::chrono::high_resolution_clock::now();
        if (encoded == 0)
        {
            std::puts("FAIL bench-mixed: encode returned 0");
            return false;
        }

        const uint32_t decoded = RawrXD::Compression::ExpandBitstream(
            predicted.data(), stream.data(), encoded, restored.data(), static_cast<uint32_t>(restored.size()));
        const auto t2 = std::chrono::high_resolution_clock::now();
        if (decoded != kBytes || std::memcmp(restored.data(), actual.data(), kBytes) != 0)
        {
            std::puts("FAIL bench-mixed: decode mismatch");
            return false;
        }

        // Parse opcode distribution (payload begins after sovereign block header).
        uint32_t p = static_cast<uint32_t>(sizeof(RawrXD::Compression::SovereignBlockHeader));
        while (p + 5 <= encoded)
        {
            const uint8_t opcode = stream[p++];
            uint32_t arg = 0;
            std::memcpy(&arg, stream.data() + p, sizeof(uint32_t));
            p += 4;

            if (opcode == RawrXD::Compression::kOpcodeEndOfBlock)
            {
                break;
            }
            if (opcode == RawrXD::Compression::kOpcodeLiteralRun)
            {
                ++literalOps;
            }
            else if (opcode == RawrXD::Compression::kOpcodeDeltaChunk)
            {
                ++deltaOps;
            }
            else if (opcode == RawrXD::Compression::kOpcodeZeroRun)
            {
                ++zeroOps;
            }

            if (opcode == RawrXD::Compression::kOpcodeLiteralRun ||
                opcode == RawrXD::Compression::kOpcodeDeltaChunk)
            {
                p += arg;
                if (p > encoded)
                {
                    std::puts("FAIL bench-mixed: malformed stream while counting opcodes");
                    return false;
                }
            }
        }

        totalEncoded += encoded;
        encodeMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
        decodeMs += std::chrono::duration<double, std::milli>(t2 - t1).count();
    }

    const double avgEncoded = static_cast<double>(totalEncoded) / static_cast<double>(kIterations);
    const double ratio = avgEncoded / static_cast<double>(kBytes);
    const double encMBps = (static_cast<double>(kBytes) * kIterations / (1024.0 * 1024.0)) / (encodeMs / 1000.0);
    const double decMBps = (static_cast<double>(kBytes) * kIterations / (1024.0 * 1024.0)) / (decodeMs / 1000.0);

    const char* divEnv = std::getenv("RAWRXD_SOVEREIGN_DELTA_ZERO_BIAS_DIVISOR");
    std::printf("PASS bench-mixed: bytes=%u iters=%d avg-encoded=%.0f ratio=%.4f enc=%.1fMB/s dec=%.1fMB/s bias-div=%s ops[L=%llu D=%llu Z=%llu]\n",
                kBytes,
                kIterations,
                avgEncoded,
                ratio,
                encMBps,
                decMBps,
                (divEnv && divEnv[0]) ? divEnv : "default(8)",
                static_cast<unsigned long long>(literalOps),
                static_cast<unsigned long long>(deltaOps),
                static_cast<unsigned long long>(zeroOps));
    return true;
}

bool runBorderlineBenchmark()
{
    constexpr uint32_t kBytes = 4 * 1024 * 1024;  // 4 MiB
    constexpr int kIterations = 12;
    constexpr uint32_t kXorZeroPercent = 18;  // intentionally between 12.5% and 25%

    std::vector<uint8_t> predicted(kBytes);
    std::vector<uint8_t> actual(kBytes);
    uint32_t rng = 0xBAD5EEDu;

    for (uint32_t i = 0; i < kBytes; ++i)
    {
        predicted[i] = static_cast<uint8_t>((xorshift32(rng) & 0xFEu) + 1u);  // avoid zeros for cleaner signal
        actual[i] = static_cast<uint8_t>((xorshift32(rng) & 0xFEu) + 1u);
    }

    // Force a controlled fraction where predicted ^ actual == 0.
    for (uint32_t i = 0; i < kBytes; ++i)
    {
        if ((xorshift32(rng) % 100u) < kXorZeroPercent)
        {
            actual[i] = predicted[i];
        }
    }

    std::vector<uint8_t> stream(kBytes * 2 + 4096, 0);
    std::vector<uint8_t> restored(kBytes, 0);

    uint64_t totalEncoded = 0;
    uint64_t literalOps = 0;
    uint64_t deltaOps = 0;
    uint64_t zeroOps = 0;

    for (int it = 0; it < kIterations; ++it)
    {
        const uint32_t encoded = RawrXD::Compression::SqueezeBitstream(
            predicted.data(), actual.data(), kBytes, stream.data(), static_cast<uint32_t>(stream.size()));
        if (encoded == 0)
        {
            std::puts("FAIL bench-borderline: encode returned 0");
            return false;
        }

        const uint32_t decoded = RawrXD::Compression::ExpandBitstream(
            predicted.data(), stream.data(), encoded, restored.data(), static_cast<uint32_t>(restored.size()));
        if (decoded != kBytes || std::memcmp(restored.data(), actual.data(), kBytes) != 0)
        {
            std::puts("FAIL bench-borderline: decode mismatch");
            return false;
        }

        uint32_t p = static_cast<uint32_t>(sizeof(RawrXD::Compression::SovereignBlockHeader));
        while (p + 5 <= encoded)
        {
            const uint8_t opcode = stream[p++];
            uint32_t arg = 0;
            std::memcpy(&arg, stream.data() + p, sizeof(uint32_t));
            p += 4;

            if (opcode == RawrXD::Compression::kOpcodeEndOfBlock)
            {
                break;
            }
            if (opcode == RawrXD::Compression::kOpcodeLiteralRun)
            {
                ++literalOps;
            }
            else if (opcode == RawrXD::Compression::kOpcodeDeltaChunk)
            {
                ++deltaOps;
            }
            else if (opcode == RawrXD::Compression::kOpcodeZeroRun)
            {
                ++zeroOps;
            }

            if (opcode == RawrXD::Compression::kOpcodeLiteralRun ||
                opcode == RawrXD::Compression::kOpcodeDeltaChunk)
            {
                p += arg;
                if (p > encoded)
                {
                    std::puts("FAIL bench-borderline: malformed stream while counting opcodes");
                    return false;
                }
            }
        }

        totalEncoded += encoded;
    }

    const double avgEncoded = static_cast<double>(totalEncoded) / static_cast<double>(kIterations);
    const double ratio = avgEncoded / static_cast<double>(kBytes);
    const char* divEnv = std::getenv("RAWRXD_SOVEREIGN_DELTA_ZERO_BIAS_DIVISOR");

    std::printf("PASS bench-borderline: xor0=%u%% avg-encoded=%.0f ratio=%.4f bias-div=%s ops[L=%llu D=%llu Z=%llu]\n",
                kXorZeroPercent,
                avgEncoded,
                ratio,
                (divEnv && divEnv[0]) ? divEnv : "default(8)",
                static_cast<unsigned long long>(literalOps),
                static_cast<unsigned long long>(deltaOps),
                static_cast<unsigned long long>(zeroOps));

    return true;
}

}  // namespace

int main()
{
    std::array<uint8_t, 256> predicted{};
    for (size_t i = 0; i < predicted.size(); ++i)
    {
        predicted[i] = static_cast<uint8_t>((i * 13) & 0xFF);
    }

    std::vector<uint8_t> literal(128);
    for (size_t i = 0; i < literal.size(); ++i)
    {
        literal[i] = static_cast<uint8_t>((i * 7 + 3) & 0xFF);
    }

    std::vector<uint8_t> delta(128);
    for (size_t i = 0; i < delta.size(); ++i)
    {
        delta[i] = static_cast<uint8_t>(predicted[i] ^ static_cast<uint8_t>((i * 9) & 0xFF));
    }

    std::vector<uint8_t> zeros(128, 0);

    bool ok = true;
    ok = runCase(nullptr, literal, "literal") && ok;
    ok = runCase(predicted.data(), delta, "delta") && ok;
    ok = runCase(nullptr, zeros, "zero") && ok;
    ok = runCorruptionCheck(predicted.data(), delta) && ok;
    ok = runMixedBenchmark() && ok;
    ok = runBorderlineBenchmark() && ok;

    if (!ok)
    {
        return 1;
    }

    std::puts("Sovereign codec parity check passed");
    return 0;
}
