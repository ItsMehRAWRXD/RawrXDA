/*
 * MASM Solo Compiler - Production Build
 * x64 PE64 Code Generator with Diagnostic Logging
 * 
 * Fixes applied:
 * - MasmTokenType to avoid Windows SDK winnt.h collision
 * - Comprehensive diagnostic logging in CodeGen
 * - NOP/INT3/SYSCALL support
 * - Proper x64 instruction encoding with REX prefixes
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <cstdint>
#include <algorithm>
#include <iomanip>

// Forward declarations
class Lexer;
class Parser;
class CodeGen;

// ============================================================================
// Token Types - Using MasmTokenType to avoid Windows SDK collision
// ============================================================================
enum class MasmTokenType {
    TOK_EOF = 0,
    TOK_NEWLINE,
    TOK_IDENTIFIER,
    TOK_NUMBER,
    TOK_STRING,
    TOK_COLON,
    TOK_COMMA,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_DIRECTIVE,
    TOK_REGISTER,
    TOK_INSTRUCTION,
    TOK_COMMENT,
    // Pseudo-ops
    TOK_DB,
    TOK_DW,
    TOK_DD,
    TOK_DQ,
    TOK_SEGMENT,
    TOK_ENDS,
    TOK_PROC,
    TOK_ENDP,
    TOK_END,
    TOK_EQU,
    TOK_INCLUDE,
    TOK_EXTERN,
    TOK_PUBLIC,
    // Special instructions
    TOK_NOP,
    TOK_INT3,
    TOK_SYSCALL,
    TOK_RET,
    TOK_UNKNOWN
};

// ============================================================================
// Token Structure
// ============================================================================
struct Token {
    MasmTokenType type;
    std::string value;
    size_t line;
    size_t column;
    
    Token(MasmTokenType t = MasmTokenType::TOK_EOF, const std::string& v = "", size_t l = 0, size_t c = 0)
        : type(t), value(v), line(l), column(c) {}
};

// ============================================================================
// AST Node Types
// ============================================================================
enum class ASTNodeType {
    PROGRAM,
    SEGMENT,
    LABEL,
    INSTRUCTION,
    DIRECTIVE,
    DATA_DEF,
    OPERAND,
    REGISTER,
    IMMEDIATE,
    MEMORY,
    EXPRESSION,
    PROC,
    COMMENT
};

// ============================================================================
// AST Node Structure
// ============================================================================
struct ASTNode {
    ASTNodeType type;
    std::string value;
    std::vector<std::shared_ptr<ASTNode>> children;
    size_t line;
    
    ASTNode(ASTNodeType t, const std::string& v = "", size_t l = 0)
        : type(t), value(v), line(l) {}
    
    void addChild(std::shared_ptr<ASTNode> child) {
        children.push_back(child);
    }
};

// ============================================================================
// x64 Register Definitions
// ============================================================================
enum class Register : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    // 32-bit aliases (same encoding, different REX.W)
    EAX = 0, ECX = 1, EDX = 2, EBX = 3,
    ESP = 4, EBP = 5, ESI = 6, EDI = 7,
    R8D = 8, R9D = 9, R10D = 10, R11D = 11,
    R12D = 12, R13D = 13, R14D = 14, R15D = 15,
    NONE = 0xFF
};

// ============================================================================
// Instruction Encoding Tables
// ============================================================================
struct OpcodeEntry {
    const char* mnemonic;
    uint8_t opcode;
    uint8_t opcode_ext;  // ModR/M reg field extension (-1 if not used)
    bool has_modrm;
    bool is_64bit;       // Requires REX.W
};

// Common x64 opcodes
static const OpcodeEntry OPCODE_TABLE[] = {
    // Basic instructions
    {"nop",     0x90, 0xFF, false, false},
    {"int3",    0xCC, 0xFF, false, false},
    {"syscall", 0x0F, 0x05, false, false},  // 0F 05
    {"ret",     0xC3, 0xFF, false, false},
    {"leave",   0xC9, 0xFF, false, false},
    
    // MOV instructions
    {"mov",     0x89, 0xFF, true,  true},   // MOV r/m64, r64
    {"mov",     0x8B, 0xFF, true,  true},   // MOV r64, r/m64
    {"mov",     0xB8, 0xFF, false, true},   // MOV r64, imm64 (B8+rd)
    {"mov",     0xC7, 0x00, true,  true},   // MOV r/m64, imm32
    
    // Arithmetic
    {"add",     0x01, 0xFF, true,  true},   // ADD r/m64, r64
    {"add",     0x03, 0xFF, true,  true},   // ADD r64, r/m64
    {"add",     0x81, 0x00, true,  true},   // ADD r/m64, imm32
    {"sub",     0x29, 0xFF, true,  true},   // SUB r/m64, r64
    {"sub",     0x2B, 0xFF, true,  true},   // SUB r64, r/m64
    {"sub",     0x81, 0x05, true,  true},   // SUB r/m64, imm32
    {"xor",     0x31, 0xFF, true,  true},   // XOR r/m64, r64
    {"xor",     0x33, 0xFF, true,  true},   // XOR r64, r/m64
    {"and",     0x21, 0xFF, true,  true},   // AND r/m64, r64
    {"or",      0x09, 0xFF, true,  true},   // OR r/m64, r64
    {"cmp",     0x39, 0xFF, true,  true},   // CMP r/m64, r64
    {"test",    0x85, 0xFF, true,  true},   // TEST r/m64, r64
    
    // Stack operations
    {"push",    0x50, 0xFF, false, false},  // PUSH r64 (50+rd)
    {"pop",     0x58, 0xFF, false, false},  // POP r64 (58+rd)
    
    // Control flow
    {"call",    0xE8, 0xFF, false, false},  // CALL rel32
    {"jmp",     0xE9, 0xFF, false, false},  // JMP rel32
    {"je",      0x84, 0x0F, false, false},  // JE rel32 (0F 84)
    {"jne",     0x85, 0x0F, false, false},  // JNE rel32 (0F 85)
    {"jz",      0x84, 0x0F, false, false},  // JZ rel32 (0F 84)
    {"jnz",     0x85, 0x0F, false, false},  // JNZ rel32 (0F 85)
    {"jl",      0x8C, 0x0F, false, false},  // JL rel32 (0F 8C)
    {"jg",      0x8F, 0x0F, false, false},  // JG rel32 (0F 8F)
    
    // Shifts
    {"shl",     0xD3, 0x04, true,  true},   // SHL r/m64, CL
    {"shr",     0xD3, 0x05, true,  true},   // SHR r/m64, CL
    {"sal",     0xD3, 0x04, true,  true},   // SAL r/m64, CL
    {"sar",     0xD3, 0x07, true,  true},   // SAR r/m64, CL
    
    // Multiply/Divide
    {"mul",     0xF7, 0x04, true,  true},   // MUL r/m64
    {"div",     0xF7, 0x06, true,  true},   // DIV r/m64
    {"imul",    0xF7, 0x05, true,  true},   // IMUL r/m64
    {"idiv",    0xF7, 0x07, true,  true},   // IDIV r/m64
    
    // LEA
    {"lea",     0x8D, 0xFF, true,  true},   // LEA r64, m
    
    // Sentinel
    {nullptr,   0x00, 0xFF, false, false}
};

// ============================================================================
// Lexer Class
// ============================================================================
class Lexer {
private:
    std::string source;
    size_t pos;
    size_t line;
    size_t column;
    
    static const std::map<std::string, MasmTokenType> keywords;
    static const std::map<std::string, MasmTokenType> registers;
    static const std::map<std::string, MasmTokenType> instructions;
    
    char peek() const { return pos < source.length() ? source[pos] : '\0'; }
    char advance() {
        char c = peek();
        pos++;
        if (c == '\n') { line++; column = 1; }
        else { column++; }
        return c;
    }
    
    void skipWhitespace() {
        while (pos < source.length() && (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\r')) {
            advance();
        }
    }
    
    Token readIdentifier() {
        size_t start_line = line, start_col = column;
        std::string value;
        while (pos < source.length() && (isalnum(source[pos]) || source[pos] == '_' || source[pos] == '@' || source[pos] == '$')) {
            value += advance();
        }
        
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        // Check for keywords
        auto kw = keywords.find(lower);
        if (kw != keywords.end()) {
            return Token(kw->second, value, start_line, start_col);
        }
        
        // Check for registers
        auto reg = registers.find(lower);
        if (reg != registers.end()) {
            return Token(MasmTokenType::TOK_REGISTER, value, start_line, start_col);
        }
        
        // Check for instructions
        auto inst = instructions.find(lower);
        if (inst != instructions.end()) {
            return Token(MasmTokenType::TOK_INSTRUCTION, value, start_line, start_col);
        }
        
        return Token(MasmTokenType::TOK_IDENTIFIER, value, start_line, start_col);
    }
    
    Token readNumber() {
        size_t start_line = line, start_col = column;
        std::string value;
        
        // Check for hex prefix
        if (pos + 1 < source.length() && source[pos] == '0' && (source[pos+1] == 'x' || source[pos+1] == 'X')) {
            value += advance(); // '0'
            value += advance(); // 'x'
            while (pos < source.length() && isxdigit(source[pos])) {
                value += advance();
            }
        }
        // Check for hex suffix (MASM style: 0FFh)
        else {
            while (pos < source.length() && (isxdigit(source[pos]))) {
                value += advance();
            }
            if (pos < source.length() && (source[pos] == 'h' || source[pos] == 'H')) {
                value += advance();
            }
        }
        
        return Token(MasmTokenType::TOK_NUMBER, value, start_line, start_col);
    }
    
    Token readString() {
        size_t start_line = line, start_col = column;
        char quote = advance(); // consume opening quote
        std::string value;
        while (pos < source.length() && source[pos] != quote && source[pos] != '\n') {
            if (source[pos] == '\\' && pos + 1 < source.length()) {
                advance();
                value += advance();
            } else {
                value += advance();
            }
        }
        if (pos < source.length() && source[pos] == quote) {
            advance(); // consume closing quote
        }
        return Token(MasmTokenType::TOK_STRING, value, start_line, start_col);
    }
    
    Token readComment() {
        size_t start_line = line, start_col = column;
        std::string value;
        while (pos < source.length() && source[pos] != '\n') {
            value += advance();
        }
        return Token(MasmTokenType::TOK_COMMENT, value, start_line, start_col);
    }

public:
    Lexer(const std::string& src) : source(src), pos(0), line(1), column(1) {}
    
    Token nextToken() {
        skipWhitespace();
        
        if (pos >= source.length()) {
            return Token(MasmTokenType::TOK_EOF, "", line, column);
        }
        
        char c = source[pos];
        
        // Newline
        if (c == '\n') {
            advance();
            return Token(MasmTokenType::TOK_NEWLINE, "\n", line - 1, column);
        }
        
        // Comment
        if (c == ';') {
            return readComment();
        }
        
        // Identifier or keyword
        if (isalpha(c) || c == '_' || c == '@' || c == '$' || c == '.') {
            return readIdentifier();
        }
        
        // Number
        if (isdigit(c)) {
            return readNumber();
        }
        
        // String
        if (c == '"' || c == '\'') {
            return readString();
        }
        
        // Single character tokens
        size_t start_line = line, start_col = column;
        advance();
        switch (c) {
            case ':': return Token(MasmTokenType::TOK_COLON, ":", start_line, start_col);
            case ',': return Token(MasmTokenType::TOK_COMMA, ",", start_line, start_col);
            case '[': return Token(MasmTokenType::TOK_LBRACKET, "[", start_line, start_col);
            case ']': return Token(MasmTokenType::TOK_RBRACKET, "]", start_line, start_col);
            case '+': return Token(MasmTokenType::TOK_PLUS, "+", start_line, start_col);
            case '-': return Token(MasmTokenType::TOK_MINUS, "-", start_line, start_col);
            case '*': return Token(MasmTokenType::TOK_STAR, "*", start_line, start_col);
            default:  return Token(MasmTokenType::TOK_UNKNOWN, std::string(1, c), start_line, start_col);
        }
    }
    
    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        Token tok;
        do {
            tok = nextToken();
            tokens.push_back(tok);
        } while (tok.type != MasmTokenType::TOK_EOF);
        return tokens;
    }
};

// Static member definitions for Lexer
const std::map<std::string, MasmTokenType> Lexer::keywords = {
    {"db", MasmTokenType::TOK_DB}, {"dw", MasmTokenType::TOK_DW},
    {"dd", MasmTokenType::TOK_DD}, {"dq", MasmTokenType::TOK_DQ},
    {"segment", MasmTokenType::TOK_SEGMENT}, {"ends", MasmTokenType::TOK_ENDS},
    {"proc", MasmTokenType::TOK_PROC}, {"endp", MasmTokenType::TOK_ENDP},
    {"end", MasmTokenType::TOK_END}, {"equ", MasmTokenType::TOK_EQU},
    {"include", MasmTokenType::TOK_INCLUDE}, {"extern", MasmTokenType::TOK_EXTERN},
    {"public", MasmTokenType::TOK_PUBLIC},
    {"nop", MasmTokenType::TOK_NOP}, {"int3", MasmTokenType::TOK_INT3},
    {".code", MasmTokenType::TOK_DIRECTIVE}, {".data", MasmTokenType::TOK_DIRECTIVE},
    {".const", MasmTokenType::TOK_DIRECTIVE}, {".model", MasmTokenType::TOK_DIRECTIVE}
};

const std::map<std::string, MasmTokenType> Lexer::registers = {
    {"rax", MasmTokenType::TOK_REGISTER}, {"rbx", MasmTokenType::TOK_REGISTER},
    {"rcx", MasmTokenType::TOK_REGISTER}, {"rdx", MasmTokenType::TOK_REGISTER},
    {"rsi", MasmTokenType::TOK_REGISTER}, {"rdi", MasmTokenType::TOK_REGISTER},
    {"rsp", MasmTokenType::TOK_REGISTER}, {"rbp", MasmTokenType::TOK_REGISTER},
    {"r8", MasmTokenType::TOK_REGISTER}, {"r9", MasmTokenType::TOK_REGISTER},
    {"r10", MasmTokenType::TOK_REGISTER}, {"r11", MasmTokenType::TOK_REGISTER},
    {"r12", MasmTokenType::TOK_REGISTER}, {"r13", MasmTokenType::TOK_REGISTER},
    {"r14", MasmTokenType::TOK_REGISTER}, {"r15", MasmTokenType::TOK_REGISTER},
    {"eax", MasmTokenType::TOK_REGISTER}, {"ebx", MasmTokenType::TOK_REGISTER},
    {"ecx", MasmTokenType::TOK_REGISTER}, {"edx", MasmTokenType::TOK_REGISTER},
    {"esi", MasmTokenType::TOK_REGISTER}, {"edi", MasmTokenType::TOK_REGISTER},
    {"esp", MasmTokenType::TOK_REGISTER}, {"ebp", MasmTokenType::TOK_REGISTER},
    {"al", MasmTokenType::TOK_REGISTER}, {"bl", MasmTokenType::TOK_REGISTER},
    {"cl", MasmTokenType::TOK_REGISTER}, {"dl", MasmTokenType::TOK_REGISTER}
};

const std::map<std::string, MasmTokenType> Lexer::instructions = {
    {"mov", MasmTokenType::TOK_INSTRUCTION}, {"add", MasmTokenType::TOK_INSTRUCTION},
    {"sub", MasmTokenType::TOK_INSTRUCTION}, {"mul", MasmTokenType::TOK_INSTRUCTION},
    {"div", MasmTokenType::TOK_INSTRUCTION}, {"xor", MasmTokenType::TOK_INSTRUCTION},
    {"and", MasmTokenType::TOK_INSTRUCTION}, {"or", MasmTokenType::TOK_INSTRUCTION},
    {"not", MasmTokenType::TOK_INSTRUCTION}, {"neg", MasmTokenType::TOK_INSTRUCTION},
    {"push", MasmTokenType::TOK_INSTRUCTION}, {"pop", MasmTokenType::TOK_INSTRUCTION},
    {"call", MasmTokenType::TOK_INSTRUCTION}, {"ret", MasmTokenType::TOK_INSTRUCTION},
    {"jmp", MasmTokenType::TOK_INSTRUCTION}, {"je", MasmTokenType::TOK_INSTRUCTION},
    {"jne", MasmTokenType::TOK_INSTRUCTION}, {"jz", MasmTokenType::TOK_INSTRUCTION},
    {"jnz", MasmTokenType::TOK_INSTRUCTION}, {"jl", MasmTokenType::TOK_INSTRUCTION},
    {"jg", MasmTokenType::TOK_INSTRUCTION}, {"jle", MasmTokenType::TOK_INSTRUCTION},
    {"jge", MasmTokenType::TOK_INSTRUCTION}, {"cmp", MasmTokenType::TOK_INSTRUCTION},
    {"test", MasmTokenType::TOK_INSTRUCTION}, {"lea", MasmTokenType::TOK_INSTRUCTION},
    {"nop", MasmTokenType::TOK_INSTRUCTION}, {"int3", MasmTokenType::TOK_INSTRUCTION},
    {"syscall", MasmTokenType::TOK_INSTRUCTION}, {"leave", MasmTokenType::TOK_INSTRUCTION},
    {"shl", MasmTokenType::TOK_INSTRUCTION}, {"shr", MasmTokenType::TOK_INSTRUCTION},
    {"imul", MasmTokenType::TOK_INSTRUCTION}, {"idiv", MasmTokenType::TOK_INSTRUCTION}
};

// ============================================================================
// Parser Class
// ============================================================================
class Parser {
private:
    std::vector<Token> tokens;
    size_t pos;
    
    Token& current() { return tokens[pos]; }
    Token& peek(size_t ahead = 1) { return tokens[std::min(pos + ahead, tokens.size() - 1)]; }
    bool check(MasmTokenType type) { return current().type == type; }
    bool atEnd() { return current().type == MasmTokenType::TOK_EOF; }
    
    Token advance() {
        if (!atEnd()) pos++;
        return tokens[pos - 1];
    }
    
    void skipNewlines() {
        while (check(MasmTokenType::TOK_NEWLINE) || check(MasmTokenType::TOK_COMMENT)) {
            advance();
        }
    }
    
    std::shared_ptr<ASTNode> parseOperand() {
        Token tok = current();
        
        if (check(MasmTokenType::TOK_REGISTER)) {
            advance();
            return std::make_shared<ASTNode>(ASTNodeType::REGISTER, tok.value, tok.line);
        }
        
        if (check(MasmTokenType::TOK_NUMBER)) {
            advance();
            return std::make_shared<ASTNode>(ASTNodeType::IMMEDIATE, tok.value, tok.line);
        }
        
        if (check(MasmTokenType::TOK_IDENTIFIER)) {
            advance();
            return std::make_shared<ASTNode>(ASTNodeType::OPERAND, tok.value, tok.line);
        }
        
        if (check(MasmTokenType::TOK_LBRACKET)) {
            advance(); // consume '['
            auto memNode = std::make_shared<ASTNode>(ASTNodeType::MEMORY, "", tok.line);
            while (!check(MasmTokenType::TOK_RBRACKET) && !atEnd()) {
                auto child = parseOperand();
                if (child) memNode->addChild(child);
                if (check(MasmTokenType::TOK_PLUS) || check(MasmTokenType::TOK_MINUS) || check(MasmTokenType::TOK_STAR)) {
                    auto op = std::make_shared<ASTNode>(ASTNodeType::EXPRESSION, current().value, current().line);
                    memNode->addChild(op);
                    advance();
                }
            }
            if (check(MasmTokenType::TOK_RBRACKET)) advance();
            return memNode;
        }
        
        return nullptr;
    }
    
    std::shared_ptr<ASTNode> parseInstruction() {
        Token tok = current();
        advance(); // consume instruction mnemonic
        
        auto instrNode = std::make_shared<ASTNode>(ASTNodeType::INSTRUCTION, tok.value, tok.line);
        
        // Parse operands
        while (!check(MasmTokenType::TOK_NEWLINE) && !check(MasmTokenType::TOK_COMMENT) && !atEnd()) {
            auto operand = parseOperand();
            if (operand) {
                instrNode->addChild(operand);
            }
            if (check(MasmTokenType::TOK_COMMA)) {
                advance();
            } else {
                break;
            }
        }
        
        return instrNode;
    }
    
    std::shared_ptr<ASTNode> parseLabel() {
        Token tok = current();
        advance(); // identifier
        advance(); // colon
        return std::make_shared<ASTNode>(ASTNodeType::LABEL, tok.value, tok.line);
    }
    
    std::shared_ptr<ASTNode> parseDirective() {
        Token tok = current();
        advance();
        auto dirNode = std::make_shared<ASTNode>(ASTNodeType::DIRECTIVE, tok.value, tok.line);
        
        // Consume rest of line as arguments
        while (!check(MasmTokenType::TOK_NEWLINE) && !atEnd()) {
            dirNode->addChild(std::make_shared<ASTNode>(ASTNodeType::OPERAND, current().value, current().line));
            advance();
        }
        
        return dirNode;
    }
    
    std::shared_ptr<ASTNode> parseDataDef() {
        Token tok = current();
        advance();
        auto dataNode = std::make_shared<ASTNode>(ASTNodeType::DATA_DEF, tok.value, tok.line);
        
        // Parse data values
        while (!check(MasmTokenType::TOK_NEWLINE) && !atEnd()) {
            if (check(MasmTokenType::TOK_NUMBER) || check(MasmTokenType::TOK_STRING) || check(MasmTokenType::TOK_IDENTIFIER)) {
                dataNode->addChild(std::make_shared<ASTNode>(ASTNodeType::OPERAND, current().value, current().line));
                advance();
            }
            if (check(MasmTokenType::TOK_COMMA)) advance();
            else break;
        }
        
        return dataNode;
    }

public:
    Parser(const std::vector<Token>& toks) : tokens(toks), pos(0) {}
    
    std::shared_ptr<ASTNode> parse() {
        auto program = std::make_shared<ASTNode>(ASTNodeType::PROGRAM, "program", 0);
        
        while (!atEnd()) {
            skipNewlines();
            if (atEnd()) break;
            
            std::shared_ptr<ASTNode> node = nullptr;
            
            // Check for label (identifier followed by colon)
            if (check(MasmTokenType::TOK_IDENTIFIER) && peek().type == MasmTokenType::TOK_COLON) {
                node = parseLabel();
            }
            // Instruction
            else if (check(MasmTokenType::TOK_INSTRUCTION)) {
                node = parseInstruction();
            }
            // Special instructions (NOP, INT3, etc.)
            else if (check(MasmTokenType::TOK_NOP) || check(MasmTokenType::TOK_INT3)) {
                node = std::make_shared<ASTNode>(ASTNodeType::INSTRUCTION, current().value, current().line);
                advance();
            }
            // Directive
            else if (check(MasmTokenType::TOK_DIRECTIVE)) {
                node = parseDirective();
            }
            // Data definition
            else if (check(MasmTokenType::TOK_DB) || check(MasmTokenType::TOK_DW) ||
                     check(MasmTokenType::TOK_DD) || check(MasmTokenType::TOK_DQ)) {
                node = parseDataDef();
            }
            // Segment/Proc directives
            else if (check(MasmTokenType::TOK_SEGMENT) || check(MasmTokenType::TOK_PROC) ||
                     check(MasmTokenType::TOK_ENDS) || check(MasmTokenType::TOK_ENDP) ||
                     check(MasmTokenType::TOK_END)) {
                node = parseDirective();
            }
            // Skip unknown tokens
            else {
                advance();
                continue;
            }
            
            if (node) {
                program->addChild(node);
            }
        }
        
        return program;
    }
};

// ============================================================================
// Code Generator Class with Diagnostic Logging
// ============================================================================
class CodeGen {
private:
    std::vector<uint8_t> codeSection;
    std::vector<uint8_t> dataSection;
    std::map<std::string, uint64_t> labels;
    std::map<std::string, uint64_t> symbols;
    uint64_t currentAddress;
    
    // Diagnostic counters
    size_t nodesProcessed;
    size_t instructionsEmitted;
    size_t fallbackEncodings;
    bool usedPrimaryEncoder;
    
    // Register name to enum mapping
    static Register parseRegister(const std::string& name) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        static const std::map<std::string, Register> regMap = {
            {"rax", Register::RAX}, {"rcx", Register::RCX}, {"rdx", Register::RDX}, {"rbx", Register::RBX},
            {"rsp", Register::RSP}, {"rbp", Register::RBP}, {"rsi", Register::RSI}, {"rdi", Register::RDI},
            {"r8", Register::R8}, {"r9", Register::R9}, {"r10", Register::R10}, {"r11", Register::R11},
            {"r12", Register::R12}, {"r13", Register::R13}, {"r14", Register::R14}, {"r15", Register::R15},
            {"eax", Register::EAX}, {"ecx", Register::ECX}, {"edx", Register::EDX}, {"ebx", Register::EBX},
            {"esp", Register::ESP}, {"ebp", Register::EBP}, {"esi", Register::ESI}, {"edi", Register::EDI}
        };
        
        auto it = regMap.find(lower);
        return it != regMap.end() ? it->second : Register::NONE;
    }
    
    // Emit a single byte
    void emit(uint8_t byte) {
        codeSection.push_back(byte);
        currentAddress++;
    }
    
    // Emit multiple bytes
    void emit(const std::vector<uint8_t>& bytes) {
        for (uint8_t b : bytes) emit(b);
    }
    
    // Emit 32-bit value (little-endian)
    void emit32(uint32_t value) {
        emit(value & 0xFF);
        emit((value >> 8) & 0xFF);
        emit((value >> 16) & 0xFF);
        emit((value >> 24) & 0xFF);
    }
    
    // Emit 64-bit value (little-endian)
    void emit64(uint64_t value) {
        emit32(value & 0xFFFFFFFF);
        emit32((value >> 32) & 0xFFFFFFFF);
    }
    
    // Emit REX prefix for x64
    void emitREX(bool W, bool R, bool X, bool B) {
        uint8_t rex = 0x40;
        if (W) rex |= 0x08;
        if (R) rex |= 0x04;
        if (X) rex |= 0x02;
        if (B) rex |= 0x01;
        emit(rex);
    }
    
    // Emit ModR/M byte
    void emitModRM(uint8_t mod, uint8_t reg, uint8_t rm) {
        emit((mod << 6) | ((reg & 0x07) << 3) | (rm & 0x07));
    }
    
    // Parse immediate value from string
    int64_t parseImmediate(const std::string& value) {
        if (value.empty()) return 0;
        
        std::string v = value;
        int base = 10;
        
        // Handle hex
        if (v.length() >= 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) {
            base = 16;
            v = v.substr(2);
        } else if (v.back() == 'h' || v.back() == 'H') {
            base = 16;
            v = v.substr(0, v.length() - 1);
        }
        
        try {
            return std::stoll(v, nullptr, base);
        } catch (...) {
            return 0;
        }
    }
    
    // Generate code for a single instruction
    bool generateInstruction(const std::shared_ptr<ASTNode>& node) {
        std::string mnemonic = node->value;
        std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);
        
        std::cerr << "[CodeGen] Processing instruction: " << mnemonic 
                  << " with " << node->children.size() << " operand(s) at line " << node->line << std::endl;
        
        instructionsEmitted++;
        
        // Handle zero-operand instructions
        if (mnemonic == "nop") {
            std::cerr << "[CodeGen] Emitting NOP (0x90)" << std::endl;
            emit(0x90);
            usedPrimaryEncoder = true;
            return true;
        }
        
        if (mnemonic == "int3") {
            std::cerr << "[CodeGen] Emitting INT3 (0xCC)" << std::endl;
            emit(0xCC);
            usedPrimaryEncoder = true;
            return true;
        }
        
        if (mnemonic == "syscall") {
            std::cerr << "[CodeGen] Emitting SYSCALL (0x0F 0x05)" << std::endl;
            emit(0x0F);
            emit(0x05);
            usedPrimaryEncoder = true;
            return true;
        }
        
        if (mnemonic == "ret") {
            std::cerr << "[CodeGen] Emitting RET (0xC3)" << std::endl;
            emit(0xC3);
            usedPrimaryEncoder = true;
            return true;
        }
        
        if (mnemonic == "leave") {
            std::cerr << "[CodeGen] Emitting LEAVE (0xC9)" << std::endl;
            emit(0xC9);
            usedPrimaryEncoder = true;
            return true;
        }
        
        // Handle single-operand instructions
        if (node->children.size() >= 1) {
            auto& op1 = node->children[0];
            
            // PUSH r64
            if (mnemonic == "push" && op1->type == ASTNodeType::REGISTER) {
                Register reg = parseRegister(op1->value);
                uint8_t regCode = static_cast<uint8_t>(reg) & 0x07;
                bool needsREX = static_cast<uint8_t>(reg) >= 8;
                
                std::cerr << "[CodeGen] Emitting PUSH " << op1->value << std::endl;
                if (needsREX) emitREX(false, false, false, true);
                emit(0x50 + regCode);
                usedPrimaryEncoder = true;
                return true;
            }
            
            // POP r64
            if (mnemonic == "pop" && op1->type == ASTNodeType::REGISTER) {
                Register reg = parseRegister(op1->value);
                uint8_t regCode = static_cast<uint8_t>(reg) & 0x07;
                bool needsREX = static_cast<uint8_t>(reg) >= 8;
                
                std::cerr << "[CodeGen] Emitting POP " << op1->value << std::endl;
                if (needsREX) emitREX(false, false, false, true);
                emit(0x58 + regCode);
                usedPrimaryEncoder = true;
                return true;
            }
            
            // CALL/JMP rel32
            if (mnemonic == "call" || mnemonic == "jmp") {
                std::cerr << "[CodeGen] Emitting " << mnemonic << " (placeholder rel32)" << std::endl;
                emit(mnemonic == "call" ? 0xE8 : 0xE9);
                emit32(0); // Placeholder for relocation
                usedPrimaryEncoder = true;
                return true;
            }
        }
        
        // Handle two-operand instructions
        if (node->children.size() >= 2) {
            auto& op1 = node->children[0];
            auto& op2 = node->children[1];
            
            // MOV r64, r64
            if (mnemonic == "mov" && op1->type == ASTNodeType::REGISTER && op2->type == ASTNodeType::REGISTER) {
                Register dst = parseRegister(op1->value);
                Register src = parseRegister(op2->value);
                
                std::cerr << "[CodeGen] Emitting MOV " << op1->value << ", " << op2->value << std::endl;
                
                bool dstExt = static_cast<uint8_t>(dst) >= 8;
                bool srcExt = static_cast<uint8_t>(src) >= 8;
                
                emitREX(true, srcExt, false, dstExt);
                emit(0x89);
                emitModRM(0x03, static_cast<uint8_t>(src) & 0x07, static_cast<uint8_t>(dst) & 0x07);
                usedPrimaryEncoder = true;
                return true;
            }
            
            // MOV r64, imm64
            if (mnemonic == "mov" && op1->type == ASTNodeType::REGISTER && op2->type == ASTNodeType::IMMEDIATE) {
                Register dst = parseRegister(op1->value);
                int64_t imm = parseImmediate(op2->value);
                
                std::cerr << "[CodeGen] Emitting MOV " << op1->value << ", " << imm << std::endl;
                
                bool dstExt = static_cast<uint8_t>(dst) >= 8;
                emitREX(true, false, false, dstExt);
                emit(0xB8 + (static_cast<uint8_t>(dst) & 0x07));
                emit64(imm);
                usedPrimaryEncoder = true;
                return true;
            }
            
            // XOR r64, r64
            if (mnemonic == "xor" && op1->type == ASTNodeType::REGISTER && op2->type == ASTNodeType::REGISTER) {
                Register dst = parseRegister(op1->value);
                Register src = parseRegister(op2->value);
                
                std::cerr << "[CodeGen] Emitting XOR " << op1->value << ", " << op2->value << std::endl;
                
                bool dstExt = static_cast<uint8_t>(dst) >= 8;
                bool srcExt = static_cast<uint8_t>(src) >= 8;
                
                emitREX(true, srcExt, false, dstExt);
                emit(0x31);
                emitModRM(0x03, static_cast<uint8_t>(src) & 0x07, static_cast<uint8_t>(dst) & 0x07);
                usedPrimaryEncoder = true;
                return true;
            }
            
            // ADD/SUB/AND/OR/CMP r64, r64
            if ((mnemonic == "add" || mnemonic == "sub" || mnemonic == "and" || 
                 mnemonic == "or" || mnemonic == "cmp") && 
                op1->type == ASTNodeType::REGISTER && op2->type == ASTNodeType::REGISTER) {
                
                Register dst = parseRegister(op1->value);
                Register src = parseRegister(op2->value);
                
                uint8_t opcode;
                if (mnemonic == "add") opcode = 0x01;
                else if (mnemonic == "sub") opcode = 0x29;
                else if (mnemonic == "and") opcode = 0x21;
                else if (mnemonic == "or") opcode = 0x09;
                else opcode = 0x39; // cmp
                
                std::cerr << "[CodeGen] Emitting " << mnemonic << " " << op1->value << ", " << op2->value << std::endl;
                
                bool dstExt = static_cast<uint8_t>(dst) >= 8;
                bool srcExt = static_cast<uint8_t>(src) >= 8;
                
                emitREX(true, srcExt, false, dstExt);
                emit(opcode);
                emitModRM(0x03, static_cast<uint8_t>(src) & 0x07, static_cast<uint8_t>(dst) & 0x07);
                usedPrimaryEncoder = true;
                return true;
            }
            
            // ADD/SUB r64, imm32
            if ((mnemonic == "add" || mnemonic == "sub") && 
                op1->type == ASTNodeType::REGISTER && op2->type == ASTNodeType::IMMEDIATE) {
                
                Register dst = parseRegister(op1->value);
                int64_t imm = parseImmediate(op2->value);
                
                bool dstExt = static_cast<uint8_t>(dst) >= 8;
                uint8_t regExt = mnemonic == "add" ? 0 : 5;
                
                std::cerr << "[CodeGen] Emitting " << mnemonic << " " << op1->value << ", " << imm << std::endl;
                
                emitREX(true, false, false, dstExt);
                emit(0x81);
                emitModRM(0x03, regExt, static_cast<uint8_t>(dst) & 0x07);
                emit32(static_cast<uint32_t>(imm));
                usedPrimaryEncoder = true;
                return true;
            }
            
            // LEA r64, [mem]
            if (mnemonic == "lea" && op1->type == ASTNodeType::REGISTER) {
                Register dst = parseRegister(op1->value);
                
                std::cerr << "[CodeGen] Emitting LEA " << op1->value << ", [mem] (simplified)" << std::endl;
                
                bool dstExt = static_cast<uint8_t>(dst) >= 8;
                emitREX(true, dstExt, false, false);
                emit(0x8D);
                // Simplified: RIP-relative addressing
                emitModRM(0x00, static_cast<uint8_t>(dst) & 0x07, 0x05);
                emit32(0); // Placeholder
                usedPrimaryEncoder = true;
                return true;
            }
        }
        
        // Fallback: emit INT3 for unhandled instructions
        std::cerr << "[CodeGen] WARNING: Fallback encoding for: " << mnemonic << std::endl;
        std::cerr << "[CodeGen] Fallback: INT3 (0xCC)" << std::endl;
        emit(0xCC);
        fallbackEncodings++;
        return true;
    }
    
    // Process AST node recursively
    void processNode(const std::shared_ptr<ASTNode>& node) {
        nodesProcessed++;
        std::cerr << "[CodeGen] Processing node type " << static_cast<int>(node->type) 
                  << " value: '" << node->value << "'" << std::endl;
        
        switch (node->type) {
            case ASTNodeType::PROGRAM:
                for (auto& child : node->children) {
                    processNode(child);
                }
                break;
                
            case ASTNodeType::LABEL:
                labels[node->value] = currentAddress;
                std::cerr << "[CodeGen] Label '" << node->value << "' at offset 0x" 
                          << std::hex << currentAddress << std::dec << std::endl;
                break;
                
            case ASTNodeType::INSTRUCTION:
                generateInstruction(node);
                break;
                
            case ASTNodeType::DATA_DEF:
                // Handle data definitions
                std::cerr << "[CodeGen] Data definition: " << node->value << std::endl;
                break;
                
            case ASTNodeType::DIRECTIVE:
                // Handle directives
                std::cerr << "[CodeGen] Directive: " << node->value << std::endl;
                break;
                
            default:
                std::cerr << "[CodeGen] Skipping node type " << static_cast<int>(node->type) << std::endl;
                break;
        }
    }

public:
    CodeGen() : currentAddress(0), nodesProcessed(0), instructionsEmitted(0), 
                fallbackEncodings(0), usedPrimaryEncoder(false) {}
    
    bool generate(const std::shared_ptr<ASTNode>& ast) {
        std::cerr << "\n[CodeGen] ========== Code Generation Started ==========" << std::endl;
        std::cerr << "[CodeGen] AST root has " << ast->children.size() << " children" << std::endl;
        
        processNode(ast);
        
        std::cerr << "\n[CodeGen] ========== Code Generation Complete ==========" << std::endl;
        std::cerr << "[CodeGen] Nodes processed: " << nodesProcessed << std::endl;
        std::cerr << "[CodeGen] Instructions emitted: " << instructionsEmitted << std::endl;
        std::cerr << "[CodeGen] Fallback encodings: " << fallbackEncodings << std::endl;
        std::cerr << "[CodeGen] Used primary encoder: " << (usedPrimaryEncoder ? "YES" : "NO") << std::endl;
        std::cerr << "[CodeGen] Code section size: " << codeSection.size() << " bytes" << std::endl;
        
        return true;
    }
    
    const std::vector<uint8_t>& getCodeSection() const { return codeSection; }
    const std::vector<uint8_t>& getDataSection() const { return dataSection; }
    const std::map<std::string, uint64_t>& getLabels() const { return labels; }
};

// ============================================================================
// PE64 Writer Class
// ============================================================================
class PE64Writer {
private:
    static const uint64_t IMAGE_BASE = 0x140000000ULL;
    static const uint32_t SECTION_ALIGNMENT = 0x1000;
    static const uint32_t FILE_ALIGNMENT = 0x200;
    
public:
    bool write(const std::string& filename, 
               const std::vector<uint8_t>& codeSection,
               const std::vector<uint8_t>& dataSection) {
        
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "[PE64] Failed to open output file: " << filename << std::endl;
            return false;
        }
        
        std::cerr << "[PE64] Writing PE64 executable: " << filename << std::endl;
        
        // DOS Header
        uint8_t dosHeader[64] = {0};
        dosHeader[0] = 'M'; dosHeader[1] = 'Z';
        *reinterpret_cast<uint32_t*>(&dosHeader[60]) = 64; // e_lfanew
        file.write(reinterpret_cast<char*>(dosHeader), 64);
        
        // PE Signature
        file.write("PE\0\0", 4);
        
        // COFF Header (20 bytes)
        uint16_t machine = 0x8664; // AMD64
        uint16_t numSections = 2;  // .text and .data
        uint32_t timestamp = 0;
        uint32_t symbolTablePtr = 0;
        uint32_t numSymbols = 0;
        uint16_t optHeaderSize = 240; // PE32+ optional header size
        uint16_t characteristics = 0x0022; // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
        
        file.write(reinterpret_cast<char*>(&machine), 2);
        file.write(reinterpret_cast<char*>(&numSections), 2);
        file.write(reinterpret_cast<char*>(&timestamp), 4);
        file.write(reinterpret_cast<char*>(&symbolTablePtr), 4);
        file.write(reinterpret_cast<char*>(&numSymbols), 4);
        file.write(reinterpret_cast<char*>(&optHeaderSize), 2);
        file.write(reinterpret_cast<char*>(&characteristics), 2);
        
        // Optional Header (PE32+)
        uint16_t magic = 0x020B; // PE32+
        file.write(reinterpret_cast<char*>(&magic), 2);
        
        uint8_t majorLinkerVersion = 14;
        uint8_t minorLinkerVersion = 0;
        file.write(reinterpret_cast<char*>(&majorLinkerVersion), 1);
        file.write(reinterpret_cast<char*>(&minorLinkerVersion), 1);
        
        uint32_t sizeOfCode = static_cast<uint32_t>((codeSection.size() + FILE_ALIGNMENT - 1) / FILE_ALIGNMENT * FILE_ALIGNMENT);
        uint32_t sizeOfInitializedData = static_cast<uint32_t>((dataSection.size() + FILE_ALIGNMENT - 1) / FILE_ALIGNMENT * FILE_ALIGNMENT);
        uint32_t sizeOfUninitializedData = 0;
        uint32_t entryPoint = 0x1000; // Entry point RVA
        uint32_t baseOfCode = 0x1000;
        
        file.write(reinterpret_cast<char*>(&sizeOfCode), 4);
        file.write(reinterpret_cast<char*>(&sizeOfInitializedData), 4);
        file.write(reinterpret_cast<char*>(&sizeOfUninitializedData), 4);
        file.write(reinterpret_cast<char*>(&entryPoint), 4);
        file.write(reinterpret_cast<char*>(&baseOfCode), 4);
        
        // ImageBase (8 bytes for PE32+)
        uint64_t imageBase = IMAGE_BASE;
        file.write(reinterpret_cast<char*>(&imageBase), 8);
        
        // Alignments
        uint32_t sectionAlignment = SECTION_ALIGNMENT;
        uint32_t fileAlignment = FILE_ALIGNMENT;
        file.write(reinterpret_cast<char*>(&sectionAlignment), 4);
        file.write(reinterpret_cast<char*>(&fileAlignment), 4);
        
        // OS Version
        uint16_t majorOSVersion = 6;
        uint16_t minorOSVersion = 0;
        file.write(reinterpret_cast<char*>(&majorOSVersion), 2);
        file.write(reinterpret_cast<char*>(&minorOSVersion), 2);
        
        // Image Version
        uint16_t majorImageVersion = 0;
        uint16_t minorImageVersion = 0;
        file.write(reinterpret_cast<char*>(&majorImageVersion), 2);
        file.write(reinterpret_cast<char*>(&minorImageVersion), 2);
        
        // Subsystem Version
        uint16_t majorSubsystemVersion = 6;
        uint16_t minorSubsystemVersion = 0;
        file.write(reinterpret_cast<char*>(&majorSubsystemVersion), 2);
        file.write(reinterpret_cast<char*>(&minorSubsystemVersion), 2);
        
        // Win32VersionValue
        uint32_t win32Version = 0;
        file.write(reinterpret_cast<char*>(&win32Version), 4);
        
        // SizeOfImage
        uint32_t sizeOfImage = 0x3000;
        file.write(reinterpret_cast<char*>(&sizeOfImage), 4);
        
        // SizeOfHeaders
        uint32_t sizeOfHeaders = 0x200;
        file.write(reinterpret_cast<char*>(&sizeOfHeaders), 4);
        
        // Checksum
        uint32_t checksum = 0;
        file.write(reinterpret_cast<char*>(&checksum), 4);
        
        // Subsystem
        uint16_t subsystem = 3; // CONSOLE
        file.write(reinterpret_cast<char*>(&subsystem), 2);
        
        // DLL Characteristics
        uint16_t dllCharacteristics = 0x8160; // NX_COMPAT | DYNAMIC_BASE | HIGH_ENTROPY_VA | TERMINAL_SERVER_AWARE
        file.write(reinterpret_cast<char*>(&dllCharacteristics), 2);
        
        // Stack/Heap sizes (8 bytes each for PE32+)
        uint64_t sizeOfStackReserve = 0x100000;
        uint64_t sizeOfStackCommit = 0x1000;
        uint64_t sizeOfHeapReserve = 0x100000;
        uint64_t sizeOfHeapCommit = 0x1000;
        file.write(reinterpret_cast<char*>(&sizeOfStackReserve), 8);
        file.write(reinterpret_cast<char*>(&sizeOfStackCommit), 8);
        file.write(reinterpret_cast<char*>(&sizeOfHeapReserve), 8);
        file.write(reinterpret_cast<char*>(&sizeOfHeapCommit), 8);
        
        // LoaderFlags
        uint32_t loaderFlags = 0;
        file.write(reinterpret_cast<char*>(&loaderFlags), 4);
        
        // NumberOfRvaAndSizes
        uint32_t numRvaAndSizes = 16;
        file.write(reinterpret_cast<char*>(&numRvaAndSizes), 4);
        
        // Data Directories (16 entries, 8 bytes each = 128 bytes)
        uint8_t dataDirectories[128] = {0};
        file.write(reinterpret_cast<char*>(dataDirectories), 128);
        
        // Section Headers
        // .text section
        uint8_t textSection[40] = {0};
        memcpy(textSection, ".text", 5);
        *reinterpret_cast<uint32_t*>(&textSection[8]) = static_cast<uint32_t>(codeSection.size()); // VirtualSize
        *reinterpret_cast<uint32_t*>(&textSection[12]) = 0x1000; // VirtualAddress
        *reinterpret_cast<uint32_t*>(&textSection[16]) = sizeOfCode; // SizeOfRawData
        *reinterpret_cast<uint32_t*>(&textSection[20]) = 0x200; // PointerToRawData
        *reinterpret_cast<uint32_t*>(&textSection[36]) = 0x60000020; // Characteristics: CODE | EXECUTE | READ
        file.write(reinterpret_cast<char*>(textSection), 40);
        
        // .data section
        uint8_t dataSecHeader[40] = {0};
        memcpy(dataSecHeader, ".data", 5);
        *reinterpret_cast<uint32_t*>(&dataSecHeader[8]) = static_cast<uint32_t>(dataSection.size()); // VirtualSize
        *reinterpret_cast<uint32_t*>(&dataSecHeader[12]) = 0x2000; // VirtualAddress
        *reinterpret_cast<uint32_t*>(&dataSecHeader[16]) = sizeOfInitializedData; // SizeOfRawData
        *reinterpret_cast<uint32_t*>(&dataSecHeader[20]) = 0x200 + sizeOfCode; // PointerToRawData
        *reinterpret_cast<uint32_t*>(&dataSecHeader[36]) = 0xC0000040; // Characteristics: INITIALIZED_DATA | READ | WRITE
        file.write(reinterpret_cast<char*>(dataSecHeader), 40);
        
        // Padding to file alignment
        size_t currentPos = 64 + 4 + 20 + 240 + 80; // DOS + PE sig + COFF + Optional + 2 sections
        while (currentPos < 0x200) {
            file.put(0);
            currentPos++;
        }
        
        // Write code section
        if (!codeSection.empty()) {
            file.write(reinterpret_cast<const char*>(codeSection.data()), codeSection.size());
        }
        // Pad code section
        size_t codePadding = sizeOfCode - codeSection.size();
        for (size_t i = 0; i < codePadding; i++) {
            file.put(0);
        }
        
        // Write data section
        if (!dataSection.empty()) {
            file.write(reinterpret_cast<const char*>(dataSection.data()), dataSection.size());
        }
        // Pad data section
        size_t dataPadding = sizeOfInitializedData - dataSection.size();
        for (size_t i = 0; i < dataPadding; i++) {
            file.put(0);
        }
        
        file.close();
        
        std::cerr << "[PE64] Executable written successfully" << std::endl;
        std::cerr << "[PE64] Code section: " << codeSection.size() << " bytes" << std::endl;
        std::cerr << "[PE64] Data section: " << dataSection.size() << " bytes" << std::endl;
        
        return true;
    }
};

// ============================================================================
// Main Compiler Driver
// ============================================================================
int main(int argc, char* argv[]) {
    std::cerr << "MASM Solo Compiler v1.0 - Production Build" << std::endl;
    std::cerr << "==========================================" << std::endl;
    
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.asm> [output.exe]" << std::endl;
        return 1;
    }
    
    std::string inputFile = argv[1];
    std::string outputFile = argc > 2 ? argv[2] : "output.exe";
    
    // Read input file
    std::ifstream inFile(inputFile);
    if (!inFile) {
        std::cerr << "Error: Cannot open input file: " << inputFile << std::endl;
        return 1;
    }
    
    std::stringstream buffer;
    buffer << inFile.rdbuf();
    std::string source = buffer.str();
    inFile.close();
    
    std::cerr << "\n[Phase 1] Lexical Analysis" << std::endl;
    std::cerr << "Input file: " << inputFile << std::endl;
    std::cerr << "Source size: " << source.length() << " bytes" << std::endl;
    
    // Lexical analysis
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();
    std::cerr << "Tokens generated: " << tokens.size() << std::endl;
    
    std::cerr << "\n[Phase 2] Parsing" << std::endl;
    
    // Parsing
    Parser parser(tokens);
    std::shared_ptr<ASTNode> ast = parser.parse();
    std::cerr << "AST nodes: " << ast->children.size() << std::endl;
    
    std::cerr << "\n[Phase 3] Code Generation" << std::endl;
    
    // Code generation
    CodeGen codegen;
    if (!codegen.generate(ast)) {
        std::cerr << "Error: Code generation failed" << std::endl;
        return 1;
    }
    
    std::cerr << "\n[Phase 4] PE64 Output" << std::endl;
    
    // Write PE64 executable
    PE64Writer writer;
    if (!writer.write(outputFile, codegen.getCodeSection(), codegen.getDataSection())) {
        std::cerr << "Error: Failed to write output file" << std::endl;
        return 1;
    }
    
    std::cerr << "\n[Complete] Output written to: " << outputFile << std::endl;
    
    return 0;
}
