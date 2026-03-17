#pragma once
// RawrXD_EditorCore.h — Unified editor core tying together:
//   • PieceTable (text buffer)
//   • Lexer (syntax tokenization — CppLexer, MASMLexer, or custom)
//   • Renderer2D (DirectWrite-based drawing with per-token color)
//   • UndoStack (with merge support)
//   • Line / column / selection state
//
// Zero Qt. Pure Win32 + DirectWrite + C++20.

#ifndef RAWRXD_EDITORCORE_H
#define RAWRXD_EDITORCORE_H

#include "RawrXD_PieceTable.h"
#include "RawrXD_Renderer_D2D.h"
#include "RawrXD_UndoStack.h"
#include "RawrXD_Lexer.h"
#include "RawrXD_StyleManager.h"
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace RawrXD {

// ═══════════════════════════════════════════════════════════════════════════
// SyntaxHighlightCache — per-line token cache with dirty tracking
// ═══════════════════════════════════════════════════════════════════════════

struct LineSyntaxCache {
    std::vector<Token> tokens;
    int                endState;  // lexer state at end of this line (for multiline comments etc.)
    bool               dirty;
};

// ═══════════════════════════════════════════════════════════════════════════
// EditorCore — the "big one"
// ═══════════════════════════════════════════════════════════════════════════

class EditorCore {
public:
    // Cursor position
    struct CursorPos {
        uint32_t line;
        uint32_t col;  // visual column (not byte offset)
    };

    // Selection = anchor + cursor (they differ when there's an active selection)
    struct Selection {
        CursorPos anchor;
        CursorPos cursor;
        
        bool hasSelection() const {
            return anchor.line != cursor.line || anchor.col != cursor.col;
        }
        
        void collapse() { anchor = cursor; }
        
        CursorPos start() const {
            if (cursor.line < anchor.line) return cursor;
            if (cursor.line > anchor.line) return anchor;
            return cursor.col < anchor.col ? cursor : anchor;
        }
        CursorPos end() const {
            if (cursor.line > anchor.line) return cursor;
            if (cursor.line < anchor.line) return anchor;
            return cursor.col > anchor.col ? cursor : anchor;
        }
    };

    // Editor configuration
    struct Config {
        std::wstring fontFamily     = L"Consolas";
        float        fontSize       = 13.0f;
        int          tabSize        = 4;
        bool         insertSpaces   = true;  // tabs → spaces
        bool         wordWrap       = false;
        bool         showLineNumbers = true;
        bool         showMinimap    = false;
        bool         enableLigatures = true;
        float        lineSpacing    = 1.3f;  // multiplier on font height
        
        // Theme colors
        Color bgColor           = Color(30, 30, 30);       // dark bg
        Color textColor         = Color(212, 212, 212);     // light text
        Color gutterBg          = Color(37, 37, 38);
        Color gutterText        = Color(133, 133, 133);
        Color selectionColor    = Color(38, 79, 120, 180);
        Color cursorColor       = Color(255, 255, 255);
        Color lineHighlight     = Color(40, 40, 40, 80);
        Color matchHighlight    = Color(80, 80, 0, 100);
    };

private:
    PieceTable      m_buffer;
    UndoStack       m_undoStack;
    Selection       m_selection;
    Config          m_config;

    // Rendering
    Renderer2D      m_renderer;
    Font            m_font;
    bool            m_rendererReady = false;
    HWND            m_hwnd = nullptr;

    // Syntax highlighting
    Lexer*          m_lexer        = nullptr;      // non-owning
    StyleManager*   m_styleManager = nullptr;      // non-owning
    std::vector<LineSyntaxCache> m_syntaxCache;

    // Layout metrics (computed from font)
    float           m_lineHeight   = 20.0f;
    float           m_charWidth    = 9.0f;        // monospace assumption
    float           m_gutterWidth  = 55.0f;
    float           m_descent      = 3.0f;

    // Scroll state
    int             m_scrollY      = 0;  // in pixels
    int             m_scrollX      = 0;

    // Target column for vertical movement
    int             m_targetCol    = -1;

    // Dirty flag for content changes
    bool            m_dirty        = false;

    // ─── Internal undo commands ────────────────────────────────────────

    class InsertCmd : public UndoCommand {
        EditorCore* m_editor;
        uint32_t m_pos;
        std::wstring m_text;
    public:
        InsertCmd(EditorCore* e, uint32_t pos, const std::wstring& text)
            : m_editor(e), m_pos(pos), m_text(text) {}
        void undo() override {
            m_editor->m_buffer.remove(m_pos, static_cast<uint32_t>(m_text.size()));
            m_editor->m_dirty = true;
            m_editor->invalidateSyntaxFrom(m_editor->m_buffer.lineFromPosition(m_pos));
        }
        void redo() override {
            m_editor->m_buffer.insert(m_pos, m_text);
            m_editor->m_dirty = true;
            m_editor->invalidateSyntaxFrom(m_editor->m_buffer.lineFromPosition(m_pos));
        }
        int id() const override { return 1; }
        bool mergeWith(const UndoCommand* other) override {
            auto* ins = dynamic_cast<const InsertCmd*>(other);
            if (!ins) return false;
            // Merge consecutive single-char inserts
            if (ins->m_pos == m_pos + m_text.size() && ins->m_text.size() == 1) {
                const_cast<InsertCmd*>(this)->m_text += ins->m_text;
                return true;
            }
            return false;
        }
    };

    class DeleteCmd : public UndoCommand {
        EditorCore* m_editor;
        uint32_t m_pos;
        std::wstring m_text;
    public:
        DeleteCmd(EditorCore* e, uint32_t pos, const std::wstring& text)
            : m_editor(e), m_pos(pos), m_text(text) {}
        void undo() override {
            m_editor->m_buffer.insert(m_pos, m_text);
            m_editor->m_dirty = true;
            m_editor->invalidateSyntaxFrom(m_editor->m_buffer.lineFromPosition(m_pos));
        }
        void redo() override {
            m_editor->m_buffer.remove(m_pos, static_cast<uint32_t>(m_text.size()));
            m_editor->m_dirty = true;
            m_editor->invalidateSyntaxFrom(m_editor->m_buffer.lineFromPosition(m_pos));
        }
    };

    // ─── Syntax cache management ──────────────────────────────────────

    void invalidateSyntaxFrom(uint32_t line) {
        if (line < m_syntaxCache.size()) {
            for (uint32_t i = line; i < m_syntaxCache.size(); ++i)
                m_syntaxCache[i].dirty = true;
        }
    }

    void ensureSyntaxCacheSize() {
        uint32_t lc = m_buffer.lineCount();
        if (m_syntaxCache.size() < lc) {
            m_syntaxCache.resize(lc, {{}, 0, true});
        } else if (m_syntaxCache.size() > lc) {
            m_syntaxCache.resize(lc);
        }
    }

    void retokenizeLine(uint32_t line) {
        if (!m_lexer) return;
        ensureSyntaxCacheSize();
        if (line >= m_syntaxCache.size()) return;

        auto& cache = m_syntaxCache[line];
        cache.tokens.clear();

        std::wstring lineText = m_buffer.getLine(line);

        int prevState = (line > 0 && line - 1 < m_syntaxCache.size()) ? m_syntaxCache[line - 1].endState : 0;
        cache.endState = m_lexer->lexStateful(lineText, prevState, cache.tokens);
        cache.dirty = false;
    }

    // ─── Position helpers ─────────────────────────────────────────────

    uint32_t posFromLineCol(uint32_t line, uint32_t col) const {
        uint32_t start = m_buffer.getLineStart(line);
        uint32_t len   = m_buffer.getLineLength(line);
        return start + std::min(col, len);
    }

    void lineColFromPos(uint32_t pos, uint32_t& line, uint32_t& col) const {
        line = m_buffer.lineFromPosition(pos);
        col  = pos - m_buffer.getLineStart(line);
    }

    uint32_t clampCol(uint32_t line, uint32_t col) const {
        uint32_t len = m_buffer.getLineLength(line);
        return std::min(col, len);
    }

    // ─── Character classification helpers ─────────────────────────────

    static bool isWordChar(wchar_t ch) {
        return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') ||
               (ch >= L'0' && ch <= L'9') || ch == L'_';
    }

    static bool isWhitespaceOrNewline(wchar_t ch) {
        return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n';
    }

public:
    EditorCore() : m_font(L"Consolas", 13.0f) {}
    ~EditorCore() = default;

    // ─── Initialization ───────────────────────────────────────────────

    bool initialize(HWND hwnd) {
        m_hwnd = hwnd;
        if (!m_renderer.initialize(hwnd)) return false;
        m_rendererReady = true;

        // Measure font
        m_font = Font(m_config.fontFamily, m_config.fontSize);
        SizeF sz = m_renderer.measureText(L"M", m_font);
        if (sz.width > 0) {
            m_charWidth  = sz.width;
            m_lineHeight = sz.height * m_config.lineSpacing;
        }
        return true;
    }

    // ─── Configuration ────────────────────────────────────────────────

    Config& config() { return m_config; }
    const Config& config() const { return m_config; }

    void setLexer(Lexer* l) { m_lexer = l; m_syntaxCache.clear(); }
    void setStyleManager(StyleManager* sm) { m_styleManager = sm; }

    void setFont(const std::wstring& family, float size) {
        m_config.fontFamily = family;
        m_config.fontSize   = size;
        m_font = Font(family, size);
        if (m_rendererReady) {
            SizeF sz = m_renderer.measureText(L"M", m_font);
            if (sz.width > 0) {
                m_charWidth  = sz.width;
                m_lineHeight = sz.height * m_config.lineSpacing;
            }
        }
    }

    // ─── Buffer access ────────────────────────────────────────────────

    PieceTable& buffer() { return m_buffer; }
    const PieceTable& buffer() const { return m_buffer; }
    UndoStack& undoStack() { return m_undoStack; }

    void setText(const std::wstring& text) {
        m_buffer.setText(text);
        m_undoStack.clear();
        m_selection = {{0,0},{0,0}};
        m_scrollX = m_scrollY = 0;
        m_syntaxCache.clear();
        m_dirty = false;
    }

    std::wstring getText() const { return m_buffer.getText(); }
    bool isDirty() const { return m_dirty; }
    void setClean() { m_dirty = false; m_undoStack.setClean(); }

    // ─── Editing operations ───────────────────────────────────────────

    void insertText(const std::wstring& text) {
        if (text.empty()) return;

        // Delete selection first
        if (m_selection.hasSelection()) deleteSelection();

        uint32_t pos = posFromLineCol(m_selection.cursor.line, m_selection.cursor.col);

        m_undoStack.push(std::make_unique<InsertCmd>(this, pos, text));
        m_buffer.insert(pos, text);
        m_dirty = true;

        // Advance cursor past inserted text
        uint32_t newPos = pos + static_cast<uint32_t>(text.size());
        lineColFromPos(newPos, m_selection.cursor.line, m_selection.cursor.col);
        m_selection.collapse();

        invalidateSyntaxFrom(m_buffer.lineFromPosition(pos));
        m_targetCol = -1;
    }

    void deleteSelection() {
        if (!m_selection.hasSelection()) return;

        auto s = m_selection.start();
        auto e = m_selection.end();
        uint32_t startPos = posFromLineCol(s.line, s.col);
        uint32_t endPos   = posFromLineCol(e.line, e.col);
        uint32_t len = endPos - startPos;
        if (len == 0) return;

        std::wstring deleted = m_buffer.substring(startPos, len);
        m_undoStack.push(std::make_unique<DeleteCmd>(this, startPos, deleted));
        m_buffer.remove(startPos, len);
        m_dirty = true;

        m_selection.cursor = s;
        m_selection.collapse();
        invalidateSyntaxFrom(s.line);
        m_targetCol = -1;
    }

    void backspace() {
        if (m_selection.hasSelection()) { deleteSelection(); return; }

        uint32_t pos = posFromLineCol(m_selection.cursor.line, m_selection.cursor.col);
        if (pos == 0) return;

        // Check for \r\n
        uint32_t delLen = 1;
        if (pos >= 2 && m_buffer.charAt(pos - 2) == L'\r' && m_buffer.charAt(pos - 1) == L'\n')
            delLen = 2;

        std::wstring deleted = m_buffer.substring(pos - delLen, delLen);
        m_undoStack.push(std::make_unique<DeleteCmd>(this, pos - delLen, deleted));
        m_buffer.remove(pos - delLen, delLen);
        m_dirty = true;

        lineColFromPos(pos - delLen, m_selection.cursor.line, m_selection.cursor.col);
        m_selection.collapse();
        invalidateSyntaxFrom(m_selection.cursor.line);
        m_targetCol = -1;
    }

    void deleteForward() {
        if (m_selection.hasSelection()) { deleteSelection(); return; }

        uint32_t pos = posFromLineCol(m_selection.cursor.line, m_selection.cursor.col);
        if (pos >= m_buffer.length()) return;

        uint32_t delLen = 1;
        if (m_buffer.charAt(pos) == L'\r' && pos + 1 < m_buffer.length() && m_buffer.charAt(pos + 1) == L'\n')
            delLen = 2;

        std::wstring deleted = m_buffer.substring(pos, delLen);
        m_undoStack.push(std::make_unique<DeleteCmd>(this, pos, deleted));
        m_buffer.remove(pos, delLen);
        m_dirty = true;

        invalidateSyntaxFrom(m_selection.cursor.line);
        m_targetCol = -1;
    }

    // ─── Cursor movement ──────────────────────────────────────────────

    void moveCursorLeft(bool keepSelection = false) {
        auto& c = m_selection.cursor;
        if (c.col > 0) { c.col--; }
        else if (c.line > 0) {
            c.line--;
            c.col = m_buffer.getLineLength(c.line);
        }
        if (!keepSelection) m_selection.collapse();
        m_targetCol = -1;
    }

    void moveCursorRight(bool keepSelection = false) {
        auto& c = m_selection.cursor;
        uint32_t len = m_buffer.getLineLength(c.line);
        if (c.col < len) { c.col++; }
        else if (c.line + 1 < m_buffer.lineCount()) {
            c.line++;
            c.col = 0;
        }
        if (!keepSelection) m_selection.collapse();
        m_targetCol = -1;
    }

    void moveCursorUp(bool keepSelection = false) {
        auto& c = m_selection.cursor;
        if (c.line == 0) return;
        if (m_targetCol < 0) m_targetCol = static_cast<int>(c.col);
        c.line--;
        c.col = clampCol(c.line, static_cast<uint32_t>(m_targetCol));
        if (!keepSelection) m_selection.collapse();
    }

    void moveCursorDown(bool keepSelection = false) {
        auto& c = m_selection.cursor;
        if (c.line + 1 >= m_buffer.lineCount()) return;
        if (m_targetCol < 0) m_targetCol = static_cast<int>(c.col);
        c.line++;
        c.col = clampCol(c.line, static_cast<uint32_t>(m_targetCol));
        if (!keepSelection) m_selection.collapse();
    }

    void moveCursorHome(bool keepSelection = false) {
        m_selection.cursor.col = 0;
        if (!keepSelection) m_selection.collapse();
        m_targetCol = -1;
    }

    void moveCursorEnd(bool keepSelection = false) {
        m_selection.cursor.col = m_buffer.getLineLength(m_selection.cursor.line);
        if (!keepSelection) m_selection.collapse();
        m_targetCol = -1;
    }

    void selectAll() {
        m_selection.anchor = {0, 0};
        uint32_t lastLine = m_buffer.lineCount() > 0 ? m_buffer.lineCount() - 1 : 0;
        m_selection.cursor = { lastLine, m_buffer.getLineLength(lastLine) };
    }

    // ─── Word-boundary movement (Ctrl+Left / Ctrl+Right) ─────────────

    void moveCursorWordLeft(bool keepSelection = false) {
        auto& c = m_selection.cursor;
        uint32_t pos = posFromLineCol(c.line, c.col);
        if (pos == 0) { if (!keepSelection) m_selection.collapse(); return; }

        pos--; // step back at least one

        // Skip whitespace / newlines leftward
        while (pos > 0 && isWhitespaceOrNewline(m_buffer.charAt(pos)))
            pos--;
        // If we're on a word char, skip word chars leftward
        if (pos > 0 && isWordChar(m_buffer.charAt(pos))) {
            while (pos > 0 && isWordChar(m_buffer.charAt(pos - 1)))
                pos--;
        } else if (pos > 0) {
            // On punctuation — skip contiguous punctuation
            while (pos > 0 && !isWordChar(m_buffer.charAt(pos - 1)) &&
                   !isWhitespaceOrNewline(m_buffer.charAt(pos - 1)))
                pos--;
        }

        lineColFromPos(pos, c.line, c.col);
        if (!keepSelection) m_selection.collapse();
        m_targetCol = -1;
    }

    void moveCursorWordRight(bool keepSelection = false) {
        auto& c = m_selection.cursor;
        uint32_t pos = posFromLineCol(c.line, c.col);
        uint32_t len = m_buffer.length();
        if (pos >= len) { if (!keepSelection) m_selection.collapse(); return; }

        // If on a word char, skip to end of word
        if (isWordChar(m_buffer.charAt(pos))) {
            while (pos < len && isWordChar(m_buffer.charAt(pos)))
                pos++;
        } else if (!isWhitespaceOrNewline(m_buffer.charAt(pos))) {
            // On punctuation — skip contiguous punctuation
            while (pos < len && !isWordChar(m_buffer.charAt(pos)) &&
                   !isWhitespaceOrNewline(m_buffer.charAt(pos)))
                pos++;
        }
        // Skip trailing whitespace
        while (pos < len && isWhitespaceOrNewline(m_buffer.charAt(pos)))
            pos++;

        lineColFromPos(pos, c.line, c.col);
        if (!keepSelection) m_selection.collapse();
        m_targetCol = -1;
    }

    // ─── Double-click word selection ──────────────────────────────────

    /// Select the word under the given cursor position. Returns the
    /// selected word.
    std::wstring selectWordAt(uint32_t line, uint32_t col) {
        uint32_t pos = posFromLineCol(line, col);
        uint32_t len = m_buffer.length();
        if (pos >= len) return {};

        wchar_t ch = m_buffer.charAt(pos);
        uint32_t wordStart = pos;
        uint32_t wordEnd   = pos;

        if (isWordChar(ch)) {
            while (wordStart > 0 && isWordChar(m_buffer.charAt(wordStart - 1)))
                wordStart--;
            while (wordEnd < len && isWordChar(m_buffer.charAt(wordEnd)))
                wordEnd++;
        } else if (!isWhitespaceOrNewline(ch)) {
            // Select contiguous non-word non-whitespace (operators, braces, etc.)
            while (wordStart > 0 && !isWordChar(m_buffer.charAt(wordStart - 1)) &&
                   !isWhitespaceOrNewline(m_buffer.charAt(wordStart - 1)))
                wordStart--;
            while (wordEnd < len && !isWordChar(m_buffer.charAt(wordEnd)) &&
                   !isWhitespaceOrNewline(m_buffer.charAt(wordEnd)))
                wordEnd++;
        } else {
            // On whitespace — select contiguous whitespace
            while (wordStart > 0 && isWhitespaceOrNewline(m_buffer.charAt(wordStart - 1)))
                wordStart--;
            while (wordEnd < len && isWhitespaceOrNewline(m_buffer.charAt(wordEnd)))
                wordEnd++;
        }

        lineColFromPos(wordStart, m_selection.anchor.line, m_selection.anchor.col);
        lineColFromPos(wordEnd,   m_selection.cursor.line, m_selection.cursor.col);
        m_targetCol = -1;

        if (wordEnd > wordStart)
            return m_buffer.substring(wordStart, wordEnd - wordStart);
        return {};
    }

    /// Handle double-click at pixel coordinates — selects the word
    void handleDoubleClick(int x, int y) {
        CursorPos cp = hitTest(x, y);
        selectWordAt(cp.line, cp.col);
    }

    // ─── Find / Replace ────────────────────────────────────────────────

    struct FindResult {
        uint32_t line;
        uint32_t col;
        uint32_t pos;    // absolute position in buffer
        uint32_t length; // match length
    };

    /// Find next occurrence of \a needle starting from \a startPos.
    /// Returns true if found, populating \a result.
    bool findNext(const std::wstring& needle, uint32_t startPos, FindResult& result,
                  bool caseSensitive = true, bool wrapAround = true) const {
        if (needle.empty()) return false;
        uint32_t len = m_buffer.length();
        if (len == 0) return false;

        // Get full text (could be optimized for very large files with a
        // streaming search, but for now this is correct)
        std::wstring text = m_buffer.getText();

        auto searcher = [&](const std::wstring& hay, const std::wstring& ndl,
                            size_t from) -> size_t {
            if (caseSensitive) {
                return hay.find(ndl, from);
            }
            // Case-insensitive
            auto toLower = [](std::wstring s) {
                for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
                return s;
            };
            return toLower(hay).find(toLower(ndl), from);
        };

        size_t found = searcher(text, needle, startPos);
        if (found == std::wstring::npos && wrapAround && startPos > 0) {
            found = searcher(text, needle, 0);
        }
        if (found == std::wstring::npos) return false;

        result.pos    = static_cast<uint32_t>(found);
        result.length = static_cast<uint32_t>(needle.size());
        lineColFromPos(result.pos, result.line, result.col);
        return true;
    }

    /// Find previous occurrence (reverse search).
    bool findPrev(const std::wstring& needle, uint32_t startPos, FindResult& result,
                  bool caseSensitive = true, bool wrapAround = true) const {
        if (needle.empty()) return false;
        std::wstring text = m_buffer.getText();
        if (text.empty()) return false;

        auto searcher = [&](const std::wstring& hay, const std::wstring& ndl,
                            size_t from) -> size_t {
            if (caseSensitive) {
                return hay.rfind(ndl, from);
            }
            auto toLower = [](std::wstring s) {
                for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
                return s;
            };
            return toLower(hay).rfind(toLower(ndl), from);
        };

        size_t from = (startPos > 0) ? startPos - 1 : std::wstring::npos;
        size_t found = searcher(text, needle, from);
        if (found == std::wstring::npos && wrapAround) {
            found = searcher(text, needle, text.size());
        }
        if (found == std::wstring::npos) return false;

        result.pos    = static_cast<uint32_t>(found);
        result.length = static_cast<uint32_t>(needle.size());
        lineColFromPos(result.pos, result.line, result.col);
        return true;
    }

    /// Find all occurrences.
    std::vector<FindResult> findAll(const std::wstring& needle,
                                    bool caseSensitive = true) const {
        std::vector<FindResult> results;
        if (needle.empty()) return results;
        std::wstring text = m_buffer.getText();

        auto toLower = [](std::wstring s) {
            for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
            return s;
        };

        std::wstring haystack = caseSensitive ? text : toLower(text);
        std::wstring ndl      = caseSensitive ? needle : toLower(needle);

        size_t pos = 0;
        while ((pos = haystack.find(ndl, pos)) != std::wstring::npos) {
            FindResult fr;
            fr.pos    = static_cast<uint32_t>(pos);
            fr.length = static_cast<uint32_t>(needle.size());
            lineColFromPos(fr.pos, fr.line, fr.col);
            results.push_back(fr);
            pos += needle.size();
        }
        return results;
    }

    /// Find and select the next occurrence starting from cursor.
    bool findAndSelect(const std::wstring& needle, bool caseSensitive = true) {
        uint32_t startPos = posFromLineCol(m_selection.cursor.line, m_selection.cursor.col);
        FindResult fr;
        if (!findNext(needle, startPos, fr, caseSensitive)) return false;

        // Select the match
        lineColFromPos(fr.pos, m_selection.anchor.line, m_selection.anchor.col);
        lineColFromPos(fr.pos + fr.length, m_selection.cursor.line, m_selection.cursor.col);
        m_targetCol = -1;
        return true;
    }

    /// Replace the current selection with \a replacement, then find next.
    bool replaceAndFindNext(const std::wstring& needle,
                            const std::wstring& replacement,
                            bool caseSensitive = true) {
        if (!m_selection.hasSelection()) {
            return findAndSelect(needle, caseSensitive);
        }

        // Replace selection
        auto s = m_selection.start();
        auto e = m_selection.end();
        uint32_t sp = posFromLineCol(s.line, s.col);
        uint32_t ep = posFromLineCol(e.line, e.col);
        uint32_t selLen = ep - sp;

        // Verify selection matches needle
        std::wstring selected = m_buffer.substring(sp, selLen);
        bool match = caseSensitive
            ? (selected == needle)
            : (_wcsicmp(selected.c_str(), needle.c_str()) == 0);

        if (match) {
            // Delete + insert via undo
            deleteSelection();
            insertText(replacement);
        }

        return findAndSelect(needle, caseSensitive);
    }

    /// Replace all occurrences. Returns the number of replacements made.
    int replaceAll(const std::wstring& needle,
                   const std::wstring& replacement,
                   bool caseSensitive = true) {
        auto matches = findAll(needle, caseSensitive);
        if (matches.empty()) return 0;

        // Replace from end to start to preserve positions
        int count = 0;
        for (int i = static_cast<int>(matches.size()) - 1; i >= 0; --i) {
            auto& fr = matches[i];
            std::wstring old = m_buffer.substring(fr.pos, fr.length);
            m_undoStack.push(std::make_unique<DeleteCmd>(this, fr.pos, old));
            m_buffer.remove(fr.pos, fr.length);
            m_undoStack.push(std::make_unique<InsertCmd>(this, fr.pos, replacement));
            m_buffer.insert(fr.pos, replacement);
            ++count;
        }
        m_dirty = true;
        m_syntaxCache.clear();
        return count;
    }

    Selection& selection() { return m_selection; }
    const Selection& selection() const { return m_selection; }

    // ─── Clipboard (Win32) ────────────────────────────────────────────

    void copy() const {
        if (!m_selection.hasSelection() || !m_hwnd) return;
        auto s = m_selection.start();
        auto e = m_selection.end();
        uint32_t sp = posFromLineCol(s.line, s.col);
        uint32_t ep = posFromLineCol(e.line, e.col);
        std::wstring text = m_buffer.substring(sp, ep - sp);

        if (OpenClipboard(m_hwnd)) {
            EmptyClipboard();
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
            if (hg) {
                wchar_t* dest = static_cast<wchar_t*>(GlobalLock(hg));
                if (dest) {
                    wmemcpy(dest, text.c_str(), text.size());
                    dest[text.size()] = 0;
                    GlobalUnlock(hg);
                    SetClipboardData(CF_UNICODETEXT, hg);
                } else { GlobalFree(hg); }
            }
            CloseClipboard();
        }
    }

    void paste() {
        if (!m_hwnd) return;
        if (OpenClipboard(m_hwnd)) {
            HANDLE hData = GetClipboardData(CF_UNICODETEXT);
            if (hData) {
                const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(hData));
                if (text) {
                    insertText(text);
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
        }
    }

    void cut() {
        copy();
        deleteSelection();
    }

    // ─── Scroll ───────────────────────────────────────────────────────

    void scrollTo(int x, int y) { m_scrollX = x; m_scrollY = y; }
    int scrollX() const { return m_scrollX; }
    int scrollY() const { return m_scrollY; }

    void scrollCursorIntoView(int viewportHeight) {
        int cursorPixelY = static_cast<int>(m_selection.cursor.line * m_lineHeight);
        if (cursorPixelY < m_scrollY)
            m_scrollY = cursorPixelY;
        else if (cursorPixelY + m_lineHeight > m_scrollY + viewportHeight)
            m_scrollY = static_cast<int>(cursorPixelY + m_lineHeight - viewportHeight);
    }

    // ─── Hit test ─────────────────────────────────────────────────────

    CursorPos hitTest(int x, int y) const {
        CursorPos pos;
        pos.line = static_cast<uint32_t>((y + m_scrollY) / m_lineHeight);
        if (pos.line >= m_buffer.lineCount())
            pos.line = m_buffer.lineCount() > 0 ? m_buffer.lineCount() - 1 : 0;

        pos.col = static_cast<uint32_t>(std::max(0.0f, (x + m_scrollX - m_gutterWidth - 2.0f) / m_charWidth));
        pos.col = clampCol(pos.line, pos.col);
        return pos;
    }

    // ─── Rendering ────────────────────────────────────────────────────

    void paint(int viewportWidth, int viewportHeight) {
        if (!m_rendererReady) return;
        m_renderer.beginPaint();
        m_renderer.clear(m_config.bgColor);

        uint32_t startLine = static_cast<uint32_t>(m_scrollY / m_lineHeight);
        int visibleLines = static_cast<int>(viewportHeight / m_lineHeight) + 2;
        uint32_t endLine = std::min(m_buffer.lineCount(), startLine + static_cast<uint32_t>(visibleLines));

        // ── Gutter ──
        if (m_config.showLineNumbers) {
            m_renderer.fillRect(Rect(0, 0, static_cast<int>(m_gutterWidth), viewportHeight), m_config.gutterBg);
        }

        for (uint32_t i = startLine; i < endLine; ++i) {
            float y = static_cast<float>(i) * m_lineHeight - m_scrollY;

            // Current line highlight
            if (i == m_selection.cursor.line) {
                m_renderer.fillRect(Rect(static_cast<int>(m_gutterWidth), static_cast<int>(y),
                                         viewportWidth - static_cast<int>(m_gutterWidth), static_cast<int>(m_lineHeight)),
                                    m_config.lineHighlight);
            }

            // Line number
            if (m_config.showLineNumbers) {
                m_renderer.drawText(Point(4, static_cast<int>(y)), String::number(i + 1), m_font, m_config.gutterText);
            }

            // ── Selection highlight ──
            if (m_selection.hasSelection()) {
                auto s = m_selection.start();
                auto e = m_selection.end();
                if (i >= s.line && i <= e.line) {
                    uint32_t lineLen = m_buffer.getLineLength(i);
                    uint32_t hlStart = 0, hlEnd = lineLen;
                    if (i == s.line) hlStart = s.col;
                    if (i == e.line) hlEnd = e.col;
                    if (hlEnd > hlStart) {
                        float x1 = m_gutterWidth + 2 - m_scrollX + hlStart * m_charWidth;
                        float x2 = m_gutterWidth + 2 - m_scrollX + hlEnd * m_charWidth;
                        m_renderer.fillRect(Rect(static_cast<int>(x1), static_cast<int>(y),
                                                 static_cast<int>(x2 - x1), static_cast<int>(m_lineHeight)),
                                            m_config.selectionColor);
                    }
                }
            }

            // ── Text ──
            std::wstring lineText = m_buffer.getLine(i);
            // Strip trailing \r\n for display
            while (!lineText.empty() && (lineText.back() == L'\n' || lineText.back() == L'\r'))
                lineText.pop_back();

            float textX = m_gutterWidth + 2.0f - m_scrollX;
            Point textPos(static_cast<int>(textX), static_cast<int>(y));

            if (m_lexer && m_styleManager && !lineText.empty()) {
                // Ensure syntax tokens for this line
                ensureSyntaxCacheSize();
                if (i < m_syntaxCache.size() && m_syntaxCache[i].dirty) {
                    retokenizeLine(i);
                }

                std::vector<TextRun> runs;
                if (i < m_syntaxCache.size()) {
                    for (auto& t : m_syntaxCache[i].tokens) {
                        // Clamp token to visible line text
                        if (t.start >= static_cast<int>(lineText.size())) break;
                        int len = std::min(t.length, static_cast<int>(lineText.size()) - t.start);
                        const TextStyle& style = m_styleManager->getStyle(t.type);
                        runs.push_back({t.start, len, style.color, style.bold, style.italic});
                    }
                }
                m_renderer.drawStyledTextColored(textPos, String(lineText), m_font, runs, m_config.textColor);
            } else if (!lineText.empty()) {
                if (m_config.enableLigatures) {
                    m_renderer.drawTextAdvanced(textPos, String(lineText), m_font, m_config.textColor,
                                                10000.0f, true);
                } else {
                    m_renderer.drawText(textPos, String(lineText), m_font, m_config.textColor);
                }
            }

            // ── Cursor caret ──
            if (i == m_selection.cursor.line) {
                float caretX = m_gutterWidth + 2.0f - m_scrollX + m_selection.cursor.col * m_charWidth;
                m_renderer.fillRect(Rect(static_cast<int>(caretX), static_cast<int>(y),
                                         2, static_cast<int>(m_lineHeight)),
                                    m_config.cursorColor);
            }
        }

        m_renderer.endPaint();
    }

    void resize(int w, int h) { m_renderer.resize(w, h); }

    // ─── Accessors for layout ─────────────────────────────────────────

    float lineHeight() const { return m_lineHeight; }
    float charWidth()  const { return m_charWidth; }
    float gutterWidth() const { return m_gutterWidth; }
    Renderer2D& renderer() { return m_renderer; }
};

} // namespace RawrXD

#endif // RAWRXD_EDITORCORE_H
