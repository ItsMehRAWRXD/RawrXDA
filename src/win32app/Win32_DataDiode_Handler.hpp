// Win32_DataDiode_Handler.hpp — Uni-directional ingest staging + Sneakernet bundles
//
// Policy: no sockets, no outbound I/O. File-system only — compatible with physical
// data-diode drops (operator copies files into a staging directory) and encrypted
// USB / hardware-key transport between sovereign workstations.
//
// Crypto: AES-256-GCM (BCrypt CNG), key = PBKDF2-HMAC-SHA256(secret, salt, 310000 iter).

#pragma once

// This header is often included after Win32IDE.h / Windows.h. CRT `unexpected` and Win32 `error`
// macros break <expected> and std::unexpected; strip them before pulling the standard header.
#ifdef _WIN32
#ifdef unexpected
#undef unexpected
#endif
#ifdef error
#undef error
#endif
#endif

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::data_diode
{

enum class DiodeError
{
    InvalidArgument,
    IoFailure,
    CryptoUnavailable,
    CryptoOperationFailed,
    PayloadTooLarge,
    FormatInvalid,
    UnpackPathUnsafe,
};

struct StagingFileSummary
{
    std::string relativePath;
    std::uint64_t sizeBytes = 0;
    std::string sha256Hex;
};

// Logical row in `.sneaker-chain.json` (sidecar to `.rxdsk`); file on disk is JSON, not this struct.
struct DiodeManifestEntry
{
    std::string filename;
    std::uint64_t size = 0;
    std::uint8_t sha256[32]{};
    std::uint8_t prev_link_hash[32]{};
};

// %LOCALAPPDATA%\RawrXD\Sovereign\DiodeIngest (created on demand when packing scans).
[[nodiscard]] std::string defaultDiodeIngestPathUtf8();

[[nodiscard]] std::expected<void, DiodeError> ensureDirectoryUtf8(std::string_view dirUtf8);

// Lists regular files under dirUtf8 (non-recursive). Computes SHA-256 per file.
[[nodiscard]] std::expected<std::vector<StagingFileSummary>, DiodeError> scanDiodeStagingDirectory(
    std::string_view dirUtf8);

// Recursive pack: all files under sourceDirUtf8 into one authenticated bundle at outputFileUtf8.
// Caps: max 4096 files, max 256 MiB per file, max 1 GiB total plaintext.
[[nodiscard]] std::expected<void, DiodeError> packSneakernetBundle(std::string_view sourceDirUtf8,
                                                                   std::string_view outputFileUtf8,
                                                                   std::string_view secretUtf8);

// Decrypts bundle into destDirUtf8 (must exist or be creatable). Refuses path traversal in names.
[[nodiscard]] std::expected<void, DiodeError> unpackSneakernetBundle(std::string_view bundlePathUtf8,
                                                                     std::string_view destDirUtf8,
                                                                     std::string_view secretUtf8);

[[nodiscard]] const char* diodeErrorMessage(DiodeError e);

}  // namespace rawrxd::data_diode
