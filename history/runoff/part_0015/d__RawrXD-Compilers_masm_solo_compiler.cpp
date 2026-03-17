// masm_solo_compiler.cpp - Production-ish x64 MASM/NASM Solo Compiler
// Output: PE32+ executables (no external dependencies beyond Win32 headers)
// Build:  cl.exe /EHsc /O2 /nologo /Fe:masm_solo_compiler.exe masm_solo_compiler.cpp
// Usage:  masm_solo_compiler.exe <input.asm> <output.exe>

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace {

constexpr uint32_t kImageBase = 0x0000000140000000u;
constexpr uint32_t kSectionAlign = 0x1000;
constexpr uint32_t kFileAlign = 0x200;
constexpr uint32_t kHeadersSize = 0x400;

static uint32_t alignUp(uint32_t v, uint32_t a) {
    return (v + (a - 1)) & ~(a - 1);
}

static std::string toLower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::string trim(std::string s) {
    auto isWs = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && isWs((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isWs((unsigned char)s.back())) s.pop_back();
    return s;
}

static bool startsWithI(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); i++) {
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) return false;
    }
    return true;
}

static bool isIdentStart(char c) {
    return (c == '_' || c == '.' || isalpha((unsigned char)c));
}

static bool isIdentChar(char c) {
    return (c == '_' || c == '.' || isalnum((unsigned char)c));
}

static bool parseStringLiteral(const std::string& s, std::string& out) {
    std::string t = trim(s);
    if (t.size() < 2) return false;
    if ((t.front() != '"' || t.back() != '"') && (t.front() != '\'' || t.back() != '\'')) return false;
    out.clear();
    for (size_t i = 1; i + 1 < t.size(); i++) {
        char c = t[i];
        if (c == '\\' && i + 1 < t.size() - 1) {
            char n = t[++i];
            switch (n) {
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case '\\': out.push_back('\\'); break;
                case '\'': out.push_back('\''); break;
                case '"': out.push_back('"'); break;
                default: out.push_back(n); break;
            }
        } else {
            out.push_back(c);
        }
    }
    return true;
}

static bool isNumberToken(const std::string& s) {
    std::string t = trim(s);
    if (t.empty()) return false;
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) return true;
    if (isdigit((unsigned char)t[0])) return true;
    if (t.size() > 1 && t.back() == 'h') return true; // MASM hex
    return false;
}

static uint64_t parseNumberToken(const std::string& s, bool* ok = nullptr) {
    std::string t = trim(s);
    if (ok) *ok = false;
    if (t.empty()) return 0;

    int base = 10;
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        base = 16;
        t = t.substr(2);
    } else if (t.back() == 'h') {
        base = 16;
        t.pop_back();
    }

    char* endp = nullptr;
    uint64_t v = strtoull(t.c_str(), &endp, base);
    if (endp && *endp == 0) {
        if (ok) *ok = true;
        return v;
    }
    return 0;
}

enum class SectionKind { Text, Data };

struct AsmLine {
    int line = 0;
    std::string opcode;              // lowercased, empty if not instruction/directive
    std::vector<std::string> ops;    // raw operand strings (trimmed)
    bool isLabel = false;
    std::string label;
    bool isSection = false;
    SectionKind section = SectionKind::Text;
    bool isProcLabel = false;        // "name PROC" treated as label
    bool isEndp = false;
};

struct Fixup {
    enum class Kind { Rel32, RipDisp32 };
    Kind kind;
    SectionKind section;
    uint32_t atOffset;          // offset within section where disp32 begins
    uint32_t nextRip;           // RVA of next instruction (for rel calc)
    std::string target;
    int line;
};

struct EncodeError {
    int line;
    std::string msg;
};

static int regIndex64(std::string r) {
    r = toLower(trim(r));

    if (r == "rax" || r == "eax") return 0;
    if (r == "rcx" || r == "ecx") return 1;
    if (r == "rdx" || r == "edx") return 2;
    if (r == "rbx" || r == "ebx") return 3;
    if (r == "rsp" || r == "esp") return 4;
    if (r == "rbp" || r == "ebp") return 5;
    if (r == "rsi" || r == "esi") return 6;
    if (r == "rdi" || r == "edi") return 7;
    if (r == "r8" || r == "r8d") return 8;
    if (r == "r9" || r == "r9d") return 9;
    if (r == "r10" || r == "r10d") return 10;
    if (r == "r11" || r == "r11d") return 11;
    if (r == "r12" || r == "r12d") return 12;
    if (r == "r13" || r == "r13d") return 13;
    if (r == "r14" || r == "r14d") return 14;
    if (r == "r15" || r == "r15d") return 15;
    return -1;
}

static void emitU8(std::vector<uint8_t>& out, uint8_t b) { out.push_back(b); }
static void emitU16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
}
static void emitU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v & 0xFF));
    out.push_back((uint8_t)((v >> 8) & 0xFF));
    out.push_back((uint8_t)((v >> 16) & 0xFF));
    out.push_back((uint8_t)((v >> 24) & 0xFF));
}
static void emitU64(std::vector<uint8_t>& out, uint64_t v) {
    emitU32(out, (uint32_t)(v & 0xFFFFFFFFu));
    emitU32(out, (uint32_t)((v >> 32) & 0xFFFFFFFFu));
}

static uint8_t rex(bool w, int reg, int rm) {
    uint8_t r = 0x40;
    if (w) r |= 0x08;
    if (reg >= 8) r |= 0x04;
    if (rm >= 8) r |= 0x01;
    return r;
}

static uint8_t modrm(int mod, int reg, int rm) {
    return (uint8_t)((mod << 6) | ((reg & 7) << 3) | (rm & 7));
}

static bool parseMemLabel(const std::string& op, std::string& labelOut) {
    // Accept: [label] or [rel label] or [rip+label] (very loose)
    std::string t = toLower(trim(op));
    if (t.size() < 3 || t.front() != '[' || t.back() != ']') return false;
    t = trim(t.substr(1, t.size() - 2));

    if (startsWithI(t, "rel ")) t = trim(t.substr(4));
    if (startsWithI(t, "rip+")) t = trim(t.substr(4));
    if (startsWithI(t, "rip +")) t = trim(t.substr(5));

    // Strip optional +0 / -0 etc not supported
    if (t.empty()) return false;

    // Only allow plain identifier as label for now
    if (!isIdentStart(t[0])) return false;
    for (char c : t) {
        if (!isIdentChar(c)) return false;
    }

    labelOut = t;
    return true;
}

static std::vector<std::string> splitOperands(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    bool inStr = false;
    char strQuote = 0;

    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (!inStr && (c == '"' || c == '\'')) {
            inStr = true;
            strQuote = c;
            cur.push_back(c);
            continue;
        }
        if (inStr) {
            cur.push_back(c);
            if (c == '\\' && i + 1 < s.size()) {
                cur.push_back(s[++i]);
                continue;
            }
            if (c == strQuote) {
                inStr = false;
                strQuote = 0;
            }
            continue;
        }
        if (c == ',') {
            out.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

static bool parseLineToAsm(const std::string& raw, int lineNo, AsmLine& out) {
    out = {};
    out.line = lineNo;

    std::string s = raw;
    // strip comments ';'
    if (auto semi = s.find(';'); semi != std::string::npos) s = s.substr(0, semi);
    s = trim(s);
    if (s.empty()) return false;

    // MASM: .CODE / .DATA
    if (startsWithI(s, ".code")) {
        out.isSection = true;
        out.section = SectionKind::Text;
        return true;
    }
    if (startsWithI(s, ".data")) {
        out.isSection = true;
        out.section = SectionKind::Data;
        return true;
    }

    // NASM: section .text / section .data
    if (startsWithI(s, "section")) {
        auto rest = trim(s.substr(7));
        rest = trim(rest);
        out.isSection = true;
        out.section = (startsWithI(rest, ".data") ? SectionKind::Data : SectionKind::Text);
        return true;
    }

    // ignore global / extrn / end
    if (startsWithI(s, "global") || startsWithI(s, "extern") || startsWithI(s, "extrn") || startsWithI(s, "end ") || toLower(s) == "end") {
        return true;
    }

    // MASM proc label: name PROC
    {
        std::string low = toLower(s);
        auto posProc = low.find(" proc");
        if (posProc != std::string::npos) {
            std::string name = trim(s.substr(0, posProc));
            if (!name.empty() && isIdentStart(name[0])) {
                out.isLabel = true;
                out.isProcLabel = true;
                out.label = name;
                return true;
            }
        }
        auto posEndp = low.find(" endp");
        if (posEndp != std::string::npos) {
            out.isEndp = true;
            return true;
        }
    }

    // label:
    if (auto colon = s.find(':'); colon != std::string::npos) {
        std::string name = trim(s.substr(0, colon));
        if (!name.empty() && isIdentStart(name[0])) {
            out.isLabel = true;
            out.label = name;
            // If there's text after label, try parse instruction too
            std::string rest = trim(s.substr(colon + 1));
            if (rest.empty()) return true;
            s = rest;
        }
    }

    // instruction/directive
    // mnemonic = first identifier-like token
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    size_t start = i;
    while (i < s.size() && !isspace((unsigned char)s[i])) i++;
    if (i == start) return true;

    std::string mnemonic = s.substr(start, i - start);
    std::string rest = trim(s.substr(i));

    out.opcode = toLower(mnemonic);
    if (!rest.empty()) out.ops = splitOperands(rest);
    return true;
}

struct Assembler {
    std::vector<AsmLine> lines;
    std::unordered_map<std::string, uint32_t> labelRva;
    std::vector<Fixup> fixups;
    std::vector<EncodeError> errors;

    std::vector<uint8_t> text;
    std::vector<uint8_t> data;

    SectionKind curSection = SectionKind::Text;

    uint32_t textRvaBase = 0x1000;
    uint32_t dataRvaBase = 0x2000;

    uint32_t textOffset = 0;
    uint32_t dataOffset = 0;

    static bool isDirective(const std::string& op) {
        return op == "db" || op == "dw" || op == "dd" || op == "dq";
    }

    uint32_t estimateInstSize(const AsmLine& l) {
        const std::string& op = l.opcode;
        if (op.empty()) return 0;

        if (op == "ret" || op == "nop") return 1;
        if (op == "syscall") return 2;
        if (op == "call" || op == "jmp") return 5;
        if (op == "je" || op == "jne") return 6; // near rel32
        if (op == "push" || op == "pop") return 2; // worst case w/ rex
        if (op == "xor" || op == "add" || op == "sub" || op == "cmp" || op == "mov") {
            // mov r64, imm64 -> 10 (rex + op + imm64)
            // mov r64, r64 / xor/add/sub/cmp r64,r64 -> 3 (rex + opcode + modrm)
            if (op == "mov" && l.ops.size() >= 2 && isNumberToken(l.ops[1])) return 10;
            if (op == "cmp" && l.ops.size() >= 2 && isNumberToken(l.ops[1])) return 4; // rex + 83 + modrm + imm8
            return 3;
        }
        if (op == "lea") {
            // lea r64, [rip+disp32]
            return 7;
        }
        if (isDirective(op)) {
            uint32_t size = 0;
            for (const auto& o : l.ops) {
                std::string lit;
                if (parseStringLiteral(o, lit)) {
                    size += (uint32_t)lit.size();
                    continue;
                }
                size += (op == "db") ? 1u : (op == "dw") ? 2u : (op == "dd") ? 4u : 8u;
            }
            return size;
        }
        return 0;
    }

    void addError(int line, const char* fmt, ...) {
        char buf[1024];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        errors.push_back({line, buf});
    }

    bool firstPassCollectLabels() {
        curSection = SectionKind::Text;
        textOffset = 0;
        dataOffset = 0;
        labelRva.clear();

        for (const auto& l : lines) {
            if (l.isSection) {
                curSection = l.section;
                continue;
            }
            if (l.isLabel) {
                std::string key = toLower(l.label);
                uint32_t rva = (curSection == SectionKind::Text) ? (textRvaBase + textOffset) : (dataRvaBase + dataOffset);
                labelRva[key] = rva;
                continue;
            }
            if (l.opcode.empty()) continue;

            uint32_t sz = estimateInstSize(l);
            if (curSection == SectionKind::Text) textOffset += sz;
            else dataOffset += sz;
        }
        return errors.empty();
    }

    bool emitBytes(SectionKind s, const std::vector<uint8_t>& bs) {
        auto& out = (s == SectionKind::Text) ? text : data;
        out.insert(out.end(), bs.begin(), bs.end());
        return true;
    }

    bool emitU32At(SectionKind s, uint32_t at, uint32_t v) {
        auto& out = (s == SectionKind::Text) ? text : data;
        if (at + 4 > out.size()) return false;
        out[at + 0] = (uint8_t)(v & 0xFF);
        out[at + 1] = (uint8_t)((v >> 8) & 0xFF);
        out[at + 2] = (uint8_t)((v >> 16) & 0xFF);
        out[at + 3] = (uint8_t)((v >> 24) & 0xFF);
        return true;
    }

    uint32_t curRva(SectionKind s) const {
        return (s == SectionKind::Text) ? (textRvaBase + (uint32_t)text.size()) : (dataRvaBase + (uint32_t)data.size());
    }

    bool emitMov(SectionKind s, const AsmLine& l) {
        if (l.ops.size() != 2) { addError(l.line, "mov requires 2 operands"); return false; }
        int dst = regIndex64(l.ops[0]);
        if (dst < 0) { addError(l.line, "mov invalid dst register '%s'", l.ops[0].c_str()); return false; }

        // mov r64, imm64
        if (isNumberToken(l.ops[1])) {
            bool ok = false;
            uint64_t imm = parseNumberToken(l.ops[1], &ok);
            if (!ok) { addError(l.line, "mov invalid immediate '%s'", l.ops[1].c_str()); return false; }

            std::vector<uint8_t> bs;
            emitU8(bs, rex(true, -1, dst));
            emitU8(bs, (uint8_t)(0xB8 + (dst & 7)));
            emitU64(bs, imm);
            return emitBytes(s, bs);
        }

        // mov r64, r64
        int src = regIndex64(l.ops[1]);
        if (src < 0) { addError(l.line, "mov invalid src register '%s'", l.ops[1].c_str()); return false; }

        std::vector<uint8_t> bs;
        emitU8(bs, rex(true, src, dst));
        emitU8(bs, 0x89); // mov r/m64, r64
        emitU8(bs, modrm(3, src, dst));
        return emitBytes(s, bs);
    }

    bool emitBinOpRR(SectionKind s, const AsmLine& l, uint8_t opcode) {
        if (l.ops.size() != 2) { addError(l.line, "%s requires 2 operands", l.opcode.c_str()); return false; }
        int dst = regIndex64(l.ops[0]);
        int src = regIndex64(l.ops[1]);
        if (dst < 0 || src < 0) { addError(l.line, "%s invalid registers '%s','%s'", l.opcode.c_str(), l.ops[0].c_str(), l.ops[1].c_str()); return false; }

        std::vector<uint8_t> bs;
        emitU8(bs, rex(true, src, dst));
        emitU8(bs, opcode);
        emitU8(bs, modrm(3, src, dst));
        return emitBytes(s, bs);
    }

    bool emitCmp(SectionKind s, const AsmLine& l) {
        if (l.ops.size() != 2) { addError(l.line, "cmp requires 2 operands"); return false; }
        int dst = regIndex64(l.ops[0]);
        if (dst < 0) { addError(l.line, "cmp invalid dst register '%s'", l.ops[0].c_str()); return false; }

        if (isNumberToken(l.ops[1])) {
            bool ok = false;
            uint64_t imm64 = parseNumberToken(l.ops[1], &ok);
            if (!ok) { addError(l.line, "cmp invalid immediate '%s'", l.ops[1].c_str()); return false; }
            uint8_t imm8 = (uint8_t)(imm64 & 0xFF);

            std::vector<uint8_t> bs;
            emitU8(bs, rex(true, -1, dst));
            emitU8(bs, 0x83);            // grp1 imm8
            emitU8(bs, modrm(3, 7, dst)); // /7 cmp
            emitU8(bs, imm8);
            return emitBytes(s, bs);
        }

        int src = regIndex64(l.ops[1]);
        if (src < 0) { addError(l.line, "cmp invalid src register '%s'", l.ops[1].c_str()); return false; }

        std::vector<uint8_t> bs;
        emitU8(bs, rex(true, src, dst));
        emitU8(bs, 0x39); // cmp r/m64, r64
        emitU8(bs, modrm(3, src, dst));
        return emitBytes(s, bs);
    }

    bool emitPushPop(SectionKind s, const AsmLine& l, bool isPush) {
        if (l.ops.size() != 1) { addError(l.line, "%s requires 1 operand", l.opcode.c_str()); return false; }
        int r = regIndex64(l.ops[0]);
        if (r < 0) { addError(l.line, "%s invalid register '%s'", l.opcode.c_str(), l.ops[0].c_str()); return false; }

        std::vector<uint8_t> bs;
        if (r >= 8) emitU8(bs, 0x41); // REX.B
        emitU8(bs, (uint8_t)((isPush ? 0x50 : 0x58) + (r & 7)));
        return emitBytes(s, bs);
    }

    bool emitRel32Branch(SectionKind s, const AsmLine& l, uint8_t opcode1, uint8_t opcode2, Fixup::Kind kind) {
        if (l.ops.size() != 1) { addError(l.line, "%s requires 1 label operand", l.opcode.c_str()); return false; }
        std::string tgt = toLower(trim(l.ops[0]));
        uint32_t here = curRva(s);

        std::vector<uint8_t> bs;
        emitU8(bs, opcode1);
        if (opcode2 != 0) emitU8(bs, opcode2);

        // placeholder disp32
        uint32_t dispAt = (s == SectionKind::Text) ? (uint32_t)text.size() + (uint32_t)bs.size() : (uint32_t)data.size() + (uint32_t)bs.size();
        emitU32(bs, 0);

        uint32_t nextRip = here + (uint32_t)bs.size();
        fixups.push_back({kind, s, dispAt, nextRip, tgt, l.line});

        return emitBytes(s, bs);
    }

    bool emitLeaRipLabel(SectionKind s, const AsmLine& l) {
        // lea r64, [label]
        if (l.ops.size() != 2) { addError(l.line, "lea requires 2 operands"); return false; }
        int dst = regIndex64(l.ops[0]);
        if (dst < 0) { addError(l.line, "lea invalid dst register '%s'", l.ops[0].c_str()); return false; }

        std::string label;
        if (!parseMemLabel(l.ops[1], label)) {
            // also accept bare label without brackets
            label = toLower(trim(l.ops[1]));
            if (label.empty() || !isIdentStart(label[0])) {
                addError(l.line, "lea requires [label] or label, got '%s'", l.ops[1].c_str());
                return false;
            }
        }

        uint32_t here = curRva(s);

        std::vector<uint8_t> bs;
        emitU8(bs, rex(true, dst, 5)); // rm=101 rip
        emitU8(bs, 0x8D);
        emitU8(bs, modrm(0, dst, 5)); // mod=00 rm=101 (RIP+disp32)

        uint32_t dispAt = (s == SectionKind::Text) ? (uint32_t)text.size() + (uint32_t)bs.size() : (uint32_t)data.size() + (uint32_t)bs.size();
        emitU32(bs, 0);

        uint32_t nextRip = here + (uint32_t)bs.size();
        fixups.push_back({Fixup::Kind::RipDisp32, s, dispAt, nextRip, toLower(label), l.line});

        return emitBytes(s, bs);
    }

    bool emitDirective(SectionKind s, const AsmLine& l) {
        const std::string& op = l.opcode;
        if (!isDirective(op)) return false;

        for (const auto& raw : l.ops) {
            std::string lit;
            if (parseStringLiteral(raw, lit)) {
                for (unsigned char c : lit) emitU8((s == SectionKind::Text) ? text : data, (uint8_t)c);
                continue;
            }

            bool ok = false;
            uint64_t v = parseNumberToken(raw, &ok);
            if (!ok) {
                addError(l.line, "directive %s invalid literal '%s'", op.c_str(), raw.c_str());
                return false;
            }

            if (op == "db") {
                emitU8((s == SectionKind::Text) ? text : data, (uint8_t)(v & 0xFF));
            } else if (op == "dw") {
                emitU16((s == SectionKind::Text) ? text : data, (uint16_t)(v & 0xFFFF));
            } else if (op == "dd") {
                emitU32((s == SectionKind::Text) ? text : data, (uint32_t)(v & 0xFFFFFFFFu));
            } else if (op == "dq") {
                emitU64((s == SectionKind::Text) ? text : data, (uint64_t)v);
            }
        }

        return true;
    }

    bool secondPassEmit() {
        curSection = SectionKind::Text;
        text.clear();
        data.clear();
        fixups.clear();

        for (const auto& l : lines) {
            if (l.isSection) { curSection = l.section; continue; }
            if (l.isEndp) continue;
            if (l.isLabel) continue;
            if (l.opcode.empty()) continue;

            // directives
            if (isDirective(l.opcode)) {
                if (!emitDirective(curSection, l)) return false;
                continue;
            }

            // instructions
            if (l.opcode == "ret") {
                emitU8((curSection == SectionKind::Text) ? text : data, 0xC3);
                continue;
            }
            if (l.opcode == "nop") {
                emitU8((curSection == SectionKind::Text) ? text : data, 0x90);
                continue;
            }
            if (l.opcode == "syscall") {
                emitU8((curSection == SectionKind::Text) ? text : data, 0x0F);
                emitU8((curSection == SectionKind::Text) ? text : data, 0x05);
                continue;
            }
            if (l.opcode == "mov") {
                if (!emitMov(curSection, l)) return false;
                continue;
            }
            if (l.opcode == "xor") {
                if (!emitBinOpRR(curSection, l, 0x31)) return false; // xor r/m64, r64
                continue;
            }
            if (l.opcode == "add") {
                if (!emitBinOpRR(curSection, l, 0x01)) return false; // add r/m64, r64
                continue;
            }
            if (l.opcode == "sub") {
                if (!emitBinOpRR(curSection, l, 0x29)) return false; // sub r/m64, r64
                continue;
            }
            if (l.opcode == "cmp") {
                if (!emitCmp(curSection, l)) return false;
                continue;
            }
            if (l.opcode == "push") {
                if (!emitPushPop(curSection, l, true)) return false;
                continue;
            }
            if (l.opcode == "pop") {
                if (!emitPushPop(curSection, l, false)) return false;
                continue;
            }
            if (l.opcode == "call") {
                if (!emitRel32Branch(curSection, l, 0xE8, 0x00, Fixup::Kind::Rel32)) return false;
                continue;
            }
            if (l.opcode == "jmp") {
                if (!emitRel32Branch(curSection, l, 0xE9, 0x00, Fixup::Kind::Rel32)) return false;
                continue;
            }
            if (l.opcode == "je") {
                if (!emitRel32Branch(curSection, l, 0x0F, 0x84, Fixup::Kind::Rel32)) return false;
                continue;
            }
            if (l.opcode == "jne") {
                if (!emitRel32Branch(curSection, l, 0x0F, 0x85, Fixup::Kind::Rel32)) return false;
                continue;
            }
            if (l.opcode == "lea") {
                if (!emitLeaRipLabel(curSection, l)) return false;
                continue;
            }

            addError(l.line, "unimplemented opcode '%s'", l.opcode.c_str());
            return false;
        }

        return true;
    }

    bool applyFixups() {
        for (const auto& f : fixups) {
            auto it = labelRva.find(toLower(f.target));
            if (it == labelRva.end()) {
                addError(f.line, "undefined label '%s'", f.target.c_str());
                return false;
            }
            uint32_t target = it->second;
            int32_t disp = (int32_t)(target - f.nextRip);
            if (!emitU32At(f.section, f.atOffset, (uint32_t)disp)) {
                addError(f.line, "internal fixup patch OOB for '%s'", f.target.c_str());
                return false;
            }
        }
        return true;
    }

    bool buildPE(const char* outPath, uint32_t entryRva) {
        uint32_t textRawSize = alignUp((uint32_t)text.size(), kFileAlign);
        uint32_t dataRawSize = alignUp((uint32_t)data.size(), kFileAlign);

        uint32_t textVirtSize = (uint32_t)text.size();
        uint32_t dataVirtSize = (uint32_t)data.size();

        uint32_t sizeOfImage = alignUp(dataRvaBase + alignUp(dataVirtSize, kSectionAlign), kSectionAlign);

        IMAGE_DOS_HEADER dos{};
        dos.e_magic = IMAGE_DOS_SIGNATURE;
        dos.e_cblp = 0x0090;
        dos.e_cp = 0x0003;
        dos.e_cparhdr = 0x0004;
        dos.e_maxalloc = 0xFFFF;
        dos.e_sp = 0x00B8;
        dos.e_lfarlc = 0x0040;
        dos.e_lfanew = 0x80;

        IMAGE_NT_HEADERS64 nt{};
        nt.Signature = IMAGE_NT_SIGNATURE;
        nt.FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
        nt.FileHeader.NumberOfSections = 2;
        nt.FileHeader.TimeDateStamp = (uint32_t)time(nullptr);
        nt.FileHeader.PointerToSymbolTable = 0;
        nt.FileHeader.NumberOfSymbols = 0;
        nt.FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
        nt.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;

        auto& opt = nt.OptionalHeader;
        opt.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
        opt.MajorLinkerVersion = 1;
        opt.MinorLinkerVersion = 0;
        opt.SizeOfCode = textRawSize;
        opt.SizeOfInitializedData = dataRawSize;
        opt.SizeOfUninitializedData = 0;
        opt.AddressOfEntryPoint = entryRva;
        opt.BaseOfCode = textRvaBase;
        opt.ImageBase = (ULONGLONG)kImageBase;
        opt.SectionAlignment = kSectionAlign;
        opt.FileAlignment = kFileAlign;
        opt.MajorOperatingSystemVersion = 6;
        opt.MinorOperatingSystemVersion = 0;
        opt.MajorSubsystemVersion = 6;
        opt.MinorSubsystemVersion = 0;
        opt.SizeOfImage = sizeOfImage;
        opt.SizeOfHeaders = kHeadersSize;
        opt.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI;
        opt.DllCharacteristics = 0;
        opt.SizeOfStackReserve = 0x100000;
        opt.SizeOfStackCommit = 0x1000;
        opt.SizeOfHeapReserve = 0x100000;
        opt.SizeOfHeapCommit = 0x1000;
        opt.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;

        IMAGE_SECTION_HEADER shText{};
        memcpy(shText.Name, ".text", 5);
        shText.Misc.VirtualSize = textVirtSize;
        shText.VirtualAddress = textRvaBase;
        shText.SizeOfRawData = textRawSize;
        shText.PointerToRawData = kHeadersSize;
        shText.Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;

        IMAGE_SECTION_HEADER shData{};
        memcpy(shData.Name, ".data", 5);
        shData.Misc.VirtualSize = dataVirtSize;
        shData.VirtualAddress = dataRvaBase;
        shData.SizeOfRawData = dataRawSize;
        shData.PointerToRawData = kHeadersSize + textRawSize;
        shData.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

        FILE* f = fopen(outPath, "wb");
        if (!f) {
            perror("failed to open output");
            return false;
        }

        // DOS header + stub
        fwrite(&dos, sizeof(dos), 1, f);
        uint8_t stub[0x80 - sizeof(dos)]{};
        // Minimal DOS stub: "This program cannot be run in DOS mode.\r\n$"
        const char msg[] = "This program cannot be run in DOS mode.\r\n$";
        memcpy(stub + 0x20, msg, sizeof(msg) - 1);
        fwrite(stub, sizeof(stub), 1, f);

        fwrite(&nt, sizeof(nt), 1, f);
        fwrite(&shText, sizeof(shText), 1, f);
        fwrite(&shData, sizeof(shData), 1, f);

        // Pad headers to kHeadersSize
        long cur = ftell(f);
        if (cur < (long)kHeadersSize) {
            std::vector<uint8_t> pad((size_t)kHeadersSize - (size_t)cur, 0);
            fwrite(pad.data(), 1, pad.size(), f);
        }

        // .text
        fwrite(text.data(), 1, text.size(), f);
        if (textRawSize > text.size()) {
            std::vector<uint8_t> pad(textRawSize - (uint32_t)text.size(), 0);
            fwrite(pad.data(), 1, pad.size(), f);
        }

        // .data
        fwrite(data.data(), 1, data.size(), f);
        if (dataRawSize > data.size()) {
            std::vector<uint8_t> pad(dataRawSize - (uint32_t)data.size(), 0);
            fwrite(pad.data(), 1, pad.size(), f);
        }

        fclose(f);
        return true;
    }

    bool assembleToExe(const char* outPath) {
        if (!firstPassCollectLabels()) return false;
        if (!secondPassEmit()) return false;
        if (!applyFixups()) return false;

        // Choose entry: _start, main, then first label in .text if any
        uint32_t entry = textRvaBase;
        if (auto it = labelRva.find("_start"); it != labelRva.end()) entry = it->second;
        else if (auto it2 = labelRva.find("main"); it2 != labelRva.end()) entry = it2->second;

        if (!buildPE(outPath, entry)) {
            addError(0, "failed to build PE");
            return false;
        }

        return true;
    }
};

static bool readAll(const char* path, std::string& out) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.assign((size_t)sz, '\0');
    if (sz > 0) fread(out.data(), 1, (size_t)sz, f);
    fclose(f);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Usage: %s <input.asm> <output.exe>\n", argv[0]);
        return 1;
    }

    std::string src;
    if (!readAll(argv[1], src)) {
        fprintf(stderr, "ERROR: cannot open %s\n", argv[1]);
        return 1;
    }

    Assembler a;

    // Line-based parse.
    int lineNo = 1;
    size_t start = 0;
    while (start <= src.size()) {
        size_t end = src.find('\n', start);
        if (end == std::string::npos) end = src.size();
        std::string line = src.substr(start, end - start);

        AsmLine al;
        if (parseLineToAsm(line, lineNo, al)) {
            a.lines.push_back(std::move(al));
        }

        if (end == src.size()) break;
        start = end + 1;
        lineNo++;
    }

    if (!a.assembleToExe(argv[2])) {
        for (const auto& e : a.errors) {
            if (e.line > 0) fprintf(stderr, "[ERR:%d] %s\n", e.line, e.msg.c_str());
            else fprintf(stderr, "[ERR] %s\n", e.msg.c_str());
        }
        return 1;
    }

    printf("OK: wrote %s\n", argv[2]);
    printf("  .text = %zu bytes\n", a.text.size());
    printf("  .data = %zu bytes\n", a.data.size());
    return 0;
}
