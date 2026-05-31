// D:\rawrxd\generator\schema_parser.cpp
// Schema Parser Implementation

#include "schema_parser.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace RawrXD::CodeGen {

SchemaParser::SchemaParser() = default;
SchemaParser::~SchemaParser() = default;

std::expected<AST::Module, ParseError> 
SchemaParser::ParseString(const std::string& schema_text) {
    input_ = schema_text;
    pos_ = 0;
    line_ = 1;
    column_ = 1;
    tokens_.clear();
    token_pos_ = 0;
    
    Tokenize();
    return ParseModule();
}

std::expected<AST::Module, ParseError> 
SchemaParser::ParseFile(const std::filesystem::path& schema_path) {
    std::ifstream file(schema_path);
    if (!file) {
        return std::unexpected(ParseError::FileNotFound);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseString(buffer.str());
}

void SchemaParser::Tokenize() {
    while (pos_ < input_.length()) {
        SkipWhitespace();
        if (pos_ >= input_.length()) break;
        
        if (auto token = NextToken(); token) {
            tokens_.push_back(*token);
        }
    }
    
    tokens_.push_back(Token{TokenType::END_OF_FILE, "", line_, column_});
}

bool SchemaParser::IsAlpha(char c) const {
    return std::isalpha(c) || c == '_';
}

bool SchemaParser::IsDigit(char c) const {
    return std::isdigit(c);
}

bool SchemaParser::IsAlphaNum(char c) const {
    return IsAlpha(c) || IsDigit(c);
}

char SchemaParser::Peek(size_t offset) const {
    if (pos_ + offset >= input_.length()) return '\0';
    return input_[pos_ + offset];
}

char SchemaParser::Consume() {
    if (pos_ >= input_.length()) return '\0';
    char c = input_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

void SchemaParser::SkipWhitespace() {
    while (pos_ < input_.length() && std::isspace(input_[pos_])) {
        Consume();
    }
}

void SchemaParser::SkipComment() {
    if (Peek() == '/' && Peek(1) == '/') {
        while (pos_ < input_.length() && Peek() != '\n') {
            Consume();
        }
        if (Peek() == '\n') Consume();
    }
}

std::expected<Token, ParseError> 
SchemaParser::NextToken() {
    SkipWhitespace();
    SkipComment();
    SkipWhitespace();
    
    if (pos_ >= input_.length()) {
        return Token{TokenType::END_OF_FILE, "", line_, column_};
    }
    
    char c = Consume();
    Token token;
    token.line = line_;
    token.column = column_ - 1;
    
    // Single-character tokens
    switch (c) {
        case '{': token.type = TokenType::LBRACE; return token;
        case '}': token.type = TokenType::RBRACE; return token;
        case '(': token.type = TokenType::LPAREN; return token;
        case ')': token.type = TokenType::RPAREN; return token;
        case ',': token.type = TokenType::COMMA; return token;
        case ':': token.type = TokenType::COLON; return token;
        case ';': token.type = TokenType::SEMICOLON; return token;
        case '@': token.type = TokenType::AT_SYMBOL; return token;
        case '*': token.type = TokenType::STAR; return token;
        case '&': token.type = TokenType::AMPERSAND; return token;
        case '?': token.type = TokenType::QUESTION; return token;
        case '+': token.type = TokenType::PLUS; return token;
        case '-': 
            if (Peek() == '>') {
                Consume();
                token.type = TokenType::ARROW;
            } else {
                token.type = TokenType::MINUS;
            }
            return token;
    }
    
    // String literals
    if (c == '"') {
        while (pos_ < input_.length() && Peek() != '"') {
            Consume();
        }
        if (Peek() == '"') Consume();
        token.type = TokenType::STRING_LITERAL;
        return token;
    }
    
    // Numbers
    if (IsDigit(c)) {
        while (pos_ < input_.length() && (IsDigit(Peek()) || Peek() == '.')) {
            token.value += Consume();
        }
        token.type = TokenType::NUMBER_LITERAL;
        return token;
    }
    
    // Identifiers and keywords
    if (IsAlpha(c)) {
        token.value += c;
        while (pos_ < input_.length() && IsAlphaNum(Peek())) {
            token.value += Consume();
        }
        
        // Keyword recognition
        if (token.value == "namespace") token.type = TokenType::KEYWORD_NAMESPACE;
        else if (token.value == "class") token.type = TokenType::KEYWORD_CLASS;
        else if (token.value == "struct") token.type = TokenType::KEYWORD_STRUCT;
        else if (token.value == "method") token.type = TokenType::KEYWORD_METHOD;
        else if (token.value == "field") token.type = TokenType::KEYWORD_FIELD;
        else if (token.value == "const") token.type = TokenType::KEYWORD_CONST;
        else if (token.value == "atomic") token.type = TokenType::KEYWORD_ATOMIC;
        else if (token.value == "fail") token.type = TokenType::KEYWORD_FAIL;
        else if (token.value == "expected") token.type = TokenType::KEYWORD_EXPECTED;
        else if (token.value == "void") token.type = TokenType::TYPE_NAME;
        else if (token.value == "bool") token.type = TokenType::TYPE_NAME;
        else if (token.value == "int") token.type = TokenType::TYPE_NAME;
        else if (token.value == "size_t") token.type = TokenType::TYPE_NAME;
        else if (token.value == "uint32_t") token.type = TokenType::TYPE_NAME;
        else if (token.value == "uint64_t") token.type = TokenType::TYPE_NAME;
        else token.type = TokenType::IDENTIFIER;
        
        return token;
    }
    
    return Token{TokenType::ERROR_TOKEN, std::string(1, c), line_, column_ - 1};
}

Token SchemaParser::Current() const {
    if (token_pos_ < tokens_.size()) {
        return tokens_[token_pos_];
    }
    return Token{TokenType::END_OF_FILE, "", 0, 0};
}

Token SchemaParser::Expect(TokenType type) {
    Token token = Current();
    if (token.type != type) {
        return Token{TokenType::ERROR_TOKEN, "", 0, 0};
    }
    token_pos_++;
    return token;
}

bool SchemaParser::Match(TokenType type) {
    if (Check(type)) {
        token_pos_++;
        return true;
    }
    return false;
}

bool SchemaParser::Match(const std::vector<TokenType>& types) {
    for (auto type : types) {
        if (Check(type)) {
            token_pos_++;
            return true;
        }
    }
    return false;
}

bool SchemaParser::Check(TokenType type) const {
    return Current().type == type;
}

std::expected<AST::Module, ParseError> 
SchemaParser::ParseModule() {
    AST::Module module;
    
    // Parse namespace (optional)
    if (Check(TokenType::KEYWORD_NAMESPACE)) {
        Match(TokenType::KEYWORD_NAMESPACE);
        auto ns_result = ParseNamespace();
        if (!ns_result) return std::unexpected(ns_result.error());
        module.namespace_name = *ns_result;
    }
    
    // Parse types
    while (!Check(TokenType::END_OF_FILE)) {
        // Parse annotations
        auto annotations = ParseAnnotations();
        
        if (Check(TokenType::KEYWORD_CLASS) || Check(TokenType::KEYWORD_STRUCT)) {
            auto type_result = ParseType();
            if (!type_result) return std::unexpected(type_result.error());
            module.types.push_back(*type_result);
        } else if (!Check(TokenType::END_OF_FILE)) {
            break;
        }
    }
    
    return module;
}

std::expected<std::string, ParseError> 
SchemaParser::ParseNamespace() {
    Match(TokenType::KEYWORD_NAMESPACE);
    
    Token ident = Expect(TokenType::IDENTIFIER);
    if (ident.type == TokenType::ERROR_TOKEN) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    std::string ns = ident.value;
    while (Match(TokenType::COLON)) {
        if (Match(TokenType::COLON)) {
            Token part = Expect(TokenType::IDENTIFIER);
            if (part.type == TokenType::ERROR_TOKEN) {
                return std::unexpected(ParseError::SyntaxError);
            }
            ns += "::" + part.value;
        }
    }
    
    return ns;
}

std::expected<std::vector<std::string>, ParseError> 
SchemaParser::ParseAnnotations() {
    std::vector<std::string> annotations;
    
    while (Match(TokenType::AT_SYMBOL)) {
        Token annot = Expect(TokenType::IDENTIFIER);
        if (annot.type == TokenType::ERROR_TOKEN) {
            return std::unexpected(ParseError::SyntaxError);
        }
        annotations.push_back(annot.value);
    }
    
    return annotations;
}

std::expected<AST::Type, ParseError> 
SchemaParser::ParseType() {
    AST::Type type;
    
    if (Match(TokenType::KEYWORD_STRUCT)) {
        type.is_struct = true;
    } else if (Match(TokenType::KEYWORD_CLASS)) {
        type.is_struct = false;
    } else {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    Token name = Expect(TokenType::IDENTIFIER);
    if (name.type == TokenType::ERROR_TOKEN) {
        return std::unexpected(ParseError::SyntaxError);
    }
    type.name = name.value;
    
    if (!Match(TokenType::LBRACE)) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    // Parse fields and methods
    while (!Check(TokenType::RBRACE) && !Check(TokenType::END_OF_FILE)) {
        if (Check(TokenType::KEYWORD_FIELD)) {
            Match(TokenType::KEYWORD_FIELD);
            auto field_result = ParseField();
            if (!field_result) return std::unexpected(field_result.error());
            type.fields.push_back(*field_result);
        } else if (Check(TokenType::KEYWORD_METHOD)) {
            Match(TokenType::KEYWORD_METHOD);
            auto method_result = ParseMethod();
            if (!method_result) return std::unexpected(method_result.error());
            type.methods.push_back(*method_result);
        } else {
            token_pos_++;
        }
    }
    
    if (!Match(TokenType::RBRACE)) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    return type;
}

std::expected<AST::Field, ParseError> 
SchemaParser::ParseField() {
    AST::Field field;
    
    auto type_result = ParseTypeName();
    if (!type_result) return std::unexpected(type_result.error());
    field.type_name = *type_result;
    
    Token name = Expect(TokenType::IDENTIFIER);
    if (name.type == TokenType::ERROR_TOKEN) {
        return std::unexpected(ParseError::SyntaxError);
    }
    field.name = name.value;
    
    if (!Match(TokenType::SEMICOLON)) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    return field;
}

std::expected<AST::Method, ParseError> 
SchemaParser::ParseMethod() {
    AST::Method method;
    
    Token name = Expect(TokenType::IDENTIFIER);
    if (name.type == TokenType::ERROR_TOKEN) {
        return std::unexpected(ParseError::SyntaxError);
    }
    method.name = name.value;
    
    if (!Match(TokenType::LPAREN)) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    // Parse parameters
    while (!Check(TokenType::RPAREN)) {
        auto param_result = ParseParameter();
        if (!param_result) return std::unexpected(param_result.error());
        method.parameters.push_back(*param_result);
        
        if (Check(TokenType::COMMA)) {
            Match(TokenType::COMMA);
        } else {
            break;
        }
    }
    
    if (!Match(TokenType::RPAREN)) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    // Parse return type
    if (Match(TokenType::ARROW)) {
        auto ret_type = ParseTypeName();
        if (!ret_type) return std::unexpected(ret_type.error());
        method.return_type = *ret_type;
    }
    
    if (!Match(TokenType::SEMICOLON)) {
        return std::unexpected(ParseError::SyntaxError);
    }
    
    return method;
}

std::expected<AST::Parameter, ParseError> 
SchemaParser::ParseParameter() {
    AST::Parameter param;
    
    if (Match(TokenType::KEYWORD_CONST)) {
        param.is_const_ref = true;
    }
    
    auto type_result = ParseTypeName();
    if (!type_result) return std::unexpected(type_result.error());
    param.type_name = *type_result;
    
    if (Match(TokenType::AMPERSAND)) {
        param.is_const_ref = true;
    }
    
    if (Match(TokenType::QUESTION)) {
        param.is_optional = true;
    }
    
    Token name = Expect(TokenType::IDENTIFIER);
    if (name.type == TokenType::ERROR_TOKEN) {
        return std::unexpected(ParseError::SyntaxError);
    }
    param.name = name.value;
    
    return param;
}

std::expected<std::string, ParseError> 
SchemaParser::ParseTypeName() {
    std::string type_name;
    
    if (Check(TokenType::TYPE_NAME) || Check(TokenType::IDENTIFIER)) {
        type_name = Current().value;
        token_pos_++;
        
        // Handle template/qualified names
        while (Match(TokenType::COLON)) {
            if (Match(TokenType::COLON)) {
                if (Check(TokenType::IDENTIFIER) || Check(TokenType::TYPE_NAME)) {
                    type_name += "::" + Current().value;
                    token_pos_++;
                }
            }
        }
        
        return type_name;
    }
    
    return std::unexpected(ParseError::UndefinedType);
}

} // namespace RawrXD::CodeGen
