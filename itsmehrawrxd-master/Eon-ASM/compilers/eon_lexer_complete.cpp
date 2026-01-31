#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cctype>

// EON Language Token Types
enum TokenType {
    // Keywords
    TOKEN_MOV,
    TOKEN_JMP,
    TOKEN_CALL,
    TOKEN_RET,
    TOKEN_PUSH,
    TOKEN_POP,
    TOKEN_ADD,
    TOKEN_SUB,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_MOD,
    TOKEN_INC,
    TOKEN_DEC,
    TOKEN_CMP,
    TOKEN_JE,
    TOKEN_JNE,
    TOKEN_JG,
    TOKEN_JL,
    TOKEN_SECTION,
    TOKEN_GLOBAL,

    // Registers
    TOKEN_RAX, TOKEN_RBX, TOKEN_RCX, TOKEN_RDX, TOKEN_RSP, TOKEN_RBP,
    TOKEN_RSI, TOKEN_RDI, TOKEN_R8, TOKEN_R9, TOKEN_R10, TOKEN_R11,
    TOKEN_R12, TOKEN_R13, TOKEN_R14, TOKEN_R15,

    // Punctuation and Separators
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_ASTERISK,
    TOKEN_SLASH,

    // Literals and Identifiers
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER_LITERAL,
    TOKEN_STRING_LITERAL,

    // Other
    TOKEN_EOF,
    TOKEN_ILLEGAL
};

// Token structure
struct Token {
    TokenType type;
    std::string value;
};

// Map of keywords for quick lookup
std::map<std::string, TokenType> keywords = {
    {"mov", TOKEN_MOV},
    {"jmp", TOKEN_JMP},
    {"call", TOKEN_CALL},
    {"ret", TOKEN_RET},
    {"push", TOKEN_PUSH},
    {"pop", TOKEN_POP},
    {"add", TOKEN_ADD},
    {"sub", TOKEN_SUB},
    {"mul", TOKEN_MUL},
    {"div", TOKEN_DIV},
    {"mod", TOKEN_MOD},
    {"inc", TOKEN_INC},
    {"dec", TOKEN_DEC},
    {"cmp", TOKEN_CMP},
    {"je", TOKEN_JE},
    {"jne", TOKEN_JNE},
    {"jg", TOKEN_JG},
    {"jl", TOKEN_JL},
    {"section", TOKEN_SECTION},
    {"global", TOKEN_GLOBAL}
};

// Map of registers for quick lookup
std::map<std::string, TokenType> registers = {
    {"rax", TOKEN_RAX}, {"rbx", TOKEN_RBX}, {"rcx", TOKEN_RCX}, {"rdx", TOKEN_RDX},
    {"rsp", TOKEN_RSP}, {"rbp", TOKEN_RBP}, {"rsi", TOKEN_RSI}, {"rdi", TOKEN_RDI},
    {"r8", TOKEN_R8}, {"r9", TOKEN_R9}, {"r10", TOKEN_R10}, {"r11", TOKEN_R11},
    {"r12", TOKEN_R12}, {"r13", TOKEN_R13}, {"r14", TOKEN_R14}, {"r15", TOKEN_R15}
};

class EONLexer {
public:
    EONLexer(const std::string& input) : input_(input), position_(0) {}

    Token getNextToken() {
        skipWhitespace();

        if (position_ >= input_.length()) {
            return {TOKEN_EOF, ""};
        }

        char currentChar = input_[position_];

        // Handle single-character tokens
        switch (currentChar) {
            case ',':
                position_++;
                return {TOKEN_COMMA, ","};
            case ':':
                position_++;
                return {TOKEN_COLON, ":"};
            case '(':
                position_++;
                return {TOKEN_LEFT_PAREN, "("};
            case ')':
                position_++;
                return {TOKEN_RIGHT_PAREN, ")"};
            case '+':
                position_++;
                return {TOKEN_PLUS, "+"};
            case '-':
                position_++;
                return {TOKEN_MINUS, "-"};
            case '*':
                position_++;
                return {TOKEN_ASTERISK, "*"};
            case '/':
                position_++;
                return {TOKEN_SLASH, "/"};
        }

        // Handle identifiers, keywords, and registers
        if (isalpha(currentChar) || currentChar == '_') {
            std::string identifier = "";
            while (position_ < input_.length() && (isalnum(input_[position_]) || input_[position_] == '_')) {
                identifier += input_[position_];
                position_++;
            }

            // Check if it's a register
            if (registers.count(identifier)) {
                return {registers[identifier], identifier};
            }

            // Check if it's a keyword
            if (keywords.count(identifier)) {
                return {keywords[identifier], identifier};
            }

            // Otherwise, it's a generic identifier (e.g., a label or variable)
            return {TOKEN_IDENTIFIER, identifier};
        }

        // Handle integer literals (decimal for now)
        if (isdigit(currentChar)) {
            std::string number = "";
            while (position_ < input_.length() && isdigit(input_[position_])) {
                number += input_[position_];
                position_++;
            }
            return {TOKEN_INTEGER_LITERAL, number};
        }

        // Handle string literals (e.g., "Hello World")
        if (currentChar == '"') {
            position_++; // Skip the opening quote
            std::string literal = "";
            while (position_ < input_.length() && input_[position_] != '"') {
                literal += input_[position_];
                position_++;
            }
            if (input_[position_] == '"') {
                position_++; // Skip the closing quote
            }
            return {TOKEN_STRING_LITERAL, literal};
        }

        // If none of the above, it's an illegal character
        position_++;
        return {TOKEN_ILLEGAL, std::string(1, currentChar)};
    }

private:
    void skipWhitespace() {
        while (position_ < input_.length() && (isspace(input_[position_]) || input_[position_] == ';')) {
            if (input_[position_] == ';') { // Handle comments
                while (position_ < input_.length() && input_[position_] != '\n') {
                    position_++;
                }
            }
            position_++;
        }
    }

private:
    std::string input_;
    size_t position_;
};

// Helper function to convert token type to string for display
std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TOKEN_MOV: return "MOV";
        case TOKEN_JMP: return "JMP";
        case TOKEN_CALL: return "CALL";
        case TOKEN_RET: return "RET";
        case TOKEN_PUSH: return "PUSH";
        case TOKEN_POP: return "POP";
        case TOKEN_ADD: return "ADD";
        case TOKEN_SUB: return "SUB";
        case TOKEN_MUL: return "MUL";
        case TOKEN_DIV: return "DIV";
        case TOKEN_MOD: return "MOD";
        case TOKEN_INC: return "INC";
        case TOKEN_DEC: return "DEC";
        case TOKEN_CMP: return "CMP";
        case TOKEN_JE: return "JE";
        case TOKEN_JNE: return "JNE";
        case TOKEN_JG: return "JG";
        case TOKEN_JL: return "JL";
        case TOKEN_SECTION: return "SECTION";
        case TOKEN_GLOBAL: return "GLOBAL";
        case TOKEN_RAX: return "RAX";
        case TOKEN_RBX: return "RBX";
        case TOKEN_RCX: return "RCX";
        case TOKEN_RDX: return "RDX";
        case TOKEN_RSP: return "RSP";
        case TOKEN_RBP: return "RBP";
        case TOKEN_RSI: return "RSI";
        case TOKEN_RDI: return "RDI";
        case TOKEN_R8: return "R8";
        case TOKEN_R9: return "R9";
        case TOKEN_R10: return "R10";
        case TOKEN_R11: return "R11";
        case TOKEN_R12: return "R12";
        case TOKEN_R13: return "R13";
        case TOKEN_R14: return "R14";
        case TOKEN_R15: return "R15";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_COLON: return "COLON";
        case TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_ASTERISK: return "ASTERISK";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TOKEN_STRING_LITERAL: return "STRING_LITERAL";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ILLEGAL: return "ILLEGAL";
        default: return "UNKNOWN";
    }
}

// Main function to demonstrate the lexer
int main() {
    std::string sourceCode = "section .text\n"
                             "global main\n"
                             "\n"
                             "main:\n"
                             "  mov rax, 60   ; Move 60 into RAX\n"
                             "  mov rdi, 2\n"
                             "  add rax, rdi\n"
                             "  jmp exit\n"
                             "\n"
                             "exit:\n"
                             "  ret\n"
                             "\n"
                             "section .data\n"
                             "  msg db \"Hello, World!\", 0x0a\n";

    EONLexer lexer(sourceCode);
    Token token;
    
    std::cout << "🚀 n0mn0m EON Lexer - Professional Grade Tokenizer" << std::endl;
    std::cout << "=================================================" << std::endl;
    std::cout << "Source Code:" << std::endl;
    std::cout << sourceCode << std::endl;
    std::cout << "Token Stream:" << std::endl;
    std::cout << "=============" << std::endl;

    int tokenCount = 0;
    do {
        token = lexer.getNextToken();
        std::cout << "Token " << ++tokenCount << ": Type: " << tokenTypeToString(token.type) 
                  << ", Value: '" << token.value << "'" << std::endl;
    } while (token.type != TOKEN_EOF);

    std::cout << std::endl;
    std::cout << "✅ EON Lexer completed successfully!" << std::endl;
    std::cout << "📊 Total tokens processed: " << tokenCount << std::endl;
    std::cout << "🎯 Ready for next phase: EON Parser!" << std::endl;

    return 0;
}
