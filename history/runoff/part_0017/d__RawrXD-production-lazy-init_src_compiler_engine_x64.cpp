// ============================================================
// compiler_engine_x64.cpp - Main x64 compilation pipeline
// NASM/MASM → Token Stream → Parser → x64 Codegen → PE64 Emitter
// ============================================================
#include "codegen_x64.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <memory>
#include <cctype>

namespace rawrxd {
namespace compiler {

// ─── Tokenizer for assembly input ──────────────────────────────────────────
enum class TokenKind {
    TK_UNKNOWN,
    TK_LABEL,      // foo:
    TK_INSTRUCTION, // mov, add, ret, etc.
    TK_REGISTER,    // rax, rcx, etc.
    TK_IMMEDIATE,   // 0x1234, 1024
    TK_STRING,      // "text"
    TK_DIRECTIVE,   // .align, .byte, etc.
    TK_COMMA,
    TK_LPAREN,
    TK_RPAREN,
    TK_LBRACKET,
    TK_RBRACKET,
    TK_EOF
};

struct Token {
    TokenKind kind;
    std::string value;
    uint64_t numeric_value = 0;
    size_t line = 0;
    size_t column = 0;
};

class Tokenizer {
    std::string source;
    size_t pos = 0;
    size_t line = 1;
    size_t column = 0;
    
public:
    Tokenizer(const std::string& src) : source(src) {}
    
    Token next_token() {
        skip_whitespace_and_comments();
        
        if (pos >= source.size()) {
            return {TokenKind::TK_EOF, "", 0, line, column};
        }
        
        char c = source[pos];
        size_t start_line = line, start_col = column;
        
        // Labels (word followed by :)
        if (std::isalpha(c) || c == '_') {
            std::string word = read_identifier();
            
            if (pos < source.size() && source[pos] == ':') {
                pos++;
                column++;
                return {TokenKind::TK_LABEL, word, 0, start_line, start_col};
            }
            
            // Instructions
            if (is_instruction(word)) {
                return {TokenKind::TK_INSTRUCTION, word, 0, start_line, start_col};
            }
            
            // Registers
            if (is_register(word)) {
                return {TokenKind::TK_REGISTER, word, 0, start_line, start_col};
            }
            
            // Directives
            if (word[0] == '.') {
                return {TokenKind::TK_DIRECTIVE, word, 0, start_line, start_col};
            }
            
            // Identifiers (label references)
            return {TokenKind::TK_REGISTER, word, 0, start_line, start_col};
        }
        
        // Immediates (numbers)
        if (std::isdigit(c) || (c == '0' && pos+1 < source.size() && 
            source[pos+1] == 'x')) {
            uint64_t val = read_number();
            return {TokenKind::TK_IMMEDIATE, "", val, start_line, start_col};
        }
        
        // Strings
        if (c == '"') {
            std::string str = read_string();
            return {TokenKind::TK_STRING, str, 0, start_line, start_col};
        }
        
        // Single-char tokens
        pos++;
        column++;
        
        if (c == ',') return {TokenKind::TK_COMMA, ",", 0, start_line, start_col};
        if (c == '(') return {TokenKind::TK_LPAREN, "(", 0, start_line, start_col};
        if (c == ')') return {TokenKind::TK_RPAREN, ")", 0, start_line, start_col};
        if (c == '[') return {TokenKind::TK_LBRACKET, "[", 0, start_line, start_col};
        if (c == ']') return {TokenKind::TK_RBRACKET, "]", 0, start_line, start_col};
        
        return {TokenKind::TK_UNKNOWN, std::string(1, c), 0, start_line, start_col};
    }
    
private:
    void skip_whitespace_and_comments() {
        while (pos < source.size()) {
            if (source[pos] == ';') {
                while (pos < source.size() && source[pos] != '\n') pos++;
            } else if (source[pos] == '#') {
                while (pos < source.size() && source[pos] != '\n') pos++;
            } else if (source[pos] == '\n') {
                line++;
                column = 0;
                pos++;
            } else if (std::isspace(source[pos])) {
                pos++;
                column++;
            } else {
                break;
            }
        }
    }
    
    std::string read_identifier() {
        std::string result;
        while (pos < source.size() && 
               (std::isalnum(source[pos]) || source[pos] == '_')) {
            result += source[pos];
            pos++;
            column++;
        }
        return result;
    }
    
    uint64_t read_number() {
        std::string numstr;
        
        if (pos < source.size() && source[pos] == '0' && 
            pos+1 < source.size() && source[pos+1] == 'x') {
            pos += 2;
            column += 2;
            while (pos < source.size() && std::isxdigit(source[pos])) {
                numstr += source[pos];
                pos++;
                column++;
            }
            return std::stoull(numstr, nullptr, 16);
        }
        
        while (pos < source.size() && std::isdigit(source[pos])) {
            numstr += source[pos];
            pos++;
            column++;
        }
        return std::stoull(numstr, nullptr, 10);
    }
    
    std::string read_string() {
        pos++; // skip opening "
        column++;
        std::string result;
        
        while (pos < source.size() && source[pos] != '"') {
            if (source[pos] == '\\' && pos+1 < source.size()) {
                pos++;
                column++;
                char esc = source[pos];
                if (esc == 'n') result += '\n';
                else if (esc == 't') result += '\t';
                else if (esc == 'r') result += '\r';
                else result += esc;
            } else {
                result += source[pos];
            }
            pos++;
            column++;
        }
        
        if (pos < source.size() && source[pos] == '"') {
            pos++;
            column++;
        }
        return result;
    }
    
    bool is_instruction(const std::string& word) {
        static const std::string instrs[] = {
            "mov", "add", "sub", "and", "or", "xor", "push", "pop", "ret",
            "call", "jmp", "je", "jne", "jz", "jnz", "lea", "nop", "syscall",
            "int3", "cli", "sti", "hlt"
        };
        
        for (const auto& ins : instrs) {
            if (word == ins) return true;
        }
        return false;
    }
    
    bool is_register(const std::string& word) {
        static const std::string regs[] = {
            "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
            "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
            "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
        };
        
        for (const auto& reg : regs) {
            if (word == reg) return true;
        }
        return false;
    }
};

// ─── Simple Parser ────────────────────────────────────────────────────────
class Parser {
    std::vector<Token> tokens;
    size_t pos = 0;
    
public:
    Parser(const std::string& source) {
        Tokenizer tokenizer(source);
        Token tok = tokenizer.next_token();
        while (tok.kind != TokenKind::TK_EOF) {
            tokens.push_back(tok);
            tok = tokenizer.next_token();
        }
        tokens.push_back(tok); // EOF token
    }
    
    x64::ASTNode parse() {
        x64::ASTNode program;
        program.type = x64::NodeType::PROGRAM;
        
        while (current().kind != TokenKind::TK_EOF) {
            auto node = parse_statement();
            if (!node.value.empty() || !node.children.empty()) {
                program.children.push_back(node);
            }
        }
        
        return program;
    }
    
private:
    const Token& current() const {
        return tokens[std::min(pos, tokens.size() - 1)];
    }
    
    Token advance() {
        if (pos < tokens.size()) pos++;
        return tokens[std::min(pos - 1, tokens.size() - 1)];
    }
    
    bool match(TokenKind kind) {
        if (current().kind == kind) {
            advance();
            return true;
        }
        return false;
    }
    
    x64::ASTNode parse_statement() {
        x64::ASTNode node;
        
        if (current().kind == TokenKind::TK_LABEL) {
            node.type = x64::NodeType::LABEL;
            node.value = current().value;
            advance();
            return node;
        }
        
        if (current().kind == TokenKind::TK_DIRECTIVE) {
            return parse_directive();
        }
        
        if (current().kind == TokenKind::TK_INSTRUCTION) {
            return parse_instruction();
        }
        
        advance(); // Skip unknown tokens
        return node;
    }
    
    x64::ASTNode parse_directive() {
        x64::ASTNode dir;
        dir.type = x64::NodeType::DIRECTIVE;
        dir.value = current().value;
        advance();
        
        if (dir.value == ".align") {
            if (match(TokenKind::TK_IMMEDIATE)) {
                x64::ASTNode imm;
                imm.type = x64::NodeType::IMMEDIATE;
                imm.imm_data = tokens[pos - 1].numeric_value;
                dir.children.push_back(imm);
            }
        }
        
        return dir;
    }
    
    x64::ASTNode parse_instruction() {
        x64::ASTNode inst;
        inst.type = x64::NodeType::INSTRUCTION;
        inst.value = current().value;
        advance();
        
        while (current().kind != TokenKind::TK_EOF && 
               current().kind != TokenKind::TK_LABEL &&
               current().kind != TokenKind::TK_INSTRUCTION &&
               current().kind != TokenKind::TK_DIRECTIVE) {
            
            auto operand = parse_operand();
            if (!operand.value.empty() || !operand.children.empty()) {
                inst.children.push_back(operand);
            }
            
            if (!match(TokenKind::TK_COMMA)) {
                break;
            }
        }
        
        return inst;
    }
    
    x64::ASTNode parse_operand() {
        x64::ASTNode op;
        
        if (current().kind == TokenKind::TK_REGISTER) {
            op.type = x64::NodeType::REGISTER;
            op.value = current().value;
            advance();
        } else if (current().kind == TokenKind::TK_IMMEDIATE) {
            op.type = x64::NodeType::IMMEDIATE;
            op.imm_data = current().numeric_value;
            advance();
        } else if (current().kind == TokenKind::TK_LBRACKET) {
            op.type = x64::NodeType::MEMORY;
            advance();
            op.children.push_back(parse_operand());
            match(TokenKind::TK_RBRACKET);
        } else {
            // Identifier or memory ref
            op.type = x64::NodeType::IDENTIFIER;
            if (current().kind != TokenKind::TK_COMMA &&
                current().kind != TokenKind::TK_EOF) {
                op.value = current().value;
                advance();
            }
        }
        
        return op;
    }
};

// ─── Compiler Main Entry Point ────────────────────────────────────────────
class CompilerEngine {
public:
    bool compile_file(const std::string& input_path, 
                     const std::string& output_path) {
        std::ifstream in(input_path);
        if (!in) {
            std::cerr << "❌ Cannot open input file: " << input_path << std::endl;
            return false;
        }
        
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string source = buffer.str();
        
        // Parse
        Parser parser(source);
        auto ast = parser.parse();
        
        // Generate x64 code
        x64::Generator gen;
        gen.generate(ast);
        gen.apply_fixups();
        
        // Emit PE64 executable
        try {
            x64::PEGenerator::emit(output_path, gen.buffer());
            std::cout << "✅ Compilation successful: " << output_path << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "❌ Compilation failed: " << e.what() << std::endl;
            return false;
        }
    }
};

}} // namespace rawrxd::compiler

// ─── Public API Entry Point ────────────────────────────────────────────────
extern "C" {
    int RawrXD_CompileASM(const char* input_file, const char* output_file) {
        if (!input_file || !output_file) return -1;
        
        try {
            rawrxd::compiler::CompilerEngine engine;
            return engine.compile_file(input_file, output_file) ? 0 : 1;
        } catch (const std::exception& e) {
            std::cerr << "💥 Compiler crashed: " << e.what() << std::endl;
            return 2;
        }
    }
}

// Test main (can be compiled standalone)
#ifdef COMPILER_ENGINE_TEST
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: compiler <input.asm> <output.exe>" << std::endl;
        return 1;
    }
    
    rawrxd::compiler::CompilerEngine engine;
    return engine.compile_file(argv[1], argv[2]) ? 0 : 1;
}
#endif
