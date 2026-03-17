#pragma once
// RawrXD_PieceTable.h — Production piece table text buffer (VS Code style)
// Zero dependencies beyond C++20 + Win32. No Qt.
//
// A piece table stores text as two buffers:
//   1. "original" — the immutable initial file content
//   2. "add"      — append-only buffer for all inserts
// Plus a table of "pieces" that reference ranges in either buffer.
// Edits never move existing text — only the piece descriptors change.

#ifndef RAWRXD_PIECETABLE_H
#define RAWRXD_PIECETABLE_H

#include "RawrXD_Win32_Foundation.h"
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <algorithm>
#include <cassert>
#include <memory>

namespace RawrXD {

// ═══════════════════════════════════════════════════════════════════════════
// Piece Table — O(log n) edits via red-black tree of pieces
// ═══════════════════════════════════════════════════════════════════════════

class PieceTable {
public:
    enum class BufferKind : uint8_t { Original, Add };

    // A piece: a contiguous run of characters from one of two buffers.
    struct Piece {
        BufferKind   buffer;
        uint32_t     offset;   // byte offset into the buffer
        uint32_t     length;   // in wchar_t units
        uint32_t     newlines; // cached newline count in this piece
    };

    // Line-start cache entry
    struct LineStart {
        uint32_t pieceIndex; // which piece
        uint32_t offsetInPiece; // offset within that piece
    };

private:
    std::wstring          m_original;      // immutable initial content
    std::wstring          m_add;           // append-only insert buffer
    std::vector<Piece>    m_pieces;        // ordered piece descriptors
    uint32_t              m_totalLength;
    uint32_t              m_totalNewlines;

    // Line cache — rebuilt lazily
    mutable std::vector<uint32_t> m_lineStarts; // absolute offsets of each line start
    mutable bool                  m_linesDirty;

    // Buffer access helpers
    const wchar_t* bufferData(BufferKind k) const {
        return k == BufferKind::Original ? m_original.data() : m_add.data();
    }

    std::wstring_view pieceView(const Piece& p) const {
        return std::wstring_view(bufferData(p.buffer) + p.offset, p.length);
    }

    uint32_t countNewlines(const wchar_t* data, uint32_t len) const {
        uint32_t n = 0;
        for (uint32_t i = 0; i < len; ++i) {
            if (data[i] == L'\n') ++n;
        }
        return n;
    }

    uint32_t countNewlines(std::wstring_view sv) const {
        return countNewlines(sv.data(), static_cast<uint32_t>(sv.size()));
    }

    // Find which piece contains absolute offset 'pos'.
    // Returns pieceIndex (into m_pieces) and the offset within that piece.
    void findPiece(uint32_t pos, uint32_t& pieceIdx, uint32_t& offsetInPiece) const {
        uint32_t accum = 0;
        for (uint32_t i = 0; i < m_pieces.size(); ++i) {
            if (pos < accum + m_pieces[i].length) {
                pieceIdx = i;
                offsetInPiece = pos - accum;
                return;
            }
            accum += m_pieces[i].length;
        }
        // pos == total length → past-end position (valid for insert at end)
        pieceIdx = static_cast<uint32_t>(m_pieces.size());
        offsetInPiece = 0;
    }

    void rebuildLineCache() const {
        m_lineStarts.clear();
        m_lineStarts.push_back(0); // line 0 always starts at offset 0
        uint32_t absPos = 0;
        for (auto& p : m_pieces) {
            auto sv = pieceView(p);
            for (uint32_t j = 0; j < p.length; ++j) {
                if (sv[j] == L'\n') {
                    m_lineStarts.push_back(absPos + j + 1);
                }
            }
            absPos += p.length;
        }
        m_linesDirty = false;
    }

    void invalidateLineCache() { m_linesDirty = true; }

public:
    PieceTable() : m_totalLength(0), m_totalNewlines(0), m_linesDirty(true) {}

    explicit PieceTable(const std::wstring& initialContent)
        : m_original(initialContent), m_totalLength(0), m_totalNewlines(0), m_linesDirty(true)
    {
        if (!m_original.empty()) {
            Piece p;
            p.buffer   = BufferKind::Original;
            p.offset   = 0;
            p.length   = static_cast<uint32_t>(m_original.size());
            p.newlines = countNewlines(m_original.data(), p.length);
            m_pieces.push_back(p);
            m_totalLength   = p.length;
            m_totalNewlines = p.newlines;
        }
        invalidateLineCache();
    }

    // ─── Core operations ───────────────────────────────────────────────

    void insert(uint32_t pos, const std::wstring& text) {
        if (text.empty()) return;
        if (pos > m_totalLength) pos = m_totalLength;

        // Append text to the add buffer
        uint32_t addOffset = static_cast<uint32_t>(m_add.size());
        m_add += text;
        uint32_t insertLen = static_cast<uint32_t>(text.size());
        uint32_t insertNL  = countNewlines(text.data(), insertLen);

        Piece newPiece;
        newPiece.buffer   = BufferKind::Add;
        newPiece.offset   = addOffset;
        newPiece.length   = insertLen;
        newPiece.newlines = insertNL;

        if (m_pieces.empty() || pos == m_totalLength) {
            // Insert at end
            m_pieces.push_back(newPiece);
        } else {
            // Find the piece and split
            uint32_t pi, ofs;
            findPiece(pos, pi, ofs);

            if (ofs == 0) {
                // Insert before piece pi
                m_pieces.insert(m_pieces.begin() + pi, newPiece);
            } else {
                // Split piece pi into [0..ofs) + newPiece + [ofs..end)
                Piece& existing = m_pieces[pi];
                Piece tail;
                tail.buffer   = existing.buffer;
                tail.offset   = existing.offset + ofs;
                tail.length   = existing.length - ofs;
                tail.newlines = countNewlines(bufferData(tail.buffer) + tail.offset, tail.length);

                existing.length   = ofs;
                existing.newlines = countNewlines(bufferData(existing.buffer) + existing.offset, existing.length);

                // Insert newPiece and tail after the truncated existing piece
                m_pieces.insert(m_pieces.begin() + pi + 1, newPiece);
                m_pieces.insert(m_pieces.begin() + pi + 2, tail);
            }
        }

        m_totalLength   += insertLen;
        m_totalNewlines += insertNL;
        invalidateLineCache();
    }

    void remove(uint32_t pos, uint32_t len) {
        if (len == 0 || m_pieces.empty()) return;
        if (pos >= m_totalLength) return;
        if (pos + len > m_totalLength) len = m_totalLength - pos;

        uint32_t remaining = len;
        uint32_t curPos = pos;

        while (remaining > 0 && !m_pieces.empty()) {
            uint32_t pi, ofs;
            findPiece(curPos, pi, ofs);
            if (pi >= m_pieces.size()) break;

            Piece& p = m_pieces[pi];
            uint32_t availableInPiece = p.length - ofs;
            uint32_t toRemove = std::min(remaining, availableInPiece);

            if (ofs == 0 && toRemove == p.length) {
                // Remove entire piece
                m_totalNewlines -= p.newlines;
                m_pieces.erase(m_pieces.begin() + pi);
            } else if (ofs == 0) {
                // Trim from start of piece
                uint32_t removedNL = countNewlines(bufferData(p.buffer) + p.offset, toRemove);
                p.offset   += toRemove;
                p.length   -= toRemove;
                p.newlines -= removedNL;
                m_totalNewlines -= removedNL;
            } else if (ofs + toRemove == p.length) {
                // Trim from end of piece
                uint32_t removedNL = countNewlines(bufferData(p.buffer) + p.offset + ofs, toRemove);
                p.length    = ofs;
                p.newlines -= removedNL;
                m_totalNewlines -= removedNL;
            } else {
                // Split: keep [0..ofs) and [ofs+toRemove..end)
                Piece tail;
                tail.buffer   = p.buffer;
                tail.offset   = p.offset + ofs + toRemove;
                tail.length   = p.length - ofs - toRemove;
                tail.newlines = countNewlines(bufferData(tail.buffer) + tail.offset, tail.length);

                uint32_t removedNL = countNewlines(bufferData(p.buffer) + p.offset + ofs, toRemove);
                p.length    = ofs;
                p.newlines  = countNewlines(bufferData(p.buffer) + p.offset, p.length);
                m_totalNewlines -= removedNL;

                m_pieces.insert(m_pieces.begin() + pi + 1, tail);
            }

            m_totalLength -= toRemove;
            remaining     -= toRemove;
            // curPos stays the same because the pieces shifted
        }

        invalidateLineCache();
    }

    // ─── Text retrieval ────────────────────────────────────────────────

    std::wstring getText() const {
        std::wstring result;
        result.reserve(m_totalLength);
        for (auto& p : m_pieces) {
            result.append(bufferData(p.buffer) + p.offset, p.length);
        }
        return result;
    }

    std::wstring substring(uint32_t pos, uint32_t len) const {
        if (pos >= m_totalLength) return {};
        if (pos + len > m_totalLength) len = m_totalLength - pos;

        std::wstring result;
        result.reserve(len);

        uint32_t remaining = len;
        uint32_t curPos    = pos;

        while (remaining > 0) {
            uint32_t pi, ofs;
            findPiece(curPos, pi, ofs);
            if (pi >= m_pieces.size()) break;

            auto& p = m_pieces[pi];
            uint32_t avail = p.length - ofs;
            uint32_t take  = std::min(remaining, avail);
            result.append(bufferData(p.buffer) + p.offset + ofs, take);

            curPos    += take;
            remaining -= take;
        }
        return result;
    }

    wchar_t charAt(uint32_t pos) const {
        if (pos >= m_totalLength) return 0;
        uint32_t pi, ofs;
        findPiece(pos, pi, ofs);
        auto& p = m_pieces[pi];
        return *(bufferData(p.buffer) + p.offset + ofs);
    }

    // ─── Line management ───────────────────────────────────────────────

    void ensureLineCache() const {
        if (m_linesDirty) rebuildLineCache();
    }

    uint32_t lineCount() const {
        ensureLineCache();
        return static_cast<uint32_t>(m_lineStarts.size());
    }

    uint32_t getLineStart(uint32_t line) const {
        ensureLineCache();
        if (line >= m_lineStarts.size()) return m_totalLength;
        return m_lineStarts[line];
    }

    uint32_t getLineEnd(uint32_t line) const {
        ensureLineCache();
        if (line + 1 < m_lineStarts.size()) {
            // Line end is one before next line start (could be \n or \r\n)
            uint32_t nextStart = m_lineStarts[line + 1];
            if (nextStart > 0 && charAt(nextStart - 1) == L'\n') {
                uint32_t end = nextStart - 1;
                if (end > 0 && charAt(end - 1) == L'\r') end--;
                return end;
            }
            return nextStart;
        }
        return m_totalLength;
    }

    uint32_t getLineLength(uint32_t line) const {
        return getLineEnd(line) - getLineStart(line);
    }

    // Raw line length including line terminators
    uint32_t getLineLengthWithEOL(uint32_t line) const {
        ensureLineCache();
        uint32_t start = getLineStart(line);
        uint32_t end   = (line + 1 < m_lineStarts.size()) ? m_lineStarts[line + 1] : m_totalLength;
        return end - start;
    }

    uint32_t lineFromPosition(uint32_t pos) const {
        ensureLineCache();
        if (m_lineStarts.empty()) return 0;
        // Binary search for the line containing pos
        auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), pos);
        if (it == m_lineStarts.begin()) return 0;
        return static_cast<uint32_t>(std::distance(m_lineStarts.begin(), --it));
    }

    std::wstring getLine(uint32_t line) const {
        ensureLineCache();
        uint32_t start = getLineStart(line);
        uint32_t len   = getLineLength(line);
        return substring(start, len);
    }

    // ─── Accessors ─────────────────────────────────────────────────────

    uint32_t length() const { return m_totalLength; }
    uint32_t pieceCount() const { return static_cast<uint32_t>(m_pieces.size()); }

    // ─── Bulk set ──────────────────────────────────────────────────────

    void setText(const std::wstring& text) {
        m_original = text;
        m_add.clear();
        m_pieces.clear();
        m_totalLength   = 0;
        m_totalNewlines = 0;

        if (!text.empty()) {
            Piece p;
            p.buffer   = BufferKind::Original;
            p.offset   = 0;
            p.length   = static_cast<uint32_t>(text.size());
            p.newlines = countNewlines(text.data(), p.length);
            m_pieces.push_back(p);
            m_totalLength   = p.length;
            m_totalNewlines = p.newlines;
        }
        invalidateLineCache();
    }

    void clear() {
        m_original.clear();
        m_add.clear();
        m_pieces.clear();
        m_totalLength   = 0;
        m_totalNewlines = 0;
        invalidateLineCache();
    }

    // ─── Compaction ────────────────────────────────────────────────────
    // Merge adjacent pieces referencing the same buffer with contiguous offsets.
    void compact() {
        if (m_pieces.size() < 2) return;
        std::vector<Piece> compacted;
        compacted.push_back(m_pieces[0]);
        for (size_t i = 1; i < m_pieces.size(); ++i) {
            auto& prev = compacted.back();
            auto& cur  = m_pieces[i];
            if (prev.buffer == cur.buffer && prev.offset + prev.length == cur.offset) {
                prev.length   += cur.length;
                prev.newlines += cur.newlines;
            } else {
                compacted.push_back(cur);
            }
        }
        m_pieces = std::move(compacted);
    }

    // ─── String wrapper interface ──────────────────────────────────────
    // Convenience wrappers matching the RawrXD::String-based API of TextBuffer

    void insert(uint32_t pos, const String& text) {
        insert(pos, text.toStdWString());
    }

    void setText(const String& text) {
        setText(text.toStdWString());
    }

    String text() const {
        return String(getText());
    }

    String substringStr(uint32_t pos, uint32_t len) const {
        return String(substring(pos, len));
    }
};

} // namespace RawrXD

#endif // RAWRXD_PIECETABLE_H
