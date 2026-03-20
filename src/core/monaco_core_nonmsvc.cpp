#if !defined(_MSC_VER)

#include "RawrXD_MonacoCore.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

static uint32_t computeLineCount(const MC_GapBuffer* pGB) {
    if (!pGB || !pGB->pBuffer || pGB->capacity == 0) {
        return 1;
    }

    uint32_t lines = 1;
    for (uint32_t i = 0; i < pGB->gapStart; ++i) {
        if (pGB->pBuffer[i] == '\n') {
            ++lines;
        }
    }
    for (uint32_t i = pGB->gapEnd; i < pGB->capacity; ++i) {
        if (pGB->pBuffer[i] == '\n') {
            ++lines;
        }
    }
    return lines;
}

static uint32_t gapSize(const MC_GapBuffer* pGB) {
    if (!pGB || pGB->gapEnd < pGB->gapStart) {
        return 0;
    }
    return pGB->gapEnd - pGB->gapStart;
}

static bool ensureGap(MC_GapBuffer* pGB, uint32_t need) {
    if (!pGB || !pGB->pBuffer) {
        return false;
    }
    if (gapSize(pGB) >= need) {
        return true;
    }

    uint32_t newCapacity = pGB->capacity > 0 ? pGB->capacity : 64;
    while ((newCapacity - pGB->used) < need) {
        newCapacity = (newCapacity < (1U << 30)) ? (newCapacity * 2U) : (newCapacity + need);
    }

    uint8_t* newBuf = static_cast<uint8_t*>(std::malloc(newCapacity));
    if (!newBuf) {
        return false;
    }
    std::memset(newBuf, 0, newCapacity);

    const uint32_t left = pGB->gapStart;
    const uint32_t right = pGB->capacity - pGB->gapEnd;
    std::memcpy(newBuf, pGB->pBuffer, left);
    std::memcpy(newBuf + newCapacity - right, pGB->pBuffer + pGB->gapEnd, right);

    std::free(pGB->pBuffer);
    pGB->pBuffer = newBuf;
    pGB->capacity = newCapacity;
    pGB->gapStart = left;
    pGB->gapEnd = newCapacity - right;
    return true;
}

static void moveGap(MC_GapBuffer* pGB, uint32_t pos) {
    if (!pGB || !pGB->pBuffer) {
        return;
    }
    if (pos > pGB->used) {
        pos = pGB->used;
    }

    if (pos < pGB->gapStart) {
        const uint32_t delta = pGB->gapStart - pos;
        std::memmove(pGB->pBuffer + pGB->gapEnd - delta, pGB->pBuffer + pos, delta);
        pGB->gapStart -= delta;
        pGB->gapEnd -= delta;
    } else if (pos > pGB->gapStart) {
        const uint32_t delta = pos - pGB->gapStart;
        std::memmove(pGB->pBuffer + pGB->gapStart, pGB->pBuffer + pGB->gapEnd, delta);
        pGB->gapStart += delta;
        pGB->gapEnd += delta;
    }
}

static std::string flatten(const MC_GapBuffer* pGB) {
    if (!pGB || !pGB->pBuffer || pGB->used == 0) {
        return {};
    }
    std::string out;
    out.resize(pGB->used);
    const uint32_t left = pGB->gapStart;
    std::memcpy(out.data(), pGB->pBuffer, left);
    const uint32_t right = pGB->capacity - pGB->gapEnd;
    if (right > 0) {
        std::memcpy(out.data() + left, pGB->pBuffer + pGB->gapEnd, right);
    }
    return out;
}

static bool isKeyword(const std::string& token) {
    static const char* kWords[] = {
        "if", "else", "for", "while", "return", "switch", "case", "break",
        "continue", "class", "struct", "namespace", "template", "typename",
        "using", "public", "private", "protected", "virtual", "override",
        "const", "static", "inline", "void", "int", "float", "double", "bool",
        "char", "unsigned", "signed", "auto", "new", "delete", "try", "catch"
    };
    for (const char* w : kWords) {
        if (token == w) {
            return true;
        }
    }
    return false;
}

}  // namespace

extern "C" int64_t asm_symbol_hash_lookup(const uint64_t* hashArray, int64_t count,
                                          uint64_t targetHash) {
    if (!hashArray || count <= 0) {
        return -1;
    }
    int64_t lo = 0;
    int64_t hi = count - 1;
    while (lo <= hi) {
        const int64_t mid = lo + ((hi - lo) / 2);
        const uint64_t v = hashArray[mid];
        if (v == targetHash) {
            return mid;
        }
        if (v < targetHash) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return -1;
}

extern "C" int MC_GapBuffer_Init(MC_GapBuffer* pGB, uint32_t initialCapacity) {
    if (!pGB) {
        return 0;
    }
    if (initialCapacity < 64) {
        initialCapacity = 64;
    }
    uint8_t* buf = static_cast<uint8_t*>(std::malloc(initialCapacity));
    if (!buf) {
        std::memset(pGB, 0, sizeof(*pGB));
        return 0;
    }
    std::memset(buf, 0, initialCapacity);
    pGB->pBuffer = buf;
    pGB->gapStart = 0;
    pGB->gapEnd = initialCapacity;
    pGB->capacity = initialCapacity;
    pGB->used = 0;
    pGB->lineCount = 1;
    pGB->reserved = 0;
    return 1;
}

extern "C" void MC_GapBuffer_Destroy(MC_GapBuffer* pGB) {
    if (!pGB) {
        return;
    }
    if (pGB->pBuffer) {
        std::free(pGB->pBuffer);
    }
    std::memset(pGB, 0, sizeof(*pGB));
}

extern "C" void MC_GapBuffer_MoveGap(MC_GapBuffer* pGB, uint32_t pos) {
    moveGap(pGB, pos);
}

extern "C" int MC_GapBuffer_Insert(MC_GapBuffer* pGB, uint32_t pos,
                                   const char* text, uint32_t len) {
    if (!pGB || !text || len == 0 || pos > pGB->used) {
        return 0;
    }
    moveGap(pGB, pos);
    if (!ensureGap(pGB, len)) {
        return 0;
    }
    std::memcpy(pGB->pBuffer + pGB->gapStart, text, len);
    pGB->gapStart += len;
    pGB->used += len;
    pGB->lineCount = computeLineCount(pGB);
    return 1;
}

extern "C" int MC_GapBuffer_Delete(MC_GapBuffer* pGB, uint32_t pos, uint32_t len) {
    if (!pGB || !pGB->pBuffer || len == 0 || pos >= pGB->used) {
        return 0;
    }
    moveGap(pGB, pos);
    const uint32_t available = pGB->used - pos;
    const uint32_t removeLen = std::min(len, available);
    pGB->gapEnd += removeLen;
    pGB->used -= removeLen;
    pGB->lineCount = computeLineCount(pGB);
    return 1;
}

extern "C" uint32_t MC_GapBuffer_GetLine(MC_GapBuffer* pGB, uint32_t lineIdx,
                                         char* outBuffer, uint32_t maxLen) {
    if (!pGB || !outBuffer || maxLen == 0) {
        return 0;
    }
    outBuffer[0] = '\0';

    const std::string text = flatten(pGB);
    uint32_t currentLine = 0;
    size_t start = 0;
    for (size_t i = 0; i <= text.size(); ++i) {
        const bool isLineEnd = (i == text.size() || text[i] == '\n');
        if (!isLineEnd) {
            continue;
        }
        if (currentLine == lineIdx) {
            const size_t len = i - start;
            const size_t copyLen = std::min<size_t>(len, static_cast<size_t>(maxLen - 1));
            std::memcpy(outBuffer, text.data() + start, copyLen);
            outBuffer[copyLen] = '\0';
            return static_cast<uint32_t>(copyLen);
        }
        ++currentLine;
        start = i + 1;
    }
    return 0;
}

extern "C" uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB) {
    return pGB ? pGB->used : 0;
}

extern "C" uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB) {
    if (!pGB) {
        return 0;
    }
    return pGB->lineCount == 0 ? 1 : pGB->lineCount;
}

extern "C" int MC_IsRegister(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len == 0) {
        return 0;
    }
    std::string tok(line + offset, line + offset + len);
    for (char& c : tok) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (tok == "rax" || tok == "rbx" || tok == "rcx" || tok == "rdx" ||
        tok == "rsi" || tok == "rdi" || tok == "rsp" || tok == "rbp" ||
        tok == "eax" || tok == "ebx" || tok == "ecx" || tok == "edx") {
        return 1;
    }
    if ((tok.size() == 2 || tok.size() == 3) && tok[0] == 'r' &&
        std::all_of(tok.begin() + 1, tok.end(), [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); })) {
        return 1;
    }
    return 0;
}

extern "C" int MC_IsInstruction(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len == 0) {
        return 0;
    }
    std::string tok(line + offset, line + offset + len);
    for (char& c : tok) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const char* kInst[] = {"mov", "lea", "push", "pop", "cmp", "test", "add", "sub",
                                  "mul", "div", "and", "or", "xor", "jmp", "call", "ret", "nop"};
    for (const char* i : kInst) {
        if (tok == i) {
            return 1;
        }
    }
    return 0;
}

extern "C" int MC_IsDirective(const char* line, uint32_t offset, uint32_t len) {
    if (!line || len == 0) {
        return 0;
    }
    if (line[offset] == '.') {
        return 1;
    }
    std::string tok(line + offset, line + offset + len);
    for (char& c : tok) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return (tok == "section" || tok == "global" || tok == "extern" ||
            tok == "db" || tok == "dw" || tok == "dd" || tok == "dq") ? 1 : 0;
}

extern "C" uint32_t MC_TokenizeLine(const char* line, uint32_t len,
                                    MC_Token* outTokens, uint32_t maxTokens) {
    if (!line || !outTokens || maxTokens == 0) {
        return 0;
    }

    uint32_t count = 0;
    uint32_t i = 0;
    while (i < len && count < maxTokens) {
        const unsigned char ch = static_cast<unsigned char>(line[i]);

        if (std::isspace(ch)) {
            const uint32_t start = i;
            while (i < len && std::isspace(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            outTokens[count++] = MC_Token{start, i - start,
                                          static_cast<uint32_t>(MC_TokenType::Whitespace),
                                          MC_GetTokenColor(MC_TokenType::Whitespace)};
            continue;
        }

        if (ch == '/' && (i + 1) < len && line[i + 1] == '/') {
            outTokens[count++] = MC_Token{i, len - i,
                                          static_cast<uint32_t>(MC_TokenType::Comment),
                                          MC_GetTokenColor(MC_TokenType::Comment)};
            break;
        }

        if (ch == '"' || ch == '\'') {
            const char quote = static_cast<char>(ch);
            const uint32_t start = i++;
            while (i < len) {
                if (line[i] == '\\' && (i + 1) < len) {
                    i += 2;
                    continue;
                }
                if (line[i] == quote) {
                    ++i;
                    break;
                }
                ++i;
            }
            outTokens[count++] = MC_Token{start, i - start,
                                          static_cast<uint32_t>(MC_TokenType::String),
                                          MC_GetTokenColor(MC_TokenType::String)};
            continue;
        }

        if (std::isdigit(ch)) {
            const uint32_t start = i;
            while (i < len && (std::isalnum(static_cast<unsigned char>(line[i])) ||
                               line[i] == '.' || line[i] == 'x' || line[i] == 'X')) {
                ++i;
            }
            outTokens[count++] = MC_Token{start, i - start,
                                          static_cast<uint32_t>(MC_TokenType::Number),
                                          MC_GetTokenColor(MC_TokenType::Number)};
            continue;
        }

        if (std::isalpha(ch) || ch == '_') {
            const uint32_t start = i;
            while (i < len && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                ++i;
            }
            const uint32_t tokLen = i - start;
            const std::string token(line + start, line + i);

            MC_TokenType tt = MC_TokenType::Identifier;
            if (isKeyword(token)) {
                tt = MC_TokenType::Keyword;
            } else if (MC_IsRegister(line, start, tokLen)) {
                tt = MC_TokenType::Register;
            } else if (MC_IsInstruction(line, start, tokLen)) {
                tt = MC_TokenType::Instruction;
            } else if (MC_IsDirective(line, start, tokLen)) {
                tt = MC_TokenType::Directive;
            }
            outTokens[count++] = MC_Token{start, tokLen,
                                          static_cast<uint32_t>(tt),
                                          MC_GetTokenColor(tt)};
            continue;
        }

        outTokens[count++] = MC_Token{i, 1,
                                      static_cast<uint32_t>(MC_TokenType::Operator),
                                      MC_GetTokenColor(MC_TokenType::Operator)};
        ++i;
    }

    return count;
}

#endif  // !defined(_MSC_VER)
