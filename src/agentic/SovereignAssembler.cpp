#include "SovereignAssembler.h"
#include <immintrin.h>
#include <intrin.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SovereignAssembler
{
// -----------------------------------------------------------------------------
// Phase-A scalar opcode tables (extensible toward VEX/EVEX without rewriting dispatch).
//
// g_ScalarAluOpcodeMap — one row per mnemonic: r/m,r opcode, r,r/m opcode, group-81 /digit.
//   EncType::AluScalar covers reg/reg, reg/mem, mem/reg via emitMemOp + ModRM (reg = src for r/m,r).
//   Immediates use 81/83 with group81Ext (same row; no duplicate map keys).
//
// kInsnEmitByMnemonic — non-ALU mnemonics (mov variants, lea, jcc via second table, etc.).
// `test` reg/mem forms live in g_ScalarAluOpcodeMap; immediates use F7 /0 (not group 81/83).
// -----------------------------------------------------------------------------
enum class EncType : uint8_t
{
    AluScalar = 0,
    /// Same ModRM patterns as AluScalar for r,r / r,mem / mem,r; immediate -> F7 /0 imm32 (not 81/83).
    TestLike = 1,
};

struct ScalarAluDesc
{
    EncType kind = EncType::AluScalar;
    /// ADD/AND/TEST/... r/m64, r64 (ModRM reg = src).
    uint8_t opc_rm_r = 0;
    /// r64, r/m64 (ModRM reg = dest in Intel "r, r/m" — encoder passes dst as reg field).
    uint8_t opc_r_rm = 0;
    /// /digit for 81/83 immediate groups (0=ADD ... 7=CMP). Ignored when kind == TestLike.
    uint8_t group81Ext = 0;
};

static const std::unordered_map<std::string, ScalarAluDesc>& g_ScalarAluOpcodeMap()
{
    static const std::unordered_map<std::string, ScalarAluDesc> kTable = {
        {"add", {EncType::AluScalar, 0x01, 0x03, 0}}, {"or", {EncType::AluScalar, 0x09, 0x0B, 1}},
        {"and", {EncType::AluScalar, 0x21, 0x23, 4}}, {"sub", {EncType::AluScalar, 0x29, 0x2B, 5}},
        {"xor", {EncType::AluScalar, 0x31, 0x33, 6}}, {"cmp", {EncType::AluScalar, 0x39, 0x3B, 7}},
        {"test", {EncType::TestLike, 0x85, 0x85, 0}},
    };
    return kTable;
}

static const ScalarAluDesc* tryLookupScalarAlu(const std::string& mnem)
{
    const auto& t = g_ScalarAluOpcodeMap();
    const auto it = t.find(mnem);
    return it == t.end() ? nullptr : &it->second;
}

static std::string sovereignAsciiLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

/// Byte after 0x0F for near jcc (rel32). 255 = mnemonic is not a conditional jump.
static uint8_t jccNearSecondByteOr255(const std::string& m)
{
    static const std::unordered_map<std::string, uint8_t> kJcc = {
        {"jo", 0x80},  {"jno", 0x81}, {"jb", 0x82},  {"jc", 0x82},   {"jnae", 0x82}, {"jnb", 0x83},
        {"jae", 0x83}, {"jnc", 0x83}, {"je", 0x84},  {"jz", 0x84},   {"jne", 0x85},  {"jnz", 0x85},
        {"jbe", 0x86}, {"jna", 0x86}, {"ja", 0x87},  {"jnbe", 0x87}, {"js", 0x88},   {"jns", 0x89},
        {"jp", 0x8A},  {"jpe", 0x8A}, {"jnp", 0x8B}, {"jpo", 0x8B},  {"jl", 0x8C},   {"jnge", 0x8C},
        {"jge", 0x8D}, {"jnl", 0x8D}, {"jle", 0x8E}, {"jng", 0x8E},  {"jg", 0x8F},   {"jnle", 0x8F},
    };
    const auto it = kJcc.find(m);
    return it == kJcc.end() ? static_cast<uint8_t>(255) : it->second;
}

// -----------------------------------------------------------------------------
// Scalar tokenizer delimiter scan (default implementation)
// -----------------------------------------------------------------------------
static const char* FindNextDelimiter_Scalar(const char* start, const char* end)
{
    while (start < end)
    {
        const char c = *start;
        if (c == ' ' || c == '\t' || c == ',' || c == '\n' || c == '\r' || c == '\0' || c == ';' || c == '+' ||
            c == '-' || c == '*' || c == '[' || c == ']' || c == ':')
        {
            return start;
        }
        ++start;
    }
    return end;
}

FindDelimiterFn g_findNextDelimiter = FindNextDelimiter_Scalar;

// -----------------------------------------------------------------------------
// SELF-EVOLUTION: AVX2-accelerated delimiter scan
// This is the "internal" version that can be swapped at runtime.
// -----------------------------------------------------------------------------
static const char* FindNextDelimiter_AVX2(const char* start, const char* end)
{
    const size_t len = static_cast<size_t>(end - start);
    if (len < 32)
    {
        return FindNextDelimiter_Scalar(start, end);
    }

    // Alignment check: if we are not 32-byte aligned, scan scalar until we are
    while (start < end && (reinterpret_cast<uintptr_t>(start) & 31) != 0)
    {
        const char c = *start;
        if (c == ' ' || c == '\t' || c == ',' || c == '\n' || c == '\r' || c == '\0' || c == ';' || c == '+' ||
            c == '-' || c == '*' || c == '[' || c == ']' || c == ':')
            return start;
        ++start;
    }

    const __m256i v_spc = _mm256_set1_epi8(' ');
    const __m256i v_tab = _mm256_set1_epi8('\t');
    const __m256i v_com = _mm256_set1_epi8(',');
    const __m256i v_nl = _mm256_set1_epi8('\n');
    const __m256i v_cr = _mm256_set1_epi8('\r');
    const __m256i v_null = _mm256_set1_epi8('\0');
    const __m256i v_sc = _mm256_set1_epi8(';');
    const __m256i v_pls = _mm256_set1_epi8('+');
    const __m256i v_mns = _mm256_set1_epi8('-');
    const __m256i v_ast = _mm256_set1_epi8('*');
    const __m256i v_lbr = _mm256_set1_epi8('[');
    const __m256i v_rbr = _mm256_set1_epi8(']');
    const __m256i v_col = _mm256_set1_epi8(':');

    while (start + 32 <= end)
    {
        __m256i v = _mm256_load_si256(reinterpret_cast<const __m256i*>(start));

        __m256i m = _mm256_cmpeq_epi8(v, v_spc);
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_tab));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_com));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_nl));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_cr));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_null));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_sc));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_pls));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_mns));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_ast));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_lbr));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_rbr));
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, v_col));

        uint32_t mask = _mm256_movemask_epi8(m);
        if (mask != 0)
        {
            unsigned long pos;
            _BitScanForward(&pos, mask);
            return start + pos;
        }
        start += 32;
    }

    return FindNextDelimiter_Scalar(start, end);
}

static bool enableTokenizerAvx2Local()
{
    // Switch to internal AVX2 tokenizer path when CPU capability is present.
    int cpuInfo[4];
    __cpuid(cpuInfo, 7);
    if ((cpuInfo[1] & (1 << 5)) != 0)
    {
        g_findNextDelimiter = FindNextDelimiter_AVX2;
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// PE checksum (Microsoft image checksum, scalar)
// -----------------------------------------------------------------------------
static uint32_t computePeChecksum(const uint8_t* data, size_t size, size_t checksumFieldOffset)
{
    std::vector<uint8_t> buf(data, data + size);
    if (checksumFieldOffset + 4 <= buf.size())
    {
        std::memset(buf.data() + checksumFieldOffset, 0, 4);
    }
    uint64_t sum = 0;
    for (size_t i = 0; i + 1 < buf.size(); i += 2)
    {
        const uint16_t word = static_cast<uint16_t>(buf[i]) | (static_cast<uint16_t>(buf[i + 1]) << 8);
        sum += word;
        sum = (sum & 0xFFFFull) + (sum >> 16);
    }
    if ((buf.size() & 1u) != 0)
    {
        sum += buf.back();
        sum = (sum & 0xFFFFull) + (sum >> 16);
    }
    while (sum >> 16)
    {
        sum = (sum & 0xFFFFull) + (sum >> 16);
    }
    return static_cast<uint32_t>(sum) + static_cast<uint32_t>(size);
}

bool VerifyPEChecksum(const std::vector<uint8_t>& peBinary)
{
    if (peBinary.size() < sizeof(IMAGE_DOS_HEADER))
    {
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(peBinary.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return false;
    }
    const size_t peOff = static_cast<size_t>(dos->e_lfanew);
    if (peOff + sizeof(IMAGE_NT_HEADERS64) > peBinary.size())
    {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(peBinary.data() + peOff);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
        return false;
    }
    const size_t checksumOff =
        peOff + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, CheckSum);
    if (checksumOff + 4 > peBinary.size())
    {
        return false;
    }
    uint32_t stored = 0;
    std::memcpy(&stored, peBinary.data() + checksumOff, sizeof(stored));
    const uint32_t computed = computePeChecksum(peBinary.data(), peBinary.size(), checksumOff);
    return stored == computed;
}

bool HotPatchTokenizer(const wchar_t* dllPath)
{
    if (!dllPath || !*dllPath)
    {
        return enableTokenizerAvx2Local();
    }

    std::ifstream ifs(dllPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        return false;
    }

    const auto sz = static_cast<size_t>(ifs.tellg());
    if (sz == 0)
    {
        return false;
    }
    std::vector<uint8_t> buffer(sz);
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(sz));
    ifs.close();

    if (!VerifyPEChecksum(buffer))
    {
        OutputDebugStringA("SovereignAssembler: HotPatchTokenizer PE checksum mismatch (continuing load).\n");
    }

    const HMODULE hMod = LoadLibraryW(dllPath);
    if (!hMod)
    {
        return enableTokenizerAvx2Local();
    }

    void* avx2Func = reinterpret_cast<void*>(GetProcAddress(hMod, "find_next_delimiter_avx2"));
    if (!avx2Func)
    {
        FreeLibrary(hMod);
        return enableTokenizerAvx2Local();
    }

    const FindDelimiterFn oldVal = g_findNextDelimiter;
    if (InterlockedExchangePointer(reinterpret_cast<PVOID*>(&g_findNextDelimiter), avx2Func) == oldVal)
    {
        OutputDebugStringA("SovereignAssembler: tokenizer hot-patched (find_next_delimiter_avx2).\n");
    }
    return true;
}

enum class Reg64 : uint8_t
{
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8 = 8,
    R9 = 9,
    R10 = 10,
    R11 = 11,
    R12 = 12,
    R13 = 13,
    R14 = 14,
    R15 = 15
};

static uint8_t rexW(bool w, uint8_t r, uint8_t x, uint8_t b)
{
    return static_cast<uint8_t>(0x40 | (w ? 8 : 0) | ((r & 1u) << 2) | ((x & 1u) << 1) | (b & 1u));
}

static uint8_t modrm(uint8_t mod, uint8_t reg, uint8_t rm)
{
    return static_cast<uint8_t>((mod << 6) | ((reg & 7u) << 3) | (rm & 7u));
}

struct Token
{
    enum Type
    {
        MNEMONIC,
        REG,
        REG8,
        IMM,
        LABEL_DEF,
        DIRECTIVE,
        COMMA,
        COLON,
        LBRACKET,
        RBRACKET,
        PLUS,
        MINUS,
        STAR,
        END
    } type = END;
    std::string text;
    uint64_t value = 0;
};

struct MemOperand
{
    uint8_t baseReg = 0xFF;
    uint8_t indexReg = 0xFF;
    uint8_t scale = 0;
    int32_t displacement = 0;
    bool hasBase = false;
    bool hasIndex = false;
    bool isMemory = false;
    /// x64 [rip+disp32] — ModRM r/m=101, no SIB. Optional label fixup (REL32); otherwise immediate disp only.
    bool ripRelative = false;
    std::string ripLabel;
};

static std::vector<Token> tokenize(const std::string& src)
{
    std::vector<Token> tokens;
    const char* data = src.c_str();
    const size_t len = src.size();
    size_t i = 0;

    while (i < len)
    {
        const char* currentPos = data + i;
        const char* nextDelim = g_findNextDelimiter(currentPos, data + len);

        if (nextDelim == currentPos)
        {
            if (static_cast<unsigned char>(data[i]) <= ' ' && data[i] != 0)
            {
                ++i;
                continue;
            }
            if (data[i] == ';')
            {
                while (i < len && data[i] != '\n')
                {
                    ++i;
                }
                continue;
            }
            switch (data[i])
            {
                case ',':
                    tokens.push_back({Token::COMMA, ","});
                    ++i;
                    continue;
                case ':':
                    tokens.push_back({Token::COLON, ":"});
                    ++i;
                    continue;
                case '[':
                    tokens.push_back({Token::LBRACKET, "["});
                    ++i;
                    continue;
                case ']':
                    tokens.push_back({Token::RBRACKET, "]"});
                    ++i;
                    continue;
                case '+':
                    tokens.push_back({Token::PLUS, "+"});
                    ++i;
                    continue;
                case '-':
                    tokens.push_back({Token::MINUS, "-"});
                    ++i;
                    continue;
                case '*':
                    tokens.push_back({Token::STAR, "*"});
                    ++i;
                    continue;
                default:
                    ++i;
                    continue;
            }
        }

        const size_t start = i;
        i = static_cast<size_t>(nextDelim - data);
        std::string word = src.substr(start, i - start);
        if (word.empty())
        {
            continue;
        }

        if (std::isalpha(static_cast<unsigned char>(word[0])) != 0 || word[0] == '_')
        {
            std::string lowerWord = word;
            std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(), ::tolower);

            Token t;
            if (lowerWord == "db" || lowerWord == "dw" || lowerWord == "dd" || lowerWord == "dq" ||
                lowerWord == "import")
            {
                t.type = Token::DIRECTIVE;
            }
            else if (lowerWord == "al" || lowerWord == "cl" || lowerWord == "dl" || lowerWord == "bl" ||
                     lowerWord == "spl" || lowerWord == "bpl" || lowerWord == "sil" || lowerWord == "dil" ||
                     lowerWord == "r8b" || lowerWord == "r9b" || lowerWord == "r10b" || lowerWord == "r11b" ||
                     lowerWord == "r12b" || lowerWord == "r13b" || lowerWord == "r14b" || lowerWord == "r15b")
            {
                t.type = Token::REG8;
            }
            else if (lowerWord == "rax" || lowerWord == "rcx" || lowerWord == "rdx" || lowerWord == "rbx" ||
                     lowerWord == "rsp" || lowerWord == "rbp" || lowerWord == "rsi" || lowerWord == "rdi" ||
                     lowerWord == "eax" || lowerWord == "ecx" || lowerWord == "edx" || lowerWord == "ebx" ||
                     lowerWord == "esp" || lowerWord == "ebp" || lowerWord == "esi" || lowerWord == "edi" ||
                     lowerWord == "r8d" || lowerWord == "r9d" || lowerWord == "r10d" || lowerWord == "r11d" ||
                     lowerWord == "r12d" || lowerWord == "r13d" || lowerWord == "r14d" || lowerWord == "r15d" ||
                     (lowerWord.size() >= 2 && lowerWord[0] == 'r' &&
                      (lowerWord == "r8" || lowerWord == "r9" || lowerWord == "r10" || lowerWord == "r11" ||
                       lowerWord == "r12" || lowerWord == "r13" || lowerWord == "r14" || lowerWord == "r15")))
            {
                t.type = Token::REG;
            }
            else
            {
                t.type = Token::MNEMONIC;
            }
            // Registers/directives stay lowercased; arbitrary identifiers keep source spelling (imports, labels).
            if (t.type == Token::DIRECTIVE || t.type == Token::REG8 || t.type == Token::REG)
            {
                t.text = std::move(lowerWord);
            }
            else
            {
                t.text = std::move(word);
            }
            tokens.push_back(std::move(t));
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(word[0])) != 0 ||
            (word[0] == '-' && word.size() > 1 && std::isdigit(static_cast<unsigned char>(word[1])) != 0))
        {
            uint64_t val = 0;
            if (!word.empty() && word.back() == 'h')
            {
                const std::string numOnly = word.substr(0, word.size() - 1);
                val = std::strtoull(numOnly.c_str(), nullptr, 16);
            }
            else
            {
                val = std::strtoull(word.c_str(), nullptr, 0);
            }
            tokens.push_back({Token::IMM, "", val});
            continue;
        }

        ++i;
    }

    tokens.push_back({Token::END});
    return tokens;
}

class Assembler
{
  public:
    AssemblyResult result;
    std::unordered_map<std::string, uint64_t> labelCodeOffsets;
    std::unordered_map<std::string, uint64_t> labelDataOffsets;
    std::vector<std::pair<size_t, std::string>> fixups;
    /// (DLL name, import name) in source order; duplicates are merged when building the PE.
    std::vector<std::pair<std::string, std::string>> importEntries;
    std::unordered_set<std::string> importNames;
    std::vector<std::pair<size_t, std::string>> importCallFixups;

    // applyPeFixups: true = patch jmp/call displacements in-place for minimal PE; false = leave zeros for COFF REL32.
    bool assemble(const std::string& source, bool applyPeFixups)
    {
        try
        {
            const std::vector<Token> tokens = tokenize(source);
            size_t pc = 0;
            while (pc < tokens.size() && tokens[pc].type != Token::END)
            {
                if (tokens[pc].type == Token::MNEMONIC && pc + 1 < tokens.size() && tokens[pc + 1].type == Token::COLON)
                {
                    std::vector<std::string> labelChain;
                    while (pc + 1 < tokens.size() && tokens[pc].type == Token::MNEMONIC &&
                           tokens[pc + 1].type == Token::COLON)
                    {
                        labelChain.push_back(sovereignAsciiLower(tokens[pc].text));
                        pc += 2;
                    }
                    const bool isData =
                        (pc < tokens.size() && tokens[pc].type == Token::DIRECTIVE && tokens[pc].text != "import");
                    for (const std::string& ln : labelChain)
                    {
                        if (isData)
                        {
                            labelDataOffsets[ln] = result.data.size();
                        }
                        else
                        {
                            labelCodeOffsets[ln] = result.code.size();
                        }
                    }
                    continue;
                }
                if (tokens[pc].type == Token::DIRECTIVE)
                {
                    if (tokens[pc].text == "import")
                    {
                        emitImportDirective(tokens, pc);
                    }
                    else
                    {
                        emitDirective(tokens, pc);
                    }
                    continue;
                }
                if (tokens[pc].type == Token::MNEMONIC)
                {
                    emitInstruction(tokens, pc);
                    continue;
                }
                ++pc;
            }
            constexpr uint32_t kPeTextRva = 0x1000;
            const uint32_t kPeDataRva = kPeTextRva + static_cast<uint32_t>((result.code.size() + 0xFFFu) & ~0xFFFu);

            for (auto& [offset, label] : fixups)
            {
                const auto itCode = labelCodeOffsets.find(label);
                const auto itData = labelDataOffsets.find(label);
                if (itCode == labelCodeOffsets.end() && itData == labelDataOffsets.end())
                {
                    result.error = "Undefined label: " + label;
                    return false;
                }
                if (applyPeFixups)
                {
                    int32_t rel = 0;
                    if (itCode != labelCodeOffsets.end() && itData != labelDataOffsets.end())
                    {
                        result.error = "Duplicate label (code+data): " + label;
                        return false;
                    }
                    if (itCode != labelCodeOffsets.end())
                    {
                        rel = static_cast<int32_t>(itCode->second - (offset + 4));
                    }
                    else
                    {
                        rel = static_cast<int32_t>((kPeDataRva + static_cast<uint32_t>(itData->second)) -
                                                   (kPeTextRva + static_cast<uint32_t>(offset) + 4u));
                    }
                    std::memcpy(result.code.data() + offset, &rel, sizeof(rel));
                }
            }
            result.success = true;
            return true;
        }
        catch (const std::exception& e)
        {
            result.error = e.what();
            return false;
        }
    }

  private:
    static Reg64 parseGpReg(const std::string& s)
    {
        static const std::unordered_map<std::string, Reg64> regMap = {
            {"rax", Reg64::RAX},  {"rcx", Reg64::RCX},  {"rdx", Reg64::RDX},  {"rbx", Reg64::RBX},
            {"rsp", Reg64::RSP},  {"rbp", Reg64::RBP},  {"rsi", Reg64::RSI},  {"rdi", Reg64::RDI},
            {"r8", Reg64::R8},    {"r9", Reg64::R9},    {"r10", Reg64::R10},  {"r11", Reg64::R11},
            {"r12", Reg64::R12},  {"r13", Reg64::R13},  {"r14", Reg64::R14},  {"r15", Reg64::R15},
            {"eax", Reg64::RAX},  {"ecx", Reg64::RCX},  {"edx", Reg64::RDX},  {"ebx", Reg64::RBX},
            {"esp", Reg64::RSP},  {"ebp", Reg64::RBP},  {"esi", Reg64::RSI},  {"edi", Reg64::RDI},
            {"r8d", Reg64::R8},   {"r9d", Reg64::R9},   {"r10d", Reg64::R10}, {"r11d", Reg64::R11},
            {"r12d", Reg64::R12}, {"r13d", Reg64::R13}, {"r14d", Reg64::R14}, {"r15d", Reg64::R15}};
        const auto it = regMap.find(s);
        if (it == regMap.end())
        {
            throw std::runtime_error("Invalid GP register (sovereign subset): " + s);
        }
        return it->second;
    }

    static uint8_t parseReg8(const std::string& s)
    {
        static const std::unordered_map<std::string, uint8_t> reg8 = {
            {"al", 0},  {"cl", 1},  {"dl", 2},    {"bl", 3},    {"spl", 4},   {"bpl", 5},   {"sil", 6},   {"dil", 7},
            {"r8b", 8}, {"r9b", 9}, {"r10b", 10}, {"r11b", 11}, {"r12b", 12}, {"r13b", 13}, {"r14b", 14}, {"r15b", 15}};
        const auto it = reg8.find(s);
        if (it == reg8.end())
        {
            throw std::runtime_error("Invalid 8-bit register (sovereign subset): " + s);
        }
        return it->second;
    }

    static void skipOptionalSizePtr(const std::vector<Token>& tokens, size_t& p)
    {
        if (p + 2 <= tokens.size() && tokens[p].type == Token::MNEMONIC)
        {
            const std::string& w = tokens[p].text;
            if (w == "byte" || w == "word" || w == "dword" || w == "qword")
            {
                if (tokens[p + 1].type == Token::MNEMONIC && tokens[p + 1].text == "ptr")
                {
                    p += 2;
                }
            }
        }
    }

    MemOperand parseMemOperand(const std::vector<Token>& tokens, size_t& p)
    {
        MemOperand mem;
        mem.isMemory = true;
        mem.baseReg = 0xFF;
        mem.indexReg = 0xFF;
        mem.scale = 0;
        mem.displacement = 0;

        if (p >= tokens.size() || tokens[p].type != Token::LBRACKET)
        {
            return mem;
        }
        ++p;

        // [rip+disp|label...] — RIP-relative addressing (x64)
        if (p < tokens.size() && tokens[p].type == Token::MNEMONIC && tokens[p].text == "rip")
        {
            mem.ripRelative = true;
            ++p;
            while (p < tokens.size() && tokens[p].type != Token::RBRACKET)
            {
                if (tokens[p].type == Token::PLUS)
                {
                    ++p;
                    continue;
                }
                if (tokens[p].type == Token::IMM)
                {
                    mem.displacement += static_cast<int32_t>(tokens[p].value);
                    ++p;
                    continue;
                }
                if (tokens[p].type == Token::MNEMONIC)
                {
                    if (!mem.ripLabel.empty())
                    {
                        throw std::runtime_error("[rip+...]: only one label displacement allowed");
                    }
                    mem.ripLabel = tokens[p].text;
                    ++p;
                    continue;
                }
                if (tokens[p].type == Token::MINUS)
                {
                    ++p;
                    if (p < tokens.size() && tokens[p].type == Token::IMM)
                    {
                        mem.displacement -= static_cast<int32_t>(tokens[p].value);
                        ++p;
                        continue;
                    }
                }
                throw std::runtime_error("[rip+...]: expected imm or label");
            }
            if (p < tokens.size() && tokens[p].type == Token::RBRACKET)
            {
                ++p;
            }
            return mem;
        }

        // [symbol] — treat as [rip+symbol] (common MASM x64 style for globals)
        if (p + 1 < tokens.size() && tokens[p].type == Token::MNEMONIC && tokens[p + 1].type == Token::RBRACKET)
        {
            mem.ripRelative = true;
            mem.ripLabel = tokens[p].text;
            mem.displacement = 0;
            p += 2;
            return mem;
        }

        while (p < tokens.size() && tokens[p].type != Token::RBRACKET)
        {
            if (tokens[p].type == Token::REG)
            {
                const uint8_t idx = static_cast<uint8_t>(parseGpReg(tokens[p].text));
                if (!mem.hasBase)
                {
                    mem.baseReg = idx;
                    mem.hasBase = true;
                }
                else if (!mem.hasIndex)
                {
                    mem.indexReg = idx;
                    mem.hasIndex = true;
                }
                ++p;
            }
            else if (tokens[p].type == Token::IMM)
            {
                mem.displacement += static_cast<int32_t>(tokens[p].value);
                ++p;
            }
            else if (tokens[p].type == Token::PLUS)
            {
                ++p;
            }
            else if (tokens[p].type == Token::MINUS)
            {
                ++p;
                if (p < tokens.size() && tokens[p].type == Token::IMM)
                {
                    mem.displacement -= static_cast<int32_t>(tokens[p].value);
                    ++p;
                }
            }
            else if (tokens[p].type == Token::STAR)
            {
                ++p;
                if (p < tokens.size() && tokens[p].type == Token::IMM)
                {
                    const uint64_t s = tokens[p].value;
                    if (s == 1)
                    {
                        mem.scale = 0;
                    }
                    else if (s == 2)
                    {
                        mem.scale = 1;
                    }
                    else if (s == 4)
                    {
                        mem.scale = 2;
                    }
                    else if (s == 8)
                    {
                        mem.scale = 3;
                    }
                    ++p;
                }
            }
            else if (tokens[p].type == Token::MNEMONIC)
            {
                throw std::runtime_error("unexpected identifier in [...] (use [rip+name] for RIP-relative)");
            }
            else
            {
                ++p;
            }
        }
        if (p < tokens.size() && tokens[p].type == Token::RBRACKET)
        {
            ++p;
        }
        return mem;
    }

    static void emitModRM(uint8_t mod, uint8_t reg, uint8_t rm, std::vector<uint8_t>& code)
    {
        code.push_back(static_cast<uint8_t>((mod << 6) | ((reg & 7u) << 3) | (rm & 7u)));
    }

    static void emitSIB(uint8_t scale, uint8_t index, uint8_t base, std::vector<uint8_t>& code)
    {
        code.push_back(static_cast<uint8_t>((scale << 6) | ((index & 7u) << 3) | (base & 7u)));
    }

    void emitMemOp(uint8_t opcode, uint8_t reg, const MemOperand& mem, std::vector<uint8_t>& code)
    {
        if (mem.ripRelative)
        {
            code.push_back(rexW(true, reg >> 3u, 0, 0));
            code.push_back(opcode);
            emitModRM(0, reg, 5, code);
            const size_t dispPos = code.size();
            code.resize(dispPos + 4, 0);
            if (!mem.ripLabel.empty())
            {
                fixups.emplace_back(dispPos, sovereignAsciiLower(mem.ripLabel));
            }
            else
            {
                const int32_t d = mem.displacement;
                std::memcpy(code.data() + dispPos, &d, sizeof(d));
            }
            return;
        }

        // x64: [rbp] and [r13] cannot use mod=00 + base only — force disp8 0.
        int32_t disp = mem.displacement;
        uint8_t mod = 0;
        int dispSize = 0;
        const bool rbpLike = mem.hasBase && !mem.hasIndex && ((mem.baseReg & 7u) == 5);
        if (rbpLike && disp == 0)
        {
            mod = 1;
            dispSize = 1;
        }
        else if (disp == 0 && (mem.baseReg & 7u) != 5 && (mem.baseReg & 7u) != 13)
        {
            mod = 0;
            dispSize = 0;
        }
        else if (disp >= -128 && disp <= 127)
        {
            mod = 1;
            dispSize = 1;
        }
        else
        {
            mod = 2;
            dispSize = 4;
        }

        const uint8_t ix = mem.hasIndex ? mem.indexReg : 0;
        const uint8_t bx = mem.hasBase ? mem.baseReg : 0;
        code.push_back(rexW(true, reg >> 3u, ix >> 3u, bx >> 3u));
        code.push_back(opcode);

        if (!mem.hasIndex && (mem.baseReg & 7u) != 4)
        {
            emitModRM(mod, reg, mem.baseReg, code);
        }
        else
        {
            emitModRM(mod, reg, 4, code);
            const uint8_t base = mem.hasBase ? mem.baseReg : 5;
            const uint8_t index = mem.hasIndex ? mem.indexReg : 4;
            emitSIB(mem.scale, index, base, code);
            if (!mem.hasBase && mod == 0)
            {
                dispSize = 4;
            }
        }

        if (dispSize == 1)
        {
            code.push_back(static_cast<uint8_t>(disp));
        }
        else if (dispSize == 4)
        {
            for (int i = 0; i < 4; ++i)
            {
                code.push_back(static_cast<uint8_t>(static_cast<uint32_t>(disp) >> (i * 8)));
            }
        }
    }

    void emitMemOpEscaped(uint8_t opcodeEsc, uint8_t reg, const MemOperand& mem, std::vector<uint8_t>& code)
    {
        if (mem.ripRelative)
        {
            code.push_back(rexW(true, reg >> 3u, 0, 0));
            code.push_back(0x0F);
            code.push_back(opcodeEsc);
            emitModRM(0, reg, 5, code);
            const size_t dispPos = code.size();
            code.resize(dispPos + 4, 0);
            if (!mem.ripLabel.empty())
            {
                fixups.emplace_back(dispPos, sovereignAsciiLower(mem.ripLabel));
            }
            else
            {
                const int32_t d = mem.displacement;
                std::memcpy(code.data() + dispPos, &d, sizeof(d));
            }
            return;
        }

        int32_t disp = mem.displacement;
        uint8_t mod = 0;
        int dispSize = 0;
        const bool rbpLike = mem.hasBase && !mem.hasIndex && ((mem.baseReg & 7u) == 5);
        if (rbpLike && disp == 0)
        {
            mod = 1;
            dispSize = 1;
        }
        else if (disp == 0 && (mem.baseReg & 7u) != 5 && (mem.baseReg & 7u) != 13)
        {
            mod = 0;
            dispSize = 0;
        }
        else if (disp >= -128 && disp <= 127)
        {
            mod = 1;
            dispSize = 1;
        }
        else
        {
            mod = 2;
            dispSize = 4;
        }

        const uint8_t ix = mem.hasIndex ? mem.indexReg : 0;
        const uint8_t bx = mem.hasBase ? mem.baseReg : 0;
        code.push_back(rexW(true, reg >> 3u, ix >> 3u, bx >> 3u));
        code.push_back(0x0F);
        code.push_back(opcodeEsc);

        if (!mem.hasIndex && (mem.baseReg & 7u) != 4)
        {
            emitModRM(mod, reg, mem.baseReg, code);
        }
        else
        {
            emitModRM(mod, reg, 4, code);
            const uint8_t base = mem.hasBase ? mem.baseReg : 5;
            const uint8_t index = mem.hasIndex ? mem.indexReg : 4;
            emitSIB(mem.scale, index, base, code);
            if (!mem.hasBase && mod == 0)
            {
                dispSize = 4;
            }
        }

        if (dispSize == 1)
        {
            code.push_back(static_cast<uint8_t>(disp));
        }
        else if (dispSize == 4)
        {
            for (int i = 0; i < 4; ++i)
            {
                code.push_back(static_cast<uint8_t>(static_cast<uint32_t>(disp) >> (i * 8)));
            }
        }
    }

    void emitGroup81(uint8_t groupExt, Reg64 rmReg, uint32_t imm32, std::vector<uint8_t>& code)
    {
        code.push_back(rexW(true, 0, 0, static_cast<uint8_t>(rmReg) >> 3));
        code.push_back(0x81);
        code.push_back(modrm(3, groupExt, static_cast<uint8_t>(rmReg) & 7));
        for (int i = 0; i < 4; ++i)
        {
            code.push_back(static_cast<uint8_t>(imm32 >> (i * 8)));
        }
    }

    void emitGroup83(uint8_t groupExt, Reg64 rmReg, int8_t imm8, std::vector<uint8_t>& code)
    {
        code.push_back(rexW(true, 0, 0, static_cast<uint8_t>(rmReg) >> 3));
        code.push_back(0x83);
        code.push_back(modrm(3, groupExt, static_cast<uint8_t>(rmReg) & 7));
        code.push_back(static_cast<uint8_t>(imm8));
    }

    static void emitGroupF7TestImm(Reg64 rmReg, uint32_t imm32, std::vector<uint8_t>& code)
    {
        code.push_back(rexW(true, 0, 0, static_cast<uint8_t>(rmReg) >> 3));
        code.push_back(0xF7);
        code.push_back(modrm(3, 0, static_cast<uint8_t>(rmReg) & 7));
        for (int i = 0; i < 4; ++i)
        {
            code.push_back(static_cast<uint8_t>(imm32 >> (i * 8)));
        }
    }

    static void emitAluRegReg(const ScalarAluDesc& d, Reg64 dst, Reg64 src, std::vector<uint8_t>& code)
    {
        code.push_back(rexW(true, static_cast<uint8_t>(src) >> 3, 0, static_cast<uint8_t>(dst) >> 3));
        code.push_back(d.opc_rm_r);
        code.push_back(modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7));
    }

    void emitAluRegMem(const ScalarAluDesc& d, Reg64 dst, const MemOperand& srcMem)
    {
        emitMemOp(d.opc_r_rm, static_cast<uint8_t>(dst), srcMem, result.code);
    }

    void emitAluMemReg(const ScalarAluDesc& d, const MemOperand& dstMem, Reg64 src)
    {
        emitMemOp(d.opc_rm_r, static_cast<uint8_t>(src), dstMem, result.code);
    }

    void emitAluInstruction(const ScalarAluDesc& d, const std::string& mnem, const std::vector<Token>& tokens,
                            size_t& p)
    {
        skipOptionalSizePtr(tokens, p);
        if (p >= tokens.size())
        {
            throw std::runtime_error(mnem + ": missing operands");
        }
        if (tokens[p].type == Token::LBRACKET)
        {
            MemOperand dstMem = parseMemOperand(tokens, p);
            if (p >= tokens.size() || tokens[p].type != Token::COMMA)
            {
                throw std::runtime_error(mnem + " [mem], ...");
            }
            ++p;
            skipOptionalSizePtr(tokens, p);
            if (p >= tokens.size() || tokens[p].type != Token::REG)
            {
                throw std::runtime_error(mnem + " [mem], reg expected");
            }
            const Reg64 src = parseGpReg(tokens[p].text);
            ++p;
            emitAluMemReg(d, dstMem, src);
            return;
        }
        if (tokens[p].type != Token::REG)
        {
            throw std::runtime_error(mnem + ": expected register or [mem] destination");
        }
        const Reg64 dst = parseGpReg(tokens[p].text);
        ++p;
        if (p >= tokens.size() || tokens[p].type != Token::COMMA)
        {
            throw std::runtime_error(mnem + ": expected comma");
        }
        ++p;
        skipOptionalSizePtr(tokens, p);
        if (p >= tokens.size())
        {
            throw std::runtime_error(mnem + ": missing source");
        }
        if (tokens[p].type == Token::IMM)
        {
            const uint32_t imm = static_cast<uint32_t>(tokens[p].value);
            ++p;
            if (d.kind == EncType::TestLike)
            {
                emitGroupF7TestImm(dst, imm, result.code);
                return;
            }
            const int32_t simm = static_cast<int32_t>(imm);
            if (simm >= -128 && simm <= 127)
            {
                emitGroup83(d.group81Ext, dst, static_cast<int8_t>(simm), result.code);
            }
            else
            {
                emitGroup81(d.group81Ext, dst, imm, result.code);
            }
            return;
        }
        if (tokens[p].type == Token::REG)
        {
            const Reg64 src = parseGpReg(tokens[p].text);
            ++p;
            emitAluRegReg(d, dst, src, result.code);
            return;
        }
        if (tokens[p].type == Token::LBRACKET)
        {
            MemOperand srcMem = parseMemOperand(tokens, p);
            emitAluRegMem(d, dst, srcMem);
            return;
        }
        throw std::runtime_error(mnem + ": unsupported source operand");
    }

    void emitMovx(const std::string& mnem, const std::vector<Token>& tokens, size_t& p)
    {
        const bool isZx = (mnem == "movzx");
        if (p >= tokens.size() || tokens[p].type != Token::REG)
        {
            throw std::runtime_error(mnem + ": dest must be 64-bit GP");
        }
        const Reg64 dst = parseGpReg(tokens[p].text);
        ++p;
        if (p >= tokens.size() || tokens[p].type != Token::COMMA)
        {
            throw std::runtime_error(mnem + ": expected comma");
        }
        ++p;

        uint8_t opEsc = isZx ? 0xB6 : 0xBE;
        if (p + 2 <= tokens.size() && tokens[p].type == Token::MNEMONIC && tokens[p].text == "word" &&
            tokens[p + 1].type == Token::MNEMONIC && tokens[p + 1].text == "ptr")
        {
            p += 2;
            opEsc = isZx ? 0xB7 : 0xBF;
        }
        else
        {
            skipOptionalSizePtr(tokens, p);
        }

        if (p >= tokens.size())
        {
            throw std::runtime_error(mnem + ": missing source");
        }
        if (tokens[p].type == Token::REG8)
        {
            if (opEsc == 0xB7 || opEsc == 0xBF)
            {
                throw std::runtime_error(mnem + ": word ptr not valid with 8-bit register");
            }
            const uint8_t src8 = parseReg8(tokens[p].text);
            ++p;
            result.code.push_back(rexW(true, static_cast<uint8_t>(dst) >> 3, 0, src8 >> 3));
            result.code.push_back(0x0F);
            result.code.push_back(opEsc);
            result.code.push_back(modrm(3, static_cast<uint8_t>(dst) & 7, src8 & 7));
            return;
        }
        if (tokens[p].type == Token::LBRACKET)
        {
            MemOperand srcMem = parseMemOperand(tokens, p);
            emitMemOpEscaped(opEsc, static_cast<uint8_t>(dst), srcMem, result.code);
            return;
        }
        throw std::runtime_error(mnem + ": expected r8 or [mem]");
    }

    void emitLeaInstruction(const std::vector<Token>& tokens, size_t& p)
    {
        skipOptionalSizePtr(tokens, p);
        if (p >= tokens.size() || tokens[p].type != Token::REG)
        {
            throw std::runtime_error("lea: expected destination register");
        }
        const Reg64 dst = parseGpReg(tokens[p].text);
        ++p;
        if (p >= tokens.size() || tokens[p].type != Token::COMMA)
        {
            throw std::runtime_error("lea: expected comma");
        }
        ++p;
        skipOptionalSizePtr(tokens, p);
        if (p >= tokens.size() || tokens[p].type != Token::LBRACKET)
        {
            throw std::runtime_error("lea: expected memory operand");
        }
        MemOperand mem = parseMemOperand(tokens, p);
        emitMemOp(0x8D, static_cast<uint8_t>(dst), mem, result.code);
    }

    void emitMovsxdInstruction(const std::vector<Token>& tokens, size_t& p)
    {
        skipOptionalSizePtr(tokens, p);
        if (p >= tokens.size() || tokens[p].type != Token::REG)
        {
            throw std::runtime_error("movsxd: expected 64-bit destination");
        }
        const Reg64 dst = parseGpReg(tokens[p].text);
        ++p;
        if (p >= tokens.size() || tokens[p].type != Token::COMMA)
        {
            throw std::runtime_error("movsxd: expected comma");
        }
        ++p;
        skipOptionalSizePtr(tokens, p);
        if (p >= tokens.size())
        {
            throw std::runtime_error("movsxd: missing source");
        }
        if (tokens[p].type == Token::LBRACKET)
        {
            MemOperand srcMem = parseMemOperand(tokens, p);
            emitMemOp(0x63, static_cast<uint8_t>(dst), srcMem, result.code);
            return;
        }
        if (tokens[p].type == Token::REG)
        {
            const Reg64 src = parseGpReg(tokens[p].text);
            ++p;
            result.code.push_back(rexW(true, static_cast<uint8_t>(dst) >> 3, 0, static_cast<uint8_t>(src) >> 3));
            result.code.push_back(0x63);
            result.code.push_back(modrm(3, static_cast<uint8_t>(dst) & 7, static_cast<uint8_t>(src) & 7));
            return;
        }
        throw std::runtime_error("movsxd: expected r32 or [mem]");
    }

    void emitImportDirective(const std::vector<Token>& tokens, size_t& p)
    {
        ++p;
        if (p >= tokens.size())
        {
            throw std::runtime_error("import: missing DLL name");
        }
        std::string dll;
        if (tokens[p].type == Token::MNEMONIC || tokens[p].type == Token::DIRECTIVE)
        {
            dll = tokens[p].text;
        }
        else
        {
            throw std::runtime_error("import: expected DLL name");
        }
        ++p;
        while (p < tokens.size())
        {
            if (tokens[p].type == Token::COMMA)
            {
                ++p;
                continue;
            }
            if (tokens[p].type == Token::DIRECTIVE && tokens[p].text == "import")
            {
                break;
            }
            if (tokens[p].type == Token::END)
            {
                break;
            }
            // `label:` begins code/data; do not treat the label as an imported symbol.
            if (tokens[p].type == Token::MNEMONIC && p + 1 < tokens.size() && tokens[p + 1].type == Token::COLON)
            {
                break;
            }
            if (tokens[p].type != Token::MNEMONIC)
            {
                throw std::runtime_error("import: expected function name");
            }
            const std::string& fn = tokens[p].text;
            importEntries.push_back({dll, fn});
            importNames.insert(sovereignAsciiLower(fn));
            ++p;
        }
    }

    void emitDirective(const std::vector<Token>& tokens, size_t& p)
    {
        const std::string& dir = tokens[p].text;
        ++p;
        while (p < tokens.size() && tokens[p].type != Token::END)
        {
            if (tokens[p].type == Token::IMM)
            {
                const uint64_t val = tokens[p].value;
                if (dir == "db")
                {
                    result.data.push_back(static_cast<uint8_t>(val));
                }
                else if (dir == "dw")
                {
                    result.data.push_back(static_cast<uint8_t>(val));
                    result.data.push_back(static_cast<uint8_t>(val >> 8));
                }
                else if (dir == "dd")
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        result.data.push_back(static_cast<uint8_t>(val >> (i * 8)));
                    }
                }
                else if (dir == "dq")
                {
                    for (int i = 0; i < 8; ++i)
                    {
                        result.data.push_back(static_cast<uint8_t>(val >> (i * 8)));
                    }
                }
                ++p;
            }
            else if (tokens[p].type == Token::COMMA)
            {
                ++p;
            }
            else
            {
                break;
            }
        }
    }

    using InsnEmitFn = void (Assembler::*)(const std::vector<Token>&, size_t&);

    void emitInsnRet(const std::vector<Token>&, size_t&) { result.code.push_back(0xC3); }

    void emitInsnNop(const std::vector<Token>&, size_t&) { result.code.push_back(0x90); }

    void emitInsnSyscall(const std::vector<Token>&, size_t&)
    {
        result.code.push_back(0x0F);
        result.code.push_back(0x05);
    }

    void emitInsnPush(const std::vector<Token>& tokens, size_t& p)
    {
        if (p >= tokens.size() || tokens[p].type != Token::REG)
        {
            throw std::runtime_error("push expects a GP register");
        }
        const Reg64 reg = parseGpReg(tokens[p].text);
        ++p;
        constexpr uint8_t op = 0x50;
        if (static_cast<uint8_t>(reg) < 8)
        {
            result.code.push_back(static_cast<uint8_t>(op + static_cast<uint8_t>(reg)));
        }
        else
        {
            result.code.push_back(0x41);
            result.code.push_back(static_cast<uint8_t>(op + (static_cast<uint8_t>(reg) - 8)));
        }
    }

    void emitInsnPop(const std::vector<Token>& tokens, size_t& p)
    {
        if (p >= tokens.size() || tokens[p].type != Token::REG)
        {
            throw std::runtime_error("pop expects a GP register");
        }
        const Reg64 reg = parseGpReg(tokens[p].text);
        ++p;
        constexpr uint8_t op = 0x58;
        if (static_cast<uint8_t>(reg) < 8)
        {
            result.code.push_back(static_cast<uint8_t>(op + static_cast<uint8_t>(reg)));
        }
        else
        {
            result.code.push_back(0x41);
            result.code.push_back(static_cast<uint8_t>(op + (static_cast<uint8_t>(reg) - 8)));
        }
    }

    void emitInsnMov(const std::vector<Token>& tokens, size_t& p)
    {
        if (p >= tokens.size())
        {
            throw std::runtime_error("mov: missing operands");
        }

        if (tokens[p].type == Token::REG)
        {
            const Reg64 dst = parseGpReg(tokens[p].text);
            ++p;
            if (p >= tokens.size() || tokens[p].type != Token::COMMA)
            {
                throw std::runtime_error("mov: expected comma");
            }
            ++p;
            if (p >= tokens.size())
            {
                throw std::runtime_error("mov: missing source");
            }

            if (tokens[p].type == Token::IMM)
            {
                const uint64_t val = tokens[p].value;
                ++p;
                result.code.push_back(rexW(true, 0, 0, static_cast<uint8_t>(dst) >> 3));
                result.code.push_back(static_cast<uint8_t>(0xB8 + (static_cast<uint8_t>(dst) & 7)));
                for (int i = 0; i < 8; ++i)
                {
                    result.code.push_back(static_cast<uint8_t>(val >> (i * 8)));
                }
                return;
            }
            if (tokens[p].type == Token::REG)
            {
                const Reg64 src = parseGpReg(tokens[p].text);
                ++p;
                result.code.push_back(rexW(true, static_cast<uint8_t>(src) >> 3, 0, static_cast<uint8_t>(dst) >> 3));
                result.code.push_back(0x89);
                result.code.push_back(modrm(3, static_cast<uint8_t>(src) & 7, static_cast<uint8_t>(dst) & 7));
                return;
            }
            if (tokens[p].type == Token::LBRACKET)
            {
                MemOperand srcMem = parseMemOperand(tokens, p);
                emitMemOp(0x8B, static_cast<uint8_t>(dst), srcMem, result.code);
                return;
            }
            throw std::runtime_error("mov: unsupported source operand");
        }

        if (tokens[p].type == Token::LBRACKET)
        {
            MemOperand dstMem = parseMemOperand(tokens, p);
            if (p >= tokens.size() || tokens[p].type != Token::COMMA)
            {
                throw std::runtime_error("mov [mem], ... expected comma");
            }
            ++p;
            if (p >= tokens.size() || tokens[p].type != Token::REG)
            {
                throw std::runtime_error("mov [mem], reg expected register");
            }
            const Reg64 src = parseGpReg(tokens[p].text);
            ++p;
            emitMemOp(0x89, static_cast<uint8_t>(src), dstMem, result.code);
            return;
        }

        throw std::runtime_error("mov: unsupported destination");
    }

    void emitInsnMovzx(const std::vector<Token>& tokens, size_t& p) { emitMovx("movzx", tokens, p); }

    void emitInsnMovsx(const std::vector<Token>& tokens, size_t& p) { emitMovx("movsx", tokens, p); }

    void emitInsnLea(const std::vector<Token>& tokens, size_t& p) { emitLeaInstruction(tokens, p); }

    void emitInsnMovsxd(const std::vector<Token>& tokens, size_t& p) { emitMovsxdInstruction(tokens, p); }

    void emitNearJmpOrCall(bool isCall, const std::vector<Token>& tokens, size_t& p)
    {
        result.code.push_back(isCall ? 0xE8 : 0xE9);
        const size_t fixupPos = result.code.size();
        result.code.resize(result.code.size() + 4, 0);
        if (p < tokens.size() && (tokens[p].type == Token::MNEMONIC || tokens[p].type == Token::IMM))
        {
            if (tokens[p].type == Token::IMM)
            {
                const int32_t rel = static_cast<int32_t>(tokens[p].value);
                std::memcpy(result.code.data() + fixupPos, &rel, sizeof(rel));
                ++p;
            }
            else
            {
                fixups.emplace_back(fixupPos, sovereignAsciiLower(tokens[p].text));
                ++p;
            }
        }
        else
        {
            throw std::runtime_error("jmp/call: need label or immediate");
        }
    }

    void emitInsnJmp(const std::vector<Token>& tokens, size_t& p) { emitNearJmpOrCall(false, tokens, p); }

    void emitInsnCall(const std::vector<Token>& tokens, size_t& p) { emitNearJmpOrCall(true, tokens, p); }

    void emitNearJcc(uint8_t secondByte, const std::vector<Token>& tokens, size_t& p)
    {
        result.code.push_back(0x0F);
        result.code.push_back(secondByte);
        const size_t fixupPos = result.code.size();
        result.code.resize(fixupPos + 4, 0);
        if (p < tokens.size() && (tokens[p].type == Token::MNEMONIC || tokens[p].type == Token::IMM))
        {
            if (tokens[p].type == Token::IMM)
            {
                const int32_t rel = static_cast<int32_t>(tokens[p].value);
                std::memcpy(result.code.data() + fixupPos, &rel, sizeof(rel));
                ++p;
            }
            else
            {
                fixups.emplace_back(fixupPos, sovereignAsciiLower(tokens[p].text));
                ++p;
            }
        }
        else
        {
            throw std::runtime_error("jcc: need label or immediate displacement");
        }
    }

    void emitInstruction(const std::vector<Token>& tokens, size_t& p)
    {
        const std::string mnemRaw = tokens[p].text;
        ++p;
        const std::string mnemKey = sovereignAsciiLower(mnemRaw);

        if (mnemKey == "call" && p < tokens.size() && tokens[p].type == Token::MNEMONIC &&
            importNames.count(sovereignAsciiLower(tokens[p].text)) != 0)
        {
            const std::string impKey = sovereignAsciiLower(tokens[p].text);
            ++p;
            result.code.push_back(0xFF);
            result.code.push_back(0x15);
            const size_t dispOff = result.code.size();
            result.code.resize(dispOff + 4, 0);
            importCallFixups.emplace_back(dispOff, impKey);
            return;
        }

        if (const ScalarAluDesc* aluDef = tryLookupScalarAlu(mnemKey))
        {
            emitAluInstruction(*aluDef, mnemKey, tokens, p);
            return;
        }

        const uint8_t jccB = jccNearSecondByteOr255(mnemKey);
        if (jccB != 255)
        {
            emitNearJcc(jccB, tokens, p);
            return;
        }

        static const std::unordered_map<std::string, InsnEmitFn> kInsnEmitByMnemonic = {
            {"ret", &Assembler::emitInsnRet},         {"nop", &Assembler::emitInsnNop},
            {"syscall", &Assembler::emitInsnSyscall}, {"push", &Assembler::emitInsnPush},
            {"pop", &Assembler::emitInsnPop},         {"mov", &Assembler::emitInsnMov},
            {"movzx", &Assembler::emitInsnMovzx},     {"movsx", &Assembler::emitInsnMovsx},
            {"lea", &Assembler::emitInsnLea},         {"movsxd", &Assembler::emitInsnMovsxd},
            {"jmp", &Assembler::emitInsnJmp},         {"call", &Assembler::emitInsnCall},
        };

        const auto it = kInsnEmitByMnemonic.find(mnemKey);
        if (it != kInsnEmitByMnemonic.end())
        {
            (this->*(it->second))(tokens, p);
            return;
        }

        throw std::runtime_error("Unsupported mnemonic (sovereign subset): " + mnemRaw);
    }
};

namespace
{
void appendU16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

void appendU32(std::vector<uint8_t>& b, uint32_t v)
{
    for (int i = 0; i < 4; ++i)
    {
        b.push_back(static_cast<uint8_t>((v >> (i * 8)) & 0xFF));
    }
}

void appendSymbol18(std::vector<uint8_t>& b, const char name8[8], uint32_t value, int16_t sectionNumber, uint16_t type,
                    uint8_t storageClass, uint8_t numAux)
{
    for (int i = 0; i < 8; ++i)
    {
        b.push_back(static_cast<uint8_t>(name8[i]));
    }
    appendU32(b, value);
    appendU16(b, static_cast<uint16_t>(sectionNumber));
    appendU16(b, type);
    b.push_back(storageClass);
    b.push_back(numAux);
}

void appendSymbol18Long(std::vector<uint8_t>& b, uint32_t strTableOffset, uint32_t value, int16_t sectionNumber,
                        uint16_t type, uint8_t storageClass, uint8_t numAux)
{
    appendU32(b, 0);
    appendU32(b, strTableOffset);
    appendU32(b, value);
    appendU16(b, static_cast<uint16_t>(sectionNumber));
    appendU16(b, type);
    b.push_back(storageClass);
    b.push_back(numAux);
}

// IMAGE_AUX_SYMBOL Section (18 bytes): Length, NumberOfRelocations, NumberOfLinenumbers, Checksum, Number, Selection,
// ...
void appendAuxSectionDef(std::vector<uint8_t>& b, uint32_t length, uint16_t numReloc, uint32_t checksum,
                         uint16_t oneBasedSectionIndex)
{
    appendU32(b, length);
    appendU16(b, numReloc);
    appendU16(b, 0);  // linenumbers
    appendU32(b, checksum);
    appendU16(b, oneBasedSectionIndex);
    b.push_back(0);   // Selection (non-COMDAT)
    b.push_back(0);   // unused
    appendU16(b, 0);  // padding / NumberHighPart in some toolchains
}

// COFF relocation record is 10 bytes (fixed layout; avoids struct padding surprises).
void appendRelocEntry(std::vector<uint8_t>& b, uint32_t virtualAddress, uint32_t symbolIndex, uint16_t type)
{
    appendU32(b, virtualAddress);
    appendU32(b, symbolIndex);
    appendU16(b, type);
}

static bool writeCOFF(const std::vector<uint8_t>& text, const std::vector<uint8_t>& data,
                      const std::vector<std::pair<size_t, std::string>>& fixups,
                      const std::unordered_map<std::string, uint64_t>& labelText,
                      const std::unordered_map<std::string, uint64_t>& labelData, const std::wstring& outPath,
                      std::string& error)
{
    constexpr uint16_t kRelAmd64Rel32 = 4;  // IMAGE_REL_AMD64_REL32

    for (const auto& kv : labelText)
    {
        if (labelData.find(kv.first) != labelData.end())
        {
            error = "COFF: label defined in both .text and .data: " + kv.first;
            return false;
        }
    }

    std::vector<std::string> sortedLabels;
    sortedLabels.reserve(labelText.size() + labelData.size());
    for (const auto& kv : labelText)
    {
        sortedLabels.push_back(kv.first);
    }
    for (const auto& kv : labelData)
    {
        sortedLabels.push_back(kv.first);
    }
    std::sort(sortedLabels.begin(), sortedLabels.end());
    sortedLabels.erase(std::unique(sortedLabels.begin(), sortedLabels.end()), sortedLabels.end());

    std::unordered_map<std::string, uint32_t> labelSymIndex;
    uint32_t nextSym = 4;  // 0..3 reserved for .text + aux + .data + aux
    for (const std::string& name : sortedLabels)
    {
        labelSymIndex[name] = nextSym++;
    }

    // Long-name string table (4-byte size prefix + NUL-terminated strings).
    uint32_t strTableSize = 4;
    std::unordered_map<std::string, uint32_t> longNameOffset;
    for (const std::string& name : sortedLabels)
    {
        if (name.size() > 8)
        {
            longNameOffset[name] = strTableSize;
            strTableSize += static_cast<uint32_t>(name.size() + 1);
        }
    }

    const auto alignUp = [](size_t v, size_t a) -> size_t { return (v + a - 1) & ~(a - 1); };

    const size_t textRawSize = alignUp(text.size(), 4);
    const size_t dataRawSize = alignUp(data.size(), 4);

    const size_t hdrAndSect = sizeof(IMAGE_FILE_HEADER) + 2u * sizeof(IMAGE_SECTION_HEADER);
    const size_t textRawOff = alignUp(hdrAndSect, 4);
    const size_t dataRawOff = textRawOff + textRawSize;
    const size_t textRelocOff = dataRawOff + dataRawSize;
    constexpr size_t kRelocEntryBytes = 10;
    const size_t textRelocBytes = fixups.size() * kRelocEntryBytes;
    const size_t symTableOff = textRelocOff + textRelocBytes;

    const uint32_t numSymbols =
        4u + static_cast<uint32_t>(sortedLabels.size());  // section×2 + label symbols (no aux on labels)

    std::vector<uint8_t> sym;
    // .text section symbol + auxiliary
    {
        char n[8] = {'.', 't', 'e', 'x', 't', 0, 0, 0};
        appendSymbol18(sym, n, 0, 1, IMAGE_SYM_TYPE_NULL, IMAGE_SYM_CLASS_STATIC, 1);
        appendAuxSectionDef(sym, static_cast<uint32_t>(text.size()), static_cast<uint16_t>(fixups.size()), 0, 1);
    }
    // .data section symbol + auxiliary
    {
        char n[8] = {'.', 'd', 'a', 't', 'a', 0, 0, 0};
        appendSymbol18(sym, n, 0, 2, IMAGE_SYM_TYPE_NULL, IMAGE_SYM_CLASS_STATIC, 1);
        appendAuxSectionDef(sym, static_cast<uint32_t>(data.size()), 0, 0, 2);
    }
    for (const std::string& name : sortedLabels)
    {
        const auto itT = labelText.find(name);
        const auto itD = labelData.find(name);
        int16_t sectNum = 0;
        uint64_t val = 0;
        if (itT != labelText.end())
        {
            sectNum = 1;
            val = itT->second;
        }
        else if (itD != labelData.end())
        {
            sectNum = 2;
            val = itD->second;
        }
        else
        {
            error = "COFF internal: missing label section for " + name;
            return false;
        }
        if (name.size() <= 8)
        {
            char n[8] = {};
            std::memcpy(n, name.c_str(), name.size());
            appendSymbol18(sym, n, static_cast<uint32_t>(val), sectNum, IMAGE_SYM_TYPE_NULL, IMAGE_SYM_CLASS_EXTERNAL,
                           0);
        }
        else
        {
            appendSymbol18Long(sym, longNameOffset.at(name), static_cast<uint32_t>(val), sectNum, IMAGE_SYM_TYPE_NULL,
                               IMAGE_SYM_CLASS_EXTERNAL, 0);
        }
    }

    if (sym.size() != static_cast<size_t>(numSymbols) * 18u)
    {
        error = "COFF internal: symbol table size mismatch";
        return false;
    }

    std::vector<uint8_t> strTab;
    strTab.resize(strTableSize);
    std::memcpy(strTab.data(), &strTableSize, 4);
    for (const auto& kv : longNameOffset)
    {
        if (kv.first.size() > 8)
        {
            std::memcpy(strTab.data() + kv.second, kv.first.c_str(), kv.first.size() + 1);
        }
    }

    std::vector<uint8_t> file;
    file.reserve(symTableOff + sym.size() + strTab.size() + text.size() + data.size() + textRelocBytes + 256);

    IMAGE_FILE_HEADER fh = {};
    fh.Machine = IMAGE_FILE_MACHINE_AMD64;
    fh.NumberOfSections = 2;
    fh.TimeDateStamp = 0;
    fh.PointerToSymbolTable = static_cast<DWORD>(symTableOff);
    fh.NumberOfSymbols = numSymbols;
    fh.SizeOfOptionalHeader = 0;
    fh.Characteristics = 0;
    const uint8_t* fhBytes = reinterpret_cast<const uint8_t*>(&fh);
    file.insert(file.end(), fhBytes, fhBytes + sizeof(fh));

    IMAGE_SECTION_HEADER shText = {};
    std::memcpy(shText.Name, ".text", 5);
    shText.Misc.VirtualSize = static_cast<DWORD>(text.size());
    shText.VirtualAddress = 0;
    shText.SizeOfRawData = static_cast<DWORD>(textRawSize);
    shText.PointerToRawData = static_cast<DWORD>(textRawOff);
    shText.PointerToRelocations = static_cast<DWORD>(textRelocOff);
    shText.PointerToLinenumbers = 0;
    shText.NumberOfRelocations = static_cast<WORD>(fixups.size());
    shText.NumberOfLinenumbers = 0;
    shText.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ | IMAGE_SCN_ALIGN_16BYTES;

    IMAGE_SECTION_HEADER shData = {};
    std::memcpy(shData.Name, ".data", 5);
    shData.Misc.VirtualSize = static_cast<DWORD>(data.size());
    shData.VirtualAddress = 0;
    shData.SizeOfRawData = static_cast<DWORD>(dataRawSize);
    shData.PointerToRawData = data.empty() ? 0 : static_cast<DWORD>(dataRawOff);
    shData.PointerToRelocations = 0;
    shData.PointerToLinenumbers = 0;
    shData.NumberOfRelocations = 0;
    shData.NumberOfLinenumbers = 0;
    shData.Characteristics =
        IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_ALIGN_16BYTES;

    const uint8_t* st = reinterpret_cast<const uint8_t*>(&shText);
    file.insert(file.end(), st, st + sizeof(shText));
    const uint8_t* sd = reinterpret_cast<const uint8_t*>(&shData);
    file.insert(file.end(), sd, sd + sizeof(shData));

    while (file.size() < textRawOff)
    {
        file.push_back(0);
    }
    file.insert(file.end(), text.begin(), text.end());
    while (file.size() < textRawOff + textRawSize)
    {
        file.push_back(0);
    }
    if (!data.empty())
    {
        while (file.size() < dataRawOff)
        {
            file.push_back(0);
        }
        file.insert(file.end(), data.begin(), data.end());
    }
    while (file.size() < dataRawOff + dataRawSize)
    {
        file.push_back(0);
    }

    while (file.size() < textRelocOff)
    {
        file.push_back(0);
    }
    for (const auto& fu : fixups)
    {
        const auto itSym = labelSymIndex.find(fu.second);
        if (itSym == labelSymIndex.end())
        {
            error = "COFF internal: missing symbol for label " + fu.second;
            return false;
        }
        appendRelocEntry(file, static_cast<uint32_t>(fu.first), itSym->second, kRelAmd64Rel32);
    }

    while (file.size() < symTableOff)
    {
        file.push_back(0);
    }
    file.insert(file.end(), sym.begin(), sym.end());
    file.insert(file.end(), strTab.begin(), strTab.end());

    const HANDLE hFile =
        CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        error = "CreateFileW failed (COFF)";
        return false;
    }
    DWORD written = 0;
    WriteFile(hFile, file.data(), static_cast<DWORD>(file.size()), &written, nullptr);
    CloseHandle(hFile);
    if (written != file.size())
    {
        error = "WriteFile incomplete (COFF)";
        return false;
    }
    return true;
}
}  // namespace

/// Merge `import dll, a, b` lines into one descriptor per DLL (stable order: first mention wins).
static std::vector<std::pair<std::string, std::vector<std::string>>> groupImportEntries(
    const std::vector<std::pair<std::string, std::string>>& flat)
{
    std::vector<std::pair<std::string, std::vector<std::string>>> out;
    std::unordered_map<std::string, size_t> dllIndex;
    for (const auto& pr : flat)
    {
        const std::string& dll = pr.first;
        const std::string& fn = pr.second;
        const auto it = dllIndex.find(dll);
        if (it == dllIndex.end())
        {
            dllIndex[dll] = out.size();
            out.push_back({dll, std::vector<std::string>{fn}});
        }
        else
        {
            std::vector<std::string>& vec = out[it->second].second;
            if (std::find(vec.begin(), vec.end(), fn) == vec.end())
            {
                vec.push_back(fn);
            }
        }
    }
    return out;
}

/// Builds one `.idata` blob: import descriptors at RVA `idataRva`, ILT/IAT, hint/name, DLL names.
/// Fills `fnToIatSlotRva` with the RVA of each import's IAT slot (used by `call` fixups).
static bool buildIdataSection(uint32_t idataRva,
                              const std::vector<std::pair<std::string, std::vector<std::string>>>& dllGroups,
                              std::vector<uint8_t>& outBlob, std::unordered_map<std::string, uint32_t>& fnToIatSlotRva,
                              uint32_t& importDescriptorTotalSize)
{
    fnToIatSlotRva.clear();
    outBlob.clear();
    importDescriptorTotalSize = 0;
    if (dllGroups.empty())
    {
        return true;
    }

    const size_t nDll = dllGroups.size();
    const size_t descBytes = (nDll + 1u) * sizeof(IMAGE_IMPORT_DESCRIPTOR);
    outBlob.assign(descBytes, 0);

    size_t cursor = descBytes;
    auto align2 = [](size_t x) -> size_t { return (x + 1u) & ~size_t(1); };
    auto align8 = [](size_t x) -> size_t { return (x + 7u) & ~size_t(7); };

    struct PerDll
    {
        std::vector<uint32_t> hintNameRva;
        uint32_t dllNameRva = 0;
        uint32_t intTableRva = 0;
        uint32_t iatTableRva = 0;
    };
    std::vector<PerDll> per(nDll);

    for (size_t di = 0; di < nDll; ++di)
    {
        const auto& fns = dllGroups[di].second;
        per[di].hintNameRva.reserve(fns.size());
        for (const auto& fn : fns)
        {
            cursor = align2(cursor);
            const size_t hintOff = cursor;
            const size_t inameSize = 2u + fn.size() + 1u;
            outBlob.resize(cursor + inameSize);
            uint16_t hint = 0;
            std::memcpy(outBlob.data() + cursor, &hint, sizeof(hint));
            std::memcpy(outBlob.data() + cursor + 2, fn.c_str(), fn.size() + 1);
            per[di].hintNameRva.push_back(static_cast<uint32_t>(idataRva + hintOff));
            cursor = outBlob.size();
        }
        cursor = align2(cursor);
        const size_t dllNameOff = cursor;
        const std::string& dllNm = dllGroups[di].first;
        outBlob.resize(cursor + dllNm.size() + 1u);
        std::memcpy(outBlob.data() + cursor, dllNm.c_str(), dllNm.size() + 1);
        per[di].dllNameRva = static_cast<uint32_t>(idataRva + dllNameOff);
        cursor = outBlob.size();
    }

    for (size_t di = 0; di < nDll; ++di)
    {
        const auto& fns = dllGroups[di].second;
        const size_t n = fns.size();
        cursor = align8(cursor);
        const size_t intOff = cursor;
        outBlob.resize(cursor + 8u * (n + 1u));
        for (size_t i = 0; i < n; ++i)
        {
            const uint64_t q = per[di].hintNameRva[i];
            std::memcpy(outBlob.data() + intOff + i * 8u, &q, sizeof(q));
        }
        const uint64_t zero = 0;
        std::memcpy(outBlob.data() + intOff + n * 8u, &zero, sizeof(zero));
        per[di].intTableRva = static_cast<uint32_t>(idataRva + intOff);
        cursor = outBlob.size();

        cursor = align8(cursor);
        const size_t iatOff = cursor;
        outBlob.resize(cursor + 8u * (n + 1u));
        for (size_t i = 0; i < n; ++i)
        {
            const uint64_t q = per[di].hintNameRva[i];
            std::memcpy(outBlob.data() + iatOff + i * 8u, &q, sizeof(q));
        }
        std::memcpy(outBlob.data() + iatOff + n * 8u, &zero, sizeof(zero));
        per[di].iatTableRva = static_cast<uint32_t>(idataRva + iatOff);
        cursor = outBlob.size();

        for (size_t i = 0; i < n; ++i)
        {
            const uint32_t slotRva = static_cast<uint32_t>(idataRva + iatOff + i * 8u);
            fnToIatSlotRva[sovereignAsciiLower(fns[i])] = slotRva;
        }
    }

    for (size_t di = 0; di < nDll; ++di)
    {
        IMAGE_IMPORT_DESCRIPTOR d = {};
        d.OriginalFirstThunk = per[di].intTableRva;
        d.Name = per[di].dllNameRva;
        d.FirstThunk = per[di].iatTableRva;
        std::memcpy(outBlob.data() + di * sizeof(IMAGE_IMPORT_DESCRIPTOR), &d, sizeof(d));
    }

    importDescriptorTotalSize = static_cast<uint32_t>(descBytes);
    return true;
}

static bool writePE(std::vector<uint8_t> text, const std::vector<uint8_t>& data, uint64_t entryRVA,
                    const std::wstring& outPath, const std::vector<std::pair<size_t, std::string>>& importCallFixups,
                    const std::vector<std::pair<std::string, std::string>>& importEntriesFlat, std::string& error)
{
    const DWORD sectAlign = 0x1000;
    const DWORD fileAlign = 0x200;
    const DWORD textVA = 0x1000;

    // Empty sections consume no virtual/file space; rounding 0 up to one alignment unit breaks PE layout
    // (e.g. SizeOfRawData != 0 with PointerToRawData == 0 -> ERROR_BAD_EXE_FORMAT).
    auto alignSect = [sectAlign](size_t sz) -> DWORD
    {
        if (sz == 0)
        {
            return 0;
        }
        return static_cast<DWORD>((sz + static_cast<size_t>(sectAlign) - 1u) & ~(static_cast<size_t>(sectAlign) - 1u));
    };
    auto alignFile = [fileAlign](size_t sz) -> DWORD
    {
        if (sz == 0)
        {
            return 0;
        }
        return static_cast<DWORD>((sz + static_cast<size_t>(fileAlign) - 1u) & ~(static_cast<size_t>(fileAlign) - 1u));
    };

    const std::vector<std::pair<std::string, std::vector<std::string>>> dllGroups =
        groupImportEntries(importEntriesFlat);

    if (!importCallFixups.empty() && dllGroups.empty())
    {
        error = "import call fixups require at least one `import` directive";
        return false;
    }

    std::vector<uint8_t> idataBlob;
    std::unordered_map<std::string, uint32_t> fnToIatSlotRva;
    uint32_t importDescSize = 0;
    DWORD idataVA = 0;
    const DWORD textVirt = alignSect(text.size());
    const DWORD dataVA = textVA + textVirt;
    const bool hasDataSection = !data.empty();
    const bool hasIdata = !dllGroups.empty();

    if (hasIdata)
    {
        // With no `.data` section, `.idata` starts at `dataVA` (first VA after aligned `.text`).
        // If `.data` exists, `.idata` follows the aligned `.data` image.
        idataVA = hasDataSection ? (dataVA + alignSect(data.size())) : dataVA;
        if (!buildIdataSection(idataVA, dllGroups, idataBlob, fnToIatSlotRva, importDescSize))
        {
            error = "buildIdataSection failed";
            return false;
        }
    }

    for (const auto& ic : importCallFixups)
    {
        const auto it = fnToIatSlotRva.find(ic.second);
        if (it == fnToIatSlotRva.end())
        {
            error = "Unresolved import call: " + ic.second;
            return false;
        }
        if (ic.first + 4u > text.size())
        {
            error = "import fixup out of range";
            return false;
        }
        const uint32_t insnEndRva = textVA + static_cast<uint32_t>(ic.first) + 4u;
        const int32_t rel = static_cast<int32_t>(it->second - insnEndRva);
        std::memcpy(text.data() + ic.first, &rel, sizeof(rel));
    }

    const DWORD textRawSize = alignFile(text.size());
    const DWORD dataRawSize = alignFile(data.size());
    const DWORD idataRawSize = hasIdata ? alignFile(idataBlob.size()) : 0;

    DWORD sizeOfImage = 0;
    if (hasIdata)
    {
        const DWORD idataVirt = alignSect(idataBlob.size());
        sizeOfImage = idataVA + idataVirt;
    }
    else
    {
        const DWORD lastVA = hasDataSection ? (dataVA + alignSect(data.size())) : (textVA + textVirt);
        sizeOfImage = (lastVA + sectAlign - 1u) & ~(sectAlign - 1u);
    }

    IMAGE_DOS_HEADER dos = {};
    dos.e_magic = IMAGE_DOS_SIGNATURE;
    dos.e_lfanew = sizeof(IMAGE_DOS_HEADER);

    IMAGE_NT_HEADERS64 nt = {};
    nt.Signature = IMAGE_NT_SIGNATURE;
    nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt.FileHeader.NumberOfSections = static_cast<WORD>(1u + (hasDataSection ? 1u : 0u) + (hasIdata ? 1u : 0u));
    nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
    nt.OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt.OptionalHeader.MajorOperatingSystemVersion = 6;
    nt.OptionalHeader.MinorOperatingSystemVersion = 0;
    nt.OptionalHeader.MajorImageVersion = 0;
    nt.OptionalHeader.MinorImageVersion = 0;
    nt.OptionalHeader.MajorSubsystemVersion = 6;
    nt.OptionalHeader.MinorSubsystemVersion = 0;
    nt.OptionalHeader.AddressOfEntryPoint = static_cast<DWORD>(entryRVA);
    nt.OptionalHeader.ImageBase = 0x140000000ull;
    nt.OptionalHeader.SectionAlignment = sectAlign;
    nt.OptionalHeader.FileAlignment = fileAlign;
    nt.OptionalHeader.SizeOfImage = sizeOfImage;
    nt.OptionalHeader.SizeOfHeaders = 0x400;
    // Loader rejects images with zero stack reserve (ERROR_BAD_EXE_FORMAT).
    nt.OptionalHeader.SizeOfStackReserve = 0x100000;
    nt.OptionalHeader.SizeOfStackCommit = 0x1000;
    nt.OptionalHeader.SizeOfHeapReserve = 0x100000;
    nt.OptionalHeader.SizeOfHeapCommit = 0x1000;
    nt.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
    nt.OptionalHeader.DllCharacteristics = IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA |
                                           IMAGE_DLLCHARACTERISTICS_NX_COMPAT | IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE;
    nt.OptionalHeader.CheckSum = 0;
    nt.OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    if (hasIdata)
    {
        nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = idataVA;
        nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = importDescSize;
    }

    IMAGE_SECTION_HEADER textSect = {};
    std::memcpy(textSect.Name, ".text", 5);
    textSect.Misc.VirtualSize = static_cast<DWORD>(text.size());
    textSect.VirtualAddress = textVA;
    textSect.SizeOfRawData = textRawSize;
    textSect.PointerToRawData = 0x400;
    textSect.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

    IMAGE_SECTION_HEADER dataSect = {};
    std::memcpy(dataSect.Name, ".data", 5);
    dataSect.Misc.VirtualSize = static_cast<DWORD>(data.size());
    dataSect.VirtualAddress = dataVA;
    dataSect.SizeOfRawData = dataRawSize;
    dataSect.PointerToRawData = data.empty() ? 0 : 0x400 + textRawSize;
    dataSect.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

    IMAGE_SECTION_HEADER idataSect = {};
    std::memcpy(idataSect.Name, ".idata", 6);
    idataSect.Misc.VirtualSize = static_cast<DWORD>(idataBlob.size());
    idataSect.VirtualAddress = idataVA;
    idataSect.SizeOfRawData = idataRawSize;
    idataSect.PointerToRawData = hasIdata ? (0x400 + textRawSize + dataRawSize) : 0;
    idataSect.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

    const HANDLE hFile =
        CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        error = "CreateFileW failed";
        return false;
    }

    DWORD written = 0;
    WriteFile(hFile, &dos, sizeof(dos), &written, nullptr);
    WriteFile(hFile, &nt, sizeof(nt), &written, nullptr);
    WriteFile(hFile, &textSect, sizeof(textSect), &written, nullptr);
    if (hasDataSection)
    {
        WriteFile(hFile, &dataSect, sizeof(dataSect), &written, nullptr);
    }
    if (hasIdata)
    {
        WriteFile(hFile, &idataSect, sizeof(idataSect), &written, nullptr);
    }

    auto writeSectionPadded = [&](const uint8_t* dataPtr, DWORD dataLen, DWORD rawSize)
    {
        if (dataLen > 0)
        {
            WriteFile(hFile, dataPtr, dataLen, &written, nullptr);
        }
        if (rawSize > dataLen)
        {
            const DWORD pad = rawSize - dataLen;
            std::vector<uint8_t> zeros(pad, 0);
            WriteFile(hFile, zeros.data(), pad, &written, nullptr);
        }
    };

    SetFilePointer(hFile, 0x400, nullptr, FILE_BEGIN);
    writeSectionPadded(text.data(), static_cast<DWORD>(text.size()), textRawSize);
    if (hasDataSection)
    {
        SetFilePointer(hFile, static_cast<LONG>(0x400 + textRawSize), nullptr, FILE_BEGIN);
        writeSectionPadded(data.data(), static_cast<DWORD>(data.size()), dataRawSize);
    }
    if (hasIdata)
    {
        SetFilePointer(hFile, static_cast<LONG>(0x400 + textRawSize + dataRawSize), nullptr, FILE_BEGIN);
        writeSectionPadded(idataBlob.data(), static_cast<DWORD>(idataBlob.size()), idataRawSize);
    }
    CloseHandle(hFile);

    std::ifstream in(outPath, std::ios::binary | std::ios::ate);
    if (!in.is_open())
    {
        error = "reopen for checksum failed";
        return false;
    }
    const auto fsz = static_cast<size_t>(in.tellg());
    std::vector<uint8_t> buf(fsz);
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(fsz));
    in.close();

    const auto* dosh = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf.data());
    const size_t peOff = static_cast<size_t>(dosh->e_lfanew);
    const size_t checksumOff =
        peOff + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + offsetof(IMAGE_OPTIONAL_HEADER64, CheckSum);
    const uint32_t csum = computePeChecksum(buf.data(), buf.size(), checksumOff);

    const HANDLE hPatch =
        CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hPatch == INVALID_HANDLE_VALUE)
    {
        error = "patch checksum open failed";
        return false;
    }
    SetFilePointer(hPatch, static_cast<LONG>(checksumOff), nullptr, FILE_BEGIN);
    WriteFile(hPatch, &csum, sizeof(csum), &written, nullptr);
    CloseHandle(hPatch);
    return true;
}

bool AssembleToBuffer(const std::string& source, AssemblyResult& out, std::string& errorMsg)
{
    Assembler asmbl;
    if (!asmbl.assemble(source, true))
    {
        errorMsg = asmbl.result.error;
        out = std::move(asmbl.result);
        return false;
    }
    out = std::move(asmbl.result);
    return true;
}

bool AssembleAndLink(const std::string& source, const std::wstring& outputExePath, std::string& errorMsg)
{
    Assembler asmbl;
    if (!asmbl.assemble(source, true))
    {
        errorMsg = asmbl.result.error;
        return false;
    }
    return writePE(std::move(asmbl.result.code), asmbl.result.data, 0x1000, outputExePath, asmbl.importCallFixups,
                   asmbl.importEntries, errorMsg);
}

bool AssembleToCOFF(const std::string& source, const std::wstring& outputObjPath, std::string& errorMsg)
{
    Assembler asmbl;
    if (!asmbl.assemble(source, false))
    {
        errorMsg = asmbl.result.error;
        return false;
    }
    return writeCOFF(asmbl.result.code, asmbl.result.data, asmbl.fixups, asmbl.labelCodeOffsets, asmbl.labelDataOffsets,
                     outputObjPath, errorMsg);
}

extern "C" __declspec(dllexport) int AssembleSovereign(const char* source, const wchar_t* outputPath, char* errorBuf,
                                                       size_t errorBufSize)
{
    std::string err;
    if (!AssembleAndLink(source ? source : "", outputPath ? outputPath : L"", err))
    {
        if (errorBuf && errorBufSize > 0)
        {
            strncpy_s(errorBuf, errorBufSize, err.c_str(), errorBufSize - 1);
        }
        return 1;
    }
    return 0;
}

extern "C" __declspec(dllexport) int AssembleSovereignCOFF(const char* source, const wchar_t* outputObjPath,
                                                           char* errorBuf, size_t errorBufSize)
{
    std::string err;
    if (!AssembleToCOFF(source ? source : "", outputObjPath ? outputObjPath : L"", err))
    {
        if (errorBuf && errorBufSize > 0)
        {
            strncpy_s(errorBuf, errorBufSize, err.c_str(), errorBufSize - 1);
        }
        return 1;
    }
    return 0;
}

}  // namespace SovereignAssembler
