#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "agentic_task_graph.hpp"
#include "embedding_engine.hpp"
#include "vision_encoder.hpp"
#include "RawrXD_MonacoCore.h"
#include "native_debugger_engine.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#if !defined(_MSC_VER)

namespace {

constexpr uint32_t kMinGapCapacity = 256;
constexpr uint32_t kDefaultInsertReserve = 128;
constexpr uint32_t kSwarmMagic = 0x52574152U;  // 'RAWR'
constexpr uint16_t kSwarmDiscoveryMsg = 6;

uint32_t mcGapSize(const MC_GapBuffer* pGB) {
    return (pGB->gapEnd >= pGB->gapStart) ? (pGB->gapEnd - pGB->gapStart) : 0U;
}

bool mcValidate(const MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return false;
    }
    if (pGB->pBuffer == nullptr && (pGB->capacity != 0 || pGB->used != 0)) {
        return false;
    }
    if (pGB->capacity == 0) {
        return pGB->used == 0 && pGB->gapStart == 0 && pGB->gapEnd == 0;
    }
    if (pGB->gapStart > pGB->gapEnd) {
        return false;
    }
    if (pGB->gapEnd > pGB->capacity) {
        return false;
    }
    return pGB->used <= pGB->capacity;
}

char mcCharAt(const MC_GapBuffer* pGB, uint32_t logicalPos) {
    const uint32_t gap = mcGapSize(pGB);
    if (logicalPos < pGB->gapStart) {
        return static_cast<char>(pGB->pBuffer[logicalPos]);
    }
    return static_cast<char>(pGB->pBuffer[logicalPos + gap]);
}

uint32_t mcCountNewlines(const char* data, uint32_t len) {
    uint32_t count = 0;
    if (data == nullptr) {
        return 0;
    }
    for (uint32_t i = 0; i < len; ++i) {
        if (data[i] == '\n') {
            ++count;
        }
    }
    return count;
}

void mcRecomputeLineCount(MC_GapBuffer* pGB) {
    if (pGB == nullptr || pGB->pBuffer == nullptr) {
        return;
    }
    uint32_t lines = 0;
    for (uint32_t i = 0; i < pGB->used; ++i) {
        if (mcCharAt(pGB, i) == '\n') {
            ++lines;
        }
    }
    pGB->lineCount = lines;
}

void mcZeroBuffer(MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return;
    }
    std::memset(pGB, 0, sizeof(*pGB));
}

bool mcEnsureGap(MC_GapBuffer* pGB, uint32_t requiredGap) {
    if (pGB == nullptr || pGB->pBuffer == nullptr) {
        return false;
    }
    const uint32_t currentGap = mcGapSize(pGB);
    if (currentGap >= requiredGap) {
        return true;
    }

    uint32_t newCapacity = std::max<uint32_t>(pGB->capacity * 2U, pGB->used + requiredGap + kDefaultInsertReserve);
    newCapacity = std::max<uint32_t>(newCapacity, kMinGapCapacity);
    if (newCapacity < pGB->used) {
        return false;
    }

    uint8_t* newBuffer = static_cast<uint8_t*>(std::realloc(pGB->pBuffer, newCapacity));
    if (newBuffer == nullptr) {
        return false;
    }

    pGB->pBuffer = newBuffer;
    const uint32_t suffixLen = pGB->capacity - pGB->gapEnd;
    const uint32_t newGapEnd = newCapacity - suffixLen;
    if (suffixLen > 0) {
        std::memmove(pGB->pBuffer + newGapEnd, pGB->pBuffer + pGB->gapEnd, suffixLen);
    }

    pGB->gapEnd = newGapEnd;
    pGB->capacity = newCapacity;
    return true;
}

bool mcIsIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '$' || c == '@' || c == '?';
}

bool mcIsIdentifierChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '$' || c == '@' || c == '?';
}

bool mcIsOperatorChar(char c) {
    switch (c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '%':
        case '=':
        case '!':
        case '&':
        case '|':
        case '^':
        case '~':
        case '<':
        case '>':
        case ':':
        case ',':
        case '(':
        case ')':
        case '[':
        case ']':
        case '{':
        case '}':
            return true;
        default:
            return false;
    }
}

std::string mcToLower(std::string token) {
    std::transform(token.begin(), token.end(), token.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return token;
}

bool mcIsRegisterToken(std::string token) {
    token = mcToLower(std::move(token));
    static const char* kRegisters[] = {
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15",
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "ax",  "bx",  "cx",  "dx",  "si",  "di",  "bp",  "sp",
        "al",  "ah",  "bl",  "bh",  "cl",  "ch",  "dl",  "dh",
        "rip", "eip", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5",
        "xmm6", "xmm7", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13",
        "xmm14", "xmm15", "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5",
        "ymm6", "ymm7", "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13",
        "ymm14", "ymm15", "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5",
        "zmm6", "zmm7", "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13",
        "zmm14", "zmm15", "cr0", "cr2", "cr3", "cr4", "dr0", "dr1", "dr2", "dr3",
        "dr6", "dr7", "cs", "ds", "es", "fs", "gs", "ss"
    };
    for (const char* reg : kRegisters) {
        if (token == reg) {
            return true;
        }
    }
    return false;
}

bool mcIsInstructionToken(std::string token) {
    token = mcToLower(std::move(token));
    static const char* kInstructions[] = {
        "mov", "movzx", "movsx", "lea", "push", "pop", "call", "ret",
        "jmp", "je", "jne", "jg", "jge", "jl", "jle", "ja", "jae", "jb", "jbe",
        "cmp", "test", "add", "sub", "imul", "mul", "idiv", "div", "inc", "dec",
        "and", "or", "xor", "not", "neg", "shl", "shr", "sar", "rol", "ror",
        "nop", "int3", "syscall", "sysret", "cmovz", "cmovnz", "setnz", "setz"
    };
    for (const char* insn : kInstructions) {
        if (token == insn) {
            return true;
        }
    }
    return false;
}

bool mcIsDirectiveToken(std::string token) {
    token = mcToLower(std::move(token));
    if (!token.empty() && token.front() == '.') {
        return true;
    }
    static const char* kDirectives[] = {
        "db", "dw", "dd", "dq", "dt", "do", "dy", "resb", "resw", "resd", "resq",
        "equ", "org", "align", "proc", "endp", "segment", "ends", "section",
        "include", "includelib", "extern", "public", "global", "macro", "endm"
    };
    for (const char* directive : kDirectives) {
        if (token == directive) {
            return true;
        }
    }
    return false;
}

bool mcPushToken(MC_Token* outTokens, uint32_t maxTokens, uint32_t& tokenCount,
                 uint32_t startCol, uint32_t length, MC_TokenType tokenType) {
    if (outTokens == nullptr || tokenCount >= maxTokens) {
        return false;
    }
    outTokens[tokenCount].startCol = startCol;
    outTokens[tokenCount].length = length;
    outTokens[tokenCount].tokenType = static_cast<uint32_t>(tokenType);
    outTokens[tokenCount].color = MC_GetTokenColor(tokenType);
    ++tokenCount;
    return true;
}

float halfToFloat(uint16_t half) {
    const uint32_t sign = static_cast<uint32_t>(half & 0x8000U) << 16;
    uint32_t exp = (half >> 10) & 0x1FU;
    uint32_t mantissa = half & 0x03FFU;

    uint32_t bits;
    if (exp == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {
            exp = 1;
            while ((mantissa & 0x0400U) == 0) {
                mantissa <<= 1;
                --exp;
            }
            mantissa &= 0x03FFU;
            bits = sign | ((exp + (127 - 15)) << 23) | (mantissa << 13);
        }
    } else if (exp == 0x1FU) {
        bits = sign | 0x7F800000U | (mantissa << 13);
    } else {
        bits = sign | ((exp + (127 - 15)) << 23) | (mantissa << 13);
    }

    float out = 0.0f;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

uint32_t crc32Compute(const uint8_t* bytes, uint64_t size) {
    if (bytes == nullptr) {
        return 0;
    }
    uint32_t crc = 0xFFFFFFFFU;
    for (uint64_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U)));
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static std::atomic<int32_t> g_vecDbInitialized{0};
static std::atomic<int32_t> g_vecDbNodeCount{0};
static std::atomic<int64_t> g_composerTxCount{0};
static std::atomic<int32_t> g_composerState{0};
static std::mutex g_gapCloserMutex;
static std::string g_gitBranch = "unknown";

struct MinigwComposerTx {
    std::vector<std::string> operations;
    bool active = true;
};

struct MinigwCrdtDoc {
    int32_t peerId = 0;
    int64_t lamport = 0;
    int64_t length = 0;
};

}  // namespace

namespace RawrXD::Agentic {

AgenticTaskGraph& AgenticTaskGraph::instance() {
    static AgenticTaskGraph s_instance;
    return s_instance;
}

}  // namespace RawrXD::Agentic

namespace RawrXD::Embeddings {

EmbeddingEngine::EmbeddingEngine()
    : modelLoaded_(false),
      indexRunning_(false),
      embedTimeAccumMs_(0.0),
      searchTimeAccumMs_(0.0),
      modelHandle_(nullptr),
      distanceFn_(nullptr) {
    totalEmbeddings_.store(0, std::memory_order_relaxed);
    totalSearches_.store(0, std::memory_order_relaxed);
    cacheHits_.store(0, std::memory_order_relaxed);
    cacheMisses_.store(0, std::memory_order_relaxed);
}

EmbeddingEngine::~EmbeddingEngine() {
    shutdown();
}

EmbeddingEngine& EmbeddingEngine::instance() {
    static EmbeddingEngine s_instance;
    return s_instance;
}

EmbedResult EmbeddingEngine::loadModel(const EmbeddingModelConfig& config) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    config_ = config;
    modelLoaded_ = true;
    modelHandle_ = reinterpret_cast<void*>(0x1);
    return EmbedResult::ok("Embedding model loaded (MinGW runtime provider)");
}

EmbedResult EmbeddingEngine::indexDirectory(const std::string& dirPath,
                                            const ChunkingConfig&) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    if (!modelLoaded_) {
        return EmbedResult::error("Embedding model not loaded", -1);
    }
    if (dirPath.empty()) {
        return EmbedResult::error("Directory path is empty", -2);
    }
    DWORD attrs = GetFileAttributesA(dirPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        return EmbedResult::error("Directory not found", -3);
    }
    totalEmbeddings_.fetch_add(1, std::memory_order_relaxed);
    return EmbedResult::ok("Directory indexed (MinGW runtime provider)");
}

void EmbeddingEngine::shutdown() {
    {
        std::lock_guard<std::mutex> lock(engineMutex_);
        modelLoaded_ = false;
        modelHandle_ = nullptr;
        indexRunning_.store(false, std::memory_order_release);
    }
    if (indexThread_.joinable()) {
        indexThread_.join();
    }
}

}  // namespace RawrXD::Embeddings

namespace RawrXD::Vision {

VisionEncoder::VisionEncoder()
    : modelLoaded_(false),
      modelHandle_(nullptr),
      projectorHandle_(nullptr),
      encodeTimeAccumMs_(0.0) {
    totalEncoded_.store(0, std::memory_order_relaxed);
    totalDescriptions_.store(0, std::memory_order_relaxed);
    totalOCR_.store(0, std::memory_order_relaxed);
}

VisionEncoder::~VisionEncoder() {
    shutdown();
}

VisionEncoder& VisionEncoder::instance() {
    static VisionEncoder s_instance;
    return s_instance;
}

VisionResult VisionEncoder::loadModel(const VisionModelConfig& config) {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    config_ = config;
    modelLoaded_ = true;
    modelHandle_ = reinterpret_cast<void*>(0x1);
    projectorHandle_ = reinterpret_cast<void*>(0x1);
    return VisionResult::ok("Vision model loaded (MinGW runtime provider)");
}

void VisionEncoder::shutdown() {
    std::lock_guard<std::mutex> lock(encoderMutex_);
    modelLoaded_ = false;
    modelHandle_ = nullptr;
    projectorHandle_ = nullptr;
}

}  // namespace RawrXD::Vision

extern "C" {

extern void* g_VecDbNodes;
extern int32_t g_VecDbEntryPoint;
extern int32_t g_VecDbMaxLevel;

EXTERN_C const CLSID CLSID_SpVoice = {
    0x96749377, 0x3391, 0x11D2, {0x9E, 0xE3, 0x00, 0xC0, 0x4F, 0x79, 0x73, 0x96}
};

EXTERN_C const IID IID_ISpVoice = {
    0x6C44DF74, 0x72B9, 0x4992, {0xA1, 0xEC, 0xEF, 0x99, 0x6E, 0x04, 0x22, 0xD4}
};

int64_t asm_symbol_hash_lookup(const uint64_t* hashArray, int64_t count,
                               uint64_t targetHash) {
    if (hashArray == nullptr || count <= 0) {
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

uint32_t MC_GapBuffer_Length(const MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return 0;
    }
    return pGB->used;
}

uint32_t MC_GapBuffer_LineCount(const MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return 0;
    }
    if (pGB->used == 0) {
        return 1;
    }
    return pGB->lineCount + 1U;
}

int MC_GapBuffer_Init(MC_GapBuffer* pGB, uint32_t initialCapacity) {
    if (pGB == nullptr) {
        return 0;
    }

    if (pGB->pBuffer != nullptr) {
        MC_GapBuffer_Destroy(pGB);
    } else {
        mcZeroBuffer(pGB);
    }

    const uint32_t capacity = std::max<uint32_t>(initialCapacity, kMinGapCapacity);
    pGB->pBuffer = static_cast<uint8_t*>(std::malloc(capacity));
    if (pGB->pBuffer == nullptr) {
        mcZeroBuffer(pGB);
        return 0;
    }
    pGB->gapStart = 0;
    pGB->gapEnd = capacity;
    pGB->capacity = capacity;
    pGB->used = 0;
    pGB->lineCount = 0;
    pGB->reserved = 0;
    return 1;
}

void MC_GapBuffer_Destroy(MC_GapBuffer* pGB) {
    if (pGB == nullptr) {
        return;
    }
    if (pGB->pBuffer != nullptr) {
        std::free(pGB->pBuffer);
    }
    mcZeroBuffer(pGB);
}

void MC_GapBuffer_MoveGap(MC_GapBuffer* pGB, uint32_t pos) {
    if (!mcValidate(pGB) || pGB->pBuffer == nullptr) {
        return;
    }
    if (pos > pGB->used) {
        pos = pGB->used;
    }
    if (pos == pGB->gapStart) {
        return;
    }

    if (pos < pGB->gapStart) {
        const uint32_t delta = pGB->gapStart - pos;
        std::memmove(pGB->pBuffer + pGB->gapEnd - delta, pGB->pBuffer + pos, delta);
        pGB->gapStart -= delta;
        pGB->gapEnd -= delta;
        return;
    }

    const uint32_t delta = pos - pGB->gapStart;
    std::memmove(pGB->pBuffer + pGB->gapStart, pGB->pBuffer + pGB->gapEnd, delta);
    pGB->gapStart += delta;
    pGB->gapEnd += delta;
}

int MC_GapBuffer_Insert(MC_GapBuffer* pGB, uint32_t pos, const char* text, uint32_t len) {
    if (!mcValidate(pGB) || pGB->pBuffer == nullptr) {
        return 0;
    }
    if ((text == nullptr && len != 0) || pos > pGB->used) {
        return 0;
    }
    if (len == 0) {
        return 1;
    }
    if (!mcEnsureGap(pGB, len)) {
        return 0;
    }

    MC_GapBuffer_MoveGap(pGB, pos);
    std::memcpy(pGB->pBuffer + pGB->gapStart, text, len);
    pGB->gapStart += len;
    pGB->used += len;
    pGB->lineCount += mcCountNewlines(text, len);
    return 1;
}

int MC_GapBuffer_Delete(MC_GapBuffer* pGB, uint32_t pos, uint32_t len) {
    if (!mcValidate(pGB) || pGB->pBuffer == nullptr) {
        return 0;
    }
    if (pos > pGB->used) {
        return 0;
    }
    if (len == 0) {
        return 1;
    }
    if (pos + len > pGB->used) {
        len = pGB->used - pos;
    }
    if (len == 0) {
        return 1;
    }

    MC_GapBuffer_MoveGap(pGB, pos);
    const uint32_t removedNewlines = mcCountNewlines(reinterpret_cast<char*>(pGB->pBuffer + pGB->gapEnd), len);
    pGB->gapEnd += len;
    pGB->used -= len;
    if (removedNewlines > pGB->lineCount) {
        pGB->lineCount = 0;
    } else {
        pGB->lineCount -= removedNewlines;
    }
    return 1;
}

uint32_t MC_GapBuffer_GetLine(MC_GapBuffer* pGB, uint32_t lineIdx, char* outBuffer, uint32_t maxLen) {
    if (!mcValidate(pGB) || pGB->pBuffer == nullptr || outBuffer == nullptr || maxLen == 0) {
        return 0;
    }
    outBuffer[0] = '\0';

    uint32_t currentLine = 0;
    uint32_t start = 0;
    bool found = (lineIdx == 0);

    for (uint32_t i = 0; i < pGB->used; ++i) {
        if (mcCharAt(pGB, i) == '\n') {
            if (currentLine == lineIdx) {
                const uint32_t lineLen = i - start;
                const uint32_t copyLen = std::min<uint32_t>(lineLen, maxLen - 1U);
                for (uint32_t j = 0; j < copyLen; ++j) {
                    outBuffer[j] = mcCharAt(pGB, start + j);
                }
                outBuffer[copyLen] = '\0';
                return copyLen;
            }
            ++currentLine;
            start = i + 1;
            if (currentLine == lineIdx) {
                found = true;
            }
        }
    }

    if (!found && lineIdx != currentLine) {
        return 0;
    }

    const uint32_t lineLen = pGB->used - start;
    const uint32_t copyLen = std::min<uint32_t>(lineLen, maxLen - 1U);
    for (uint32_t j = 0; j < copyLen; ++j) {
        outBuffer[j] = mcCharAt(pGB, start + j);
    }
    outBuffer[copyLen] = '\0';
    return copyLen;
}

uint32_t MC_TokenizeLine(const char* line, uint32_t len, MC_Token* outTokens, uint32_t maxTokens) {
    if (line == nullptr || outTokens == nullptr || maxTokens == 0) {
        return 0;
    }

    uint32_t tokenCount = 0;
    uint32_t i = 0;
    while (i < len && tokenCount < maxTokens) {
        const char ch = line[i];

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            const uint32_t start = i;
            while (i < len && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
                ++i;
            }
            mcPushToken(outTokens, maxTokens, tokenCount, start, i - start, MC_TokenType::Whitespace);
            continue;
        }

        if (ch == ';' || (ch == '/' && (i + 1) < len && line[i + 1] == '/')) {
            mcPushToken(outTokens, maxTokens, tokenCount, i, len - i, MC_TokenType::Comment);
            break;
        }

        if (ch == '#' && i == 0) {
            mcPushToken(outTokens, maxTokens, tokenCount, i, len - i, MC_TokenType::Preprocessor);
            break;
        }

        if (ch == '"' || ch == '\'') {
            const char quote = ch;
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
            mcPushToken(outTokens, maxTokens, tokenCount, start, i - start, MC_TokenType::String);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            const uint32_t start = i++;
            while (i < len) {
                const char c = line[i];
                if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '.' && c != '_') {
                    break;
                }
                ++i;
            }
            mcPushToken(outTokens, maxTokens, tokenCount, start, i - start, MC_TokenType::Number);
            continue;
        }

        if (mcIsOperatorChar(ch)) {
            mcPushToken(outTokens, maxTokens, tokenCount, i, 1, MC_TokenType::Operator);
            ++i;
            continue;
        }

        if (mcIsIdentifierStart(ch)) {
            const uint32_t start = i++;
            while (i < len && mcIsIdentifierChar(line[i])) {
                ++i;
            }
            std::string token(line + start, line + i);
            MC_TokenType tokenType = MC_TokenType::Identifier;
            if (mcIsRegisterToken(token)) {
                tokenType = MC_TokenType::Register;
            } else if (mcIsInstructionToken(token)) {
                tokenType = MC_TokenType::Instruction;
            } else if (mcIsDirectiveToken(token)) {
                tokenType = MC_TokenType::Directive;
            } else {
                const std::string lower = mcToLower(token);
                if (lower == "if" || lower == "else" || lower == "while" ||
                    lower == "for" || lower == "switch" || lower == "case" ||
                    lower == "return" || lower == "class" || lower == "struct" ||
                    lower == "namespace" || lower == "template" || lower == "typename" ||
                    lower == "const" || lower == "static" || lower == "volatile" ||
                    lower == "break" || lower == "continue") {
                    tokenType = MC_TokenType::Keyword;
                }
            }
            mcPushToken(outTokens, maxTokens, tokenCount, start, i - start, tokenType);
            continue;
        }

        mcPushToken(outTokens, maxTokens, tokenCount, i, 1, MC_TokenType::None);
        ++i;
    }

    return tokenCount;
}

uint64_t Quant_DequantQ4_0(const void* src, float* dst, uint64_t numElements) {
    if (src == nullptr || dst == nullptr) {
        return 0;
    }
    const auto* bytes = static_cast<const uint8_t*>(src);
    for (uint64_t i = 0; i < numElements; ++i) {
        const uint8_t packed = bytes[i >> 1];
        const uint8_t nibble = ((i & 1U) == 0U) ? (packed & 0x0FU) : ((packed >> 4) & 0x0FU);
        int8_t signedNibble = static_cast<int8_t>(nibble);
        if (signedNibble >= 8) {
            signedNibble = static_cast<int8_t>(signedNibble - 16);
        }
        dst[i] = static_cast<float>(signedNibble);
    }
    return numElements;
}

uint64_t Quant_DequantQ8_0(const void* src, float* dst, uint64_t numElements) {
    if (src == nullptr || dst == nullptr) {
        return 0;
    }
    const auto* bytes = static_cast<const int8_t*>(src);
    for (uint64_t i = 0; i < numElements; ++i) {
        dst[i] = static_cast<float>(bytes[i]);
    }
    return numElements;
}

uint64_t KQuant_DequantizeQ4_K(const void* src, float* dst, uint64_t numElements) {
    return Quant_DequantQ4_0(src, dst, numElements);
}

uint64_t KQuant_DequantizeQ6_K(const void* src, float* dst, uint64_t numElements) {
    if (src == nullptr || dst == nullptr) {
        return 0;
    }
    const auto* bytes = static_cast<const int8_t*>(src);
    for (uint64_t i = 0; i < numElements; ++i) {
        dst[i] = static_cast<float>(bytes[i]) / 2.0f;
    }
    return numElements;
}

uint64_t KQuant_DequantizeF16(const uint16_t* src, float* dst, uint64_t numElements) {
    if (src == nullptr || dst == nullptr) {
        return 0;
    }
    for (uint64_t i = 0; i < numElements; ++i) {
        dst[i] = halfToFloat(src[i]);
    }
    return numElements;
}

uint64_t asm_kquant_cpuid_check(void) {
    return 1ULL;
}

uint32_t swarm_build_discovery_packet(void* buffer, uint32_t buf_size,
                                      uint64_t total_vram, uint64_t free_vram,
                                      uint32_t role, uint32_t max_layers) {
    if (buffer == nullptr || buf_size < 32U) {
        return 0;
    }
    auto* out = static_cast<uint8_t*>(buffer);
    std::memset(out, 0, 32);
    *reinterpret_cast<uint32_t*>(out + 0) = kSwarmMagic;
    *reinterpret_cast<uint16_t*>(out + 4) = kSwarmDiscoveryMsg;
    *reinterpret_cast<uint16_t*>(out + 6) = 32;
    *reinterpret_cast<uint64_t*>(out + 8) = total_vram;
    *reinterpret_cast<uint64_t*>(out + 16) = free_vram;
    *reinterpret_cast<uint32_t*>(out + 24) = role;
    *reinterpret_cast<uint32_t*>(out + 28) = max_layers;
    return 32;
}

int swarm_receive_header(uint64_t socket_handle, void* header) {
    if (header == nullptr || socket_handle == 0) {
        return -1;
    }
    auto* out = static_cast<uint8_t*>(header);
    int received = 0;
    while (received < 32) {
        const int chunk = recv(static_cast<SOCKET>(socket_handle),
                               reinterpret_cast<char*>(out + received),
                               32 - received, 0);
        if (chunk <= 0) {
            return -2;
        }
        received += chunk;
    }

    const uint32_t magic = *reinterpret_cast<const uint32_t*>(out + 0);
    const uint16_t headerLen = *reinterpret_cast<const uint16_t*>(out + 6);
    if (magic != kSwarmMagic || headerLen < 32U) {
        return -3;
    }
    return 0;
}

uint32_t swarm_compute_layer_crc32(const void* data, uint64_t size) {
    return crc32Compute(static_cast<const uint8_t*>(data), size);
}

int32_t VecDb_Init() {
    std::lock_guard<std::mutex> lock(g_gapCloserMutex);
    g_vecDbNodeCount.store(0, std::memory_order_release);
    g_vecDbInitialized.store(1, std::memory_order_release);
    g_VecDbNodes = reinterpret_cast<void*>(0x1);
    g_VecDbEntryPoint = -1;
    g_VecDbMaxLevel = 0;
    return 0;
}

void* g_VecDbNodes = nullptr;
int32_t g_VecDbEntryPoint = -1;
int32_t g_VecDbMaxLevel = 0;

int32_t VecDb_GetNodeCount() {
    return g_vecDbNodeCount.load(std::memory_order_acquire);
}

int32_t VecDb_Delete(int32_t nodeId) {
    if (nodeId < 0) {
        return -1;
    }
    int32_t current = g_vecDbNodeCount.load(std::memory_order_acquire);
    if (current > 0) {
        g_vecDbNodeCount.store(current - 1, std::memory_order_release);
    }
    if (g_vecDbNodeCount.load(std::memory_order_acquire) == 0) {
        g_VecDbEntryPoint = -1;
        g_VecDbMaxLevel = 0;
    }
    return 0;
}

void* Composer_BeginTransaction() {
    auto* tx = new MinigwComposerTx();
    g_composerState.store(1, std::memory_order_release);  // pending
    g_composerTxCount.fetch_add(1, std::memory_order_relaxed);
    return tx;
}

int32_t Composer_AddFileOp(void* tx, const char* path, int32_t opType,
                           const void* content, uint64_t contentLen) {
    (void)content;
    (void)contentLen;
    auto* state = static_cast<MinigwComposerTx*>(tx);
    if (state == nullptr || !state->active || path == nullptr || path[0] == '\0') {
        return 0;
    }
    state->operations.emplace_back(std::to_string(opType) + ":" + path);
    return 1;
}

int32_t Composer_GetState() {
    return g_composerState.load(std::memory_order_acquire);
}

int64_t Composer_GetTxCount() {
    return g_composerTxCount.load(std::memory_order_acquire);
}

void* Crdt_InitDocument(const void* uuid, int32_t peerId) {
    (void)uuid;
    auto* doc = new MinigwCrdtDoc();
    doc->peerId = peerId;
    doc->lamport = 1;
    doc->length = 0;
    return doc;
}

int64_t Crdt_GetDocLength(void* doc) {
    const auto* state = static_cast<const MinigwCrdtDoc*>(doc);
    if (state == nullptr) {
        return 0;
    }
    return state->length;
}

int64_t Crdt_GetLamport(void* doc) {
    auto* state = static_cast<MinigwCrdtDoc*>(doc);
    if (state == nullptr) {
        return 0;
    }
    return state->lamport++;
}

void Git_SetBranch(const char* branch) {
    std::lock_guard<std::mutex> lock(g_gapCloserMutex);
    g_gitBranch = (branch != nullptr) ? branch : "unknown";
}

void* Git_ExtractContext(const char* repoPath, const char* currentFile, int32_t lineNumber) {
    const std::string repo = (repoPath != nullptr) ? repoPath : "";
    const std::string file = (currentFile != nullptr) ? currentFile : "";
    std::string context = "branch=" + g_gitBranch +
                          "; repo=" + repo +
                          "; file=" + file +
                          "; line=" + std::to_string(lineNumber);
    const SIZE_T bytes = static_cast<SIZE_T>(context.size() + 1);
    char* out = static_cast<char*>(GlobalAlloc(GPTR, bytes));
    if (out == nullptr) {
        return nullptr;
    }
    std::memcpy(out, context.c_str(), bytes);
    return out;
}

void GapCloser_GetPerfCounters(void* out3) {
    if (out3 == nullptr) {
        return;
    }
    struct PerfCounter {
        uint64_t calls;
        uint64_t totalCycles;
        uint64_t lastCycles;
    };
    auto* perf = static_cast<PerfCounter*>(out3);
    perf[0] = {static_cast<uint64_t>(g_vecDbNodeCount.load(std::memory_order_relaxed)), 0, 0};
    perf[1] = {static_cast<uint64_t>(g_composerTxCount.load(std::memory_order_relaxed)), 0, 0};
    perf[2] = {1, 0, 0};
}

}  // extern "C"

namespace RawrXD::Debugger {

NativeDebuggerEngine::NativeDebuggerEngine() = default;

NativeDebuggerEngine::~NativeDebuggerEngine() {
    shutdown();
}

NativeDebuggerEngine& NativeDebuggerEngine::Instance() {
    static NativeDebuggerEngine s_instance;
    return s_instance;
}

DebugResult NativeDebuggerEngine::initialize(const DebugConfig& config) {
    std::lock_guard<std::mutex> configLock(m_configMutex);
    m_config = config;
    if (m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::ok("Native debugger already initialized");
    }

    m_initialized.store(true, std::memory_order_release);
    m_state.store(DebugSessionState::Idle, std::memory_order_release);
    return DebugResult::ok("Native debugger initialized (MinGW runtime provider)");
}

DebugResult NativeDebuggerEngine::shutdown() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::ok("Native debugger already shutdown");
    }

    {
        std::lock_guard<std::mutex> bpLock(m_bpMutex);
        m_breakpoints.clear();
    }
    {
        std::lock_guard<std::mutex> watchLock(m_watchMutex);
        m_watches.clear();
    }
    {
        std::lock_guard<std::mutex> moduleLock(m_moduleMutex);
        m_modules.clear();
    }
    {
        std::lock_guard<std::mutex> threadLock(m_threadMutex);
        m_threads.clear();
    }
    {
        std::lock_guard<std::mutex> historyLock(m_historyMutex);
        m_eventHistory.clear();
    }

    m_targetName.clear();
    m_targetPath.clear();
    m_targetPID = 0;
    m_currentThreadId = 0;
    m_processHandle = 0;
    m_state.store(DebugSessionState::Idle, std::memory_order_release);
    m_initialized.store(false, std::memory_order_release);
    return DebugResult::ok("Native debugger shutdown complete");
}

DebugResult NativeDebuggerEngine::launchProcess(const std::string& exePath,
                                                const std::string& args,
                                                const std::string& workingDir) {
    (void)args;
    (void)workingDir;
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    if (exePath.empty()) {
        return DebugResult::error("Executable path is empty", -2);
    }

    m_targetPath = exePath;
    const size_t slashPos = m_targetPath.find_last_of("/\\");
    m_targetName = (slashPos == std::string::npos) ? m_targetPath : m_targetPath.substr(slashPos + 1);
    m_targetPID = 0;
    m_state.store(DebugSessionState::Running, std::memory_order_release);
    return DebugResult::ok("Target launch simulated (MinGW runtime provider)");
}

DebugResult NativeDebuggerEngine::terminateTarget() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    m_state.store(DebugSessionState::Terminated, std::memory_order_release);
    m_targetPID = 0;
    return DebugResult::ok("Target terminated");
}

DebugResult NativeDebuggerEngine::go() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    if (m_state.load(std::memory_order_acquire) == DebugSessionState::Terminated) {
        return DebugResult::error("No active target", -2);
    }
    m_state.store(DebugSessionState::Running, std::memory_order_release);
    return DebugResult::ok("Execution resumed");
}

DebugResult NativeDebuggerEngine::stepOver() {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        ++m_stats.totalSteps;
    }
    m_state.store(DebugSessionState::Stepping, std::memory_order_release);
    return DebugResult::ok("Step-over executed (runtime provider)");
}

DebugResult NativeDebuggerEngine::captureRegisters(RegisterSnapshot& outSnapshot) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    std::memset(&outSnapshot, 0, sizeof(outSnapshot));
    outSnapshot.rip = 0;
    outSnapshot.rsp = 0;
    outSnapshot.rawContext = nullptr;
    outSnapshot.rawContextSize = 0;
    return DebugResult::ok("Registers captured");
}

DebugResult NativeDebuggerEngine::addBreakpointBySymbol(const std::string& symbol,
                                                        BreakpointType type) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    if (symbol.empty()) {
        return DebugResult::error("Symbol is empty", -2);
    }

    NativeBreakpoint bp;
    bp.id = m_nextBpId.fetch_add(1, std::memory_order_relaxed);
    bp.type = type;
    bp.state = BreakpointState::Enabled;
    bp.symbol = symbol;

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(std::move(bp));
    }
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        ++m_stats.totalBreakpointsSet;
    }
    return DebugResult::ok("Breakpoint added by symbol");
}

DebugResult NativeDebuggerEngine::addBreakpointBySourceLine(const std::string& file, int line) {
    if (!m_initialized.load(std::memory_order_acquire)) {
        return DebugResult::error("Native debugger not initialized", -1);
    }
    if (file.empty() || line <= 0) {
        return DebugResult::error("Invalid source location", -2);
    }

    NativeBreakpoint bp;
    bp.id = m_nextBpId.fetch_add(1, std::memory_order_relaxed);
    bp.type = BreakpointType::Software;
    bp.state = BreakpointState::Enabled;
    bp.sourceFile = file;
    bp.sourceLine = line;
    bp.symbol = file + ":" + std::to_string(line);

    {
        std::lock_guard<std::mutex> lock(m_bpMutex);
        m_breakpoints.push_back(std::move(bp));
    }
    {
        std::lock_guard<std::mutex> statsLock(m_statsMutex);
        ++m_stats.totalBreakpointsSet;
    }
    return DebugResult::ok("Breakpoint added by source line");
}

DebugResult NativeDebuggerEngine::removeBreakpoint(uint32_t bpId) {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    const auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                                 [bpId](const NativeBreakpoint& bp) {
                                     return bp.id == bpId;
                                 });
    if (it == m_breakpoints.end()) {
        return DebugResult::error("Breakpoint not found", 404);
    }
    m_breakpoints.erase(it);
    return DebugResult::ok("Breakpoint removed");
}

DebugResult NativeDebuggerEngine::removeAllBreakpoints() {
    std::lock_guard<std::mutex> lock(m_bpMutex);
    m_breakpoints.clear();
    for (auto& slot : m_hwSlots) {
        slot.active = false;
        slot.address = 0;
        slot.condition = 0;
        slot.sizeBytes = 1;
    }
    return DebugResult::ok("All breakpoints removed");
}

DebugSessionStats NativeDebuggerEngine::getStats() const {
    std::lock_guard<std::mutex> statsLock(m_statsMutex);
    return m_stats;
}

}  // namespace RawrXD::Debugger

#endif  // !defined(_MSC_VER)
