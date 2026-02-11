// ============================================================================
// monaco_core_stubs.cpp — C++ fallback stubs for MonacoCore ASM functions
// ============================================================================
// PURPOSE: Provides C++ implementations of the functions normally in
//          src/asm/RawrXD_MonacoCore.asm when building with MinGW/GCC
//          (which cannot assemble MASM64).
//
// These are functional stubs — they implement correct gap buffer logic
// so the MonacoCore editor engine works on MinGW builds.
//
// On MSVC, the real ASM objects are linked instead.
// ============================================================================

#ifndef _MSC_VER  // Only compile for non-MSVC (MinGW/GCC)

#include "../../include/RawrXD_MonacoCore.h"
#include <cstdlib>
#include <cstring>

extern "C" {

// ============================================================================
// MC_GapBuffer_Init — Initialize gap buffer with given capacity
// ============================================================================
int MC_GapBuffer_Init(MC_GapBuffer* pGB, uint32_t initialCapacity) {
    if (!pGB || initialCapacity == 0) return 0;
    
    pGB->pBuffer = static_cast<uint8_t*>(malloc(initialCapacity));
    if (!pGB->pBuffer) return 0;
    
    pGB->gapStart = 0;
    pGB->gapEnd = initialCapacity;
    pGB->capacity = initialCapacity;
    pGB->used = 0;
    pGB->lineCount = 1;  // Empty buffer has 1 line
    pGB->reserved = 0;
    return 1;
}

// ============================================================================
// MC_GapBuffer_Destroy — Free gap buffer memory
// ============================================================================
void MC_GapBuffer_Destroy(MC_GapBuffer* pGB) {
    if (!pGB) return;
    if (pGB->pBuffer) {
        free(pGB->pBuffer);
    }
    memset(pGB, 0, sizeof(MC_GapBuffer));
}

// ============================================================================
// MC_GapBuffer_MoveGap — Reposition gap to logical position
// ============================================================================
void MC_GapBuffer_MoveGap(MC_GapBuffer* pGB, uint32_t pos) {
    if (!pGB || !pGB->pBuffer) return;
    if (pos > pGB->used) pos = pGB->used;
    
    uint32_t gapSize = pGB->gapEnd - pGB->gapStart;
    
    if (pos < pGB->gapStart) {
        // Move gap left: shift data right
        uint32_t moveLen = pGB->gapStart - pos;
        memmove(pGB->pBuffer + pGB->gapEnd - moveLen,
                pGB->pBuffer + pos, moveLen);
        pGB->gapStart = pos;
        pGB->gapEnd = pos + gapSize;
    } else if (pos > pGB->gapStart) {
        // Move gap right: shift data left
        uint32_t moveLen = pos - pGB->gapStart;
        memmove(pGB->pBuffer + pGB->gapStart,
                pGB->pBuffer + pGB->gapEnd, moveLen);
        pGB->gapStart = pos;
        pGB->gapEnd = pos + gapSize;
    }
}

// ============================================================================
// MC_GapBuffer_EnsureGap (internal helper) — Grow buffer if gap is too small
// ============================================================================
static int MC_GapBuffer_EnsureGap(MC_GapBuffer* pGB, uint32_t needed) {
    uint32_t gapSize = pGB->gapEnd - pGB->gapStart;
    if (gapSize >= needed) return 1;
    
    // Grow by 2x or needed, whichever is larger
    uint32_t newCapacity = pGB->capacity * 2;
    if (newCapacity < pGB->capacity + needed) {
        newCapacity = pGB->capacity + needed + 1024;
    }
    
    uint8_t* newBuf = static_cast<uint8_t*>(malloc(newCapacity));
    if (!newBuf) return 0;
    
    // Copy data before gap
    if (pGB->gapStart > 0) {
        memcpy(newBuf, pGB->pBuffer, pGB->gapStart);
    }
    
    // Copy data after gap
    uint32_t afterGap = pGB->capacity - pGB->gapEnd;
    if (afterGap > 0) {
        memcpy(newBuf + newCapacity - afterGap,
               pGB->pBuffer + pGB->gapEnd, afterGap);
    }
    
    free(pGB->pBuffer);
    pGB->pBuffer = newBuf;
    pGB->gapEnd = newCapacity - afterGap;
    pGB->capacity = newCapacity;
    return 1;
}

// ============================================================================
// MC_GapBuffer_Insert — Insert text at position
// ============================================================================
int MC_GapBuffer_Insert(MC_GapBuffer* pGB, uint32_t pos,
                        const char* text, uint32_t len) {
    if (!pGB || !pGB->pBuffer || !text || len == 0) return 0;
    if (pos > pGB->used) pos = pGB->used;
    
    if (!MC_GapBuffer_EnsureGap(pGB, len)) return 0;
    
    MC_GapBuffer_MoveGap(pGB, pos);
    
    // Insert into gap
    memcpy(pGB->pBuffer + pGB->gapStart, text, len);
    pGB->gapStart += len;
    pGB->used += len;
    
    // Update line count
    for (uint32_t i = 0; i < len; i++) {
        if (text[i] == '\n') pGB->lineCount++;
    }
    
    return 1;
}

// ============================================================================
// MC_GapBuffer_Delete — Delete range from buffer
// ============================================================================
int MC_GapBuffer_Delete(MC_GapBuffer* pGB, uint32_t pos, uint32_t len) {
    if (!pGB || !pGB->pBuffer) return 0;
    if (pos >= pGB->used) return 0;
    if (pos + len > pGB->used) len = pGB->used - pos;
    if (len == 0) return 1;
    
    MC_GapBuffer_MoveGap(pGB, pos);
    
    // Count newlines being deleted
    for (uint32_t i = 0; i < len; i++) {
        if (pGB->pBuffer[pGB->gapEnd + i] == '\n') {
            if (pGB->lineCount > 1) pGB->lineCount--;
        }
    }
    
    pGB->gapEnd += len;
    pGB->used -= len;
    return 1;
}

// ============================================================================
// Helper: get content byte at logical position
// ============================================================================
static uint8_t gb_char_at(const MC_GapBuffer* pGB, uint32_t logicalPos) {
    if (logicalPos < pGB->gapStart) {
        return pGB->pBuffer[logicalPos];
    } else {
        return pGB->pBuffer[pGB->gapEnd + (logicalPos - pGB->gapStart)];
    }
}

// ============================================================================
// MC_GapBuffer_GetLine — Extract line content
// ============================================================================
uint32_t MC_GapBuffer_GetLine(MC_GapBuffer* pGB, uint32_t lineIdx,
                               char* outBuffer, uint32_t maxLen) {
    if (!pGB || !pGB->pBuffer || !outBuffer || maxLen == 0) return 0;
    
    // Find start of the requested line
    uint32_t currentLine = 0;
    uint32_t lineStart = 0;
    
    for (uint32_t i = 0; i < pGB->used && currentLine < lineIdx; i++) {
        if (gb_char_at(pGB, i) == '\n') {
            currentLine++;
            lineStart = i + 1;
        }
    }
    
    if (currentLine != lineIdx) {
        outBuffer[0] = '\0';
        return 0;
    }
    
    // Copy line content until newline or end
    uint32_t outLen = 0;
    for (uint32_t i = lineStart; i < pGB->used && outLen < maxLen - 1; i++) {
        uint8_t ch = gb_char_at(pGB, i);
        if (ch == '\n') break;
        outBuffer[outLen++] = static_cast<char>(ch);
    }
    
    outBuffer[outLen] = '\0';
    return outLen;
}

// ============================================================================
// MC_GapBuffer_Length — Get logical content length
// ============================================================================
uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB) {
    if (!pGB) return 0;
    return pGB->used;
}

// ============================================================================
// MC_GapBuffer_LineCount — Count lines in buffer
// ============================================================================
uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB) {
    if (!pGB) return 0;
    return pGB->lineCount;
}

// ============================================================================
// MC_TokenizeLine — Tokenize a single line
// ============================================================================
uint32_t MC_TokenizeLine(const char* line, uint32_t len,
                          MC_Token* outTokens, uint32_t maxTokens) {
    if (!line || len == 0 || !outTokens || maxTokens == 0) return 0;
    
    uint32_t tokenIdx = 0;
    uint32_t i = 0;
    
    while (i < len && tokenIdx < maxTokens) {
        // Skip whitespace
        if (line[i] == ' ' || line[i] == '\t' || line[i] == '\r') {
            i++;
            continue;
        }
        
        MC_Token& tok = outTokens[tokenIdx];
        tok.startCol = i;
        tok.color = 0;  // Use theme default
        
        // Comment: ; or //
        if (line[i] == ';' || (i + 1 < len && line[i] == '/' && line[i + 1] == '/')) {
            tok.tokenType = static_cast<uint32_t>(MC_TokenType::Comment);
            tok.length = len - i;
            tokenIdx++;
            break;  // Rest of line is comment
        }
        
        // String literal
        if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            uint32_t start = i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i++;  // Skip escape
                i++;
            }
            if (i < len) i++;  // Skip closing quote
            tok.tokenType = static_cast<uint32_t>(MC_TokenType::String);
            tok.length = i - start;
            tokenIdx++;
            continue;
        }
        
        // Number
        if ((line[i] >= '0' && line[i] <= '9') ||
            (line[i] == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X'))) {
            uint32_t start = i;
            if (line[i] == '0' && i + 1 < len && (line[i + 1] == 'x' || line[i + 1] == 'X')) {
                i += 2;
                while (i < len && ((line[i] >= '0' && line[i] <= '9') ||
                       (line[i] >= 'a' && line[i] <= 'f') ||
                       (line[i] >= 'A' && line[i] <= 'F'))) i++;
            } else {
                while (i < len && line[i] >= '0' && line[i] <= '9') i++;
                if (i < len && line[i] == '.') {
                    i++;
                    while (i < len && line[i] >= '0' && line[i] <= '9') i++;
                }
            }
            tok.tokenType = static_cast<uint32_t>(MC_TokenType::Number);
            tok.length = i - start;
            tokenIdx++;
            continue;
        }
        
        // Preprocessor directive
        if (line[i] == '#') {
            uint32_t start = i;
            while (i < len && line[i] != ' ' && line[i] != '\t') i++;
            tok.tokenType = static_cast<uint32_t>(MC_TokenType::Preprocessor);
            tok.length = i - start;
            tokenIdx++;
            continue;
        }
        
        // Operator
        if (line[i] == '+' || line[i] == '-' || line[i] == '*' || line[i] == '/' ||
            line[i] == '=' || line[i] == '<' || line[i] == '>' || line[i] == '!' ||
            line[i] == '&' || line[i] == '|' || line[i] == '^' || line[i] == '~' ||
            line[i] == '(' || line[i] == ')' || line[i] == '[' || line[i] == ']' ||
            line[i] == '{' || line[i] == '}' || line[i] == ',' || line[i] == ':') {
            tok.tokenType = static_cast<uint32_t>(MC_TokenType::Operator);
            tok.length = 1;
            i++;
            tokenIdx++;
            continue;
        }
        
        // Identifier / keyword / register / instruction / directive
        if ((line[i] >= 'a' && line[i] <= 'z') || (line[i] >= 'A' && line[i] <= 'Z') ||
            line[i] == '_' || line[i] == '.') {
            uint32_t start = i;
            while (i < len && ((line[i] >= 'a' && line[i] <= 'z') ||
                   (line[i] >= 'A' && line[i] <= 'Z') ||
                   (line[i] >= '0' && line[i] <= '9') ||
                   line[i] == '_' || line[i] == '.')) i++;
            tok.tokenType = static_cast<uint32_t>(MC_TokenType::Identifier);
            tok.length = i - start;
            tokenIdx++;
            continue;
        }
        
        // Unknown character — emit as identifier
        tok.tokenType = static_cast<uint32_t>(MC_TokenType::Identifier);
        tok.length = 1;
        i++;
        tokenIdx++;
    }
    
    return tokenIdx;
}

// ============================================================================
// Classification Helpers
// ============================================================================
int MC_IsRegister(const char* line, uint32_t offset, uint32_t len) {
    // Stub: Simple x86-64 register check
    if (!line || len < 2 || len > 5) return 0;
    const char* word = line + offset;
    // Check common registers: rax, rbx, rcx, rdx, rsi, rdi, rsp, rbp, eax, etc.
    if (len == 3) {
        if ((word[0] == 'r' || word[0] == 'R') &&
            (word[1] == 'a' || word[1] == 'b' || word[1] == 'c' || word[1] == 'd') &&
            (word[2] == 'x' || word[2] == 'X')) return 1;
        if ((word[0] == 'e' || word[0] == 'E') &&
            (word[1] == 'a' || word[1] == 'b' || word[1] == 'c' || word[1] == 'd') &&
            (word[2] == 'x' || word[2] == 'X')) return 1;
        if ((word[0] == 'r' || word[0] == 'R') &&
            (word[1] == 's' || word[1] == 'b' || word[1] == 'd') &&
            (word[2] == 'p' || word[2] == 'i')) return 1;
    }
    return 0;
}

int MC_IsInstruction(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len < 2 || len > 10) return 0;
    const char* word = line + offset;
    // Check common x86 instructions
    if (len == 3 && (memcmp(word, "mov", 3) == 0 || memcmp(word, "add", 3) == 0 ||
                     memcmp(word, "sub", 3) == 0 || memcmp(word, "xor", 3) == 0 ||
                     memcmp(word, "and", 3) == 0 || memcmp(word, "ret", 3) == 0 ||
                     memcmp(word, "jmp", 3) == 0 || memcmp(word, "cmp", 3) == 0 ||
                     memcmp(word, "lea", 3) == 0 || memcmp(word, "nop", 3) == 0)) return 1;
    if (len == 4 && (memcmp(word, "push", 4) == 0 || memcmp(word, "call", 4) == 0 ||
                     memcmp(word, "test", 4) == 0)) return 1;
    return 0;
}

int MC_IsDirective(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len < 2 || len > 10) return 0;
    const char* word = line + offset;
    // Check common ASM directives
    if (word[0] == '.') return 1;  // .data, .code, .text, etc.
    if (len == 2 && (memcmp(word, "db", 2) == 0 || memcmp(word, "dw", 2) == 0 ||
                     memcmp(word, "dd", 2) == 0 || memcmp(word, "dq", 2) == 0)) return 1;
    if (len == 4 && memcmp(word, "proc", 4) == 0) return 1;
    if (len == 4 && memcmp(word, "endp", 4) == 0) return 1;
    return 0;
}

} // extern "C"

#endif // !_MSC_VER
