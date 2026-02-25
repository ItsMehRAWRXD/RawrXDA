// ============================================================================
// RawrXD_MonacoCore.h — Native Monaco-Equivalent Editor Engine C++ Interface
// ============================================================================
//
// Phase 28: MonacoCore — Drop-in browser replacement
//
// This header provides:
//   1. C-linkage declarations for the ASM-exported gap buffer and tokenizer
//   2. C++ structures matching the ASM memory layouts
//   3. Color constants for the native theme system
//   4. RAII wrapper class for safe lifecycle management
//
// The MonacoCore engine renders via Direct2D/DirectWrite directly, with
// the text model and tokenization handled by the MASM64 kernel.
//
// Dependencies: d2d1.dll, dwrite.dll, user32.dll (all Windows system)
// Binary size:  ~12KB (ASM core) vs 130MB+ (WebView2 + Monaco JS)
//
// Pattern:  PatchResult-compatible, no exceptions
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>

// ============================================================================
// Token Types (must match ASM constants in RawrXD_MonacoCore.asm)
// ============================================================================
enum class MC_TokenType : uint32_t {
    None            = 0,
    Comment         = 1,
    Keyword         = 2,
    Identifier      = 3,
    String          = 4,
    Number          = 5,
    Operator        = 6,
    Register        = 7,
    Directive       = 8,
    Instruction     = 9,
    Preprocessor    = 10,
    Whitespace      = 11,
};

// ============================================================================
// Editor Option Flags
// ============================================================================
enum MC_OptionFlags : uint32_t {
    MC_OPT_MINIMAP          = 0x00000001,
    MC_OPT_WORD_WRAP        = 0x00000002,
    MC_OPT_LINE_NUMBERS     = 0x00000004,
    MC_OPT_SHOW_WHITESPACE  = 0x00000008,
    MC_OPT_BRACKET_MATCH    = 0x00000010,
    MC_OPT_CURSOR_BLINK     = 0x00000020,
    MC_OPT_SMOOTH_SCROLL    = 0x00000040,
    MC_OPT_READ_ONLY        = 0x00000080,
    MC_OPT_GHOST_TEXT       = 0x00000100,
};

// ============================================================================
// Color Constants (BGRA format for Direct2D compatibility)
// ============================================================================
// Dark theme (default — Cyberpunk Neon inspired)
namespace MC_Colors {
    // Background
    constexpr uint32_t BG_DEFAULT       = 0xFF1E1E1E;  // #1e1e1e
    constexpr uint32_t BG_MINIMAP       = 0xFF1A1A1A;
    constexpr uint32_t BG_GUTTER        = 0xFF1E1E1E;
    constexpr uint32_t BG_CURRENT_LINE  = 0xFF282828;
    constexpr uint32_t BG_SELECTION     = 0xFF264F78;

    // Foreground
    constexpr uint32_t TEXT_DEFAULT      = 0xFFD4D4D4;  // #d4d4d4
    constexpr uint32_t KEYWORD           = 0xFF569CD6;  // Blue
    constexpr uint32_t REGISTER          = 0xFF4EC9B0;  // Cyan/teal
    constexpr uint32_t INSTRUCTION       = 0xFFCE9178;  // Orange
    constexpr uint32_t COMMENT           = 0xFF6A9955;  // Green
    constexpr uint32_t STRING            = 0xFFCE9178;  // Orange
    constexpr uint32_t NUMBER            = 0xFFB5CEA8;  // Light green
    constexpr uint32_t LINE_NUMBER       = 0xFF858585;  // Gray
    constexpr uint32_t OPERATOR          = 0xFFD4D4D4;  // Default
    constexpr uint32_t DIRECTIVE         = 0xFFC586C0;  // Pink/purple
    constexpr uint32_t PREPROCESSOR      = 0xFFC586C0;  // Pink/purple
    constexpr uint32_t IDENTIFIER        = 0xFF9CDCFE;  // Light blue
    constexpr uint32_t CURSOR            = 0xFFFFFFFF;  // White

    // Ghost text (AI overlay)
    constexpr uint32_t GHOST_TEXT        = 0x80808080;  // Semi-transparent gray
}

// ============================================================================
// Structures (must match ASM layout exactly)
// ============================================================================

// Gap buffer — core text storage primitive
// O(1) insert/delete at cursor, O(n) for random access
#pragma pack(push, 1)
struct MC_GapBuffer {
    uint8_t*    pBuffer;        // +0:  Base heap pointer
    uint32_t    gapStart;       // +8:  Byte offset to gap start
    uint32_t    gapEnd;         // +12: Byte offset to gap end
    uint32_t    capacity;       // +16: Total allocated bytes
    uint32_t    used;           // +20: Logical content size
    uint32_t    lineCount;      // +24: Number of newlines in content
    uint32_t    reserved;       // +28: Alignment padding
};
static_assert(sizeof(MC_GapBuffer) == 32, "MC_GapBuffer must be 32 bytes for ASM compatibility");
#pragma pack(pop)

// Token — result of tokenization for a single token span
#pragma pack(push, 1)
struct MC_Token {
    uint32_t    startCol;       // +0:  Start column (byte offset in line)
    uint32_t    length;         // +4:  Length in bytes
    uint32_t    tokenType;      // +8:  MC_TokenType value
    uint32_t    color;          // +12: BGRA color (0 = use theme default)
};
static_assert(sizeof(MC_Token) == 16, "MC_Token must be 16 bytes for ASM compatibility");
#pragma pack(pop)

// Maximum tokens per line (matches ASM buffer allocation)
constexpr uint32_t MC_MAX_TOKENS_PER_LINE = 128;

// Maximum extractable line length
constexpr uint32_t MC_MAX_LINE_LENGTH = 4096;

// ============================================================================
// ASM-Exported Functions (extern "C" linkage)
// ============================================================================
// These are implemented in src/asm/RawrXD_MonacoCore.asm and linked as
// object files into the Win32IDE target.

extern "C" {

// ---- Gap Buffer Operations ----

// Initialize a gap buffer with the given capacity.
// Returns: 1 on success, 0 on allocation failure.
int MC_GapBuffer_Init(MC_GapBuffer* pGB, uint32_t initialCapacity);

// Free all memory and zero the struct.
void MC_GapBuffer_Destroy(MC_GapBuffer* pGB);

// Move the gap to `pos` (logical byte offset) for O(1) insertion.
void MC_GapBuffer_MoveGap(MC_GapBuffer* pGB, uint32_t pos);

// Insert `len` bytes of `text` at logical position `pos`.
// Automatically grows the buffer if needed.
// Returns: 1 on success, 0 on allocation failure.
int MC_GapBuffer_Insert(MC_GapBuffer* pGB, uint32_t pos,
                        const char* text, uint32_t len);

// Delete `len` bytes starting at logical position `pos`.
// Returns: 1 on success, 0 on failure.
int MC_GapBuffer_Delete(MC_GapBuffer* pGB, uint32_t pos, uint32_t len);

// Extract line `lineIdx` (0-based) into `outBuffer` (null-terminated).
// Returns: length of the line (excluding null), 0 if line not found.
uint32_t MC_GapBuffer_GetLine(MC_GapBuffer* pGB, uint32_t lineIdx,
                               char* outBuffer, uint32_t maxLen);

// Get the logical content length (total bytes excluding gap).
uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB);

// Get the total number of lines (newline count + 1).
uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB);

// ---- Tokenization ----

// Tokenize a single line of text. Writes tokens to `outTokens`.
// Returns: number of tokens emitted.
uint32_t MC_TokenizeLine(const char* line, uint32_t len,
                          MC_Token* outTokens, uint32_t maxTokens);

// Classification helpers (used by tokenizer, also available externally)
int MC_IsRegister(const char* line, uint32_t offset, uint32_t len);
int MC_IsInstruction(const char* line, uint32_t offset, uint32_t len);
int MC_IsDirective(const char* line, uint32_t offset, uint32_t len);

} // extern "C"

// ============================================================================
// Utility: Map MC_TokenType to Default Color
// ============================================================================
inline uint32_t MC_GetTokenColor(MC_TokenType type) {
    switch (type) {
        case MC_TokenType::Comment:      return MC_Colors::COMMENT;
        case MC_TokenType::Keyword:      return MC_Colors::KEYWORD;
        case MC_TokenType::String:       return MC_Colors::STRING;
        case MC_TokenType::Number:       return MC_Colors::NUMBER;
        case MC_TokenType::Operator:     return MC_Colors::OPERATOR;
        case MC_TokenType::Register:     return MC_Colors::REGISTER;
        case MC_TokenType::Directive:    return MC_Colors::DIRECTIVE;
        case MC_TokenType::Instruction:  return MC_Colors::INSTRUCTION;
        case MC_TokenType::Preprocessor: return MC_Colors::PREPROCESSOR;
        case MC_TokenType::Identifier:   return MC_Colors::IDENTIFIER;
        default:                         return MC_Colors::TEXT_DEFAULT;
    }
}

// ============================================================================
// Utility: Convert BGRA uint32 to D2D1_COLOR_F
// ============================================================================
// Avoids pulling in d2d1.h here — caller can use this inline.
struct MC_ColorF {
    float r, g, b, a;
};

inline MC_ColorF MC_BGRAtoColorF(uint32_t bgra) {
    return {
        ((bgra >> 16) & 0xFF) / 255.0f,  // R
        ((bgra >>  8) & 0xFF) / 255.0f,  // G
        ((bgra >>  0) & 0xFF) / 255.0f,  // B
        ((bgra >> 24) & 0xFF) / 255.0f   // A
    };
}

// ============================================================================
// MonacoCore RAII Wrapper
// ============================================================================
// Lightweight wrapper managing the gap buffer lifecycle.
// The full editor engine (with D2D rendering, input, etc.) is implemented
// in MonacoCoreEngine (src/core/MonacoCoreEngine.cpp).
class MonacoCoreBuffer {
public:
    MonacoCoreBuffer() {
        memset(&m_buffer, 0, sizeof(m_buffer));
    }

    ~MonacoCoreBuffer() {
        destroy();
    }

    bool init(uint32_t capacity = 65536) {
        return MC_GapBuffer_Init(&m_buffer, capacity) != 0;
    }

    void destroy() {
        if (m_buffer.pBuffer) {
            MC_GapBuffer_Destroy(&m_buffer);
        }
    }

    bool insert(uint32_t pos, const char* text, uint32_t len) {
        return MC_GapBuffer_Insert(&m_buffer, pos, text, len) != 0;
    }

    bool remove(uint32_t pos, uint32_t len) {
        return MC_GapBuffer_Delete(&m_buffer, pos, len) != 0;
    }

    uint32_t getLine(uint32_t lineIdx, char* outBuffer, uint32_t maxLen) const {
        return MC_GapBuffer_GetLine(const_cast<MC_GapBuffer*>(&m_buffer),
                                     lineIdx, outBuffer, maxLen);
    }

    uint32_t length() const {
        return MC_GapBuffer_Length(&m_buffer);
    }

    uint32_t lineCount() const {
        return MC_GapBuffer_LineCount(&m_buffer);
    }

    MC_GapBuffer* raw() { return &m_buffer; }
    const MC_GapBuffer* raw() const { return &m_buffer; }

    // Disable copy
    MonacoCoreBuffer(const MonacoCoreBuffer&) = delete;
    MonacoCoreBuffer& operator=(const MonacoCoreBuffer&) = delete;

private:
    MC_GapBuffer m_buffer;
};
