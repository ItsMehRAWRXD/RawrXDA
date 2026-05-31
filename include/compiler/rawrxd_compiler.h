// =============================================================================
// RAWRXD_COMPILER.H - Pure Native C++20 Self-Hosting Compiler Infrastructure
// =============================================================================
// 
// No external dependencies:
// - No MSVC compiler driver dependency (uses cl.exe only for bootstrap)
// - No MinGW
// - No Clang
// - No MASM (ml64.exe)
// - No NASM
// - Pure C++20 with inline assembly for x64 code emission
//
// Features:
// - Self-hosting capable (can compile itself when bootstrapped)
// - C++20 subset parser
// - x64 PE code generation
// - Built-in assembler (NASM-compatible syntax, pure C++ implementation)
// - Built-in PE linker
// - 64-bit target (x64)
// - Windows host
// =============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>
#include <variant>
#include <filesystem>

namespace RawrXD {
namespace Compiler {

// =============================================================================
// ERROR HANDLING
// =============================================================================

enum class Severity { Info, Warning, Error, Fatal };

struct Location {
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
    
    bool operator==(const Location& other) const {
        return file == other.file && line == other.line && column == other.column;
    }
};

struct Diagnostic {
    Severity severity = Severity::Info;
    std::string code;
    std::string message;
    Location location;
    std::vector<std::string> notes;
};

class DiagnosticEngine {
public:
    void report(const Diagnostic& diag);
    void report(Severity sev, const std::string& code, 
                const std::string& message, const Location& loc = {});
    
    bool hasErrors() const { return errorCount_.load() > 0; }
    bool hasWarnings() const { return warningCount_.load() > 0; }
    int errorCount() const { return errorCount_.load(); }
    int warningCount() const { return warningCount_.load(); }
    
    void setWarningLevel(int level) { warningLevel_ = level; }
    void setErrorLimit(int limit) { errorLimit_ = limit; }
    
    using Callback = std::function<void(const Diagnostic&)>;
    void setCallback(Callback cb) { callback_ = std::move(cb); }
    
    void clear() { errorCount_ = 0; warningCount_ = 0; diagnostics_.clear(); }
    const std::vector<Diagnostic>& diagnostics() const { return diagnostics_; }
    
private:
    std::atomic<int> errorCount_{0};
    std::atomic<int> warningCount_{0};
    int warningLevel_ = 2;
    int errorLimit_ = 20;
    Callback callback_;
    std::mutex mutex_;
    std::vector<Diagnostic> diagnostics_;
};

// =============================================================================
// SOURCE MANAGEMENT
// =============================================================================

class SourceManager {
public:
    uint32_t openFile(const std::string& path);
    bool closeFile(uint32_t fileId);
    
    std::string_view getSource(uint32_t fileId) const;
    std::string_view getLine(uint32_t fileId, uint32_t line) const;
    char getChar(uint32_t fileId, uint32_t offset) const;
    
    Location getLocation(uint32_t fileId, uint32_t offset) const;
    std::string formatLocation(const Location& loc) const;
    
    uint32_t addIncludePath(const std::string& path);
    std::optional<uint32_t> findInclude(const std::string& name, bool isSystem);
    
private:
    struct FileInfo {
        std::string path;
        std::string content;
        std::vector<uint32_t> lineOffsets;
    };
    
    std::unordered_map<uint32_t, FileInfo> files_;
    std::vector<std::string> includePaths_;
    uint32_t nextFileId_ = 1;
    mutable std::mutex mutex_;
};

// =============================================================================
// LEXER
// =============================================================================

enum class TokenType {
    // Literals
    IntegerLiteral, FloatLiteral, StringLiteral, CharacterLiteral,
    BooleanLiteral, NullptrLiteral,
    
    // Identifier
    Identifier,
    
    // Keywords (C++20)
    KwAlignas, KwAlignof, KwAsm, KwAuto, KwBool, KwBreak, KwCase,
    KwCatch, KwChar, KwChar8_t, KwChar16_t, KwChar32_t, KwClass,
    KwConcept, KwConst, KwConsteval, KwConstexpr, KwConstinit,
    KwContinue, KwCo_await, KwCo_return, KwCo_yield, KwDecltype,
    KwDefault, KwDelete, KwDo, KwDouble, KwDynamic_cast, KwElse,
    KwEnum, KwExplicit, KwExport, KwExtern, KwFalse, KwFloat,
    KwFor, KwFriend, KwGoto, KwIf, KwInline, KwInt, KwLong,
    KwMutable, KwNamespace, KwNew, KwNoexcept, KwNullptr, KwOperator,
    KwPrivate, KwProtected, KwPublic, KwRegister, KwReinterpret_cast,
    KwRequires, KwReturn, KwShort, KwSigned, KwSizeof, KwStatic,
    KwStatic_assert, KwStatic_cast, KwStruct, KwSwitch, KwTemplate,
    KwThis, KwThread_local, KwThrow, KwTrue, KwTry, KwTypedef,
    KwTypeid, KwTypename, KwUnion, KwUnsigned, KwUsing, KwVirtual,
    KwVoid, KwVolatile, KwWchar_t, KwWhile,
    
    // Punctuation
    LeftBrace, RightBrace, LeftBracket, RightBracket,
    LeftParen, RightParen, Semicolon, Colon, Dot, Ellipsis,
    Question, Plus, Minus, Star, Slash, Percent, Caret, Ampersand,
    Pipe, Tilde, Exclaim, Assign,
    Equal, NotEqual, Less, Greater, LessEqual, GreaterEqual, Spaceship,
    PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    CaretAssign, AmpersandAssign, PipeAssign, LeftShiftAssign, RightShiftAssign,
    Comma, Arrow, PlusPlus, MinusMinus, LeftShift, RightShift,
    ScopeResolution, DotStar, ArrowStar,
    
    // Preprocessor
    Hash, HashHash,
    
    // Special
    Eof, Error, Newline
};

struct Token {
    TokenType type = TokenType::Error;
    std::string value;
    Location location;
    
    bool isKeyword() const;
    bool isLiteral() const;
    bool isOperator() const;
    bool isPunctuation() const;
};

class Lexer {
public:
    explicit Lexer(SourceManager& sourceMgr, uint32_t fileId, DiagnosticEngine& diag);
    
    Token nextToken();
    Token peekToken(size_t ahead = 0);
    bool expect(TokenType type, Token& out);
    bool consume(TokenType type);
    
    Location currentLocation() const;
    bool atEnd() const;
    
private:
    SourceManager& sourceMgr_;
    uint32_t fileId_;
    DiagnosticEngine& diag_;
    std::string_view source_;
    size_t offset_ = 0;
    uint32_t line_ = 1;
    uint32_t column_ = 1;
    
    std::vector<Token> lookahead_;
    static constexpr size_t kMaxLookahead = 3;
    
    char current() const;
    char advance();
    char peek(size_t ahead = 1) const;
    bool match(char expected);
    void skipWhitespace();
    void skipComment();
    
    Token makeToken(TokenType type, const std::string& value);
    Token makeError(const std::string& msg);
    
    Token lexIdentifier();
    Token lexNumber();
    Token lexString();
    Token lexChar();
    Token lexOperator();
    Token lexPreprocessor();
    
    static const std::unordered_map<std::string, TokenType>& keywordMap();
};

// =============================================================================
// AST
// =============================================================================

// Forward declarations
class ASTVisitor;

enum class ASTNodeType {
    TranslationUnit, NamespaceDecl, FunctionDecl, VariableDecl,
    ClassDecl, StructDecl, EnumDecl, TemplateDecl, ConceptDecl,
    CompoundStmt, IfStmt, WhileStmt, ForStmt, DoWhileStmt,
    SwitchStmt, CaseStmt, DefaultStmt, BreakStmt, ContinueStmt,
    ReturnStmt, TryStmt, CatchStmt, ThrowStmt,
    ExprStmt, DeclStmt, NullStmt,
    BinaryExpr, UnaryExpr, CallExpr, MemberExpr, ArraySubscriptExpr,
    ConditionalExpr, CastExpr, SizeofExpr, AlignofExpr, TypeidExpr,
    LiteralExpr, IdentifierExpr, ThisExpr, LambdaExpr,
    InitListExpr, DesignatedInitExpr,
    TypeName, PointerType, ReferenceType, ArrayType, FunctionType,
    TemplateType, QualifiedType, AutoType, DecltypeType,
    ParameterDecl, TemplateParameterDecl,
    AccessSpecifier, BaseClass, MemberInitializer,
    Attribute, AsmStmt, StaticAssertDecl,
    UsingDecl, UsingDirective, NamespaceAlias,
    EnumConstant, FieldDecl, MethodDecl, ConstructorDecl, DestructorDecl,
    ConversionDecl, OperatorDecl,
    CoroutineStmt, AwaitExpr, YieldExpr
};

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual ASTNodeType nodeType() const = 0;
    virtual void accept(ASTVisitor& visitor) = 0;
    virtual Location location() const { return loc_; }
    
protected:
    Location loc_;
    explicit ASTNode(Location loc) : loc_(loc) {}
};

using ASTNodePtr = std::unique_ptr<ASTNode>;

// Expression nodes
class Expr : public ASTNode {
public:
    virtual ~Expr() = default;
    bool isLValue() const { return isLValue_; }
    void setLValue(bool val) { isLValue_ = val; }
    
protected:
    explicit Expr(Location loc) : ASTNode(loc) {}
    bool isLValue_ = false;
};

class BinaryExpr : public Expr {
public:
    TokenType op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    
    BinaryExpr(Location loc, TokenType op, std::unique_ptr<Expr> left, 
               std::unique_ptr<Expr> right)
        : Expr(loc), op(op), left(std::move(left)), right(std::move(right)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::BinaryExpr; }
    void accept(ASTVisitor& visitor) override;
};

class UnaryExpr : public Expr {
public:
    TokenType op;
    std::unique_ptr<Expr> operand;
    bool postfix = false;
    
    UnaryExpr(Location loc, TokenType op, std::unique_ptr<Expr> operand, bool postfix = false)
        : Expr(loc), op(op), operand(std::move(operand)), postfix(postfix) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::UnaryExpr; }
    void accept(ASTVisitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    
    CallExpr(Location loc, std::unique_ptr<Expr> callee)
        : Expr(loc), callee(std::move(callee)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::CallExpr; }
    void accept(ASTVisitor& visitor) override;
};

class LiteralExpr : public Expr {
public:
    enum class LiteralType { Integer, Float, String, Char, Bool, Nullptr };
    LiteralType litType;
    std::string value;
    
    LiteralExpr(Location loc, LiteralType type, std::string value)
        : Expr(loc), litType(type), value(std::move(value)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::LiteralExpr; }
    void accept(ASTVisitor& visitor) override;
};

class IdentifierExpr : public Expr {
public:
    std::string name;
    
    IdentifierExpr(Location loc, std::string name)
        : Expr(loc), name(std::move(name)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::IdentifierExpr; }
    void accept(ASTVisitor& visitor) override;
};

// Statement nodes
class Stmt : public ASTNode {
public:
    virtual ~Stmt() = default;
    
protected:
    explicit Stmt(Location loc) : ASTNode(loc) {}
};

class CompoundStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> stmts;
    
    explicit CompoundStmt(Location loc) : Stmt(loc) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::CompoundStmt; }
    void accept(ASTVisitor& visitor) override;
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expr;
    
    ExprStmt(Location loc, std::unique_ptr<Expr> expr)
        : Stmt(loc), expr(std::move(expr)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::ExprStmt; }
    void accept(ASTVisitor& visitor) override;
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch;
    
    IfStmt(Location loc, std::unique_ptr<Expr> cond, 
           std::unique_ptr<Stmt> thenBranch, std::unique_ptr<Stmt> elseBranch)
        : Stmt(loc), condition(std::move(cond)), 
          thenBranch(std::move(thenBranch)), elseBranch(std::move(elseBranch)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::IfStmt; }
    void accept(ASTVisitor& visitor) override;
};

class ReturnStmt : public Stmt {
public:
    std::unique_ptr<Expr> value;
    
    ReturnStmt(Location loc, std::unique_ptr<Expr> value = nullptr)
        : Stmt(loc), value(std::move(value)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::ReturnStmt; }
    void accept(ASTVisitor& visitor) override;
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
    
    WhileStmt(Location loc, std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> body)
        : Stmt(loc), condition(std::move(cond)), body(std::move(body)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::WhileStmt; }
    void accept(ASTVisitor& visitor) override;
};

class ForStmt : public Stmt {
public:
    std::unique_ptr<Stmt> init;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> increment;
    std::unique_ptr<Stmt> body;
    
    ForStmt(Location loc, std::unique_ptr<Stmt> init,
            std::unique_ptr<Expr> cond, std::unique_ptr<Expr> inc,
            std::unique_ptr<Stmt> body)
        : Stmt(loc), init(std::move(init)), condition(std::move(cond)),
          increment(std::move(inc)), body(std::move(body)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::ForStmt; }
    void accept(ASTVisitor& visitor) override;
};

// Declaration nodes
class Decl : public ASTNode {
public:
    virtual ~Decl() = default;
    std::string name;
    
protected:
    explicit Decl(Location loc, std::string name)
        : ASTNode(loc), name(std::move(name)) {}
};

class FunctionDecl : public Decl {
public:
    std::unique_ptr<ASTNode> returnType;
    std::vector<std::unique_ptr<ASTNode>> params;
    std::unique_ptr<CompoundStmt> body;
    bool isInline = false;
    bool isStatic = false;
    bool isExtern = false;
    bool isVirtual = false;
    bool isPureVirtual = false;
    bool isOverride = false;
    bool isFinal = false;
    bool isNoexcept = false;
    bool isConstexpr = false;
    bool isConsteval = false;
    
    FunctionDecl(Location loc, std::string name)
        : Decl(loc, std::move(name)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::FunctionDecl; }
    void accept(ASTVisitor& visitor) override;
};

class VariableDecl : public Decl {
public:
    std::unique_ptr<ASTNode> type;
    std::unique_ptr<Expr> initializer;
    bool isStatic = false;
    bool isConst = false;
    bool isConstexpr = false;
    bool isThreadLocal = false;
    bool isMutable = false;
    
    VariableDecl(Location loc, std::string name)
        : Decl(loc, std::move(name)) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::VariableDecl; }
    void accept(ASTVisitor& visitor) override;
};

class ClassDecl : public Decl {
public:
    std::vector<std::unique_ptr<Decl>> members;
    std::vector<std::string> bases;
    bool isStruct = false;
    bool isUnion = false;
    bool isFinal = false;
    
    ClassDecl(Location loc, std::string name, bool isStruct = false)
        : Decl(loc, std::move(name)), isStruct(isStruct) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::ClassDecl; }
    void accept(ASTVisitor& visitor) override;
};

class NamespaceDecl : public Decl {
public:
    std::vector<std::unique_ptr<Decl>> members;
    bool isInline = false;
    
    NamespaceDecl(Location loc, std::string name, bool isInline = false)
        : Decl(loc, std::move(name)), isInline(isInline) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::NamespaceDecl; }
    void accept(ASTVisitor& visitor) override;
};

class TranslationUnit : public ASTNode {
public:
    std::vector<std::unique_ptr<Decl>> declarations;
    
    explicit TranslationUnit(Location loc) : ASTNode(loc) {}
    
    ASTNodeType nodeType() const override { return ASTNodeType::TranslationUnit; }
    void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// AST VISITOR
// =============================================================================

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    virtual void visit(TranslationUnit& node) { visitChildren(node); }
    virtual void visit(NamespaceDecl& node) { visitChildren(node); }
    virtual void visit(FunctionDecl& node) { visitChildren(node); }
    virtual void visit(VariableDecl& node) { visitChildren(node); }
    virtual void visit(ClassDecl& node) { visitChildren(node); }
    virtual void visit(CompoundStmt& node) { visitChildren(node); }
    virtual void visit(IfStmt& node) { visitChildren(node); }
    virtual void visit(WhileStmt& node) { visitChildren(node); }
    virtual void visit(ForStmt& node) { visitChildren(node); }
    virtual void visit(ReturnStmt& node) { visitChildren(node); }
    virtual void visit(ExprStmt& node) { visitChildren(node); }
    virtual void visit(BinaryExpr& node) { visitChildren(node); }
    virtual void visit(UnaryExpr& node) { visitChildren(node); }
    virtual void visit(CallExpr& node) { visitChildren(node); }
    virtual void visit(LiteralExpr& node) {}
    virtual void visit(IdentifierExpr& node) {}
    
protected:
    virtual void visitChildren(ASTNode& node) {}
};

// =============================================================================
// PARSER
// =============================================================================

class Parser {
public:
    Parser(Lexer& lexer, DiagnosticEngine& diag);
    
    std::unique_ptr<TranslationUnit> parseTranslationUnit();
    
    bool hadError() const { return hadError_; }
    
private:
    Lexer& lexer_;
    DiagnosticEngine& diag_;
    Token current_;
    bool hadError_ = false;
    
    void advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    void error(const std::string& message);
    void errorAtCurrent(const std::string& message);
    void synchronize();
    
    // Declaration parsing
    std::unique_ptr<Decl> parseDeclaration();
    std::unique_ptr<FunctionDecl> parseFunctionDecl();
    std::unique_ptr<VariableDecl> parseVariableDecl();
    std::unique_ptr<ClassDecl> parseClassDecl();
    std::unique_ptr<NamespaceDecl> parseNamespaceDecl();
    
    // Statement parsing
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<CompoundStmt> parseCompoundStatement();
    std::unique_ptr<IfStmt> parseIfStatement();
    std::unique_ptr<WhileStmt> parseWhileStatement();
    std::unique_ptr<ForStmt> parseForStatement();
    std::unique_ptr<ReturnStmt> parseReturnStatement();
    std::unique_ptr<ExprStmt> parseExpressionStatement();
    
    // Expression parsing (precedence climbing)
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAssignment();
    std::unique_ptr<Expr> parseConditional();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseBitwiseOr();
    std::unique_ptr<Expr> parseBitwiseXor();
    std::unique_ptr<Expr> parseBitwiseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseShift();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfix();
    std::unique_ptr<Expr> parsePrimary();
    
    // Type parsing
    std::unique_ptr<ASTNode> parseType();
};

// =============================================================================
// SEMANTIC ANALYSIS
// =============================================================================

class Symbol {
public:
    enum class Kind { Variable, Function, Class, Namespace, TypeAlias, 
                      Template, Concept, Enum, EnumConstant, Field, Method };
    
    Kind kind;
    std::string name;
    std::string mangledName;
    Location loc;
    std::shared_ptr<ASTNode> decl;
    std::shared_ptr<Symbol> parent;
    std::vector<std::shared_ptr<Symbol>> children;
    
    Symbol(Kind kind, std::string name, Location loc)
        : kind(kind), name(std::move(name)), loc(loc) {}
};

class SymbolTable {
public:
    void enterScope(const std::string& name = "");
    void exitScope();
    
    bool declare(std::shared_ptr<Symbol> symbol);
    std::shared_ptr<Symbol> lookup(const std::string& name);
    std::shared_ptr<Symbol> lookupCurrent(const std::string& name);
    
    std::shared_ptr<Symbol> currentScope() const { return scopeStack_.back(); }
    
private:
    std::vector<std::shared_ptr<Symbol>> scopeStack_;
    std::unordered_map<std::string, std::shared_ptr<Symbol>> globals_;
};

class SemanticAnalyzer : public ASTVisitor {
public:
    explicit SemanticAnalyzer(DiagnosticEngine& diag);
    
    bool analyze(TranslationUnit& ast);
    
    void visit(TranslationUnit& node) override;
    void visit(NamespaceDecl& node) override;
    void visit(FunctionDecl& node) override;
    void visit(VariableDecl& node) override;
    void visit(ClassDecl& node) override;
    void visit(CompoundStmt& node) override;
    void visit(IfStmt& node) override;
    void visit(WhileStmt& node) override;
    void visit(ForStmt& node) override;
    void visit(ReturnStmt& node) override;
    void visit(ExprStmt& node) override;
    void visit(BinaryExpr& node) override;
    void visit(UnaryExpr& node) override;
    void visit(CallExpr& node) override;
    void visit(LiteralExpr& node) override;
    void visit(IdentifierExpr& node) override;
    
private:
    DiagnosticEngine& diag_;
    SymbolTable symbols_;
    std::shared_ptr<Symbol> currentFunction_;
    bool inLoop_ = false;
    bool inSwitch_ = false;
    bool inTry_ = false;
    
    void checkTypeCompatibility(Expr& expr, const std::string& expected);
    void checkLValue(Expr& expr);
    void checkReturnType(Expr& expr);
};

// =============================================================================
// CODE GENERATION (x64)
// =============================================================================

enum class Register : uint8_t {
    RAX = 0, RCX = 1, RDX = 2, RBX = 3,
    RSP = 4, RBP = 5, RSI = 6, RDI = 7,
    R8 = 8, R9 = 9, R10 = 10, R11 = 11,
    R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    // 32-bit aliases
    EAX = 0, ECX = 1, EDX = 2, EBX = 3,
    ESP = 4, EBP = 5, ESI = 6, EDI = 7,
    // 16-bit aliases
    AX = 0, CX = 1, DX = 2, BX = 3,
    SP = 4, BP = 5, SI = 6, DI = 7,
    // 8-bit aliases
    AL = 0, CL = 1, DL = 2, BL = 3,
    AH = 4, CH = 5, DH = 6, BH = 7,
    SPL = 4, BPL = 5, SIL = 6, DIL = 7,
    R8B = 8, R9B = 9, R10B = 10, R11B = 11,
    R12B = 12, R13B = 13, R14B = 14, R15B = 15,
    None = 255
};

enum class X64Opcode : uint8_t {
    NOP = 0x90,
    RET = 0xC3,
    RET_FAR = 0xCB,
    PUSH_RAX = 0x50,
    PUSH_RCX = 0x51,
    PUSH_RDX = 0x52,
    PUSH_RBX = 0x53,
    PUSH_RSP = 0x54,
    PUSH_RBP = 0x55,
    PUSH_RSI = 0x56,
    PUSH_RDI = 0x57,
    POP_RAX = 0x58,
    POP_RCX = 0x59,
    POP_RDX = 0x5A,
    POP_RBX = 0x5B,
    POP_RSP = 0x5C,
    POP_RBP = 0x5D,
    POP_RSI = 0x5E,
    POP_RDI = 0x5F,
    MOV_R8_IMM = 0xB8,
    MOV_R8_RM = 0x88,
    MOV_R16_RM = 0x89,
    MOV_R32_RM = 0x89,
    MOV_R64_RM = 0x89,
    MOV_RM_R8 = 0x8A,
    MOV_RM_R16 = 0x8B,
    MOV_RM_R32 = 0x8B,
    MOV_RM_R64 = 0x8B,
    LEA = 0x8D,
    ADD = 0x01,
    SUB = 0x29,
    AND = 0x21,
    OR = 0x09,
    XOR = 0x31,
    CMP = 0x39,
    TEST = 0x85,
    JMP_REL8 = 0xEB,
    JMP_REL32 = 0xE9,
    JZ_REL8 = 0x74,
    JNZ_REL8 = 0x75,
    JL_REL8 = 0x7C,
    JGE_REL8 = 0x7D,
    JLE_REL8 = 0x7E,
    JG_REL8 = 0x7F,
    CALL_REL32 = 0xE8,
    SYSCALL = 0x0F05,
    // Two-byte opcodes
    MOVZX_R16_RM8 = 0x0FB6,
    MOVZX_R32_RM8 = 0x0FB6,
    MOVZX_R64_RM8 = 0x0FB6,
    MOVSX_R16_RM8 = 0x0FBE,
    MOVSX_R32_RM8 = 0x0FBE,
    MOVSX_R64_RM8 = 0x0FBE,
    SETZ = 0x0F94,
    SETNZ = 0x0F95,
    CMOVA = 0x0F47,
    CMOVAE = 0x0F43,
    CMOVB = 0x0F42,
    CMOVBE = 0x0F46,
    CMOVE = 0x0F44,
    CMOVG = 0x0F4F,
    CMOVGE = 0x0F4D,
    CMOVL = 0x0F4C,
    CMOVLE = 0x0F4E,
    CMOVNE = 0x0F45,
    CMOVNO = 0x0F41,
    CMOVNP = 0x0F4B,
    CMOVNS = 0x0F49,
    CMOVO = 0x0F40,
    CMOVP = 0x0F4A,
    CMOVS = 0x0F48,
    // SSE/AVX
    MOVAPS = 0x0F28,
    MOVAPD = 0x0F28,
    ADDPS = 0x0F58,
    ADDPD = 0x0F58,
    MULPS = 0x0F59,
    MULPD = 0x0F59,
    SUBPS = 0x0F5C,
    SUBPD = 0x0F5C,
    DIVPS = 0x0F5E,
    DIVPD = 0x0F5E,
    XORPS = 0x0F57,
    XORPD = 0x0F57,
    // AVX2
    VMOVAPS = 0xC5F028,
    VADDPS = 0xC5F058,
    VMULPS = 0xC5F059,
    VSUBPS = 0xC5F05C,
    VDIVPS = 0xC5F05E,
    VXORPS = 0xC5F057,
    VFMADD132PS = 0xC4E298,
    VFMADD213PS = 0xC4E2A8,
    VFMADD231PS = 0xC4E2B8,
};

class CodeBuffer {
public:
    void emit(uint8_t byte);
    void emit16(uint16_t value);
    void emit32(uint32_t value);
    void emit64(uint64_t value);
    void emitBytes(const uint8_t* data, size_t size);
    
    size_t size() const { return code_.size(); }
    size_t position() const { return position_; }
    void setPosition(size_t pos) { position_ = pos; }
    
    const std::vector<uint8_t>& data() const { return code_; }
    std::vector<uint8_t>& data() { return code_; }
    
    void* rawData() { return code_.data(); }
    
    // Label management
    size_t createLabel();
    void bindLabel(size_t label);
    void emitLabelRef(size_t label, size_t offsetSize);
    
    // Patch existing bytes
    void patch8(size_t offset, uint8_t value);
    void patch32(size_t offset, uint32_t value);
    void patch64(size_t offset, uint64_t value);
    
private:
    std::vector<uint8_t> code_;
    size_t position_ = 0;
    std::unordered_map<size_t, size_t> labels_;
    std::vector<std::pair<size_t, size_t>> pendingRefs_;
};

class X64Emitter {
public:
    explicit X64Emitter(CodeBuffer& buffer);
    
    // Stack operations
    void push(Register reg);
    void pop(Register reg);
    void pushImm32(int32_t value);
    void pushImm8(int8_t value);
    
    // Move operations
    void mov(Register dst, Register src);
    void mov(Register dst, int32_t imm);
    void mov(Register dst, int64_t imm);
    void movMemReg(Register dst, Register base, int32_t disp);
    void movRegMem(Register dst, Register base, int32_t disp);
    void lea(Register dst, Register base, int32_t disp);
    
    // Arithmetic
    void add(Register dst, Register src);
    void add(Register dst, int32_t imm);
    void sub(Register dst, Register src);
    void sub(Register dst, int32_t imm);
    void imul(Register dst, Register src);
    void idiv(Register src);
    void neg(Register reg);
    void inc(Register reg);
    void dec(Register reg);
    
    // Bitwise
    void and_(Register dst, Register src);
    void and_(Register dst, int32_t imm);
    void or_(Register dst, Register src);
    void or_(Register dst, int32_t imm);
    void xor_(Register dst, Register src);
    void xor_(Register dst, int32_t imm);
    void not_(Register reg);
    void shl(Register reg, uint8_t count);
    void shr(Register reg, uint8_t count);
    void sar(Register reg, uint8_t count);
    
    // Comparison
    void cmp(Register dst, Register src);
    void cmp(Register dst, int32_t imm);
    void test(Register dst, Register src);
    
    // Jumps
    size_t jmpRel32();
    size_t jmpRel8();
    size_t jzRel8();
    size_t jnzRel8();
    size_t jlRel8();
    size_t jgeRel8();
    size_t jleRel8();
    size_t jgRel8();
    void bindJump(size_t jumpPos);
    
    // Calls
    size_t callRel32();
    void bindCall(size_t callPos, size_t target);
    void ret();
    void ret(uint16_t popBytes);
    
    // Function prologue/epilogue
    void emitPrologue(uint32_t localSize);
    void emitEpilogue(uint32_t localSize);
    
    // SSE operations
    void movaps(Register dst, Register src);
    void addps(Register dst, Register src);
    void mulps(Register dst, Register src);
    void subps(Register dst, Register src);
    void divps(Register dst, Register src);
    void xorps(Register dst, Register src);
    
    // AVX operations
    void vmovaps(Register dst, Register src);
    void vaddps(Register dst, Register src1, Register src2);
    void vmulps(Register dst, Register src1, Register src2);
    void vsubps(Register dst, Register src1, Register src2);
    void vdivps(Register dst, Register src1, Register src2);
    void vxorps(Register dst, Register src1, Register src2);
    void vfmadd231ps(Register dst, Register src1, Register src2);
    
    // System
    void syscall();
    void cpuid();
    void rdtsc();
    void pause();
    void mfence();
    void lfence();
    void sfence();
    
    // Utility
    void nop();
    void nop(size_t count);
    void int3();
    void ud2();
    
    CodeBuffer& buffer() { return buffer_; }
    
private:
    CodeBuffer& buffer_;
    
    void emitRex(bool w, bool r, bool x, bool b);
    void emitModRM(uint8_t mod, uint8_t reg, uint8_t rm);
    void emitSIB(uint8_t scale, uint8_t index, uint8_t base);
    void emitRegisterOp(X64Opcode opcode, Register dst, Register src);
    void emitImmediateOp(X64Opcode opcode, Register dst, int32_t imm);
    void emitMemoryOp(X64Opcode opcode, Register reg, Register base, int32_t disp);
};

// =============================================================================
// PE FORMAT / LINKER
// =============================================================================

#pragma pack(push, 1)

struct DOSHeader {
    uint16_t magic = 0x5A4D;      // "MZ"
    uint16_t cblp = 0;
    uint16_t cp = 0;
    uint16_t crlc = 0;
    uint16_t cparhdr = 0;
    uint16_t minalloc = 0;
    uint16_t maxalloc = 0;
    uint16_t ss = 0;
    uint16_t sp = 0;
    uint16_t csum = 0;
    uint16_t ip = 0;
    uint16_t cs = 0;
    uint16_t lfarlc = 0;
    uint16_t ovno = 0;
    uint16_t res[4] = {};
    uint16_t oemid = 0;
    uint16_t oeminfo = 0;
    uint16_t res2[10] = {};
    uint32_t lfanew = 0x40;       // Offset to PE header
};

struct PEHeader {
    uint32_t magic = 0x00004550;  // "PE\0\0"
    uint16_t machine = 0x8664;    // AMD64
    uint16_t numSections = 0;
    uint32_t timeDateStamp = 0;
    uint32_t ptrSymbolTable = 0;
    uint32_t numSymbols = 0;
    uint16_t sizeOptionalHeader = 0;
    uint16_t characteristics = 0x22; // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
};

struct OptionalHeader64 {
    uint16_t magic = 0x20B;       // PE32+ (64-bit)
    uint8_t majorLinkerVersion = 1;
    uint8_t minorLinkerVersion = 0;
    uint32_t sizeOfCode = 0;
    uint32_t sizeOfInitializedData = 0;
    uint32_t sizeOfUninitializedData = 0;
    uint32_t entryPoint = 0;
    uint32_t baseOfCode = 0;
    uint64_t imageBase = 0x140000000;
    uint32_t sectionAlignment = 0x1000;
    uint32_t fileAlignment = 0x200;
    uint16_t majorOSVersion = 6;
    uint16_t minorOSVersion = 0;
    uint16_t majorImageVersion = 1;
    uint16_t minorImageVersion = 0;
    uint16_t majorSubsystemVersion = 6;
    uint16_t minorSubsystemVersion = 0;
    uint32_t win32VersionValue = 0;
    uint32_t sizeOfImage = 0;
    uint32_t sizeOfHeaders = 0;
    uint32_t checkSum = 0;
    uint16_t subsystem = 1;         // IMAGE_SUBSYSTEM_NATIVE
    uint16_t dllCharacteristics = 0x8160;
    uint64_t sizeOfStackReserve = 0x100000;
    uint64_t sizeOfStackCommit = 0x1000;
    uint64_t sizeOfHeapReserve = 0x100000;
    uint64_t sizeOfHeapCommit = 0x1000;
    uint32_t loaderFlags = 0;
    uint32_t numRvaAndSizes = 16;
    uint64_t dataDirectory[16] = {};
};

struct SectionHeader {
    char name[8] = {};
    uint32_t virtualSize = 0;
    uint32_t virtualAddress = 0;
    uint32_t sizeOfRawData = 0;
    uint32_t ptrToRawData = 0;
    uint32_t ptrToRelocations = 0;
    uint32_t ptrToLineNumbers = 0;
    uint16_t numRelocations = 0;
    uint16_t numLineNumbers = 0;
    uint32_t characteristics = 0;
};

struct ImportDirectoryEntry {
    uint32_t lookupTableRVA = 0;
    uint32_t timeDateStamp = 0;
    uint32_t forwarderChain = 0;
    uint32_t nameRVA = 0;
    uint32_t addressTableRVA = 0;
};

#pragma pack(pop)

class PELinker {
public:
    struct Section {
        std::string name;
        std::vector<uint8_t> data;
        uint32_t characteristics;
        uint32_t virtualAddress = 0;
        uint32_t virtualSize = 0;
    };
    
    struct Import {
        std::string dllName;
        std::vector<std::string> functions;
    };
    
    struct Relocation {
        uint32_t offset;
        uint16_t type;
    };
    
    struct Symbol {
        std::string name;
        uint32_t sectionIndex;
        uint32_t offset;
        bool isExternal;
        bool isFunction;
    };
    
    PELinker();
    
    // Section management
    size_t addSection(const std::string& name, uint32_t characteristics);
    Section& getSection(size_t index);
    size_t findSection(const std::string& name);
    
    // Code/data emission
    size_t emitCode(const std::vector<uint8_t>& code);
    size_t emitData(const std::vector<uint8_t>& data);
    size_t emitString(const std::string& str);
    size_t emitStringTable(const std::vector<std::string>& strings);
    
    // Symbol management
    void addSymbol(const std::string& name, size_t section, uint32_t offset, 
                   bool isExternal = false, bool isFunction = true);
    std::optional<Symbol> findSymbol(const std::string& name);
    
    // Import management
    void addImport(const std::string& dllName, const std::string& function);
    
    // Relocation management
    void addRelocation(size_t section, uint32_t offset, uint16_t type, 
                       const std::string& symbolName);
    
    // Build
    bool build(const std::string& outputPath);
    std::vector<uint8_t> buildToMemory();
    
    // Configuration
    void setEntryPoint(const std::string& symbolName);
    void setImageBase(uint64_t base);
    void setSubsystem(uint16_t subsystem);
    
private:
    DOSHeader dosHeader_;
    PEHeader peHeader_;
    OptionalHeader64 optionalHeader_;
    std::vector<Section> sections_;
    std::vector<Import> imports_;
    std::vector<Symbol> symbols_;
    std::unordered_map<std::string, size_t> symbolMap_;
    std::vector<std::vector<Relocation>> relocations_;
    
    std::string entryPoint_;
    uint64_t imageBase_ = 0x140000000;
    uint16_t subsystem_ = 1;
    
    uint32_t alignUp(uint32_t value, uint32_t alignment);
    void writeHeaders(std::vector<uint8_t>& output);
    void writeSections(std::vector<uint8_t>& output);
    void buildImportTable(std::vector<uint8_t>& output, uint32_t& rva);
    void applyRelocations();
    uint32_t calculateChecksum(const std::vector<uint8_t>& data);
};

// =============================================================================
// COMPILER DRIVER
// =============================================================================

struct CompileOptions {
    std::vector<std::string> inputFiles;
    std::string outputFile = "a.exe";
    std::string outputType = "exe";  // exe, dll, lib, obj
    std::vector<std::string> includePaths;
    std::vector<std::string> libraryPaths;
    std::vector<std::string> libraries;
    std::vector<std::string> defines;
    std::vector<std::string> undefines;
    
    // Optimization
    int optimizationLevel = 2;  // 0=none, 1=some, 2=full, 3=aggressive
    bool enableLTO = false;
    bool enablePGO = false;
    
    // Code generation
    bool enableAVX2 = true;
    bool enableAVX512 = false;
    bool enableSSE42 = true;
    bool positionIndependent = false;
    
    // Debug
    bool generateDebugInfo = false;
    bool optimizeForDebug = false;
    
    // Warnings
    int warningLevel = 2;
    bool warningsAsErrors = false;
    std::vector<std::string> disabledWarnings;
    
    // Standards
    int cppStandard = 20;
    bool strictConformance = true;
    bool permissive = false;
    
    // Output
    bool verbose = false;
    bool quiet = false;
    bool showTiming = false;
    bool emitAssembly = false;
    bool emitLLVM = false;
    bool emitPreprocessed = false;
    bool keepIntermediate = false;
    
    // Linking
    bool staticLink = true;
    bool stripSymbols = false;
    std::string subsystem = "console";
    uint64_t stackSize = 0x100000;
    uint64_t heapSize = 0x100000;
    
    // Security
    bool enableGS = true;
    bool enableASLR = true;
    bool enableDEP = true;
    bool enableCFGuard = true;
    bool enableSafeSEH = false;  // x64 only
    
    // Runtime
    bool staticRuntime = true;
    bool debugRuntime = false;
    bool useUnicode = true;
};

class Compiler {
public:
    Compiler();
    explicit Compiler(const CompileOptions& options);
    
    // Compilation pipeline
    bool compile();
    bool compileFile(const std::string& path);
    bool compileString(const std::string& source, const std::string& name = "<string>");
    
    // Pipeline stages
    bool lex(const std::string& source, uint32_t fileId, std::vector<Token>& tokens);
    bool parse(const std::vector<Token>& tokens, std::unique_ptr<TranslationUnit>& ast);
    bool semanticAnalyze(TranslationUnit& ast);
    bool generateCode(TranslationUnit& ast, CodeBuffer& code);
    bool link(const std::vector<CodeBuffer>& objects, const std::string& output);
    
    // Results
    bool success() const { return success_; }
    const DiagnosticEngine& diagnostics() const { return diagnostics_; }
    DiagnosticEngine& diagnostics() { return diagnostics_; }
    
    // Timing
    double lexTimeMs() const { return lexTimeMs_; }
    double parseTimeMs() const { return parseTimeMs_; }
    double semanticTimeMs() const { return semanticTimeMs_; }
    double codegenTimeMs() const { return codegenTimeMs_; }
    double linkTimeMs() const { return linkTimeMs_; }
    double totalTimeMs() const { return totalTimeMs_; }
    
private:
    CompileOptions options_;
    DiagnosticEngine diagnostics_;
    SourceManager sourceMgr_;
    bool success_ = false;
    
    // Timing
    double lexTimeMs_ = 0;
    double parseTimeMs_ = 0;
    double semanticTimeMs_ = 0;
    double codegenTimeMs_ = 0;
    double linkTimeMs_ = 0;
    double totalTimeMs_ = 0;
    
    bool runPipeline(const std::string& source, uint32_t fileId);
};

// =============================================================================
// JIT COMPILATION
// =============================================================================

class JITCompiler {
public:
    struct JITFunction {
        void* codePtr;
        size_t codeSize;
        std::string name;
        std::function<void()> deleter;
    };
    
    JITCompiler();
    ~JITCompiler();
    
    // Compile and load
    std::optional<JITFunction> compileFunction(const std::string& source, 
                                               const std::string& functionName);
    std::optional<JITFunction> compileExpression(const std::string& expr);
    
    // Execute
    template<typename R, typename... Args>
    R invoke(const JITFunction& func, Args... args) {
        using FuncType = R(*)(Args...);
        auto* f = reinterpret_cast<FuncType>(func.codePtr);
        return f(args...);
    }
    
    // Memory management
    void freeFunction(JITFunction& func);
    void clear();
    
    // Stats
    size_t allocatedMemory() const { return allocatedMemory_; }
    size_t numFunctions() const { return functions_.size(); }
    
private:
    struct MemoryBlock {
        void* base;
        size_t size;
        size_t used;
    };
    
    std::vector<MemoryBlock> blocks_;
    std::vector<JITFunction> functions_;
    size_t allocatedMemory_ = 0;
    
    void* allocateExecutableMemory(size_t size);
    bool makeExecutable(void* ptr, size_t size);
    void freeExecutableMemory(void* ptr, size_t size);
};

// =============================================================================
// INLINE ASSEMBLER (NASM-compatible syntax)
// =============================================================================

class InlineAssembler {
public:
    struct AsmInstruction {
        std::string mnemonic;
        std::vector<std::string> operands;
        Location location;
    };
    
    struct AsmLabel {
        std::string name;
        size_t offset;
    };
    
    explicit InlineAssembler(DiagnosticEngine& diag);
    
    // Parse NASM-style inline assembly
    bool parse(const std::string& asmSource, Location loc);
    
    // Assemble to machine code
    bool assemble(CodeBuffer& output);
    
    // Direct instruction emission
    void emitNop();
    void emitMov(Register dst, Register src);
    void emitMov(Register dst, int64_t imm);
    void emitPush(Register reg);
    void emitPop(Register reg);
    void emitAdd(Register dst, Register src);
    void emitSub(Register dst, Register src);
    void emitCall(const std::string& symbol);
    void emitRet();
    void emitSyscall();
    void emitLabel(const std::string& name);
    void emitJmp(const std::string& label);
    void emitJz(const std::string& label);
    void emitJnz(const std::string& label);
    
    // SSE/AVX
    void emitMovaps(Register dst, Register src);
    void emitVaddps(Register dst, Register src1, Register src2);
    void emitVmulps(Register dst, Register src1, Register src2);
    void emitVfmadd231ps(Register dst, Register src1, Register src2);
    
private:
    DiagnosticEngine& diag_;
    std::vector<AsmInstruction> instructions_;
    std::unordered_map<std::string, AsmLabel> labels_;
    std::unordered_map<std::string, size_t> pendingJumps_;
    
    bool parseInstruction(const std::string& line, AsmInstruction& inst);
    bool encodeInstruction(const AsmInstruction& inst, CodeBuffer& output);
    bool resolveLabels(CodeBuffer& output);
    
    Register parseRegister(const std::string& str);
    int64_t parseImmediate(const std::string& str);
};

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

std::string mangleName(const std::string& name, const std::vector<std::string>& params);
std::string demangleName(const std::string& mangled);
std::string registerName(Register reg);
std::string opcodeName(X64Opcode op);

// Version info
constexpr uint32_t COMPILER_VERSION_MAJOR = 1;
constexpr uint32_t COMPILER_VERSION_MINOR = 0;
constexpr uint32_t COMPILER_VERSION_PATCH = 0;
constexpr const char* COMPILER_VERSION_STRING = "1.0.0";

} // namespace Compiler
} // namespace RawrXD
