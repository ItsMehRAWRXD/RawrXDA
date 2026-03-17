// RawrXD MASM Solo Compiler v1.0 - x64 PE Generator
// Compile: cl.exe /O2 /EHsc /Femasm_solo_compiler.exe masm_solo_compiler.cpp

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include "instruction_encoder.hpp"

using namespace std;

// ============================================================================
// TOKENIZER
// ============================================================================
enum TokenType {
    TOK_EOF, TOK_IDENT, TOK_NUMBER, TOK_COLON, TOK_COMMA,
    TOK_LBRACKET, TOK_RBRACKET, TOK_PLUS, TOK_MINUS,
    TOK_SEGMENT, TOK_ENDS, TOK_PROC, TOK_ENDP, TOK_END,
    TOK_CODE, TOK_DATA, TOK_DWORD, TOK_QWORD, TOK_PTR, TOK_OFFSET,
    // Instructions
    TOK_MOV, TOK_ADD, TOK_SUB, TOK_RET, TOK_PUSH, TOK_POP,
    TOK_XOR, TOK_CALL, TOK_JMP, TOK_LEA, TOK_CMP, TOK_JE, TOK_JNE, TOK_SYSCALL, TOK_INT, TOK_NOP, TOK_INT3
};

struct Token {
    TokenType type;
    char text[64];
    int64_t value;
    int line;
};

const char* keywords[] = {
    ".CODE",".DATA",".ENDS","END","ENDP","PROC","SEGMENT",
    "DWORD","QWORD","PTR","OFFSET",
    "MOV","ADD","SUB","RET","PUSH","POP","XOR","CALL","JMP","LEA","CMP","JE","JNE","SYSCALL","INT","NOP","INT3",
    nullptr
};

TokenType kwTypes[] = {
    TOK_CODE,TOK_DATA,TOK_ENDS,TOK_END,TOK_ENDP,TOK_PROC,TOK_SEGMENT,
    TOK_DWORD,TOK_QWORD,TOK_PTR,TOK_OFFSET,
    TOK_MOV,TOK_ADD,TOK_SUB,TOK_RET,TOK_PUSH,TOK_POP,TOK_XOR,TOK_CALL,TOK_JMP,TOK_LEA,TOK_CMP,TOK_JE,TOK_JNE,TOK_SYSCALL,TOK_INT,TOK_NOP,TOK_INT3
};

class Lexer {
    const char* src;
    int pos, line;
public:
    Lexer(const char* s) : src(s), pos(0), line(1) {}
    
    void skip() {
        while (src[pos] && (src[pos] <= ' ')) {
            if (src[pos] == '\n') line++;
            pos++;
        }
        if (src[pos] == ';') while (src[pos] && src[pos] != '\n') pos++;
    }
    
    void readIdent(char* buf, int max) {
        int i = 0;
        while (src[pos] && (isalnum(src[pos]) || src[pos]=='_' || src[pos]=='.')) {
            if (i < max-1) buf[i++] = src[pos];
            pos++;
        }
        buf[i] = 0;
    }
    
    Token next() {
        skip();
        Token t = {TOK_EOF, "", 0, line};
        if (!src[pos]) return t;
        
        char c = src[pos];
        
        if (c == ':') { pos++; t.type = TOK_COLON; return t; }
        if (c == ',') { pos++; t.type = TOK_COMMA; return t; }
        if (c == '[') { pos++; t.type = TOK_LBRACKET; return t; }
        if (c == ']') { pos++; t.type = TOK_RBRACKET; return t; }
        if (c == '+') { pos++; t.type = TOK_PLUS; return t; }
        if (c == '-') { pos++; t.type = TOK_MINUS; return t; }
        
        if (isdigit(c)) {
            int64_t val = 0;
            if (src[pos] == '0' && (src[pos+1]=='x'||src[pos+1]=='X')) {
                pos += 2;
                while (isxdigit(src[pos])) {
                    val = val*16 + (isdigit(src[pos]) ? src[pos]-'0' : toupper(src[pos])-'A'+10);
                    pos++;
                }
            } else {
                while (isdigit(src[pos])) val = val*10 + (src[pos++]-'0');
            }
            t.type = TOK_NUMBER;
            t.value = val;
            sprintf(t.text, "%llX", val);
            return t;
        }
        
        if (isalpha(c) || c=='_' || c=='.') {
            readIdent(t.text, 64);
            _strupr(t.text);
            for (int i = 0; keywords[i]; i++) {
                if (strcmp(t.text, keywords[i]) == 0) {
                    t.type = kwTypes[i];
                    return t;
                }
            }
            t.type = TOK_IDENT;
            return t;
        }
        
        pos++; // skip unknown
        return next();
    }
};

// ============================================================================
// PARSER / AST
// ============================================================================
enum NodeType {
    NODE_PROGRAM, NODE_SEGMENT, NODE_PROC, NODE_INSTRUCTION, NODE_LABEL
};

struct AstNode {
    NodeType type;
    char text[64];
    TokenType opcode;
    vector<Token> operands;
    vector<AstNode> children;
};

class Parser {
    Lexer lex;
    Token cur;
    
    void advance() { cur = lex.next(); }
    
    bool eat(TokenType t) {
        if (cur.type == t) { advance(); return true; }
        return false;
    }
    
    AstNode parseSegment() {
        AstNode n; n.type = NODE_SEGMENT;
        strcpy(n.text, cur.text); // .CODE or name
        advance();
        if (eat(TOK_SEGMENT)) {
            if (cur.type == TOK_IDENT) { strcpy(n.text, cur.text); advance(); }
        }
        while (cur.type != TOK_ENDS && cur.type != TOK_EOF) {
            if (cur.type == TOK_IDENT && lex.next().type == TOK_COLON) {
                AstNode lbl; lbl.type = NODE_LABEL;
                strcpy(lbl.text, cur.text);
                advance(); eat(TOK_COLON);
                n.children.push_back(lbl);
            } else if (cur.type == TOK_PROC) {
                n.children.push_back(parseProc());
            } else if (cur.type >= TOK_MOV && cur.type <= TOK_INT) {
                AstNode inst; inst.type = NODE_INSTRUCTION; inst.opcode = cur.type;
                advance();
                while (cur.type != TOK_EOF && cur.type != TOK_ENDP && cur.type != TOK_ENDS && 
                       !(cur.type == TOK_IDENT && lex.next().type == TOK_COLON) && cur.type != TOK_PROC) {
                    if (cur.type == TOK_COMMA) { advance(); continue; }
                    inst.operands.push_back(cur);
                    advance();
                }
                n.children.push_back(inst);
            } else {
                advance(); // skip unknown
            }
        }
        eat(TOK_ENDS);
        return n;
    }
    
    AstNode parseProc() {
        eat(TOK_PROC);
        AstNode n; n.type = NODE_PROC;
        if (cur.type == TOK_IDENT) { strcpy(n.text, cur.text); advance(); }
        while (cur.type != TOK_ENDP && cur.type != TOK_EOF) {
            if (cur.type >= TOK_MOV && cur.type <= TOK_INT) {
                AstNode inst; inst.type = NODE_INSTRUCTION; inst.opcode = cur.type;
                advance();
                while (cur.type != TOK_EOF && cur.type != TOK_ENDP && cur.type != TOK_PROC) {
                    if (cur.type == TOK_COMMA) { advance(); continue; }
                    inst.operands.push_back(cur);
                    advance();
                }
                n.children.push_back(inst);
            } else {
                advance();
            }
        }
        eat(TOK_ENDP);
        return n;
    }
    
public:
    Parser(const char* src) : lex(src) { advance(); }
    
    AstNode parse() {
        AstNode root; root.type = NODE_PROGRAM;
        while (cur.type != TOK_EOF) {
            if (cur.type == TOK_SEGMENT || cur.type == TOK_CODE || cur.type == TOK_DATA) {
                root.children.push_back(parseSegment());
            } else if (cur.type == TOK_IDENT && strcmp(cur.text, "END") == 0) {
                eat(TOK_END);
                break;
            } else {
                advance();
            }
        }
        return root;
    }
};

// ============================================================================
// X64 ENCODER TABLES
// ============================================================================
struct RegInfo { const char* name; uint8_t rex, modrm; bool r64; };
RegInfo regs[] = {
    {"RAX",0,0,true},{"RCX",0,1,true},{"RDX",0,2,true},{"RBX",0,3,true},
    {"RSP",0,4,true},{"RBP",0,5,true},{"RSI",0,6,true},{"RDI",0,7,true},
    {"R8",1,0,true},{"R9",1,1,true},{"R10",1,2,true},{"R11",1,3,true},
    {"R12",1,4,true},{"R13",1,5,true},{"R14",1,6,true},{"R15",1,7,true},
    {"EAX",0,0,false},{"ECX",0,1,false},{"EDX",0,2,false},{"EBX",0,3,false},
    {"ESP",0,4,false},{"EBP",0,5,false},{"ESI",0,6,false},{"EDI",0,7,false},
    {"R8D",1,0,false},{"R9D",1,1,false},{"R10D",1,2,false},{"R11D",1,3,false},
    {nullptr,0,0,false}
};

RegInfo* findReg(const char* s) {
    char upper[16]; strcpy(upper, s); _strupr(upper);
    for (int i=0; regs[i].name; i++) if (!strcmp(regs[i].name, upper)) return &regs[i];
    return nullptr;
}

static RawrXD::MASM::Register parseRegister(const char* text) {
    char upper[16]; strcpy(upper, text); _strupr(upper);
    if (!strcmp(upper, "RAX")) return RawrXD::MASM::Register::RAX;
    if (!strcmp(upper, "RCX")) return RawrXD::MASM::Register::RCX;
    if (!strcmp(upper, "RDX")) return RawrXD::MASM::Register::RDX;
    if (!strcmp(upper, "RBX")) return RawrXD::MASM::Register::RBX;
    if (!strcmp(upper, "RSP")) return RawrXD::MASM::Register::RSP;
    if (!strcmp(upper, "RBP")) return RawrXD::MASM::Register::RBP;
    if (!strcmp(upper, "RSI")) return RawrXD::MASM::Register::RSI;
    if (!strcmp(upper, "RDI")) return RawrXD::MASM::Register::RDI;
    if (!strcmp(upper, "R8")) return RawrXD::MASM::Register::R8;
    if (!strcmp(upper, "R9")) return RawrXD::MASM::Register::R9;
    if (!strcmp(upper, "R10")) return RawrXD::MASM::Register::R10;
    if (!strcmp(upper, "R11")) return RawrXD::MASM::Register::R11;
    if (!strcmp(upper, "R12")) return RawrXD::MASM::Register::R12;
    if (!strcmp(upper, "R13")) return RawrXD::MASM::Register::R13;
    if (!strcmp(upper, "R14")) return RawrXD::MASM::Register::R14;
    if (!strcmp(upper, "R15")) return RawrXD::MASM::Register::R15;
    return RawrXD::MASM::Register::NONE;
}

// ============================================================================
// CODE GENERATOR
// ============================================================================
class CodeGen {
    vector<uint8_t> code;
    unordered_map<string, size_t> labels;
    vector<pair<size_t, string>> fixups;
    size_t baseAddr = 0x1000; // Virtual base
    bool hadError = false;
    
    // Diagnostic counters
    int nodesProcessed = 0;
    int instructionsEmitted = 0;
    int fallbackEncodings = 0;
    
    const char* tokenTypeToString(TokenType t) {
        switch(t) {
            case TOK_MOV: return "MOV"; case TOK_ADD: return "ADD"; case TOK_SUB: return "SUB";
            case TOK_RET: return "RET"; case TOK_PUSH: return "PUSH"; case TOK_POP: return "POP";
            case TOK_XOR: return "XOR"; case TOK_CALL: return "CALL"; case TOK_JMP: return "JMP";
            case TOK_LEA: return "LEA"; case TOK_CMP: return "CMP"; case TOK_JE: return "JE";
            case TOK_JNE: return "JNE"; case TOK_SYSCALL: return "SYSCALL"; case TOK_INT: return "INT";
            case TOK_NOP: return "NOP"; case TOK_INT3: return "INT3";
            default: return "???";
        }
    }
    
public:
    void reset() { code.clear(); labels.clear(); fixups.clear(); nodesProcessed = 0; instructionsEmitted = 0; fallbackEncodings = 0; }
    
    size_t currentOffset() { return code.size(); }
    
    void emit(uint8_t b) { code.push_back(b); }
    void emit32(uint32_t v) { for(int i=0;i<4;i++) emit((v>>(i*8))&0xFF); }
    void emit64(uint64_t v) { for(int i=0;i<8;i++) emit((v>>(i*8))&0xFF); }
    
    void emitREX(bool w, bool r, bool x, bool b) {
        emit(0x40 | (w<<3) | (r<<2) | (x<<1) | b);
    }
    
    void encodeModRM(int mod, int reg, int rm) {
        emit((mod<<6) | ((reg&7)<<3) | (rm&7));
    }
    
    bool assembleInst(TokenType op, vector<Token>& ops, int lineNum = 0) {
        std::cerr << "  [EMIT] " << tokenTypeToString(op) << " (" << ops.size() << " operands)";
        for(size_t i=0; i<ops.size(); i++) {
            std::cerr << " [" << ops[i].text << "]";
        }
        std::cerr << "\n";

        // Try primary encoder first
        try {
            RawrXD::MASM::Instruction inst;
            switch (op) {
                case TOK_MOV: inst.mnemonic = "mov"; break;
                case TOK_ADD: inst.mnemonic = "add"; break;
                case TOK_SUB: inst.mnemonic = "sub"; break;
                case TOK_RET: inst.mnemonic = "ret"; break;
                case TOK_PUSH: inst.mnemonic = "push"; break;
                case TOK_POP: inst.mnemonic = "pop"; break;
                case TOK_XOR: inst.mnemonic = "xor"; break;
                case TOK_CALL: inst.mnemonic = "call"; break;
                case TOK_JMP: inst.mnemonic = "jmp"; break;
                case TOK_SYSCALL: inst.mnemonic = "syscall"; break;
                case TOK_INT: inst.mnemonic = "int"; break;
                case TOK_NOP: inst.mnemonic = "nop"; break;
                case TOK_INT3: inst.mnemonic = "int3"; break;
                default: inst.mnemonic.clear(); break;
            }

            if (!inst.mnemonic.empty()) {
                for (size_t i = 0; i < ops.size() && i < 3; ++i) {
                    const auto& tok = ops[i];
                    RawrXD::MASM::Operand operand;
                    if (tok.type == TOK_NUMBER) {
                        operand.type = RawrXD::MASM::Operand::Immediate;
                        operand.imm = tok.value;
                    } else if (tok.type == TOK_IDENT) {
                        auto reg = parseRegister(tok.text);
                        if (reg != RawrXD::MASM::Register::NONE) {
                            operand.type = RawrXD::MASM::Operand::Register;
                            operand.reg = reg;
                        } else {
                            operand.type = RawrXD::MASM::Operand::None;
                        }
                    }

                    if (i == 0) inst.op1 = operand;
                    else if (i == 1) inst.op2 = operand;
                    else inst.op3 = operand;
                }

                RawrXD::MASM::InstructionEncoder encoder;
                auto bytes = encoder.encode(inst);
                for (auto b : bytes) emit(b);
                instructionsEmitted++;
                std::cerr << "       -> Primary encoder OK, " << bytes.size() << " bytes\n";
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "       -> Primary encoder failed: " << e.what() << ", using fallback\n";
            fallbackEncodings++;
        }

        // Fallback manual encoder
        switch(op) {
            case TOK_RET:
                emit(0xC3);
                instructionsEmitted++;
                std::cerr << "       -> Fallback: RET (0xC3)\n";
                return true;
            case TOK_NOP:
                emit(0x90);
                instructionsEmitted++;
                std::cerr << "       -> Fallback: NOP (0x90)\n";
                return true;
            case TOK_INT3:
                emit(0xCC);
                instructionsEmitted++;
                std::cerr << "       -> Fallback: INT3 (0xCC)\n";
                return true;
            case TOK_SYSCALL:
                emit(0x0F); emit(0x05);  // SYSCALL opcode
                instructionsEmitted++;
                std::cerr << "       -> Fallback: SYSCALL (0x0F 0x05)\n";
                return true;
            case TOK_PUSH:
                if (ops.size() == 1 && ops[0].type == TOK_NUMBER) {
                    emit(0x68); emit32(ops[0].value); // PUSH imm32
                    instructionsEmitted++;
                    std::cerr << "       -> Fallback: PUSH imm32\n";
                } else if (ops.size() == 1) {
                    RegInfo* r = findReg(ops[0].text);
                    if (r) { if(r->rex) emitREX(false,false,false,true); emit(0x50 + r->modrm); instructionsEmitted++; std::cerr << "       -> Fallback: PUSH reg\n"; }
                }
                return true;
            case TOK_POP:
                if (ops.size() == 1) {
                    RegInfo* r = findReg(ops[0].text);
                    if (r) { if(r->rex) emitREX(false,false,false,true); emit(0x58 + r->modrm); }
                }
                return true;
            case TOK_XOR:
                if (ops.size() == 2) {
                    RegInfo* d = findReg(ops[0].text);
                    RegInfo* s = findReg(ops[1].text);
                    if (d && s && d->r64) {
                        emitREX(true, s->rex, false, d->rex);
                        emit(0x31); // XOR r/m64, r64
                        encodeModRM(3, s->modrm, d->modrm);
                    }
                }
                return true;
            case TOK_MOV:
                if (ops.size() == 2) {
                    RegInfo* d = findReg(ops[0].text);
                    if (d && d->r64 && ops[1].type == TOK_NUMBER) {
                        // MOV r64, imm64 (special encoding for A-D, else generic)
                        if (d->modrm < 8 && !d->rex) {
                            emitREX(true,false,false,false);
                            emit(0xC7); emit(0xC0 | d->modrm); // MOV r/m64, imm32 (sign-extended)
                            emit32(ops[1].value);
                        } else {
                            emitREX(true, false, false, d->rex);
                            emit(0xB8 + d->modrm);
                            emit64(ops[1].value);
                        }
                    } else if (d && ops[1].type == TOK_IDENT) {
                        // MOV reg, label (needs fixup)
                        emitREX(true,false,false,d->rex);
                        emit(0xB8 + d->modrm);
                        fixups.push_back({code.size(), ops[1].text});
                        emit64(0); // placeholder
                    } else {
                        RegInfo* s = findReg(ops[1].text);
                        if (d && s && d->r64) {
                            emitREX(true, s->rex, false, d->rex);
                            emit(0x89); // MOV r/m64, r64
                            encodeModRM(3, s->modrm, d->modrm);
                        }
                    }
                }
                return true;
            case TOK_SUB:
                if (ops.size() == 2) {
                    RegInfo* d = findReg(ops[0].text);
                    if (d && d->r64 && ops[1].type == TOK_NUMBER) {
                        emitREX(true,false,false,d->rex);
                        emit(0x81); emit(0xE8 | d->modrm); emit32(ops[1].value); // SUB r/m64, imm32
                    }
                }
                return true;
            case TOK_ADD:
                if (ops.size() == 2) {
                    RegInfo* d = findReg(ops[0].text);
                    if (d && d->r64 && ops[1].type == TOK_NUMBER) {
                        emitREX(true,false,false,d->rex);
                        emit(0x81); emit(0xC0 | d->modrm); emit32(ops[1].value);
                    }
                }
                return true;
            case TOK_CALL:
            case TOK_JMP: {
                uint8_t opcode = (op == TOK_CALL) ? 0xE8 : 0xE9;
                emit(opcode);
                if (ops.size() == 1 && ops[0].type == TOK_IDENT) {
                    fixups.push_back({code.size(), ops[0].text});
                    emit32(0); // rel32 placeholder
                } else if (ops.size() == 1 && ops[0].type == TOK_NUMBER) {
                    emit32(ops[0].value);
                }
                instructionsEmitted++;
                std::cerr << "       -> Fallback: " << (op == TOK_CALL ? "CALL" : "JMP") << "\n";
                return true;
            }
            case TOK_INT:
                if (ops.size() == 1 && ops[0].type == TOK_NUMBER) {
                    if (ops[0].value == 3) emit(0xCC);
                    else { emit(0xCD); emit((uint8_t)ops[0].value); }
                }
                return true;
            default:
                std::cerr << "       -> [ERROR] Unhandled opcode in fallback: " << tokenTypeToString(op) << "\n";
                break;
        }
        std::cerr << "       -> [FATAL] Instruction encoding failed completely for " << tokenTypeToString(op) << "\n";
        return false;
    }
    
    bool firstPass(AstNode& node, int depth = 0) {
        nodesProcessed++;
        if (node.type == NODE_LABEL) {
            labels[node.text] = currentOffset();
            std::cerr << "[CODEGEN] Label '" << node.text << "' at offset 0x" << std::hex << currentOffset() << std::dec << "\n";
        } else if (node.type == NODE_INSTRUCTION) {
            if (!assembleInst(node.opcode, node.operands, 0)) {
                std::cerr << "[FATAL] Unsupported instruction opcode: " << tokenTypeToString(node.opcode) << "\n";
                hadError = true;
                return false;
            }
        }
        for (auto& c : node.children) {
            if (!firstPass(c, depth+1)) return false;
        }
        return true;
    }
    
    void resolveFixups() {
        for (auto& f : fixups) {
            size_t patchOffset = f.first;
            const string& name = f.second;
            if (labels.count(name)) {
                size_t target = labels[name];
                size_t nextInstr = patchOffset + 4;
                int32_t rel = (int32_t)(target - nextInstr);
                // Patch in place
                code[patchOffset] = rel & 0xFF;
                code[patchOffset+1] = (rel>>8) & 0xFF;
                code[patchOffset+2] = (rel>>16) & 0xFF;
                code[patchOffset+3] = (rel>>24) & 0xFF;
            } else {
                printf("Error: Undefined label %s\n", name.c_str());
            }
        }
    }
    
    bool generate(AstNode& root, vector<uint8_t>& output) {
        reset();
        hadError = false;
        std::cerr << "\n[CODEGEN] ============ Starting Code Generation ============\n";
        std::cerr << "[CODEGEN] AST has " << root.children.size() << " top-level segments\n";
        
        // Pass 1: gen code, record labels
        for (auto& seg : root.children) {
            if (seg.type == NODE_SEGMENT) {
                std::cerr << "[CODEGEN] Processing SEGMENT node: '" << seg.text << "'\n";
                for (auto& item : seg.children) {
                    if (item.type == NODE_PROC) {
                        labels[item.text] = currentOffset();
                        std::cerr << "[CODEGEN] Processing PROC '" << item.text << "' at offset 0x" << std::hex << currentOffset() << std::dec << "\n";
                        for (auto& inst : item.children) {
                            if (!firstPass(inst)) return false;
                        }
                    } else {
                        if (!firstPass(item)) return false;
                    }
                }
            }
        }
        // Pass 2: resolve fixups
        std::cerr << "[CODEGEN] Pass 2: Resolving " << fixups.size() << " fixups\n";
        resolveFixups();
        
        output = code;
        std::cerr << "[CODEGEN] ============ Code Generation Complete ============\n";
        std::cerr << "[CODEGEN] Summary: " << nodesProcessed << " nodes, " << instructionsEmitted << " instructions, " << code.size() << " bytes\n";
        std::cerr << "[CODEGEN] Primary encoder: " << (instructionsEmitted - fallbackEncodings) << ", Fallback: " << fallbackEncodings << "\n\n";
        return !hadError;
    }
    
    size_t getEntryPoint(const char* procName="main") {
        string up = procName; _strupr(&up[0]);
        if (labels.count(up)) return labels[up];
        return 0;
    }
};

// ============================================================================
// PE WRITER (64-bit)
// ============================================================================
void writePE(const char* filename, vector<uint8_t>& code, size_t entryRVA) {
    FILE* f = fopen(filename, "wb");
    if (!f) { printf("Cannot write output\n"); return; }
    
    // Alignments
    const int fileAlign = 0x200;
    const int sectAlign = 0x1000;
    const int headerSize = 0x400;
    
    int codePad = (code.size() + fileAlign - 1) & ~(fileAlign-1);
    int imageSize = ((headerSize + code.size() + sectAlign - 1) & ~(sectAlign-1)) + sectAlign;
    
    // DOS Header
    fwrite("\x4D\x5A\x90\x00\x03\x00\x00\x00\x04\x00\x00\x00\xFF\xFF\x00\x00\xB8\x00\x00\x00", 20, 1, f);
    fwrite("\x00\x00\x00\x00\x40\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 20, 1, f);
    fwrite("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xF8\x00\x00\x00\x0E\x1F\xBA\x0E", 20, 1, f);
    fwrite("\x00\xB4\x09\xCD\x21\xB8\x01\x4C\xCD\x21\x54\x68\x69\x73\x20\x70\x72\x6F\x67\x72", 20, 1, f);
    fwrite("\x61\x6D\x20\x63\x61\x6E\x6E\x6F\x74\x20\x62\x65\x20\x72\x75\x6E\x20\x69\x6E\x20", 20, 1, f);
    fwrite("\x44\x4F\x53\x20\x6D\x6F\x64\x65\x2E\x0D\x0D\x0A\x24\x00\x00\x00\x00\x00\x00\x00", 20, 1, f);
    // Pad to PE offset 0x3C+4
    while (ftell(f) < 0x3C) fputc(0, f);
    fwrite("\xF8\x00\x00\x00", 4, 1, f); // PE header offset
    
    // PE Signature
    while (ftell(f) < 0xF8) fputc(0, f);
    fwrite("PE\0\0", 4, 1, f);
    
    // COFF Header (Machine=x64=0x8664)
    fwrite("\x64\x86", 2, 1, f); // Machine
    fwrite("\x01\x00", 2, 1, f); // Sections
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Timestamp
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Symbol table
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Symbols
    fwrite("\xF0\x00", 2, 1, f); // Optional header size (64-bit optional header is 240 bytes)
    fwrite("\x22\x00", 2, 1, f); // Characteristics (EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE)
    
    // Optional Header (PE32+)
    fwrite("\x0B\x02", 2, 1, f); // Magic (PE32+)
    fwrite("\x01\x00", 2, 1, f); // Linker major/minor
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Code size (filled later)
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Initialized data
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Uninitialized data
    uint32_t entryVirt = headerSize + entryRVA;
    fwrite(&entryVirt, 4, 1, f); // Entry point
    fwrite("\x00\x10\x00\x00", 4, 1, f); // Base of code (0x1000)
    fwrite("\x00\x00\x00\x00\x00\x10\x00\x00", 8, 1, f); // Image base (0x100000000 for ASLR, but 1MB for now)
    fwrite("\x00\x10\x00\x00", 4, 1, f); // Section alignment
    fwrite("\x00\x02\x00\x00", 4, 1, f); // File alignment
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Image minor
    fwrite("\x06\x00\x00\x00", 4, 1, f); // Subsystem major (Win10)
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Subsystem minorr
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Win32 version
    fwrite("\x00\x00\x01\x00", 4, 1, f); // Size of image (placeholder)
    fwrite("\x00\x02\x00\x00", 4, 1, f); // Size of headers
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Checksumve
    fwrite("\x03\x00", 2, 1, f); // Subsystem (CONSOLE) // Stack commit
    fwrite("\x00\x00", 2, 1, f); // DllCharacteristics
    fwrite("\x00\x00\x10\x00\x00\x00\x00\x00", 8, 1, f); // Stack reservefwrite("\x00\x00\x10\x00\x00\x00\x00\x00", 8, 1, f); // Heap commit
    fwrite("\x00\x00\x10\x00\x00\x00\x00\x00", 8, 1, f); // Stack commit f); // Loader flags
    fwrite("\x00\x00\x10\x00\x00\x00\x00\x00", 8, 1, f); // Heap reserves)
    fwrite("\x00\x00\x10\x00\x00\x00\x00\x00", 8, 1, f); // Heap commit
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Loader flagsy/null)
    fwrite("\x10\x00\x00\x00", 4, 1, f); // Number of RVA and sizes (16 data directories)fwrite("\x00\x00\x00\x00\x00\x00\x00\x00", 8, 1, f);
    
    // Data directories (empty/null)
    for (int i = 0; i < 16*2; i++) fwrite("\x00\x00\x00\x00\x00\x00\x00\x00", 8, 1, f);
    
    // Section Header (.text)
    fwrite(".text\0\0\0", 8, 1, f);
    uint32_t codesize = code.size();w, should be padded to fileAlign)
    fwrite(&codesize, 4, 1, f); // Virtual size0)
    fwrite("\x00\x10\x00\x00", 4, 1, f); // Virtual address (0x1000)
    fwrite(&codesize, 4, 1, f); // Raw size (unpadded for now, should be padded to fileAlign)fwrite("\x00\x00\x00\x00", 4, 1, f); // Relocation count
    fwrite("\x00\x02\x00\x00", 4, 1, f); // Raw address (0x200)4, 1, f); // Line number count
    fwrite("\x00\x00\x00\x00\x00\x00\x00\x00", 8, 1, f); // Relocations/line numsharacteristics (CODE | EXECUTE | READ)
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Relocation count
    fwrite("\x00\x00\x00\x00", 4, 1, f); // Line number countto fileAlign
    fwrite("\x60\x00\x00\x60", 4, 1, f); // Characteristics (CODE | EXECUTE | READ));
    
    // Pad headers to fileAlign
    while (ftell(f) < fileAlign) fputc(0, f);fwrite(code.data(), code.size(), 1, f);
    fileAlign
    // Code section
    fwrite(code.data(), code.size(), 1, f);   
    // Pad to fileAlign    fclose(f);
    while ((ftell(f) % fileAlign) != 0) fputc(0, f);size(), entryRVA);
    
    fclose(f);
    printf("Generated PE64: %s (%zu bytes code, entry=$%zX)\n", filename, code.size(), entryRVA);==============================================
}

// ============================================================================, char** argv) {
// MAINf (argc < 3) {
// ============================================================================    printf("RawrXD MASM Solo Compiler\nUsage: %s <input.asm> <output.exe>\n", argv[0]);
int main(int argc, char** argv) {
    if (argc < 3) {
        printf("RawrXD MASM Solo Compiler\nUsage: %s <input.asm> <output.exe>\n", argv[0]);
        return 1;1], "rb");
    }ot open %s\n", argv[1]); return 1; }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("Cannot open %s\n", argv[1]); return 1; }SEEK_SET);
    fseek(f, 0, SEEK_END);= new char[sz+1];
    size_t sz = ftell(f);fread(src, 1, sz, f);
    fseek(f, 0, SEEK_SET);
    char* src = new char[sz+1];fclose(f);
    fread(src, 1, sz, f);
    src[sz] = 0;olo Compiler (RawrXD Edition)\nSource: %s (%zu bytes)\n", argv[1], sz);
    fclose(f);
    sis... ");
    printf("MASM Solo Compiler (RawrXD Edition)\nSource: %s (%zu bytes)\n", argv[1], sz);
    printf("Done\nParsing... ");
    printf("Lexical Analysis... ");
    Parser p(src); (%zu segments)\n", ast.children.size());
    printf("Done\nParsing... ");
    auto ast = p.parse();
    printf("Done (%zu segments)\n", ast.children.size());
    ector<uint8_t> code;
    printf("Code Generation... ");
    CodeGen gen;    printf("FAILED\n"); return 1;
    vector<uint8_t> code;
    if (!gen.generate(ast, code)) {
        printf("FAILED\n"); return 1;
    }size_t entry = gen.getEntryPoint("MAIN");
    printf("Done (%zu bytes)\n", code.size());tEntryPoint("START");
    if (entry == 0 && !code.empty()) entry = 0; // Default to section start
    size_t entry = gen.getEntryPoint("MAIN");
    if (entry == 0) entry = gen.getEntryPoint("START");rgv[2], code, entry);
    if (entry == 0 && !code.empty()) entry = 0; // Default to section start   
        delete[] src;






}    return 0;    delete[] src;        writePE(argv[2], code, entry);    return 0;
}
