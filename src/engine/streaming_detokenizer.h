#pragma once
// ============================================================================
// streaming_detokenizer.h — Incremental UTF-8-safe token-to-text decoder
// Feeds token IDs one at a time, yields only complete Unicode codepoints.
// ============================================================================
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace rawrxd {

// Forward reference: token vocab entry
struct VocabEntry {
    std::string piece;  // raw byte piece (may be partial UTF-8)
};

// ============================================================================
// UTF-8 byte classification helpers (used by the detokenizer internally)
// ============================================================================
namespace utf8 {

// Number of expected continuation bytes given a leading byte
// Returns -1 for invalid sequences
inline int sequence_length(uint8_t lead) noexcept {
    if ((lead & 0x80) == 0x00) return 1;   // 0xxxxxxx — ASCII
    if ((lead & 0xE0) == 0xC0) return 2;   // 110xxxxx
    if ((lead & 0xF0) == 0xE0) return 3;   // 1110xxxx
    if ((lead & 0xF8) == 0xF0) return 4;   // 11110xxx
    return -1;                              // continuation or overlong
}

// Returns true if byte is a UTF-8 continuation byte (10xxxxxx)
inline bool is_continuation(uint8_t b) noexcept {
    return (b & 0xC0) == 0x80;
}

// Validate a complete codepoint in buf[0..len-1] (len must match sequence_length(buf[0]))
inline bool validate_codepoint(const uint8_t* buf, int len) noexcept {
    for (int i = 1; i < len; ++i)
        if (!is_continuation(buf[i])) return false;
    return true;
}

} // namespace utf8

// ============================================================================
// StreamingDetokenizer
//
// Usage:
//   StreamingDetokenizer det(vocab, eos_id);
//   det.onFlush([](std::string_view sv){ display(sv); });
//   det.feed(token_id);  // may immediately call onFlush for complete text
//   std::string tail = det.finalize();  // flush any held incomplete bytes
// ============================================================================
class StreamingDetokenizer {
public:
    // Callback type: called with flushed text whenever a complete codepoint
    // (or run of codepoints) becomes available.
    using FlushCallback = std::function<void(std::string_view)>;

    explicit StreamingDetokenizer(
        const std::vector<VocabEntry>& vocab,
        int32_t eos_token_id = -1,
        int32_t bos_token_id = -1)
        : vocab_(vocab)
        , eos_id_(eos_token_id)
        , bos_id_(bos_token_id)
    {}

    // Register a flush callback (replaces any previously set callback)
    void onFlush(FlushCallback cb) { flush_cb_ = std::move(cb); }

    // Feed one token ID; may trigger zero or more flush callbacks.
    // Returns the number of Unicode codepoints emitted.
    int feed(int32_t token_id);

    // Flush any buffered incomplete bytes as replacement characters (U+FFFD).
    // Returns any unflushed text. After this call, the buffer is empty.
    std::string finalize();

    // Reset state for a new generation sequence
    void reset();

    // True if the last token was the EOS token
    bool reachedEos() const noexcept { return hit_eos_; }

    // Total bytes emitted so far
    size_t bytesEmitted() const noexcept { return bytes_emitted_; }

    // Total codepoints emitted so far
    size_t codepointsEmitted() const noexcept { return codepoints_emitted_; }

private:
    // Decode a piece string for a token ID.
    // Handles BPE byte-fallback notation like "<0x0A>" for raw bytes.
    std::string decodePiece(int32_t token_id) const;

    // Emit buffered codepoints up to the last safe boundary.
    // If force=true, emit everything even if sequence is incomplete.
    void flushBuffer(bool force = false);

    // Append decoded bytes to pending_bytes_, then flush complete codepoints.
    void appendAndFlush(const std::string& piece);

    const std::vector<VocabEntry>& vocab_;
    int32_t eos_id_;
    int32_t bos_id_;

    // Holds bytes that may form a partial UTF-8 sequence
    std::vector<uint8_t> pending_bytes_;

    FlushCallback flush_cb_;
    bool hit_eos_          = false;
    size_t bytes_emitted_  = 0;
    size_t codepoints_emitted_ = 0;

    // Optional: map special tokens to display strings
    std::unordered_map<int32_t, std::string> special_overrides_;
};

} // namespace rawrxd
