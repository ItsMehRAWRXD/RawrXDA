// masm_solo_compiler_verbose.cpp
// Compile: cl /O2 /EHsc /Femasm_solo_compiler_verbose.exe
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>

// Minimal PE emitter structures
#pragma pack(push, 1)
struct DOS_HEADER { WORD magic; WORD cblp; WORD cp; WORD crlc; WORD cparhdr; WORD minalloc; WORD maxalloc; WORD ss; WORD sp; WORD csum; WORD ip; WORD cs; WORD lfarlc; WORD ovno; WORD res[4]; WORD oemid; WORD oeminfo; WORD res2[10]; DWORD lfanew; };
struct COFF_FILE_HEADER { WORD Machine; WORD NumberOfSections; DWORD TimeDateStamp; DWORD PointerToSymbolTable; DWORD NumberOfSymbols; WORD SizeOfOptionalHeader; WORD Characteristics; };
struct DATA_DIRECTORY { DWORD VirtualAddress; DWORD Size; };
struct OPTIONAL_HEADER64 { WORD Magic; BYTE MajorLinkerVersion; BYTE MinorLinkerVersion; DWORD SizeOfCode; DWORD SizeOfInitializedData; DWORD SizeOfUninitializedData; DWORD AddressOfEntryPoint; DWORD BaseOfCode; ULONGLONG ImageBase; DWORD SectionAlignment; DWORD FileAlignment; WORD MajorOperatingSystemVersion; WORD MinorOperatingSystemVersion; WORD MajorImageVersion; WORD MinorImageVersion; WORD MajorSubsystemVersion; WORD MinorSubsystemVersion; DWORD Win32VersionValue; DWORD SizeOfImage; DWORD SizeOfHeaders; DWORD CheckSum; WORD Subsystem; WORD DllCharacteristics; ULONGLONG SizeOfStackReserve; ULONGLONG SizeOfStackCommit; ULONGLONG SizeOfHeapReserve; ULONGLONG SizeOfHeapCommit; DWORD LoaderFlags; DWORD NumberOfRvaAndSizes; DATA_DIRECTORY DataDirectory[16]; };
struct SECTION_HEADER { BYTE Name[8]; DWORD VirtualSize; DWORD VirtualAddress; DWORD SizeOfRawData; DWORD PointerToRawData; DWORD PointerToRelocations; DWORD PointerToLinenumbers; WORD NumberOfRelocations; WORD NumberOfLinenumbers; DWORD Characteristics; };
#pragma pack(pop)

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
            std::string num; while (isxdigit(*src) || *src=='x' || *src=='h' || *src=='b') { num += *src++; }
            toks.push_back({T_NUMBER,num,line}); continue;
        }
        if (isalpha(*src) || *src=='_') {
            std::string id;
            while (isalnum(*src) || *src=='_') id += *src++;
            TokenType tt = T_IDENT;
            // Simple register detection
            static const char* regs[] = {"rax","rbx","rcx","rdx","rsi","rdi","rbp","rsp","r8","r9","r10","r11","r12","r13","r14","r15","eax","ebx","ecx","edx","esi","edi","ebp","esp","ax","bx","cx","dx","al","ah","bl","bh","cl","ch","dl","dh",0};
            for (int i=0; regs[i]; i++) if (_stricmp(id.c_str(),regs[i])==0) { tt = T_REGISTER; break; }
            // Directive detection
            if (id[0]=='.' || _stricmp(id.c_str(),"db")==0) tt = T_DIRECTIVE;
            // Instruction detection (subset)
            static const char* insts[] = {"nop","ret","mov","add","sub","int","call","jmp","push","pop","xor",0};
            for (int i=0; insts[i]; i++) if (_stricmp(id.c_str(),insts[i])==0) { tt = T_INSTRUCTION; break; }
            toks.push_back({tt,id,line});
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

// Recursive descent parser
struct Parser {
    std::vector<Token>& t;
    size_t pos;
    Parser(std::vector<Token>& tok) : t(tok), pos(0) {}
    Token& cur() { return t[pos]; }
    void eat(TokenType tt) { if (cur().type == tt) pos++; }
    
    Node parse() {
        Node prog(N_PROG,"program",{},cur().line);
        while (cur().type != T_EOF) {
            if (cur().type == T_NEWLINE) { eat(T_NEWLINE); continue; }
            if (cur().type == T_IDENT && pos+1 < t.size() && t[pos+1].type == T_COLON) {
                Node lab(N_LABEL, cur().text, {}, cur().line);
                eat(T_IDENT); eat(T_COLON);
                prog.children.push_back(lab);
                continue;
            }
            if (cur().type == T_INSTRUCTION) {
                Node ins(N_INSTR, cur().text, {}, cur().line);
                std::string instr = cur().text;
                eat(T_INSTRUCTION);
                // Parse operands until newline or comma separation
                while (cur().type != T_NEWLINE && cur().type != T_EOF) {
                    if (cur().type == T_REGISTER || cur().type == T_IDENT || cur().type == T_NUMBER) {
                        ins.ops.push_back(cur().text);
                        eat(cur().type);
                    }
                    if (cur().type == T_COMMA) eat(T_COMMA);
                    else if (cur().type != T_NEWLINE && cur().type != T_EOF) {
                        fprintf(stderr, "[PARSE ERR:%d] Unexpected token '%s' in operand\n", cur().line, cur().text.c_str());
                        break;
                    }
                }
                eat(T_NEWLINE);
                prog.children.push_back(ins);
                continue;
            }
            if (cur().type == T_IDENT && _stricmp(cur().text.c_str(), "db")==0) {
                eat(T_IDENT);
                Node db(N_DB, "db", {}, cur().line);
                if (cur().type == T_STRING || cur().type == T_NUMBER) {
                    db.ops.push_back(cur().text);
                    eat(cur().type);
                }
                eat(T_NEWLINE);
                prog.children.push_back(db);
                continue;
            }
            fprintf(stderr, "[PARSE ERR:%d] Unexpected token '%s'\n", cur().line, cur().text.c_str());
            pos++;
        }
        return prog;
    }
};

// Code generator with verbose failure
struct CodeGen {
    std::vector<BYTE> code;
    DWORD virtualAddr = 0x1000;
    int errors = 0;
    
    void emit(BYTE b) { code.push_back(b); }
    void emit32(DWORD d) { emit(d&0xFF); emit((d>>8)&0xFF); emit((d>>16)&0xFF); emit((d>>24)&0xFF); }
    void emit64(ULONGLONG d) { emit32((DWORD)d); emit32((DWORD)(d>>32)); }
    
    void fail(int line, const char* fmt, ...) {
        errors++;
        fprintf(stderr, "[CODEGEN ERR:%d] ", line);
        va_list ap; va_start(ap,fmt); vfprintf(stderr,fmt,ap); va_end(ap);
        fprintf(stderr, "\n");
    }
    
    // Basic opcode emitter
    void emitInstruction(const Node& n) {
        const std::string& op = n.text;
        const auto& ops = n.ops;
        
        fprintf(stderr, "[CODEGEN] Processing '%s' at line %d with %zu ops\n", op.c_str(), n.line, ops.size());
        
        if (op == "nop") { emit(0x90); }
        else if (op == "ret") { emit(0xC3); }
        else if (op == "int") {
            if (ops.empty()) { fail(n.line, "int requires operand (e.g., 0x21)"); return; }
            int imm = strtol(ops[0].c_str(),0,0);
            if (imm == 3) emit(0xCC);
            else { emit(0xCD); emit(imm & 0xFF); }
        }
        else if (op == "push") {
            if (ops.empty()) { fail(n.line, "push requires register"); return; }
            // Simple reg64 detection
            static const char* regs64[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"};
            for (int i=0; i<16; i++) if (_stricmp(ops[0].c_str(), regs64[i])==0) {
                if (i < 8) emit(0x50 + i);
                else { emit(0x41); emit(0x50 + (i-8)); } // REX prefix for r8-r15
                return;
            }
            fail(n.line, "Unknown register for push: %s", ops[0].c_str());
        }
        else if (op == "pop") {
            if (ops.empty()) { fail(n.line, "pop requires register"); return; }
            static const char* regs64[] = {"rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"};
            for (int i=0; i<16; i++) if (_stricmp(ops[0].c_str(), regs64[i])==0) {
                if (i < 8) emit(0x58 + i);
                else { emit(0x41); emit(0x58 + (i-8)); }
                return;
            }
            fail(n.line, "Unknown register for pop: %s", ops[0].c_str());
        }
        else if (op == "mov") {
            if (ops.size() < 2) { fail(n.line, "mov requires 2 operands"); return; }
            // Very basic mov r64, imm64 (for rax only as demo)
            if (_stricmp(ops[0].c_str(),"rax")==0) {
                emit(0x48); emit(0xB8); // REX.W + B8+r
                ULONGLONG imm = strtoull(ops[1].c_str(),0,0);
                emit64(imm);
            } else {
                fail(n.line, "mov variant not implemented: %s", ops[0].c_str());
            }
        }
        else if (op == "jmp" || op == "call") {
            fail(n.line, "%s requires symbol relocation (not implemented in solo compiler)", op.c_str());
        }
        else {
            fail(n.line, "Unimplemented instruction: %s", op.c_str());
        }
    }
    
    void process(const Node& ast) {
        for (const auto& child : ast.children) {
            if (child.type == N_INSTR) {
                size_t before = code.size();
                emitInstruction(child);
                if (code.size() == before && errors==0) {
                    fail(child.line, "Instruction emitted zero bytes (check implementation)");
                }
            }
            else if (child.type == N_DB) {
                // Simple DB - assumes number
                if (!child.ops.empty()) {
                    int v = strtol(child.ops[0].c_str(),0,0);
                    emit(v & 0xFF);
                }
            }
            else if (child.type == N_LABEL) {
                // Just a marker, no code emission
                fprintf(stderr, "[CODEGEN] Label '%s' at offset 0x%zX\n", child.text.c_str(), code.size());
            }
        }
    }
    
    void writePE(const char* filename) {
        if (errors > 0) {
            fprintf(stderr, "\n*** ABORTING: %d codegen errors detected ***\n", errors);
            return;
        }
        
        // Align code to 0x200
        size_t rawCodeSize = (code.size() + 0x1FF) & ~0x1FF;
        code.resize(rawCodeSize, 0);
        
        FILE* f = fopen(filename, "wb");
        if (!f) { perror("fopen"); return; }
        
        // DOS Header
        DOS_HEADER dos = {}; dos.magic = 'ZM'; dos.lfanew = 0x40;
        fwrite(&dos, 1, sizeof(dos), f);
        fseek(f, 0x40, SEEK_SET);
        
        // PE Signature
        DWORD peSig = 'EP'; fwrite(&peSig, 4, 1, f);
        
        // COFF Header
        COFF_FILE_HEADER coff = {};
        coff.Machine = 0x8664; // AMD64
        coff.NumberOfSections = 1;
        coff.TimeDateStamp = (DWORD)time(0);
        coff.SizeOfOptionalHeader = sizeof(OPTIONAL_HEADER64);
        coff.Characteristics = 0x22; // Executable | LargeAddressAware
        fwrite(&coff, 1, sizeof(coff), f);
        
        // Optional Header
        OPTIONAL_HEADER64 opt = {};
        opt.Magic = 0x20B; // PE32+ (64-bit)
        opt.SizeOfCode = rawCodeSize;
        opt.AddressOfEntryPoint = 0x1000;
        opt.BaseOfCode = 0x1000;
        opt.ImageBase = 0x140000000;
        opt.SectionAlignment = 0x1000;
        opt.FileAlignment = 0x200;
        opt.MajorSubsystemVersion = 6;
        opt.MinorSubsystemVersion = 0;
        opt.SizeOfImage = 0x2000;
        opt.SizeOfHeaders = 0x400;
        opt.Subsystem = 1; // NATIVE (prefer 2 for WINDOWS_GUI in real apps)
        opt.DllCharacteristics = 0x8140;
        opt.SizeOfStackReserve = 0x100000;
        opt.SizeOfStackCommit = 0x1000;
        opt.SizeOfHeapReserve = 0x100000;
        opt.SizeOfHeapCommit = 0x1000;
        opt.NumberOfRvaAndSizes = 16;
        fwrite(&opt, 1, sizeof(opt), f);
        
        // Section Header (.text)
        SECTION_HEADER sect = {};
        memcpy(sect.Name, ".text", 6);
        sect.VirtualSize = rawCodeSize;
        sect.VirtualAddress = 0x1000;
        sect.SizeOfRawData = rawCodeSize;
        sect.PointerToRawData = 0x400;
        sect.Characteristics = 0x60000020; // Code | Execute | Read
        fwrite(&sect, 1, sizeof(sect), f);
        
        // Pad to 0x400
        long pos = ftell(f);
        while (pos++ < 0x400) fputc(0, f);
        
        // Write code
        fwrite(code.data(), 1, code.size(), f);
        fclose(f);
        
        printf("\n[OK] Generated '%s' (%zu bytes code, %zu bytes file)\n", filename, code.size(), (size_t)0x400 + code.size());
    }
};

int main(int argc, char** argv) {
    if (argc < 3) { printf("Usage: %s <input.asm> <output.exe>\n", argv[0]); return 1; }
    
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f,0,SEEK_END); long sz = ftell(f); fseek(f,0,SEEK_SET);
    std::string buf(sz+1, '\0'); fread(&buf[0], 1, sz, f); fclose(f);
    
    printf("MASM Solo Compiler (Verbose)\nReading %s (%ld bytes)\n\n", argv[1], sz);
    
    int err_line = 0;
    auto toks = lex(buf.c_str(), err_line);
    printf("Lexical: %zu tokens\n", toks.size());
    
    Parser p(toks);
    Node ast = p.parse();
    printf("AST: %zu top-level nodes\n\n", ast.children.size());
    
    printf("--- Code Generation ---\n");
    CodeGen cg;
    cg.process(ast);
    cg.writePE(argv[2]);
    
    return cg.errors > 0 ? 1 : 0;
}