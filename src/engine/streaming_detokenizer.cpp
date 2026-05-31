// ============================================================================
// streaming_detokenizer.cpp — Implementation
// ============================================================================
#include "streaming_detokenizer.h"
#include <cassert>
#include <cstring>

namespace rawrxd {

// ============================================================================
// BPE byte-fallback notation: SentencePiece encodes raw bytes as <0xNN>
// Map these to the actual byte so they render correctly.
// ============================================================================
static bool parse_byte_escape(const std::string& s, uint8_t& out_byte)
{
    // Matches "<0xNN>" where NN is two hex digits
    if (s.size() == 6 &&
        s[0] == '<' && s[1] == '0' && s[2] == 'x' && s[5] == '>')
    {
        auto hex_digit = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            return -1;
        };
        int hi = hex_digit(s[3]);
        int lo = hex_digit(s[4]);
        if (hi >= 0 && lo >= 0) {
            out_byte = (uint8_t)((hi << 4) | lo);
            return true;
        }
    }
    return false;
}

// ============================================================================
// BPE space-prefix notation: SentencePiece uses Ġ (U+2581) for leading space.
// Convert to ASCII space.
// ============================================================================
static std::string normalize_piece(const std::string& raw)
{
    std::string out;
    out.reserve(raw.size());

    size_t i = 0;
    while (i < raw.size()) {
        // Check for UTF-8 encoded U+2581 (▁) = 0xE2 0x96 0x81
        if (i + 2 < raw.size() &&
            (uint8_t)raw[i]   == 0xE2 &&
            (uint8_t)raw[i+1] == 0x96 &&
            (uint8_t)raw[i+2] == 0x81)
        {
            out += ' ';
            i += 3;
        } else {
            out += raw[i++];
        }
    }
    return out;
}

// ============================================================================
// decodePiece
// ============================================================================
std::string StreamingDetokenizer::decodePiece(int32_t token_id) const
{
    // Check special override table first
    {
        auto it = special_overrides_.find(token_id);
        if (it != special_overrides_.end()) return it->second;
    }

    if (token_id < 0 || token_id >= (int32_t)vocab_.size())
        return {};  // unknown token → silence (not replacement char)

    const std::string& piece = vocab_[token_id].piece;

    // Try byte-escape notation
    uint8_t raw_byte;
    if (parse_byte_escape(piece, raw_byte)) {
        return std::string(1, (char)raw_byte);
    }

    // Normalize space prefix
    return normalize_piece(piece);
}

// ============================================================================
// appendAndFlush — core logic
// ============================================================================
void StreamingDetokenizer::appendAndFlush(const std::string& piece)
{
    for (char c : piece)
        pending_bytes_.push_back((uint8_t)c);

    flushBuffer(false);
}

// ============================================================================
// flushBuffer — emit all complete codepoints from the front of pending_bytes_
// ============================================================================
void StreamingDetokenizer::flushBuffer(bool force)
{
    std::string emit_buf;
    size_t pos = 0;

    while (pos < pending_bytes_.size()) {
        uint8_t lead = pending_bytes_[pos];

        if (utf8::is_continuation(lead)) {
            // Unexpected continuation — treat as single replacement char
            emit_buf += "\xEF\xBF\xBD"; // U+FFFD
            ++pos;
            continue;
        }

        int seq_len = utf8::sequence_length(lead);
        if (seq_len < 0) {
            // Invalid lead byte — skip as replacement
            emit_buf += "\xEF\xBF\xBD";
            ++pos;
            continue;
        }

        size_t remaining = pending_bytes_.size() - pos;

        if ((int)remaining < seq_len) {
            // Incomplete codepoint — hold in buffer unless forced
            if (force) {
                emit_buf += "\xEF\xBF\xBD";
                pos += remaining; // consume the partial bytes
            }
            // else: leave them for next feed()
            break;
        }

        // Validate continuation bytes
        if (!utf8::validate_codepoint(pending_bytes_.data() + pos, seq_len)) {
            emit_buf += "\xEF\xBF\xBD";
            ++pos;
            continue;
        }

        // Valid complete codepoint
        for (int i = 0; i < seq_len; ++i)
            emit_buf += (char)pending_bytes_[pos + i];
        pos += seq_len;
        ++codepoints_emitted_;
    }

    // Remove consumed bytes
    if (pos > 0)
        pending_bytes_.erase(pending_bytes_.begin(), pending_bytes_.begin() + pos);

    // Emit
    if (!emit_buf.empty()) {
        bytes_emitted_ += emit_buf.size();
        if (flush_cb_) flush_cb_(emit_buf);
    }
}

// ============================================================================
// feed
// ============================================================================
int StreamingDetokenizer::feed(int32_t token_id)
{
    if (hit_eos_) return 0;

    if (token_id == eos_id_) {
        hit_eos_ = true;
        flushBuffer(true); // force remaining bytes on EOS
        return 0;
    }

    if (token_id == bos_id_) return 0; // skip BOS silently

    std::string piece = decodePiece(token_id);
    if (piece.empty()) return 0;

    size_t cp_before = codepoints_emitted_;
    appendAndFlush(piece);
    return (int)(codepoints_emitted_ - cp_before);
}

// ============================================================================
// finalize
// ============================================================================
std::string StreamingDetokenizer::finalize()
{
    // Collect anything the flush_cb_ would emit by temporarily overriding it
    std::string collected;
    FlushCallback saved = flush_cb_;
    flush_cb_ = [&](std::string_view sv) { collected += sv; };
    flushBuffer(true);
    flush_cb_ = saved;
    return collected;
}

// ============================================================================
// reset
// ============================================================================
void StreamingDetokenizer::reset()
{
    pending_bytes_.clear();
    hit_eos_             = false;
    bytes_emitted_       = 0;
    codepoints_emitted_  = 0;
}

} // namespace rawrxd
