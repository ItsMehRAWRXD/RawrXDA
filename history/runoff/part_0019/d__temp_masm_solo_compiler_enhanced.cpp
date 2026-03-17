// Enhanced MASM Solo Compiler - Full x64 Code Generator
// Compile: cl /O2 /EHsc /Fe:masm_solo_compiler_enhanced.exe
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <stdint.h>
#include <stdarg.h>
#include <cctype>
#include <ctype.h>

// Token types
enum TokenType { T_EOF, T_NEWLINE, T_IDENT, T_COLON, T_COMMA, T_NUMBER, T_STRING, T_DIRECTIVE, T_INSTRUCTION, T_REGISTER };
struct Token { TokenType type; std::string text; int line; };

// Simple lexer
std::vector<Token> lex(const char* src, int& err_line) {
    std::vector<Token> toks;
    int line = 1;
    while (*src) {
        while (*src == ' ' || *src == '\t') src++;
        if (*src == '\n') { toks.push_back({T_NEWLINE,"",line}); src++; line++; continue; }
        if (*src == ';') { while (*src && *src != '\n') src++; continue; }
        if (*src == ':') { toks.push_back({T_COLON,":",line}); src++; continue; }
        if (*src == ',') { toks.push_back({T_COMMA,",",line}); src++; continue; }
        if (isdigit(*src)) {
            std::string num; 
            while (isxdigit(*src) || *src=='x' || *src=='h' || *src=='b') { num += *src++; }
            toks.push_back({T_NUMBER,num,line}); continue;
        }
        if (isalpha(*src) || *src=='_') {
            std::string id;
            while (isalnum(*src) || *src=='_') id += *src++;
            TokenType tt = T_IDENT;
            // Register detection
            static const char* regs[] = {"rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp","r8","r9","r10","r11","r12","r13","r14","r15","eax","ebx","ecx","edx","esi","edi","ebp","esp","ax","bx","cx","dx","al","ah","bl","bh","cl","ch","dl","dh",0};
            for (int i=0; regs[i]; i++) if (_stricmp(id.c_str(),regs[i])==0) { tt = T_REGISTER; break; }
            // Directive
            if (id[0]=='.' || _stricmp(id.c_str(),"db")==0) tt = T_DIRECTIVE;
            // Instructions
            static const char* insts[] = {"nop","ret","mov","add","sub","int","call","jmp","push","pop","xor","cmp","test","lea","syscall",0};
            for (int i=0; insts[i]; i++) if (_stricmp(id.c_str(),insts[i])==0) { tt = T_INSTRUCTION; break; }
            std::string tokText = id;
            if (tt == T_INSTRUCTION) {
                for (char& c : tokText) c = (char)tolower((unsigned char)c);
            }
            toks.push_back({tt,tokText,line});
            continue;
        }
        src++; // skip unknown
    }
    toks.push_back({T_EOF,"",line});
    return toks;
}

// AST nodes
enum NodeType { N_PROG, N_LABEL, N_INSTR, N_DB };
struct Node { 
    NodeType type; 
    std::string text; 
    std::vector<std::string> ops; 
    int line; 
    std::vector<Node> children;
};

// Parser
struct Parser {
    std::vector<Token>& t;
    size_t pos;
    Parser(std::vector<Token>& tok) : t(tok), pos(0) {}
    Token& cur() { return t[pos]; }
    void eat(TokenType tt) { if (pos < t.size() && cur().type == tt) pos++; }
    
    Node parse() {
        Node prog;
        prog.type = N_PROG;
        prog.text = "program";
        prog.line = cur().line;
        
        while (cur().type != T_EOF) {
            if (cur().type == T_NEWLINE) { eat(T_NEWLINE); continue; }
            
            if (cur().type == T_IDENT && pos+1 < t.size() && t[pos+1].type == T_COLON) {
                Node lab;
                lab.type = N_LABEL;
                lab.text = cur().text;
                lab.line = cur().line;
                eat(T_IDENT); eat(T_COLON);
                prog.children.push_back(lab);
                continue;
            }
            
            if (cur().type == T_INSTRUCTION) {
                Node ins;
                ins.type = N_INSTR;
                ins.text = cur().text;
                ins.line = cur().line;
                eat(T_INSTRUCTION);
                
                // Parse operands
                while (cur().type != T_NEWLINE && cur().type != T_EOF) {
                    if (cur().type == T_REGISTER || cur().type == T_IDENT || cur().type == T_NUMBER) {
                        ins.ops.push_back(cur().text);
                        eat(cur().type);
                    }
                    if (cur().type == T_COMMA) eat(T_COMMA);
                    else if (cur().type != T_NEWLINE && cur().type != T_EOF) {
                        fprintf(stderr, "[PARSE ERR:%d] Unexpected token '%s'\n", cur().line, cur().text.c_str());
                        break;
                    }
                }
                eat(T_NEWLINE);
                prog.children.push_back(ins);
                continue;
            }
            
            fprintf(stderr, "[PARSE WARN:%d] Skipping token '%s'\n", cur().line, cur().text.c_str());
            pos++;
        }
        return prog;
    }
};

// Enhanced Code Generator with full x64 support
struct CodeGenEnhanced {
    std::vector<uint8_t> code;
    int errors = 0;

    bool isImm(const std::string& s) {
        if (s.empty()) return false;
        char c = s[0];
        return isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+';
    }

    int64_t parseImm64(const std::string& s) {
        // Supports 0x.., decimal, binary (0b), and MASM-style trailing 'h'
        if (!s.empty() && (s.back() == 'h' || s.back() == 'H')) {
            std::string trimmed = s.substr(0, s.size() - 1);
            return strtoll(trimmed.c_str(), nullptr, 16);
        }
        return strtoll(s.c_str(), nullptr, 0);
    }
    
    void emit(uint8_t b) { code.push_back(b); }
    void emit32(uint32_t d) { 
        emit(d&0xFF); emit((d>>8)&0xFF); emit((d>>16)&0xFF); emit((d>>24)&0xFF); 
    }
    void emit64(uint64_t d) { emit32((uint32_t)d); emit32((uint32_t)(d>>32)); }
    
    void fail(int line, const char* fmt, ...) {
        errors++;
        fprintf(stderr, "[CODEGEN ERR:%d] ", line);
        va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
        fprintf(stderr, "\n");
    }
    
    // Register encoding table
    int getRegId(const std::string& reg) {
        static const char* regs64[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"};
        static const char* regs32[] = {"eax","ecx","edx","ebx","esp","ebp","esi","edi","r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d"};
        
        for (int i=0; i<16; i++) {
            if (_stricmp(reg.c_str(), regs64[i])==0) return i;
            if (_stricmp(reg.c_str(), regs32[i])==0) return i;
        }
        return -1;
    }
    
    bool needsRex(int reg1, int reg2 = -1) {
        return (reg1 >= 8) || (reg2 >= 8);
    }
    
    void emitRex(bool w, int reg = -1, int rm = -1) {
        uint8_t rex = 0x40;
        if (w) rex |= 0x08;         // REX.W
        if (reg >= 8) rex |= 0x04;  // REX.R
        if (rm >= 8) rex |= 0x01;   // REX.B
        emit(rex);
    }
    
    void emitModRM(int mod, int reg, int rm) {
        emit((mod << 6) | ((reg & 7) << 3) | (rm & 7));
    }
    
    void emitInstruction(const Node& n) {
        const std::string& op = n.text;
        const auto& ops = n.ops;
        
        fprintf(stderr, "[CODEGEN] Line %d: '%s' with %zu operands:", n.line, op.c_str(), ops.size());
        for (size_t i=0; i<ops.size(); i++) fprintf(stderr, " %s", ops[i].c_str());
        fprintf(stderr, "\n");
        
        if (op == "nop") { 
            emit(0x90); 
            fprintf(stderr, "  -> Emitted: 90 (nop)\n");
        }
        else if (op == "ret") { 
            emit(0xC3); 
            fprintf(stderr, "  -> Emitted: C3 (ret)\n");
        }
        else if (op == "syscall") {
            emit(0x0F); emit(0x05);
            fprintf(stderr, "  -> Emitted: 0F 05 (syscall)\n");
        }
        else if (op == "int") {
            if (ops.empty()) { fail(n.line, "int requires operand"); return; }
            int imm = strtol(ops[0].c_str(), 0, 0);
            if (imm == 3) {
                emit(0xCC);
                fprintf(stderr, "  -> Emitted: CC (int3)\n");
            } else {
                emit(0xCD); emit(imm & 0xFF);
                fprintf(stderr, "  -> Emitted: CD %02X (int %d)\n", imm & 0xFF, imm);
            }
        }
        else if (op == "push") {
            if (ops.empty()) { fail(n.line, "push requires operand"); return; }
            int reg = getRegId(ops[0]);
            if (reg == -1) { fail(n.line, "Unknown register: %s", ops[0].c_str()); return; }
            
            if (reg >= 8) emitRex(false, -1, reg);
            emit(0x50 + (reg & 7));
            fprintf(stderr, "  -> Emitted: %s%02X (push %s)\n", 
                   reg >= 8 ? "41 " : "", 0x50 + (reg & 7), ops[0].c_str());
        }
        else if (op == "pop") {
            if (ops.empty()) { fail(n.line, "pop requires operand"); return; }
            int reg = getRegId(ops[0]);
            if (reg == -1) { fail(n.line, "Unknown register: %s", ops[0].c_str()); return; }
            
            if (reg >= 8) emitRex(false, -1, reg);
            emit(0x58 + (reg & 7));
            fprintf(stderr, "  -> Emitted: %s%02X (pop %s)\n", 
                   reg >= 8 ? "41 " : "", 0x58 + (reg & 7), ops[0].c_str());
        }
        else if (op == "mov") {
            if (ops.size() < 2) { fail(n.line, "mov requires 2 operands"); return; }
            
            int dst = getRegId(ops[0]);
            if (dst == -1) { fail(n.line, "Invalid destination register: %s", ops[0].c_str()); return; }
            
            // Check if source is immediate
            if (isImm(ops[1])) {
                // MOV r64, imm64
                emitRex(true, -1, dst);
                emit(0xB8 + (dst & 7));
                uint64_t imm = static_cast<uint64_t>(parseImm64(ops[1]));
                emit64(imm);
                fprintf(stderr, "  -> Emitted: %s%02X %016llX (mov %s, 0x%llX)\n", 
                       dst >= 8 ? "49 " : "48 ", 0xB8 + (dst & 7), imm, ops[0].c_str(), imm);
            } else {
                // MOV r64, r64
                int src = getRegId(ops[1]);
                if (src == -1) { fail(n.line, "Invalid source register: %s", ops[1].c_str()); return; }
                
                emitRex(true, src, dst);
                emit(0x89);  // MOV r/m64, r64
                emitModRM(3, src, dst);  // mod=11 (register), reg=src, rm=dst
                fprintf(stderr, "  -> Emitted: %s89 %02X (mov %s, %s)\n", 
                       needsRex(src, dst) ? "48 " : "48 ", 
                       0xC0 | ((src & 7) << 3) | (dst & 7), ops[0].c_str(), ops[1].c_str());
            }
        }
        else if (op == "xor") {
            if (ops.size() < 2) { fail(n.line, "xor requires 2 operands"); return; }
            
            int dst = getRegId(ops[0]);
            int src = getRegId(ops[1]);
            if (dst == -1 || src == -1) { 
                fail(n.line, "Invalid registers for xor: %s, %s", ops[0].c_str(), ops[1].c_str()); 
                return; 
            }
            
            // XOR r32, r32 (clears register to zero)
            if (dst == src) {
                if (needsRex(src, dst)) emitRex(false, src, dst);
                emit(0x31);  // XOR r/m32, r32
                emitModRM(3, src, dst);
                fprintf(stderr, "  -> Emitted: %s31 %02X (xor %s, %s)\n", 
                       needsRex(src, dst) ? "41 " : "", 
                       0xC0 | ((src & 7) << 3) | (dst & 7), ops[0].c_str(), ops[1].c_str());
            } else {
                emitRex(true, src, dst);
                emit(0x31);  // XOR r/m64, r64  
                emitModRM(3, src, dst);
                fprintf(stderr, "  -> Emitted: %s31 %02X (xor %s, %s)\n", 
                       needsRex(src, dst) ? "49 " : "48 ", 
                       0xC0 | ((src & 7) << 3) | (dst & 7), ops[0].c_str(), ops[1].c_str());
            }
        }
        else if (op == "and" || op == "or" || op == "cmp" || op == "test") {
            if (ops.size() < 2) { fail(n.line, "%s requires 2 operands", op.c_str()); return; }
            int dst = getRegId(ops[0]);
            if (dst == -1) { fail(n.line, "Invalid destination: %s", ops[0].c_str()); return; }

            bool isImmSrc = isImm(ops[1]);
            int srcReg = isImmSrc ? -1 : getRegId(ops[1]);
            if (!isImmSrc && srcReg == -1) { fail(n.line, "Invalid source: %s", ops[1].c_str()); return; }

            // Select opcode sets
            uint8_t regFieldImm = 0; // /digit for imm forms
            uint8_t opcodeRegRm = 0; // reg/rm variant
            uint8_t opcodeRmReg = 0; // rm/reg variant (unused for now)
            if (op == "and") { regFieldImm = 4; opcodeRegRm = 0x21; }
            else if (op == "or") { regFieldImm = 1; opcodeRegRm = 0x09; }
            else if (op == "cmp") { regFieldImm = 7; opcodeRegRm = 0x39; }
            else if (op == "test") { regFieldImm = 0; opcodeRegRm = 0x85; regFieldImm = 0; }

            if (isImmSrc) {
                int64_t imm = parseImm64(ops[1]);
                emitRex(true, -1, dst);
                if (op == "test") {
                    emit(0xF7);                  // TEST r/m64, imm32 (/0)
                    emitModRM(3, 0, dst);
                    emit32(static_cast<uint32_t>(imm));
                    fprintf(stderr, "  -> Emitted: %sF7 %02X %08X (test %s, %lld)\n", dst>=8?"49 ":"48 ", 0xC0 | dst, static_cast<uint32_t>(imm), ops[0].c_str(), imm);
                } else if (imm >= -128 && imm <= 127) {
                    emit(0x83);                  // ALU r/m64, imm8
                    emitModRM(3, regFieldImm, dst);
                    emit(static_cast<uint8_t>(imm));
                    fprintf(stderr, "  -> Emitted: %s83 %02X %02X (%s %s, %lld)\n", dst>=8?"49 ":"48 ", 0xC0 | (regFieldImm<<3) | (dst & 7), static_cast<uint8_t>(imm), op.c_str(), ops[0].c_str(), imm);
                } else {
                    emit(0x81);                  // ALU r/m64, imm32
                    emitModRM(3, regFieldImm, dst);
                    emit32(static_cast<uint32_t>(imm));
                    fprintf(stderr, "  -> Emitted: %s81 %02X %08X (%s %s, %lld)\n", dst>=8?"49 ":"48 ", 0xC0 | (regFieldImm<<3) | (dst & 7), static_cast<uint32_t>(imm), op.c_str(), ops[0].c_str(), imm);
                }
            } else {
                emitRex(true, srcReg, dst);
                emit(opcodeRegRm);
                emitModRM(3, srcReg, dst);
                fprintf(stderr, "  -> Emitted: %s%02X %02X (%s %s, %s)\n", needsRex(srcReg,dst)?"49 ":"48 ", opcodeRegRm, 0xC0 | ((srcReg&7)<<3) | (dst&7), op.c_str(), ops[0].c_str(), ops[1].c_str());
            }
        }
        else if (op == "add") {
            if (ops.size() < 2) { fail(n.line, "add requires 2 operands"); return; }
            
            int dst = getRegId(ops[0]);
            if (dst == -1) { fail(n.line, "Invalid destination: %s", ops[0].c_str()); return; }
            
            if (isImm(ops[1])) {
                // ADD r64, imm8/imm32
                int imm = strtol(ops[1].c_str(), 0, 0);
                emitRex(true, -1, dst);
                if (imm >= -128 && imm <= 127) {
                    emit(0x83);  // ADD r/m64, imm8
                    emitModRM(3, 0, dst);  // reg field = 0 for ADD
                    emit(imm & 0xFF);
                    fprintf(stderr, "  -> Emitted: %s83 %02X %02X (add %s, %d)\n", 
                           dst >= 8 ? "49 " : "48 ", 0xC0 | (dst & 7), imm & 0xFF, ops[0].c_str(), imm);
                } else {
                    emit(0x81);  // ADD r/m64, imm32
                    emitModRM(3, 0, dst);
                    emit32(imm);
                    fprintf(stderr, "  -> Emitted: %s81 %02X %08X (add %s, %d)\n", 
                           dst >= 8 ? "49 " : "48 ", 0xC0 | (dst & 7), imm, ops[0].c_str(), imm);
                }
            } else {
                int src = getRegId(ops[1]);
                if (src == -1) { fail(n.line, "Invalid source: %s", ops[1].c_str()); return; }
                emitRex(true, src, dst);
                emit(0x01);  // ADD r/m64, r64
                emitModRM(3, src, dst);
                fprintf(stderr, "  -> Emitted: %s01 %02X (add %s, %s)\n", 
                       needsRex(src, dst) ? "49 " : "48 ", 
                       0xC0 | ((src & 7) << 3) | (dst & 7), ops[0].c_str(), ops[1].c_str());
            }
        }
        else if (op == "sub") {
            if (ops.size() < 2) { fail(n.line, "sub requires 2 operands"); return; }
            
            int dst = getRegId(ops[0]);
            if (dst == -1) { fail(n.line, "Invalid destination: %s", ops[0].c_str()); return; }
            
            if (isImm(ops[1])) {
                // SUB r64, imm8/imm32
                int imm = strtol(ops[1].c_str(), 0, 0);
                emitRex(true, -1, dst);
                if (imm >= -128 && imm <= 127) {
                    emit(0x83);  // SUB r/m64, imm8
                    emitModRM(3, 5, dst);  // reg field = 5 for SUB
                    emit(imm & 0xFF);
                    fprintf(stderr, "  -> Emitted: %s83 %02X %02X (sub %s, %d)\n", 
                           dst >= 8 ? "49 " : "48 ", 0xE8 | (dst & 7), imm & 0xFF, ops[0].c_str(), imm);
                } else {
                    emit(0x81);  // SUB r/m64, imm32
                    emitModRM(3, 5, dst);
                    emit32(imm);
                    fprintf(stderr, "  -> Emitted: %s81 %02X %08X (sub %s, %d)\n", 
                           dst >= 8 ? "49 " : "48 ", 0xE8 | (dst & 7), imm, ops[0].c_str(), imm);
                }
            } else {
                int src = getRegId(ops[1]);
                if (src == -1) { fail(n.line, "Invalid source: %s", ops[1].c_str()); return; }
                emitRex(true, src, dst);
                emit(0x29);  // SUB r/m64, r64
                emitModRM(3, src, dst);
                fprintf(stderr, "  -> Emitted: %s29 %02X (sub %s, %s)\n", 
                       needsRex(src, dst) ? "49 " : "48 ", 
                       0xC0 | ((src & 7) << 3) | (dst & 7), ops[0].c_str(), ops[1].c_str());
            }
        }
        else if (op == "call") {
            if (ops.size() < 1) { fail(n.line, "call requires operand"); return; }
            int reg = getRegId(ops[0]);
            if (reg == -1) { fail(n.line, "call supports register targets only in this frontend: %s", ops[0].c_str()); return; }
            if (reg >= 8) emitRex(false, reg, -1);
            emit(0xFF);                 // CALL r/m64
            emitModRM(3, 2, reg);       // /2
            fprintf(stderr, "  -> Emitted: %sFF %02X (call %s)\n", reg>=8?"41 ":"", 0xD0 | (reg & 7), ops[0].c_str());
        }
        else if (op == "jmp") {
            if (ops.size() < 1) { fail(n.line, "jmp requires operand"); return; }
            int reg = getRegId(ops[0]);
            if (reg == -1) { fail(n.line, "jmp supports register targets only in this frontend: %s", ops[0].c_str()); return; }
            if (reg >= 8) emitRex(false, reg, -1);
            emit(0xFF);                 // JMP r/m64
            emitModRM(3, 4, reg);       // /4
            fprintf(stderr, "  -> Emitted: %sFF %02X (jmp %s)\n", reg>=8?"41 ":"", 0xE0 | (reg & 7), ops[0].c_str());
        }
        else {
            fail(n.line, "Unimplemented instruction: %s", op.c_str());
        }
    }
    
    void process(const Node& ast) {
        for (const auto& child : ast.children) {
            if (child.type == N_INSTR) {
                fprintf(stderr, "[DEBUG] N_INSTR: text='%s', ops=%zu [", child.text.c_str(), child.ops.size());
                for (size_t i=0; i<child.ops.size(); i++) {
                    fprintf(stderr, "'%s'%s", child.ops[i].c_str(), i+1<child.ops.size()?", ":"");
                }
                fprintf(stderr, "]\n");
                
                size_t before = code.size();
                emitInstruction(child);
                if (code.size() == before && errors==0) {
                    fail(child.line, "Instruction emitted ZERO bytes");
                }
            }
            else if (child.type == N_LABEL) {
                fprintf(stderr, "[CODEGEN] Label '%s' at offset 0x%zX\n", child.text.c_str(), code.size());
            }
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s <input.asm> [output.bin]\n", argv[0]); return 1; }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", argv[1]); return 1; }
    
    fseek(f,0,SEEK_END); long sz = ftell(f); fseek(f,0,SEEK_SET);
    std::string buf(sz+1, '\0'); 
    fread(&buf[0], 1, sz, f); 
    fclose(f);
    
    printf("=== Enhanced MASM Solo Compiler ===\n");
    printf("Reading: %s (%ld bytes)\n\n", argv[1], sz);
    
    int err_line = 0;
    auto toks = lex(buf.c_str(), err_line);
    printf("Lexical: %zu tokens\n", toks.size());
    
    Parser p(toks);
    Node ast = p.parse();
    printf("AST: %zu top-level nodes\n\n", ast.children.size());
    
    printf("=== Enhanced Code Generation ===\n");
    CodeGenEnhanced cg;
    cg.process(ast);
    
    if (cg.errors > 0) {
        fprintf(stderr, "\n*** FAILED: %d errors ***\n", cg.errors);
        return 1;
    }
    
    printf("\n=== SUCCESS ===\n");
    printf("Generated %zu bytes of machine code:\n", cg.code.size());
    for (size_t i=0; i<cg.code.size(); i++) {
        printf("%02X ", cg.code[i]);
        if ((i+1)%16==0) printf("\n");
    }
    if (cg.code.size()%16!=0) printf("\n");
    
    if (argc >= 3) {
        FILE* out = fopen(argv[2], "wb");
        if (out) {
            fwrite(cg.code.data(), 1, cg.code.size(), out);
            fclose(out);
            printf("\nWrote binary to: %s\n", argv[2]);
        }
    }
    
    return 0;
}