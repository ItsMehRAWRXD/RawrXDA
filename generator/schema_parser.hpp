// D:\rawrxd\generator\schema_parser.hpp
// Schema Parser - Converts Schema Text to AST

#pragma once
#include "ast.hpp"
#include <expected>
#include <string>
#include <filesystem>
#include <vector>

namespace RawrXD::CodeGen {

enum class ParseError {
    SyntaxError,
    UndefinedType,
    InvalidAnnotation,
    DuplicateSymbol,
    UnexpectedToken,
    FileNotFound
};

class SchemaParser {
public:
    SchemaParser();
    ~SchemaParser();
    
    std::expected<AST::Module, ParseError> ParseString(const std::string& schema_text);
    std::expected<AST::Module, ParseError> ParseFile(const std::filesystem::path& schema_path);
    
private:
    // Lexer tokens
    enum class TokenType {
        KEYWORD_NAMESPACE, KEYWORD_CLASS, KEYWORD_STRUCT, KEYWORD_METHOD, KEYWORD_FIELD,
        KEYWORD_CONST, KEYWORD_ATOMIC, KEYWORD_FAIL, KEYWORD_EXPECTED,
        IDENTIFIER, TYPE_NAME, STRING_LITERAL, NUMBER_LITERAL,
        LBRACE, RBRACE, LPAREN, RPAREN, COMMA, COLON, SEMICOLON, ARROW,
        AT_SYMBOL, PLUS, MINUS, STAR, AMPERSAND, QUESTION,
        END_OF_FILE, ERROR_TOKEN
    };
    
    struct Token {
        TokenType type = TokenType::ERROR_TOKEN;
        std::string value;
        size_t line = 0;
        size_t column = 0;
    };
    
    // Parsing state
    std::string input_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    std::vector<Token> tokens_;
    size_t token_pos_ = 0;
    
    // Lexer methods
    void Tokenize();
    std::expected<Token, ParseError> NextToken();
    void SkipWhitespace();
    void SkipComment();
    bool IsAlpha(char c) const;
    bool IsDigit(char c) const;
    bool IsAlphaNum(char c) const;
    char Peek(size_t offset = 0) const;
    char Consume();
    
    // Parser methods
    Token Current() const;
    Token Expect(TokenType type);
    bool Match(TokenType type);
    bool Match(const std::vector<TokenType>& types);
    bool Check(TokenType type) const;
    
    std::expected<AST::Module, ParseError> ParseModule();
    std::expected<std::string, ParseError> ParseNamespace();
    std::expected<AST::Type, ParseError> ParseType();
    std::expected<std::vector<std::string>, ParseError> ParseAnnotations();
    std::expected<AST::Method, ParseError> ParseMethod();
    std::expected<AST::Field, ParseError> ParseField();
    std::expected<AST::Parameter, ParseError> ParseParameter();
    std::expected<std::string, ParseError> ParseTypeName();
};

} // namespace RawrXD::CodeGen
