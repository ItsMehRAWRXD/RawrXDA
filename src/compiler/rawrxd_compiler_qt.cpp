/**
 * @file rawrxd_compiler_qt.cpp
 * @brief RawrXD Compiler Qt IDE Integration - Implementation
 * 
 * Copyright (c) 2024-2026 RawrXD IDE Project
 */

#include "rawrxd_compiler_qt.hpp"
namespace RawrXD {
namespace Compiler {

// ============================================================================
// CompilerEngine Implementation
// ============================================================================

CompilerEngine::CompilerEngine()
    
{
    // Create worker thread for async compilation
    m_workerThread = new std::thread(this);
    m_worker = new CompilerWorker();
    m_worker->;
    
    // Connect worker signals  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\nm_workerThread->start();
    
}

CompilerEngine::~CompilerEngine()
{
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

CompileResult CompilerEngine::compile(const CompileOptions& options)
{
    std::chrono::steady_clock::time_point timer;
    timer.start();
    
    // Read source file
    std::string source = readFile(options.inputFile);
    if (source.empty()) {
        CompileResult result;
        result.success = false;
        result.errorMessage = tr("Failed to read input file: %1");
        result.lastStage = CompilationStage::Failed;
        return result;
    }
    
    // Run compilation pipeline
    CompileResult result = runPipeline(source, options);
    result.compilationTimeMs = timer.elapsed();
    
    return result;
}

CompileResult CompilerEngine::compileString(const std::string& source, const CompileOptions& options)
{
    std::chrono::steady_clock::time_point timer;
    timer.start();
    
    CompileResult result = runPipeline(source, options);
    result.compilationTimeMs = timer.elapsed();
    
    return result;
}

void CompilerEngine::compileAsync(const CompileOptions& options)
{
    if (m_isCompiling.load()) {
        errorOccurred(tr("Compilation already in progress"));
        return;
    }
    
    m_isCompiling = true;
    m_cancelRequested = false;
    
    std::string source = readFile(options.inputFile);
    if (source.empty()) {
        m_isCompiling = false;
        errorOccurred(tr("Failed to read input file: %1"));
        return;
    }
    
    compilationStarted(options);
    
    // Start worker
    QMetaObject::invokeMethod(m_worker, "compile", QueuedConnection,
                              (CompileOptions, options),
                              (std::string, source));
}

void CompilerEngine::cancelCompilation()
{
    if (m_isCompiling.load()) {
        m_cancelRequested = true;
        if (m_worker) {
            QMetaObject::invokeMethod(m_worker, "cancel", QueuedConnection);
        }
        compilationCancelled();
    }
}

CompileResult CompilerEngine::runPipeline(const std::string& source, const CompileOptions& options)
{
    CompileResult result;
    result.inputSize = source.size();
    
    // Stage 1: Lexical Analysis
    emitProgress(CompilationStage::Lexing, 0, tr("Tokenizing source code..."));
    if (!runLexer(source, result)) {
        result.lastStage = CompilationStage::Lexing;
        return result;
    }
    result.tokenCount = m_tokens.size();
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 2: Parsing
    emitProgress(CompilationStage::Parsing, 15, tr("Building AST..."));
    if (!runParser(result)) {
        result.lastStage = CompilationStage::Parsing;
        return result;
    }
    result.astNodeCount = countASTNodes(m_astRoot.data());
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 3: Semantic Analysis
    emitProgress(CompilationStage::SemanticAnalysis, 30, tr("Analyzing semantics..."));
    if (!runSemanticAnalysis(result)) {
        result.lastStage = CompilationStage::SemanticAnalysis;
        return result;
    }
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 4: IR Generation
    emitProgress(CompilationStage::IRGeneration, 45, tr("Generating IR..."));
    if (!runIRGeneration(result)) {
        result.lastStage = CompilationStage::IRGeneration;
        return result;
    }
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 5: Optimization
    if (options.optimization != OptimizationLevel::None) {
        emitProgress(CompilationStage::Optimization, 55, tr("Optimizing code..."));
        if (!runOptimization(result, options.optimization)) {
            result.lastStage = CompilationStage::Optimization;
            return result;
        }
    }
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 6: Code Generation
    TargetArch target = options.target;
    if (target == TargetArch::Auto) {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
        target = TargetArch::X86_64;
#elif defined()
        target = TargetArch::ARM64;
#else
        target = TargetArch::X86_64;
#endif
    }
    
    emitProgress(CompilationStage::CodeGeneration, 70, 
                 tr("Generating %1 code...")));
    if (!runCodeGeneration(result, target)) {
        result.lastStage = CompilationStage::CodeGeneration;
        return result;
    }
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 7: Assembly
    emitProgress(CompilationStage::Assembly, 85, tr("Assembling object code..."));
    if (!runAssembler(result)) {
        result.lastStage = CompilationStage::Assembly;
        return result;
    }
    
    if (m_cancelRequested.load()) return result;
    
    // Stage 8: Linking
    std::string outputFile = options.outputFile;
    if (outputFile.empty()) {
        // Info fi(options.inputFile);
#ifdef _WIN32
        outputFile = fi.string() + "/" + fi.baseName() + ".exe";
#else
        outputFile = fi.string() + "/" + fi.baseName();
#endif
    }
    
    emitProgress(CompilationStage::Linking, 95, tr("Linking executable..."));
    if (!runLinker(result, options.format, outputFile)) {
        result.lastStage = CompilationStage::Linking;
        return result;
    }
    
    // Success
    result.success = true;
    result.outputFile = outputFile;
    result.outputSize = // FileInfo: outputFile).size();
    result.lastStage = CompilationStage::Complete;
    
    emitProgress(CompilationStage::Complete, 100, tr("Compilation successful!"));
    
    return result;
}

bool CompilerEngine::runLexer(const std::string& source, CompileResult& result)
{
    m_tokens.clear();
    
    // Tokenize source code
    int pos = 0;
    int line = 1;
    int column = 1;
    
    while (pos < source.length()) {
        QChar c = source[pos];
        
        // Skip whitespace
        if (c.isSpace()) {
            if (c == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            pos++;
            continue;
        }
        
        // Skip comments
        if (c == '/' && pos + 1 < source.length()) {
            QChar next = source[pos + 1];
            if (next == '/') {
                // Line comment
                while (pos < source.length() && source[pos] != '\n') pos++;
                continue;
            } else if (next == '*') {
                // Block comment
                pos += 2;
                while (pos + 1 < source.length()) {
                    if (source[pos] == '*' && source[pos + 1] == '/') {
                        pos += 2;
                        break;
                    }
                    if (source[pos] == '\n') {
                        line++;
                        column = 1;
                    }
                    pos++;
                }
                continue;
            }
        }
        
        Token token;
        token.line = line;
        token.column = column;
        token.startPos = pos;
        
        // String literal
        if (c == '"' || c == '\'') {
            QChar quote = c;
            pos++;
            int start = pos;
            while (pos < source.length() && source[pos] != quote) {
                if (source[pos] == '\\' && pos + 1 < source.length()) pos++;
                pos++;
            }
            token.text = source.mid(start, pos - start);
            token.type = (quote == '"') ? 12 : 13; // STRING_LITERAL or CHAR_LITERAL
            token.category = "string";
            token.length = pos - token.startPos + 1;
            if (pos < source.length()) pos++; // Skip closing quote
            m_tokens.append(token);
            column += token.length;
            continue;
        }
        
        // Number literal
        if (c.isDigit() || (c == '.' && pos + 1 < source.length() && source[pos + 1].isDigit())) {
            int start = pos;
            bool isFloat = false;
            
            // Check for hex/binary/octal
            if (c == '0' && pos + 1 < source.length()) {
                QChar next = source[pos + 1].toLower();
                if (next == 'x') {
                    pos += 2;
                    while (pos < source.length() && 
                           (source[pos].isDigit() || 
                            (source[pos].toLower() >= 'a' && source[pos].toLower() <= 'f'))) {
                        pos++;
                    }
                } else if (next == 'b') {
                    pos += 2;
                    while (pos < source.length() && (source[pos] == '0' || source[pos] == '1')) {
                        pos++;
                    }
                } else {
                    goto decimal;
                }
            } else {
                decimal:
                while (pos < source.length() && (source[pos].isDigit() || source[pos] == '_')) {
                    pos++;
                }
                if (pos < source.length() && source[pos] == '.') {
                    pos++;
                    isFloat = true;
                    while (pos < source.length() && (source[pos].isDigit() || source[pos] == '_')) {
                        pos++;
                    }
                }
                if (pos < source.length() && source[pos].toLower() == 'e') {
                    pos++;
                    isFloat = true;
                    if (pos < source.length() && (source[pos] == '+' || source[pos] == '-')) pos++;
                    while (pos < source.length() && source[pos].isDigit()) pos++;
                }
            }
            
            token.text = source.mid(start, pos - start);
            token.type = isFloat ? 11 : 10; // FLOAT_LITERAL or INT_LITERAL
            token.category = "number";
            token.length = pos - start;
            m_tokens.append(token);
            column += token.length;
            continue;
        }
        
        // Identifier or keyword
        if (c.isLetter() || c == '_') {
            int start = pos;
            while (pos < source.length() && (source[pos].isLetterOrNumber() || source[pos] == '_')) {
                pos++;
            }
            
            token.text = source.mid(start, pos - start);
            token.length = pos - start;
            
            // Check for keywords
            static const std::unordered_set<std::string> keywords = {
                "if", "else", "elif", "while", "for", "do", "switch", "case", "default",
                "break", "continue", "return", "fn", "func", "function", "let", "var",
                "const", "mut", "static", "extern", "pub", "public", "private",
                "struct", "class", "enum", "union", "interface", "impl", "trait",
                "type", "import", "export", "module", "package", "use", "as", "from",
                "true", "false", "null", "nil", "self", "this", "super", "new", "delete",
                "sizeof", "typeof", "cast", "try", "catch", "throw", "finally",
                "async", "await", "yield", "match", "where", "in", "not", "and", "or", "xor"
            };
            
            static const std::unordered_set<std::string> types = {
                "void", "bool", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
                "f32", "f64", "char", "string", "int", "float", "double", "long",
                "short", "byte", "uint", "ulong", "ushort"
            };
            
            if (keywords.contains(token.text)) {
                token.type = 30; // Keyword base
                token.category = "keyword";
            } else if (types.contains(token.text)) {
                token.type = 21; // TYPE_NAME
                token.category = "type";
            } else {
                token.type = 20; // IDENTIFIER
                token.category = "identifier";
            }
            
            m_tokens.append(token);
            column += token.length;
            continue;
        }
        
        // Operators and punctuation
        token.length = 1;
        token.category = "operator";
        
        // Multi-character operators
        std::string twoChar = source.mid(pos, 2);
        std::string threeChar = source.mid(pos, 3);
        
        static const std::map<std::string, int> threeCharOps = {
            {"<<=", 148}, {">>=", 149}, {"...", 132}, {"===", 116}
        };
        
        static const std::map<std::string, int> twoCharOps = {
            {"==", 116}, {"!=", 117}, {"<=", 120}, {">=", 121},
            {"<<", 122}, {">>", 123}, {"&&", 124}, {"||", 125},
            {"++", 126}, {"--", 127}, {"->", 128}, {"=>", 129},
            {"::", 130}, {"..", 131}, {"+=", 140}, {"-=", 141},
            {"*=", 142}, {"/=", 143}, {"%=", 144}, {"&=", 145},
            {"|=", 146}, {"^=", 147}
        };
        
        static const std::map<QChar, int> singleCharOps = {
            {'+', 100}, {'-', 101}, {'*', 102}, {'/', 103}, {'%', 104},
            {'&', 105}, {'|', 106}, {'^', 107}, {'~', 108}, {'!', 109},
            {'?', 110}, {':', 111}, {';', 112}, {',', 113}, {'.', 114},
            {'=', 115}, {'<', 118}, {'>', 119},
            {'(', 150}, {')', 151}, {'[', 152}, {']', 153},
            {'{', 154}, {'}', 155}
        };
        
        if (threeCharOps.contains(threeChar)) {
            token.type = threeCharOps[threeChar];
            token.text = threeChar;
            token.length = 3;
        } else if (twoCharOps.contains(twoChar)) {
            token.type = twoCharOps[twoChar];
            token.text = twoChar;
            token.length = 2;
        } else if (singleCharOps.contains(c)) {
            token.type = singleCharOps[c];
            token.text = c;
            token.length = 1;
            
            // Set category for delimiters
            if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
                token.category = "delimiter";
            }
        } else {
            // Unknown character
            addDiagnostic(DiagnosticSeverity::Error, 
                          tr("Unknown character '%1'"),
                          line, column, result);
            return false;
        }
        
        m_tokens.append(token);
        pos += token.length;
        column += token.length;
    }
    
    // Add EOF token
    Token eofToken;
    eofToken.type = 0; // TOK_EOF
    eofToken.line = line;
    eofToken.column = column;
    eofToken.text = "";
    eofToken.category = "eof";
    m_tokens.append(eofToken);
    
    return true;
}

bool CompilerEngine::runParser(CompileResult& result)
{
    // Create AST root
    m_astRoot = QSharedPointer<ASTNode>::create();
    m_astRoot->type = 0; // AST_PROGRAM
    m_astRoot->name = "program";
    
    int tokenPos = 0;
    
    // Parse top-level declarations
    while (tokenPos < m_tokens.size() && m_tokens[tokenPos].type != 0) {
        auto node = parseDeclaration(tokenPos);
        if (node) {
            m_astRoot->children.append(node);
        } else if (result.diagnostics.empty()) {
            addDiagnostic(DiagnosticSeverity::Error,
                          tr("Unexpected token '%1'"),
                          m_tokens[tokenPos].line, m_tokens[tokenPos].column, result);
            return false;
        } else {
            return false;
        }
    }
    
    return true;
}

QSharedPointer<ASTNode> CompilerEngine::parseDeclaration(int& pos)
{
    if (pos >= m_tokens.size()) return nullptr;
    
    const Token& tok = m_tokens[pos];
    
    // Check for function declaration
    if (tok.text == "fn" || tok.text == "func" || tok.text == "function") {
        return parseFunctionDecl(pos);
    }
    
    // Check for variable declaration
    if (tok.text == "let" || tok.text == "var" || tok.text == "const") {
        return parseVariableDecl(pos);
    }
    
    // Check for struct declaration
    if (tok.text == "struct") {
        return parseStructDecl(pos);
    }
    
    // Check for enum declaration
    if (tok.text == "enum") {
        return parseEnumDecl(pos);
    }
    
    // Check for import
    if (tok.text == "import" || tok.text == "use") {
        return parseImportDecl(pos);
    }
    
    // Try to parse as expression statement
    return parseExpressionStatement(pos);
}

QSharedPointer<ASTNode> CompilerEngine::parseFunctionDecl(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 1; // AST_FUNCTION
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'fn' keyword
    
    // Expect function name
    if (pos >= m_tokens.size() || m_tokens[pos].type != 20) {
        return nullptr;
    }
    
    node->name = m_tokens[pos].text;
    pos++;
    
    // Expect '('
    if (pos >= m_tokens.size() || m_tokens[pos].type != 150) {
        return nullptr;
    }
    pos++;
    
    // Parse parameters
    while (pos < m_tokens.size() && m_tokens[pos].type != 151) {
        auto param = parseParameter(pos);
        if (param) {
            node->children.append(param);
        }
        
        // Check for comma
        if (pos < m_tokens.size() && m_tokens[pos].type == 113) {
            pos++;
        }
    }
    
    // Skip ')'
    if (pos < m_tokens.size() && m_tokens[pos].type == 151) {
        pos++;
    }
    
    // Check for return type
    if (pos < m_tokens.size() && m_tokens[pos].type == 128) { // ->
        pos++;
        if (pos < m_tokens.size()) {
            node->typeName = m_tokens[pos].text;
            pos++;
        }
    }
    
    // Parse body
    if (pos < m_tokens.size() && m_tokens[pos].type == 154) { // {
        auto body = parseBlock(pos);
        if (body) {
            node->children.append(body);
        }
    }
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseParameter(int& pos)
{
    if (pos >= m_tokens.size()) return nullptr;
    
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 3; // AST_PARAMETER
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    node->name = m_tokens[pos].text;
    pos++;
    
    // Check for type annotation
    if (pos < m_tokens.size() && m_tokens[pos].type == 111) { // :
        pos++;
        if (pos < m_tokens.size()) {
            node->typeName = m_tokens[pos].text;
            pos++;
        }
    }
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseBlock(int& pos)
{
    if (pos >= m_tokens.size() || m_tokens[pos].type != 154) { // {
        return nullptr;
    }
    
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 4; // AST_BLOCK
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip '{'
    
    // Parse statements
    while (pos < m_tokens.size() && m_tokens[pos].type != 155) { // }
        auto stmt = parseStatement(pos);
        if (stmt) {
            node->children.append(stmt);
        } else {
            break;
        }
    }
    
    // Skip '}'
    if (pos < m_tokens.size() && m_tokens[pos].type == 155) {
        pos++;
    }
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseStatement(int& pos)
{
    if (pos >= m_tokens.size()) return nullptr;
    
    const Token& tok = m_tokens[pos];
    
    if (tok.text == "if") return parseIfStatement(pos);
    if (tok.text == "while") return parseWhileStatement(pos);
    if (tok.text == "for") return parseForStatement(pos);
    if (tok.text == "return") return parseReturnStatement(pos);
    if (tok.text == "break") return parseBreakStatement(pos);
    if (tok.text == "continue") return parseContinueStatement(pos);
    if (tok.text == "let" || tok.text == "var" || tok.text == "const") {
        return parseVariableDecl(pos);
    }
    if (tok.type == 154) { // {
        return parseBlock(pos);
    }
    
    return parseExpressionStatement(pos);
}

QSharedPointer<ASTNode> CompilerEngine::parseIfStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 5; // AST_IF
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'if'
    
    // Parse condition
    if (pos < m_tokens.size() && m_tokens[pos].type == 150) pos++; // Skip optional '('
    auto condition = parseExpression(pos);
    if (condition) node->children.append(condition);
    if (pos < m_tokens.size() && m_tokens[pos].type == 151) pos++; // Skip optional ')'
    
    // Parse then block
    auto thenBlock = parseBlock(pos);
    if (thenBlock) node->children.append(thenBlock);
    
    // Check for else
    if (pos < m_tokens.size() && m_tokens[pos].text == "else") {
        pos++;
        auto elseBlock = parseBlock(pos);
        if (elseBlock) node->children.append(elseBlock);
    }
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseWhileStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 6; // AST_WHILE
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'while'
    
    // Parse condition
    if (pos < m_tokens.size() && m_tokens[pos].type == 150) pos++;
    auto condition = parseExpression(pos);
    if (condition) node->children.append(condition);
    if (pos < m_tokens.size() && m_tokens[pos].type == 151) pos++;
    
    // Parse body
    auto body = parseBlock(pos);
    if (body) node->children.append(body);
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseForStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 7; // AST_FOR
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'for'
    
    // Check for iterator-style for
    if (pos + 2 < m_tokens.size() && m_tokens[pos + 1].text == "in") {
        // for x in iterable
        node->attributes["iterator"] = m_tokens[pos].text;
        pos += 2;
        auto iterable = parseExpression(pos);
        if (iterable) node->children.append(iterable);
    } else {
        // C-style for
        if (pos < m_tokens.size() && m_tokens[pos].type == 150) pos++;
        auto init = parseStatement(pos);
        if (init) node->children.append(init);
        auto cond = parseExpression(pos);
        if (cond) node->children.append(cond);
        if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++; // Skip ;
        auto update = parseExpression(pos);
        if (update) node->children.append(update);
        if (pos < m_tokens.size() && m_tokens[pos].type == 151) pos++;
    }
    
    // Parse body
    auto body = parseBlock(pos);
    if (body) node->children.append(body);
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseReturnStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 8; // AST_RETURN
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'return'
    
    // Check for return value
    if (pos < m_tokens.size() && m_tokens[pos].type != 112 && m_tokens[pos].type != 155) {
        auto value = parseExpression(pos);
        if (value) node->children.append(value);
    }
    
    // Skip semicolon
    if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++;
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseBreakStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 9; // AST_BREAK
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    pos++;
    if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++;
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseContinueStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 10; // AST_CONTINUE
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    pos++;
    if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++;
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseVariableDecl(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 2; // AST_VARIABLE
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    node->attributes["mutable"] = (m_tokens[pos].text != "const");
    
    pos++; // Skip let/var/const
    
    // Get variable name
    if (pos < m_tokens.size() && m_tokens[pos].type == 20) {
        node->name = m_tokens[pos].text;
        pos++;
    }
    
    // Check for type annotation
    if (pos < m_tokens.size() && m_tokens[pos].type == 111) { // :
        pos++;
        if (pos < m_tokens.size()) {
            node->typeName = m_tokens[pos].text;
            pos++;
        }
    }
    
    // Check for initializer
    if (pos < m_tokens.size() && m_tokens[pos].type == 115) { // =
        pos++;
        auto init = parseExpression(pos);
        if (init) node->children.append(init);
    }
    
    // Skip semicolon
    if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++;
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseStructDecl(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 20; // AST_STRUCT
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'struct'
    
    // Get struct name
    if (pos < m_tokens.size() && m_tokens[pos].type == 20) {
        node->name = m_tokens[pos].text;
        pos++;
    }
    
    // Parse body
    if (pos < m_tokens.size() && m_tokens[pos].type == 154) {
        pos++;
        while (pos < m_tokens.size() && m_tokens[pos].type != 155) {
            auto field = parseParameter(pos);
            if (field) node->children.append(field);
            if (pos < m_tokens.size() && m_tokens[pos].type == 113) pos++; // Skip comma
        }
        if (pos < m_tokens.size()) pos++; // Skip }
    }
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseEnumDecl(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 21; // AST_ENUM
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'enum'
    
    // Get enum name
    if (pos < m_tokens.size() && m_tokens[pos].type == 20) {
        node->name = m_tokens[pos].text;
        pos++;
    }
    
    // Parse variants
    if (pos < m_tokens.size() && m_tokens[pos].type == 154) {
        pos++;
        while (pos < m_tokens.size() && m_tokens[pos].type != 155) {
            auto variant = QSharedPointer<ASTNode>::create();
            variant->type = 18; // AST_IDENTIFIER
            variant->name = m_tokens[pos].text;
            node->children.append(variant);
            pos++;
            if (pos < m_tokens.size() && m_tokens[pos].type == 113) pos++; // Skip comma
        }
        if (pos < m_tokens.size()) pos++; // Skip }
    }
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseImportDecl(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 28; // AST_IMPORT
    node->line = m_tokens[pos].line;
    node->column = m_tokens[pos].column;
    
    pos++; // Skip 'import'/'use'
    
    // Get module path
    std::string path;
    while (pos < m_tokens.size() && m_tokens[pos].type != 112 && m_tokens[pos].type != 0) {
        path += m_tokens[pos].text;
        pos++;
    }
    node->name = path;
    
    if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++;
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseExpressionStatement(int& pos)
{
    auto node = QSharedPointer<ASTNode>::create();
    node->type = 11; // AST_EXPR_STMT
    
    auto expr = parseExpression(pos);
    if (expr) {
        node->children.append(expr);
        node->line = expr->line;
        node->column = expr->column;
    }
    
    // Skip semicolon
    if (pos < m_tokens.size() && m_tokens[pos].type == 112) pos++;
    
    return node;
}

QSharedPointer<ASTNode> CompilerEngine::parseExpression(int& pos, int minPrec)
{
    auto left = parsePrimaryExpression(pos);
    if (!left) return nullptr;
    
    // Parse binary operations with precedence climbing
    while (pos < m_tokens.size()) {
        int prec = getOperatorPrecedence(m_tokens[pos].type);
        if (prec < minPrec) break;
        
        auto node = QSharedPointer<ASTNode>::create();
        node->type = 12; // AST_BINARY_OP
        node->attributes["operator"] = m_tokens[pos].text;
        node->line = m_tokens[pos].line;
        node->column = m_tokens[pos].column;
        
        pos++; // Skip operator
        
        auto right = parseExpression(pos, prec + 1);
        if (!right) return nullptr;
        
        node->children.append(left);
        node->children.append(right);
        left = node;
    }
    
    return left;
}

QSharedPointer<ASTNode> CompilerEngine::parsePrimaryExpression(int& pos)
{
    if (pos >= m_tokens.size()) return nullptr;
    
    const Token& tok = m_tokens[pos];
    
    // Parenthesized expression
    if (tok.type == 150) { // (
        pos++;
        auto expr = parseExpression(pos);
        if (pos < m_tokens.size() && m_tokens[pos].type == 151) pos++; // )
        return expr;
    }
    
    // Unary operators
    if (tok.type == 101 || tok.type == 108 || tok.type == 109) { // - ~ !
        auto node = QSharedPointer<ASTNode>::create();
        node->type = 13; // AST_UNARY_OP
        node->attributes["operator"] = tok.text;
        node->line = tok.line;
        node->column = tok.column;
        pos++;
        auto operand = parsePrimaryExpression(pos);
        if (operand) node->children.append(operand);
        return node;
    }
    
    // Literals
    if (tok.type == 10 || tok.type == 11) { // INT or FLOAT
        auto node = QSharedPointer<ASTNode>::create();
        node->type = 17; // AST_LITERAL
        node->name = tok.text;
        node->typeName = (tok.type == 10) ? "int" : "float";
        node->line = tok.line;
        node->column = tok.column;
        pos++;
        return node;
    }
    
    if (tok.type == 12 || tok.type == 13) { // STRING or CHAR
        auto node = QSharedPointer<ASTNode>::create();
        node->type = 17; // AST_LITERAL
        node->name = tok.text;
        node->typeName = (tok.type == 12) ? "string" : "char";
        node->line = tok.line;
        node->column = tok.column;
        pos++;
        return node;
    }
    
    // Boolean literals
    if (tok.text == "true" || tok.text == "false") {
        auto node = QSharedPointer<ASTNode>::create();
        node->type = 17; // AST_LITERAL
        node->name = tok.text;
        node->typeName = "bool";
        node->line = tok.line;
        node->column = tok.column;
        pos++;
        return node;
    }
    
    // Identifier or function call
    if (tok.type == 20) { // IDENTIFIER
        auto node = QSharedPointer<ASTNode>::create();
        node->type = 18; // AST_IDENTIFIER
        node->name = tok.text;
        node->line = tok.line;
        node->column = tok.column;
        pos++;
        
        // Check for function call
        if (pos < m_tokens.size() && m_tokens[pos].type == 150) {
            auto call = QSharedPointer<ASTNode>::create();
            call->type = 14; // AST_CALL
            call->name = node->name;
            call->line = node->line;
            call->column = node->column;
            
            pos++; // Skip (
            while (pos < m_tokens.size() && m_tokens[pos].type != 151) {
                auto arg = parseExpression(pos);
                if (arg) call->children.append(arg);
                if (pos < m_tokens.size() && m_tokens[pos].type == 113) pos++;
            }
            if (pos < m_tokens.size()) pos++; // Skip )
            
            return call;
        }
        
        // Check for array access
        while (pos < m_tokens.size() && m_tokens[pos].type == 152) { // [
            auto index = QSharedPointer<ASTNode>::create();
            index->type = 15; // AST_INDEX
            index->children.append(node);
            index->line = m_tokens[pos].line;
            index->column = m_tokens[pos].column;
            
            pos++; // Skip [
            auto idx = parseExpression(pos);
            if (idx) index->children.append(idx);
            if (pos < m_tokens.size() && m_tokens[pos].type == 153) pos++; // ]
            
            node = index;
        }
        
        // Check for member access
        while (pos < m_tokens.size() && m_tokens[pos].type == 114) { // .
            pos++;
            if (pos < m_tokens.size() && m_tokens[pos].type == 20) {
                auto member = QSharedPointer<ASTNode>::create();
                member->type = 16; // AST_MEMBER
                member->name = m_tokens[pos].text;
                member->children.append(node);
                member->line = m_tokens[pos].line;
                member->column = m_tokens[pos].column;
                pos++;
                node = member;
            }
        }
        
        return node;
    }
    
    return nullptr;
}

int CompilerEngine::getOperatorPrecedence(int tokenType)
{
    switch (tokenType) {
        case 115: return 1;  // =
        case 125: return 2;  // ||
        case 124: return 3;  // &&
        case 106: return 4;  // |
        case 107: return 5;  // ^
        case 105: return 6;  // &
        case 116: case 117: return 7;  // == !=
        case 118: case 119: case 120: case 121: return 8;  // < > <= >=
        case 122: case 123: return 9;  // << >>
        case 100: case 101: return 10; // + -
        case 102: case 103: case 104: return 11; // * / %
        default: return 0;
    }
}

int CompilerEngine::countASTNodes(ASTNode* node)
{
    if (!node) return 0;
    int count = 1;
    for (const auto& child : node->children) {
        count += countASTNodes(child.data());
    }
    return count;
}

bool CompilerEngine::runSemanticAnalysis(CompileResult& result)
{
    // Build symbol table
    m_symbols.clear();
    collectSymbols(m_astRoot.data(), "");
    
    // Type checking and reference resolution
    return checkSemantics(m_astRoot.data(), result);
}

void CompilerEngine::collectSymbols(ASTNode* node, const std::string& scope)
{
    if (!node) return;
    
    Symbol sym;
    sym.scope = scope;
    sym.filePath = ""; // Would be set by caller
    sym.definitionLine = node->line;
    sym.definitionColumn = node->column;
    
    switch (node->type) {
        case 1: // AST_FUNCTION
            sym.name = node->name;
            sym.type = node->typeName.empty() ? "void" : node->typeName;
            sym.kind = "function";
            m_symbols.append(sym);
            // Process children in function scope
            for (const auto& child : node->children) {
                collectSymbols(child.data(), scope + "::" + node->name);
            }
            return;
            
        case 2: // AST_VARIABLE
            sym.name = node->name;
            sym.type = node->typeName;
            sym.kind = "variable";
            m_symbols.append(sym);
            break;
            
        case 3: // AST_PARAMETER
            sym.name = node->name;
            sym.type = node->typeName;
            sym.kind = "parameter";
            m_symbols.append(sym);
            break;
            
        case 20: // AST_STRUCT
            sym.name = node->name;
            sym.type = node->name;
            sym.kind = "struct";
            m_symbols.append(sym);
            for (const auto& child : node->children) {
                collectSymbols(child.data(), scope + "::" + node->name);
            }
            return;
            
        case 21: // AST_ENUM
            sym.name = node->name;
            sym.type = node->name;
            sym.kind = "enum";
            m_symbols.append(sym);
            break;
    }
    
    for (const auto& child : node->children) {
        collectSymbols(child.data(), scope);
    }
}

bool CompilerEngine::checkSemantics(ASTNode* node, CompileResult& result)
{
    if (!node) return true;
    
    // Type checking based on node type
    switch (node->type) {
        case 19: { // AST_ASSIGN
            if (node->children.size() >= 2) {
                // Check that left side is assignable
                auto left = node->children[0].data();
                if (left->type != 18 && left->type != 15 && left->type != 16) {
                    addDiagnostic(DiagnosticSeverity::Error,
                                  tr("Left side of assignment must be a variable"),
                                  node->line, node->column, result);
                    return false;
                }
            }
            break;
        }
        
        case 14: { // AST_CALL
            // Check that function exists
            bool found = false;
            for (const auto& sym : m_symbols) {
                if (sym.name == node->name && sym.kind == "function") {
                    found = true;
                    break;
                }
            }
            // Allow built-in functions
            static const std::unordered_set<std::string> builtins = {"print", "println", "input", "len", "str", "int", "float"};
            if (!found && !builtins.contains(node->name)) {
                addDiagnostic(DiagnosticSeverity::Warning,
                              tr("Unknown function '%1'"),
                              node->line, node->column, result);
            }
            break;
        }
        
        case 18: { // AST_IDENTIFIER
            // Check that variable is defined (in current scope)
            // This is a simplified check
            break;
        }
    }
    
    // Recursively check children
    for (const auto& child : node->children) {
        if (!checkSemantics(child.data(), result)) {
            return false;
        }
    }
    
    return true;
}

bool CompilerEngine::runIRGeneration(CompileResult& result)
{
    m_irBuffer.clear();
    std::stringstream ir(&m_irBuffer);
    
    // Generate IR for each function
    for (const auto& child : m_astRoot->children) {
        if (child->type == 1) { // AST_FUNCTION
            generateIRForFunction(child.data(), ir);
        }
    }
    
    result.irInstructionCount = m_irBuffer.count('\n');
    return true;
}

void CompilerEngine::generateIRForFunction(ASTNode* func, std::stringstream& ir)
{
    ir << "define @" << func->name << "(";
    
    // Parameters
    bool first = true;
    for (const auto& child : func->children) {
        if (child->type == 3) { // AST_PARAMETER
            if (!first) ir << ", ";
            ir << "%" << child->name << ": " << child->typeName;
            first = false;
        }
    }
    
    ir << ") -> " << (func->typeName.empty() ? "void" : func->typeName) << " {\n";
    ir << "entry:\n";
    
    // Generate body
    for (const auto& child : func->children) {
        if (child->type == 4) { // AST_BLOCK
            generateIRForBlock(child.data(), ir, 1);
        }
    }
    
    ir << "}\n\n";
}

void CompilerEngine::generateIRForBlock(ASTNode* block, std::stringstream& ir, int indent)
{
    std::string prefix(indent * 2, ' ');
    
    for (const auto& stmt : block->children) {
        generateIRForStatement(stmt.data(), ir, indent);
    }
}

void CompilerEngine::generateIRForStatement(ASTNode* stmt, std::stringstream& ir, int indent)
{
    std::string prefix(indent * 2, ' ');
    
    switch (stmt->type) {
        case 2: { // AST_VARIABLE
            std::string tmp = std::string("%%t%1");
            if (!stmt->children.empty()) {
                ir << prefix << tmp << " = alloca " << stmt->typeName << "\n";
                std::string val = generateIRForExpression(stmt->children[0].data(), ir, indent);
                ir << prefix << "store " << val << ", " << tmp << "\n";
            }
            m_varMap[stmt->name] = tmp;
            break;
        }
        
        case 8: { // AST_RETURN
            if (!stmt->children.empty()) {
                std::string val = generateIRForExpression(stmt->children[0].data(), ir, indent);
                ir << prefix << "ret " << val << "\n";
            } else {
                ir << prefix << "ret void\n";
            }
            break;
        }
        
        case 5: { // AST_IF
            std::string labelThen = std::string(".if_then_%1");
            std::string labelElse = std::string(".if_else_%1");
            std::string labelEnd = std::string(".if_end_%1");
            
            if (!stmt->children.empty()) {
                std::string cond = generateIRForExpression(stmt->children[0].data(), ir, indent);
                ir << prefix << "br " << cond << ", " << labelThen << ", " 
                   << (stmt->children.size() > 2 ? labelElse : labelEnd) << "\n";
            }
            
            ir << labelThen << ":\n";
            if (stmt->children.size() > 1) {
                generateIRForBlock(stmt->children[1].data(), ir, indent);
            }
            ir << prefix << "br " << labelEnd << "\n";
            
            if (stmt->children.size() > 2) {
                ir << labelElse << ":\n";
                generateIRForBlock(stmt->children[2].data(), ir, indent);
                ir << prefix << "br " << labelEnd << "\n";
            }
            
            ir << labelEnd << ":\n";
            break;
        }
        
        case 6: { // AST_WHILE
            std::string labelCond = std::string(".while_cond_%1");
            std::string labelBody = std::string(".while_body_%1");
            std::string labelEnd = std::string(".while_end_%1");
            
            ir << prefix << "br " << labelCond << "\n";
            ir << labelCond << ":\n";
            
            if (!stmt->children.empty()) {
                std::string cond = generateIRForExpression(stmt->children[0].data(), ir, indent);
                ir << prefix << "br " << cond << ", " << labelBody << ", " << labelEnd << "\n";
            }
            
            ir << labelBody << ":\n";
            if (stmt->children.size() > 1) {
                generateIRForBlock(stmt->children[1].data(), ir, indent);
            }
            ir << prefix << "br " << labelCond << "\n";
            
            ir << labelEnd << ":\n";
            break;
        }
        
        case 11: { // AST_EXPR_STMT
            if (!stmt->children.empty()) {
                generateIRForExpression(stmt->children[0].data(), ir, indent);
            }
            break;
        }
    }
}

std::string CompilerEngine::generateIRForExpression(ASTNode* expr, std::stringstream& ir, int indent)
{
    std::string prefix(indent * 2, ' ');
    std::string result;
    
    switch (expr->type) {
        case 17: { // AST_LITERAL
            if (expr->typeName == "int" || expr->typeName == "float") {
                result = expr->name;
            } else if (expr->typeName == "string") {
                result = std::string("@str_%1");
                m_strings[result] = expr->name;
            } else if (expr->typeName == "bool") {
                result = (expr->name == "true") ? "1" : "0";
            }
            break;
        }
        
        case 18: { // AST_IDENTIFIER
            if (m_varMap.contains(expr->name)) {
                std::string tmp = std::string("%%t%1");
                ir << prefix << tmp << " = load " << m_varMap[expr->name] << "\n";
                result = tmp;
            } else {
                result = "%" + expr->name;
            }
            break;
        }
        
        case 12: { // AST_BINARY_OP
            std::string left = generateIRForExpression(expr->children[0].data(), ir, indent);
            std::string right = generateIRForExpression(expr->children[1].data(), ir, indent);
            std::string tmp = std::string("%%t%1");
            
            std::string op = expr->attributes["operator"].toString();
            std::string irOp;
            
            if (op == "+") irOp = "add";
            else if (op == "-") irOp = "sub";
            else if (op == "*") irOp = "mul";
            else if (op == "/") irOp = "div";
            else if (op == "%") irOp = "rem";
            else if (op == "==") irOp = "eq";
            else if (op == "!=") irOp = "ne";
            else if (op == "<") irOp = "lt";
            else if (op == ">") irOp = "gt";
            else if (op == "<=") irOp = "le";
            else if (op == ">=") irOp = "ge";
            else if (op == "&&") irOp = "and";
            else if (op == "||") irOp = "or";
            else irOp = "unknown";
            
            ir << prefix << tmp << " = " << irOp << " " << left << ", " << right << "\n";
            result = tmp;
            break;
        }
        
        case 13: { // AST_UNARY_OP
            std::string operand = generateIRForExpression(expr->children[0].data(), ir, indent);
            std::string tmp = std::string("%%t%1");
            
            std::string op = expr->attributes["operator"].toString();
            if (op == "-") {
                ir << prefix << tmp << " = neg " << operand << "\n";
            } else if (op == "!") {
                ir << prefix << tmp << " = not " << operand << "\n";
            } else if (op == "~") {
                ir << prefix << tmp << " = bitnot " << operand << "\n";
            }
            result = tmp;
            break;
        }
        
        case 14: { // AST_CALL
            std::stringList args;
            for (const auto& arg : expr->children) {
                args << generateIRForExpression(arg.data(), ir, indent);
            }
            
            std::string tmp = std::string("%%t%1");
            ir << prefix << tmp << " = call @" << expr->name << "(" << args.join(", ") << ")\n";
            result = tmp;
            break;
        }
        
        case 19: { // AST_ASSIGN
            if (expr->children.size() >= 2) {
                std::string val = generateIRForExpression(expr->children[1].data(), ir, indent);
                std::string var = expr->children[0]->name;
                if (m_varMap.contains(var)) {
                    ir << prefix << "store " << val << ", " << m_varMap[var] << "\n";
                }
                result = val;
            }
            break;
        }
    }
    
    return result;
}

bool CompilerEngine::runOptimization(CompileResult& result, OptimizationLevel level)
{
    // Simple optimization passes
    if (level >= OptimizationLevel::Basic) {
        // Constant folding
        optimizeConstantFolding();
    }
    
    if (level >= OptimizationLevel::Standard) {
        // Dead code elimination
        optimizeDeadCodeElimination();
    }
    
    if (level >= OptimizationLevel::Aggressive) {
        // Inline small functions
        optimizeInlining();
    }
    
    return true;
}

void CompilerEngine::optimizeConstantFolding()
{
    // Simplified constant folding in IR
    std::string ir = std::string::fromUtf8(m_irBuffer);
    
    // Find patterns like: %t1 = add 5, 3
    std::regex re(R"((%t\d+) = (add|sub|mul|div) (\d+), (\d+))");
    std::regexMatchIterator it = re;
    
    while (itfalse) {
        std::regexMatch match = it;
        std::string var = match"";
        std::string op = match"";
        int left = match"";
        int right = match"";
        
        int result;
        if (op == "add") result = left + right;
        else if (op == "sub") result = left - right;
        else if (op == "mul") result = left * right;
        else if (op == "div" && right != 0) result = left / right;
        else continue;
        
        // Replace with constant
        ir.replace(match"", std::string("%1 = const %2"));
    }
    
    m_irBuffer = ir.toUtf8();
}

void CompilerEngine::optimizeDeadCodeElimination()
{
    // Simplified DCE - remove unused assignments
    std::string ir = std::string::fromUtf8(m_irBuffer);
    std::stringList lines = ir.split('\n');
    
    // Find all used variables
    std::unordered_set<std::string> used;
    std::regex useRe(R"(%t\d+)");
    for (const std::string& line : lines) {
        if (line.contains("=")) {
            std::string rhs = line.mid(line.indexOf('=') + 1);
            std::regexMatchIterator it = useRe;
            while (itfalse) {
                used.insert(it"");
            }
        }
    }
    
    // Remove unused definitions
    std::stringList result;
    for (const std::string& line : lines) {
        if (line.contains("= const") || line.contains("= add") || 
            line.contains("= sub") || line.contains("= mul") || line.contains("= div")) {
            std::regexMatch match = useRe.match(line);
            if (match.hasMatch() && !used.contains(match"")) {
                continue; // Skip unused
            }
        }
        result << line;
    }
    
    m_irBuffer = result.join('\n').toUtf8();
}

void CompilerEngine::optimizeInlining()
{
    // Would implement function inlining for small functions
    // Simplified: just mark for inlining in IR
}

bool CompilerEngine::runCodeGeneration(CompileResult& result, TargetArch target)
{
    m_codeBuffer.clear();
    std::stringstream code(&m_codeBuffer);
    
    switch (target) {
        case TargetArch::X86_64:
            generateX86_64Assembly(code);
            break;
        case TargetArch::X86_32:
            generateX86_32Assembly(code);
            break;
        case TargetArch::ARM64:
            generateARM64Assembly(code);
            break;
        default:
            generateX86_64Assembly(code);
            break;
    }
    
    return true;
}

void CompilerEngine::generateX86_64Assembly(std::stringstream& out)
{
    out << "; Generated by RawrXD Compiler v" << version() << "\n";
    out << "; Target: x86-64\n\n";
    
    out << "section .data\n";
    
    // String literals
    for (auto it = m_strings.begin(); it != m_strings.end(); ++it) {
        out << "    " << it.key() << " db \"" << it.value() << "\", 0\n";
    }
    
    out << "\nsection .text\n";
    out << "    global _start\n";
    out << "    global main\n\n";
    
    // Generate code from IR
    std::string ir = std::string::fromUtf8(m_irBuffer);
    std::stringList lines = ir.split('\n');
    
    std::map<std::string, std::string> regMap; // IR variable to register mapping
    int stackOffset = 0;
    
    for (const std::string& line : lines) {
        std::string trimmed = line.trimmed();
        
        if (trimmed.startsWith("define @")) {
            // Function definition
            std::regex re(R"(define @(\w+))");
            std::regexMatch match = re.match(trimmed);
            if (match.hasMatch()) {
                std::string funcName = match"";
                out << funcName << ":\n";
                out << "    push rbp\n";
                out << "    mov rbp, rsp\n";
                stackOffset = 0;
                regMap.clear();
            }
        } else if (trimmed == "}") {
            out << "    mov rsp, rbp\n";
            out << "    pop rbp\n";
            out << "    ret\n\n";
        } else if (trimmed.startsWith("%t")) {
            // IR instruction
            std::regex assignRe(R"((%t\d+) = (\w+) (.+))");
            std::regexMatch match = assignRe.match(trimmed);
            if (match.hasMatch()) {
                std::string dest = match"";
                std::string op = match"";
                std::string args = match"";
                
                if (op == "alloca") {
                    stackOffset += 8;
                    regMap[dest] = std::string("[rbp-%1]");
                    out << "    ; " << dest << " = alloca\n";
                } else if (op == "add" || op == "sub" || op == "mul") {
                    std::stringList parts = args.split(", ");
                    std::string left = parts.value(0);
                    std::string right = parts.value(1);
                    
                    out << "    mov rax, " << resolveOperand(left, regMap) << "\n";
                    if (op == "add") {
                        out << "    add rax, " << resolveOperand(right, regMap) << "\n";
                    } else if (op == "sub") {
                        out << "    sub rax, " << resolveOperand(right, regMap) << "\n";
                    } else if (op == "mul") {
                        out << "    imul rax, " << resolveOperand(right, regMap) << "\n";
                    }
                    out << "    mov " << regMap.value(dest, "rax") << ", rax\n";
                } else if (op == "store") {
                    std::stringList parts = args.split(", ");
                    std::string val = parts.value(0);
                    std::string loc = parts.value(1);
                    out << "    mov rax, " << resolveOperand(val, regMap) << "\n";
                    out << "    mov " << regMap.value(loc, loc) << ", rax\n";
                } else if (op == "load") {
                    out << "    mov rax, " << regMap.value(args, args) << "\n";
                    regMap[dest] = "rax";
                } else if (op == "call") {
                    std::regex callRe(R"(@(\w+)\(([^)]*)\))");
                    std::regexMatch callMatch = callRe.match(args);
                    if (callMatch.hasMatch()) {
                        std::string funcName = callMatch"";
                        std::string callArgs = callMatch"";
                        
                        // Push arguments (simplified)
                        if (!callArgs.empty()) {
                            std::stringList argList = callArgs.split(", ");
                            for (int i = argList.size() - 1; i >= 0; i--) {
                                out << "    push " << resolveOperand(argList[i], regMap) << "\n";
                            }
                        }
                        out << "    call " << funcName << "\n";
                    }
                }
            }
        } else if (trimmed.startsWith("ret ")) {
            std::string val = trimmed.mid(4);
            if (val != "void") {
                out << "    mov rax, " << resolveOperand(val, regMap) << "\n";
            }
            out << "    mov rsp, rbp\n";
            out << "    pop rbp\n";
            out << "    ret\n";
        } else if (trimmed.startsWith("br ")) {
            std::string args = trimmed.mid(3);
            if (args.contains(",")) {
                // Conditional branch
                std::stringList parts = args.split(", ");
                out << "    cmp " << resolveOperand(parts[0], regMap) << ", 0\n";
                out << "    jne " << parts[1] << "\n";
                out << "    jmp " << parts[2] << "\n";
            } else {
                // Unconditional branch
                out << "    jmp " << args << "\n";
            }
        } else if (trimmed.endsWith(":") && !trimmed.startsWith(";")) {
            // Label
            out << trimmed << "\n";
        }
    }
    
    // Entry point
    out << "_start:\n";
    out << "    call main\n";
    out << "    mov rdi, rax\n";
    out << "    mov rax, 60\n"; // sys_exit
    out << "    syscall\n";
}

std::string CompilerEngine::resolveOperand(const std::string& op, const std::map<std::string, std::string>& regMap)
{
    if (op.startsWith("%t")) {
        return regMap.value(op, op);
    }
    return op;
}

void CompilerEngine::generateX86_32Assembly(std::stringstream& out)
{
    out << "; Generated by RawrXD Compiler v" << version() << "\n";
    out << "; Target: x86-32\n\n";
    
    // Similar to x86-64 but with 32-bit registers
    out << "section .text\n";
    out << "    global _start\n";
    out << "    global main\n\n";
    
    // Simplified code generation for 32-bit
    out << "_start:\n";
    out << "    call main\n";
    out << "    mov ebx, eax\n";
    out << "    mov eax, 1\n"; // sys_exit
    out << "    int 0x80\n";
}

void CompilerEngine::generateARM64Assembly(std::stringstream& out)
{
    out << "// Generated by RawrXD Compiler v" << version() << "\n";
    out << "// Target: ARM64\n\n";
    
    out << ".text\n";
    out << ".global _start\n";
    out << ".global main\n\n";
    
    // Simplified ARM64 code generation
    out << "_start:\n";
    out << "    bl main\n";
    out << "    mov x8, #93\n"; // exit syscall
    out << "    svc #0\n";
}

bool CompilerEngine::runAssembler(CompileResult& result)
{
    // Convert assembly to machine code
    m_outputBuffer.clear();
    
    // This would call NASM/MASM or use built-in assembler
    // For now, store the assembly directly
    m_outputBuffer = m_codeBuffer;
    
    return true;
}

bool CompilerEngine::runLinker(CompileResult& result, OutputFormat format, const std::string& outputFile)
{
    // Write output file
    return writeFile(outputFile, m_outputBuffer);
}

std::vector<Token> CompilerEngine::tokenize(const std::string& source, SourceLanguage language)
{
    CompileResult result;
    runLexer(source, result);
    return m_tokens;
}

std::vector<Token> CompilerEngine::getHighlightTokens(const std::string& source, SourceLanguage language)
{
    return tokenize(source, language);
}

QSharedPointer<ASTNode> CompilerEngine::parse(const std::string& source, SourceLanguage language)
{
    CompileResult result;
    runLexer(source, result);
    if (!runParser(result)) {
        return nullptr;
    }
    return m_astRoot;
}

std::vector<Symbol> CompilerEngine::getOutline(const std::string& source, SourceLanguage language)
{
    auto ast = parse(source, language);
    if (!ast) return {};
    
    std::vector<Symbol> outline;
    for (const auto& child : ast->children) {
        if (child->type == 1 || child->type == 20 || child->type == 21) {
            Symbol sym;
            sym.name = child->name;
            sym.kind = (child->type == 1) ? "function" : 
                       (child->type == 20) ? "struct" : "enum";
            sym.type = child->typeName;
            sym.definitionLine = child->line;
            sym.definitionColumn = child->column;
            outline.append(sym);
        }
    }
    
    return outline;
}

std::vector<Diagnostic> CompilerEngine::analyze(const std::string& source, SourceLanguage language)
{
    CompileResult result;
    runLexer(source, result);
    runParser(result);
    runSemanticAnalysis(result);
    return result.diagnostics;
}

SourceLanguage CompilerEngine::detectLanguage(const std::string& filePath)
{
    std::string ext = // FileInfo: filePath).suffix().toLower();
    
    static const std::map<std::string, SourceLanguage> extMap = {
        {"eon", SourceLanguage::Eon},
        {"c", SourceLanguage::C},
        {"h", SourceLanguage::C},
        {"cpp", SourceLanguage::CPlusPlus},
        {"cxx", SourceLanguage::CPlusPlus},
        {"cc", SourceLanguage::CPlusPlus},
        {"hpp", SourceLanguage::CPlusPlus},
        {"rs", SourceLanguage::Rust},
        {"go", SourceLanguage::Go},
        {"py", SourceLanguage::Python},
        {"js", SourceLanguage::JavaScript},
        {"ts", SourceLanguage::TypeScript},
        {"java", SourceLanguage::Java},
        {"cs", SourceLanguage::CSharp},
        {"swift", SourceLanguage::Swift},
        {"kt", SourceLanguage::Kotlin},
        {"dart", SourceLanguage::Dart},
        {"lua", SourceLanguage::Lua},
        {"rb", SourceLanguage::Ruby},
        {"php", SourceLanguage::PHP},
        {"asm", SourceLanguage::Assembly},
        {"s", SourceLanguage::Assembly}
    };
    
    return extMap.value(ext, SourceLanguage::Unknown);
}

std::stringList CompilerEngine::supportedLanguages()
{
    return {"Eon", "C", "C++", "Rust", "Go", "Python", "JavaScript", "TypeScript",
            "Java", "C#", "Swift", "Kotlin", "Dart", "Lua", "Ruby", "PHP", "Assembly"};
}

void CompilerEngine::emitProgress(CompilationStage stage, int percent, const std::string& message)
{
    compilationProgress(stage, percent, message);
    if (m_verbose) {
        statusMessage(message);
    }
}

void CompilerEngine::addDiagnostic(DiagnosticSeverity severity, const std::string& message,
                                    int line, int column, CompileResult& result)
{
    Diagnostic diag;
    diag.severity = severity;
    diag.message = message;
    diag.line = line;
    diag.column = column;
    diag.timestamp = // DateTime::currentDateTime();
    result.diagnostics.append(diag);
    
    compilationDiagnostic(diag);
}

std::string CompilerEngine::readFile(const std::string& path)
{
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        return std::string();
    }
    return file.readAll();
}

bool CompilerEngine::writeFile(const std::string& path, const std::vector<uint8_t>& data)
{
    // File operation removed;
    if (!file.open(std::iostream::WriteOnly)) {
        return false;
    }
    return file.write(data) == data.size();
}

void CompilerEngine::onWorkerProgress(int percent, const std::string& message)
{
    compilationProgress(CompilationStage::Idle, percent, message);
}

void CompilerEngine::onWorkerFinished(const CompileResult& result)
{
    m_isCompiling = false;
    compilationFinished(result);
}

void CompilerEngine::onWorkerError(const std::string& error)
{
    m_isCompiling = false;
    errorOccurred(error);
}

// ============================================================================
// CompilerWorker Implementation
// ============================================================================

CompilerWorker::CompilerWorker()
    
{
}

void CompilerWorker::compile(const CompileOptions& options, const std::string& source)
{
    m_cancelled = false;
    
    CompilerEngine engine;
    CompileResult result = engine.compileString(source, options);
    
    if (!m_cancelled) {
        finished(result);
    }
}

void CompilerWorker::cancel()
{
    m_cancelled = true;
}

// ============================================================================
// ProjectManager Implementation
// ============================================================================

ProjectManager::ProjectManager()
    
    , m_compiler(new CompilerEngine(this))
{
}

ProjectManager::~ProjectManager()
{
}

bool ProjectManager::createProject(const std::string& name, const std::string& directory, SourceLanguage primaryLanguage)
{
    m_projectName = name;
    m_projectDirectory = directory;
    m_projectFile = // (directory).filePath(name + ".rawrxd");
    m_projectOpen = true;
    m_modified = true;
    
    // Create default configuration
    BuildConfiguration debug;
    debug.name = "Debug";
    debug.debugInfo = true;
    debug.optimization = OptimizationLevel::None;
    m_configurations.append(debug);
    
    BuildConfiguration release;
    release.name = "Release";
    release.debugInfo = false;
    release.optimization = OptimizationLevel::Standard;
    m_configurations.append(release);
    
    m_activeConfiguration = "Debug";
    
    projectOpened(name);
    return saveProject();
}

bool ProjectManager::openProject(const std::string& projectFile)
{
    loadProjectFile(projectFile);
    if (m_projectOpen) {
        projectOpened(m_projectName);
    }
    return m_projectOpen;
}

bool ProjectManager::saveProject()
{
    if (!m_projectOpen) return false;
    saveProjectFile(m_projectFile);
    m_modified = false;
    return true;
}

bool ProjectManager::closeProject()
{
    if (!m_projectOpen) return true;
    
    if (m_modified) {
        // Would prompt to save
    }
    
    m_projectOpen = false;
    m_projectName.clear();
    m_projectDirectory.clear();
    m_projectFile.clear();
    m_files.clear();
    m_configurations.clear();
    
    projectClosed();
    return true;
}

bool ProjectManager::addFile(const std::string& filePath)
{
    ProjectFile file;
    file.path = filePath;
    file.relativePath = // (m_projectDirectory).relativeFilePath(filePath);
    file.language = CompilerEngine::detectLanguage(filePath);
    file.compile = true;
    
    m_files.append(file);
    m_modified = true;
    
    fileAdded(filePath);
    projectModified();
    
    return true;
}

void ProjectManager::build()
{
    if (!m_projectOpen || m_building.load()) return;
    
    m_building = true;
    m_cancelBuild = false;
    m_objectFiles.clear();
    m_diagnostics.clear();
    
    buildStarted();
    
    BuildConfiguration config = getActiveConfiguration();
    std::vector<ProjectFile> sources = getSourceFiles();
    
    int current = 0;
    int total = sources.size();
    
    for (const auto& file : sources) {
        if (m_cancelBuild.load()) break;
        
        buildProgress(file.path, ++current, total);
        buildFile(file, config);
    }
    
    if (!m_cancelBuild.load()) {
        linkProject(config);
    }
    
    int errors = 0, warnings = 0;
    for (const auto& diag : m_diagnostics) {
        if (diag.severity >= DiagnosticSeverity::Error) errors++;
        else if (diag.severity == DiagnosticSeverity::Warning) warnings++;
    }
    
    m_building = false;
    buildFinished(errors == 0, errors, warnings);
}

void ProjectManager::buildFile(const ProjectFile& file, const BuildConfiguration& config)
{
    CompileOptions options;
    options.inputFile = file.path;
    options.outputFile = // (config.intermediateDirectory.empty() ? 
                              m_projectDirectory : config.intermediateDirectory)
                         .filePath(// FileInfo: file.path).baseName() + ".o");
    options.target = config.target;
    options.format = OutputFormat::ObjectFile;
    options.optimization = config.optimization;
    options.debugInfo = config.debugInfo;
    options.includePaths = config.includePaths;
    options.defines = config.defines;
    
    CompileResult result = m_compiler->compile(options);
    
    if (result.success) {
        m_objectFiles.append(result.outputFile);
    }
    
    m_diagnostics.append(result.diagnostics);
    
    for (const auto& diag : result.diagnostics) {
        buildDiagnostic(diag);
    }
    
    buildOutput(result.success ? 
                     tr("Compiled: %1") :
                     tr("Failed: %1 - %2"));
}

void ProjectManager::linkProject(const BuildConfiguration& config)
{
    // Would link object files into final executable
    buildOutput(tr("Linking %1 object files...")));
}

BuildConfiguration ProjectManager::getActiveConfiguration() const
{
    for (const auto& config : m_configurations) {
        if (config.name == m_activeConfiguration) {
            return config;
        }
    }
    return m_configurations.value(0);
}

std::vector<ProjectFile> ProjectManager::getSourceFiles() const
{
    std::vector<ProjectFile> sources;
    for (const auto& file : m_files) {
        if (file.compile) {
            sources.append(file);
        }
    }
    return sources;
}

void ProjectManager::loadProjectFile(const std::string& path)
{
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) {
        return;
    }
    
    void* doc = void*::fromJson(file.readAll());
    void* root = doc.object();
    
    m_projectName = root["name"].toString();
    m_projectDirectory = // FileInfo: path).string();
    m_projectFile = path;
    
    // Load files
    m_files.clear();
    void* filesArray = root["files"].toArray();
    for (const auto& fileVal : filesArray) {
        m_files.append(ProjectFile::fromJson(fileVal.toObject()));
    }
    
    // Load configurations
    m_configurations.clear();
    void* configsArray = root["configurations"].toArray();
    for (const auto& configVal : configsArray) {
        m_configurations.append(BuildConfiguration::fromJson(configVal.toObject()));
    }
    
    m_activeConfiguration = root["activeConfiguration"].toString();
    m_projectOpen = true;
}

void ProjectManager::saveProjectFile(const std::string& path)
{
    void* root;
    root["name"] = m_projectName;
    root["version"] = "1.0";
    
    void* filesArray;
    for (const auto& file : m_files) {
        filesArray.append(file.toJson());
    }
    root["files"] = filesArray;
    
    void* configsArray;
    for (const auto& config : m_configurations) {
        configsArray.append(config.toJson());
    }
    root["configurations"] = configsArray;
    root["activeConfiguration"] = m_activeConfiguration;
    
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        file.write(void*(root).toJson(void*::Indented));
    }
}

void* BuildConfiguration::toJson() const
{
    return void*{
        {"name", name},
        {"target", static_cast<int>(target)},
        {"format", static_cast<int>(format)},
        {"optimization", static_cast<int>(optimization)},
        {"debugInfo", debugInfo},
        {"defines", void*::fromStringList(defines)},
        {"includePaths", void*::fromStringList(includePaths)},
        {"libraryPaths", void*::fromStringList(libraryPaths)},
        {"libraries", void*::fromStringList(libraries)},
        {"outputDirectory", outputDirectory},
        {"intermediateDirectory", intermediateDirectory}
    };
}

BuildConfiguration BuildConfiguration::fromJson(const void*& json)
{
    BuildConfiguration config;
    config.name = json["name"].toString();
    config.target = static_cast<TargetArch>(json["target"]);
    config.format = static_cast<OutputFormat>(json["format"]);
    config.optimization = static_cast<OptimizationLevel>(json["optimization"]);
    config.debugInfo = json["debugInfo"].toBool();
    config.outputDirectory = json["outputDirectory"].toString();
    config.intermediateDirectory = json["intermediateDirectory"].toString();
    
    for (const auto& val : json["defines"].toArray()) config.defines << val.toString();
    for (const auto& val : json["includePaths"].toArray()) config.includePaths << val.toString();
    for (const auto& val : json["libraryPaths"].toArray()) config.libraryPaths << val.toString();
    for (const auto& val : json["libraries"].toArray()) config.libraries << val.toString();
    
    return config;
}

void* ProjectFile::toJson() const
{
    return void*{
        {"path", path},
        {"relativePath", relativePath},
        {"language", static_cast<int>(language)},
        {"compile", compile},
        {"dependencies", void*::fromStringList(dependencies)}
    };
}

ProjectFile ProjectFile::fromJson(const void*& json)
{
    ProjectFile file;
    file.path = json["path"].toString();
    file.relativePath = json["relativePath"].toString();
    file.language = static_cast<SourceLanguage>(json["language"]);
    file.compile = json["compile"].toBool(true);
    for (const auto& val : json["dependencies"].toArray()) {
        file.dependencies << val.toString();
    }
    return file;
}

// ============================================================================
// SyntaxHighlighter Implementation
// ============================================================================

SyntaxHighlighter::SyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
    setupFormats();
}

void SyntaxHighlighter::setLanguage(SourceLanguage language)
{
    m_language = language;
    rehighlight();
}

void SyntaxHighlighter::setTheme(const std::string& theme)
{
    m_theme = theme;
    loadTheme(theme);
    rehighlight();
}

void SyntaxHighlighter::setupFormats()
{
    // Default dark theme colors
    m_keywordFormat.setForeground(void("#569CD6"));
    m_keywordFormat.setFontWeight(void::Bold);
    
    m_typeFormat.setForeground(void("#4EC9B0"));
    
    m_functionFormat.setForeground(void("#DCDCAA"));
    
    m_stringFormat.setForeground(void("#CE9178"));
    
    m_numberFormat.setForeground(void("#B5CEA8"));
    
    m_commentFormat.setForeground(void("#6A9955"));
    m_commentFormat.setFontItalic(true);
    
    m_preprocessorFormat.setForeground(void("#C586C0"));
    
    m_operatorFormat.setForeground(void("#D4D4D4"));
}

void SyntaxHighlighter::loadTheme(const std::string& theme)
{
    if (theme == "light") {
        m_keywordFormat.setForeground(void("#0000FF"));
        m_typeFormat.setForeground(void("#267F99"));
        m_functionFormat.setForeground(void("#795E26"));
        m_stringFormat.setForeground(void("#A31515"));
        m_numberFormat.setForeground(void("#098658"));
        m_commentFormat.setForeground(void("#008000"));
        m_preprocessorFormat.setForeground(void("#AF00DB"));
    }
    // Add more themes as needed
}

void SyntaxHighlighter::highlightBlock(const std::string& text)
{
    switch (m_language) {
        case SourceLanguage::Eon:
            highlightEon(text);
            break;
        case SourceLanguage::C:
            highlightC(text);
            break;
        case SourceLanguage::CPlusPlus:
            highlightCpp(text);
            break;
        case SourceLanguage::Rust:
            highlightRust(text);
            break;
        case SourceLanguage::Python:
            highlightPython(text);
            break;
        case SourceLanguage::JavaScript:
        case SourceLanguage::TypeScript:
            highlightJavaScript(text);
            break;
        default:
            highlightGeneric(text);
            break;
    }
}

void SyntaxHighlighter::highlightEon(const std::string& text)
{
    // Keywords
    static std::regex keywordRe(
        "\\b(fn|func|let|var|const|if|else|elif|while|for|do|return|break|continue|"
        "struct|enum|trait|impl|type|pub|mut|static|extern|use|import|module|"
        "match|try|catch|throw|async|await|yield|true|false|null|self|new)\\b");
    
    std::regexMatchIterator it = keywordRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }
    
    // Types
    static std::regex typeRe(
        "\\b(void|bool|i8|i16|i32|i64|u8|u16|u32|u64|f32|f64|char|string|int|float)\\b");
    it = typeRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_typeFormat);
    }
    
    // Strings
    static std::regex stringRe(R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*')");
    it = stringRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }
    
    // Numbers
    static std::regex numberRe(R"(\b\d+\.?\d*([eE][+-]?\d+)?[fFlL]?\b|0x[0-9a-fA-F]+|0b[01]+)");
    it = numberRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_numberFormat);
    }
    
    // Comments
    static std::regex commentRe(R"(//[^\n]*)");
    it = commentRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_commentFormat);
    }
}

void SyntaxHighlighter::highlightC(const std::string& text)
{
    // C keywords
    static std::regex keywordRe(
        "\\b(auto|break|case|char|const|continue|default|do|double|else|enum|extern|"
        "float|for|goto|if|inline|int|long|register|restrict|return|short|signed|"
        "sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while|"
        "_Bool|_Complex|_Imaginary)\\b");
    
    std::regexMatchIterator it = keywordRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }
    
    // Preprocessor
    static std::regex preprocRe(R"(#\s*(include|define|undef|ifdef|ifndef|if|else|elif|endif|pragma|error|warning)[^\n]*)");
    it = preprocRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_preprocessorFormat);
    }
    
    // Strings and numbers (same as Eon)
    highlightGeneric(text);
}

void SyntaxHighlighter::highlightCpp(const std::string& text)
{
    highlightC(text);
    
    // C++ specific keywords
    static std::regex cppKeywordRe(
        "\\b(class|public|private|protected|virtual|override|final|template|typename|"
        "namespace|using|try|catch|throw|new|delete|nullptr|true|false|this|operator|"
        "explicit|friend|mutable|constexpr|consteval|constinit|concept|requires|"
        "co_await|co_return|co_yield|noexcept|decltype|static_assert)\\b");
    
    std::regexMatchIterator it = cppKeywordRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }
}

void SyntaxHighlighter::highlightRust(const std::string& text)
{
    static std::regex keywordRe(
        "\\b(as|async|await|break|const|continue|crate|dyn|else|enum|extern|false|"
        "fn|for|if|impl|in|let|loop|match|mod|move|mut|pub|ref|return|self|Self|"
        "static|struct|super|trait|true|type|unsafe|use|where|while)\\b");
    
    std::regexMatchIterator it = keywordRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }
    
    highlightGeneric(text);
}

void SyntaxHighlighter::highlightPython(const std::string& text)
{
    static std::regex keywordRe(
        "\\b(and|as|assert|async|await|break|class|continue|def|del|elif|else|except|"
        "False|finally|for|from|global|if|import|in|is|lambda|None|nonlocal|not|or|"
        "pass|raise|return|True|try|while|with|yield)\\b");
    
    std::regexMatchIterator it = keywordRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }
    
    // Python comments
    static std::regex commentRe(R"(#[^\n]*)");
    it = commentRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_commentFormat);
    }
    
    highlightGeneric(text);
}

void SyntaxHighlighter::highlightJavaScript(const std::string& text)
{
    static std::regex keywordRe(
        "\\b(async|await|break|case|catch|class|const|continue|debugger|default|delete|"
        "do|else|export|extends|false|finally|for|function|if|import|in|instanceof|let|"
        "new|null|of|return|static|super|switch|this|throw|true|try|typeof|undefined|"
        "var|void|while|with|yield)\\b");
    
    std::regexMatchIterator it = keywordRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_keywordFormat);
    }
    
    highlightGeneric(text);
}

void SyntaxHighlighter::highlightGeneric(const std::string& text)
{
    // Strings
    static std::regex stringRe(R"("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|`(?:[^`\\]|\\.)*`)");
    std::regexMatchIterator it = stringRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_stringFormat);
    }
    
    // Numbers
    static std::regex numberRe(R"(\b\d+\.?\d*([eE][+-]?\d+)?[fFlLuU]*\b|0x[0-9a-fA-F]+|0b[01]+|0o[0-7]+)");
    it = numberRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_numberFormat);
    }
    
    // Single-line comments
    static std::regex commentRe(R"(//[^\n]*|#[^\n]*)");
    it = commentRe;
    while (itfalse) {
        std::regexMatch match = it;
        setFormat(match.capturedStart(), match.capturedLength(), m_commentFormat);
    }
}

// ============================================================================
// DiagnosticsManager Implementation
// ============================================================================

DiagnosticsManager::DiagnosticsManager()
    
{
}

void DiagnosticsManager::clear()
{
    m_diagnostics.clear();
    diagnosticsCleared();
    updateTreeWidget();
}

void DiagnosticsManager::addDiagnostic(const Diagnostic& diagnostic)
{
    m_diagnostics.append(diagnostic);
    diagnosticAdded(diagnostic);
    updateTreeWidget();
}

void DiagnosticsManager::addDiagnostics(const std::vector<Diagnostic>& diagnostics)
{
    m_diagnostics.append(diagnostics);
    for (const auto& diag : diagnostics) {
        diagnosticAdded(diag);
    }
    updateTreeWidget();
}

std::vector<Diagnostic> DiagnosticsManager::getDiagnosticsForFile(const std::string& filePath) const
{
    std::vector<Diagnostic> result;
    for (const auto& diag : m_diagnostics) {
        if (diag.filePath == filePath) {
            result.append(diag);
        }
    }
    return result;
}

std::vector<Diagnostic> DiagnosticsManager::getErrors() const
{
    std::vector<Diagnostic> result;
    for (const auto& diag : m_diagnostics) {
        if (diag.severity >= DiagnosticSeverity::Error) {
            result.append(diag);
        }
    }
    return result;
}

std::vector<Diagnostic> DiagnosticsManager::getWarnings() const
{
    std::vector<Diagnostic> result;
    for (const auto& diag : m_diagnostics) {
        if (diag.severity == DiagnosticSeverity::Warning) {
            result.append(diag);
        }
    }
    return result;
}

int DiagnosticsManager::errorCount() const
{
    return std::count_if(m_diagnostics.begin(), m_diagnostics.end(),
        [](const Diagnostic& d) { return d.severity >= DiagnosticSeverity::Error; });
}

int DiagnosticsManager::warningCount() const
{
    return std::count_if(m_diagnostics.begin(), m_diagnostics.end(),
        [](const Diagnostic& d) { return d.severity == DiagnosticSeverity::Warning; });
}

void DiagnosticsManager::updateTreeWidget()
{
    if (!m_treeWidget) return;
    
    m_treeWidget->clear();
    
    for (const auto& diag : m_diagnostics) {
        QTreeWidgetItem* item = nullptr;
        
        std::string icon;
        switch (diag.severity) {
            case DiagnosticSeverity::Error:
            case DiagnosticSeverity::Fatal:
                icon = "🔴";
                break;
            case DiagnosticSeverity::Warning:
                icon = "🟡";
                break;
            case DiagnosticSeverity::Info:
                icon = "🔵";
                break;
            case DiagnosticSeverity::Hint:
                icon = "💡";
                break;
        }
        
        item->setText(0, icon);
        item->setText(1, // FileInfo: diag.filePath).fileName());
        item->setText(2, std::string::number(diag.line));
        item->setText(3, diag.message);
        item->setData(0, UserRole, std::any::fromValue(diag));
    }
}

// ============================================================================
// CompletionProvider Implementation
// ============================================================================

CompletionProvider::CompletionProvider()
    
{
}

std::vector<CompletionItem> CompletionProvider::getCompletions(const std::string& source, int line, int column)
{
    std::vector<CompletionItem> items;
    
    // Get keyword completions
    items.append(getKeywordCompletions());
    
    // Get symbol completions from parsed source
    if (m_compiler) {
        items.append(getSymbolCompletions(source));
    }
    
    return items;
}

std::vector<CompletionItem> CompletionProvider::getKeywordCompletions()
{
    std::vector<CompletionItem> items;
    
    std::stringList keywords;
    switch (m_language) {
        case SourceLanguage::Eon:
            keywords = {"fn", "let", "var", "const", "if", "else", "while", "for",
                        "return", "struct", "enum", "impl", "trait", "pub", "mut"};
            break;
        case SourceLanguage::CPlusPlus:
            keywords = {"class", "struct", "enum", "namespace", "template", "typename",
                        "public", "private", "protected", "virtual", "override", "const",
                        "static", "inline", "constexpr", "auto", "return", "if", "else",
                        "for", "while", "do", "switch", "case", "break", "continue"};
            break;
        default:
            keywords = {"if", "else", "while", "for", "return", "function", "var", "const"};
            break;
    }
    
    for (const std::string& kw : keywords) {
        CompletionItem item;
        item.label = kw;
        item.insertText = kw;
        item.kind = "keyword";
        item.sortOrder = 100;
        items.append(item);
    }
    
    return items;
}

std::vector<CompletionItem> CompletionProvider::getSymbolCompletions(const std::string& source)
{
    std::vector<CompletionItem> items;
    
    if (!m_compiler) return items;
    
    std::vector<Symbol> symbols = m_compiler->getOutline(source, m_language);
    
    for (const auto& sym : symbols) {
        CompletionItem item;
        item.label = sym.name;
        item.insertText = sym.name;
        item.detail = sym.type;
        item.kind = sym.kind;
        item.sortOrder = (sym.kind == "function") ? 50 : 60;
        items.append(item);
    }
    
    return items;
}

// ============================================================================
// DebuggerInterface Implementation
// ============================================================================

DebuggerInterface::DebuggerInterface()
    
{
}

DebuggerInterface::~DebuggerInterface()
{
    if (m_process) {
        stopDebugging();
    }
}

bool DebuggerInterface::startDebugging(const std::string& executable, const std::stringList& args)
{
    if (m_debugging.load()) return false;
    
    m_process = new void*(this);  // Signal connection removed\nparseResponse(output);
        outputReceived(output);
    });  // Signal connection removed\n});
    
    // Connect removed,
            this, [this](int exitCode, void*::ExitStatus) {
        m_debugging = false;
        debuggingStopped();
    });
    
    // Start debugger (e.g., gdb --interpreter=mi)
    std::stringList gdbArgs = {"--interpreter=mi", executable};
    gdbArgs.append(args);
    m_process->start("gdb", gdbArgs);
    
    if (!m_process->waitForStarted()) {
        delete m_process;
        m_process = nullptr;
        return false;
    }
    
    m_debugging = true;
    debuggingStarted();
    
    // Set breakpoints
    for (const auto& bp : m_breakpoints) {
        if (bp.enabled) {
            sendCommand(std::string("-break-insert %1:%2"));
        }
    }
    
    return true;
}

void DebuggerInterface::stopDebugging()
{
    if (!m_debugging.load() || !m_process) return;
    
    sendCommand("-gdb-exit");
    m_process->waitForFinished(1000);
    
    if (m_process->state() != void*::NotRunning) {
        m_process->kill();
    }
    
    delete m_process;
    m_process = nullptr;
    m_debugging = false;
    
    debuggingStopped();
}

void DebuggerInterface::continueExecution()
{
    sendCommand("-exec-continue");
}

void DebuggerInterface::pause()
{
    sendCommand("-exec-interrupt");
}

void DebuggerInterface::stepOver()
{
    sendCommand("-exec-next");
}

void DebuggerInterface::stepInto()
{
    sendCommand("-exec-step");
}

void DebuggerInterface::stepOut()
{
    sendCommand("-exec-finish");
}

int DebuggerInterface::addBreakpoint(const std::string& file, int line, const std::string& condition)
{
    Breakpoint bp;
    bp.id = m_nextBreakpointId++;
    bp.filePath = file;
    bp.line = line;
    bp.condition = condition;
    bp.enabled = true;
    
    m_breakpoints.append(bp);
    
    if (m_debugging.load()) {
        std::string cmd = std::string("-break-insert %1:%2");
        if (!condition.empty()) {
            cmd += std::string(" -c \"%1\"");
        }
        sendCommand(cmd);
    }
    
    return bp.id;
}

void DebuggerInterface::removeBreakpoint(int id)
{
    for (int i = 0; i < m_breakpoints.size(); i++) {
        if (m_breakpoints[i].id == id) {
            if (m_debugging.load()) {
                sendCommand(std::string("-break-delete %1"));
            }
            m_breakpoints.removeAt(i);
            break;
        }
    }
}

void DebuggerInterface::sendCommand(const std::string& command)
{
    if (m_process && m_process->state() == void*::Running) {
        m_process->write((command + "\n").toUtf8());
    }
}

void DebuggerInterface::parseResponse(const std::string& response)
{
    // Parse GDB/MI response
    // This is simplified - full implementation would parse all response types
    
    if (response.contains("*stopped")) {
        // Program stopped (breakpoint, step, etc.)
        std::regex fileRe("fullname=\"([^\"]+)\"");
        std::regex lineRe("line=\"(\\d+)\"");
        
        auto fileMatch = fileRe.match(response);
        auto lineMatch = lineRe.match(response);
        
        if (fileMatch.hasMatch() && lineMatch.hasMatch()) {
            StackFrame frame;
            frame.filePath = fileMatch"";
            frame.line = lineMatch"";
            stepped(frame);
        }
    }
}

// ============================================================================
// CompilerWidget Implementation
// ============================================================================

CompilerWidget::CompilerWidget(void* parent)
    : // Widget(parent)
    , m_compiler(new CompilerEngine(this))
    , m_projectManager(new ProjectManager(this))
    , m_diagnosticsManager(new DiagnosticsManager(this))
    , m_completionProvider(new CompletionProvider(this))
    , m_debugger(new DebuggerInterface(this))
{
    m_completionProvider->setCompiler(m_compiler);
    setupUI();
    connectSignals();
}

CompilerWidget::~CompilerWidget()
{
}

void CompilerWidget::setupUI()
{
    void* layout = new void(this);
    
    // Progress bar
    m_progressBar = new void(this);
    m_progressBar->setTextVisible(true);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);
    
    // Diagnostics tree
    m_diagnosticsTree = nullptr;
    m_diagnosticsTree->setHeaderLabels({"", "File", "Line", "Message"});
    m_diagnosticsTree->setColumnWidth(0, 30);
    m_diagnosticsTree->setColumnWidth(1, 150);
    m_diagnosticsTree->setColumnWidth(2, 50);
    m_diagnosticsManager->setTreeWidget(m_diagnosticsTree);
    layout->addWidget(m_diagnosticsTree);
    
    // Output log
    m_outputLog = new void(this);
    m_outputLog->setReadOnly(true);
    m_outputLog->setFont(void("Consolas", 10));
    layout->addWidget(m_outputLog);
    
    // Connect diagnostics tree click  // Signal connection removed\nif (data.isValid()) {
            Diagnostic diag = data.value<Diagnostic>();
            diagnosticClicked(diag.filePath, diag.line, diag.column);
        }
    });
}

void CompilerWidget::connectSignals()
{  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n});  // Signal connection removed\n});
}

void CompilerWidget::compileCurrentFile()
{
    compilationRequested(std::string());
}

void CompilerWidget::buildProject()
{
    m_diagnosticsManager->clear();
    m_outputLog->clear();
    m_projectManager->build();
}

void CompilerWidget::runProject()
{
    runRequested();
}

void CompilerWidget::debugProject()
{
    debugRequested();
}

void CompilerWidget::onCompilationStarted(const CompileOptions& options)
{
    m_progressBar->show();
    m_progressBar->setValue(0);
    m_outputLog->appendPlainText(tr("Compiling: %1"));
}

void CompilerWidget::onCompilationProgress(CompilationStage stage, int percent, const std::string& message)
{
    m_progressBar->setValue(percent);
    m_progressBar->setFormat(std::string("%1 - %p%")));
}

void CompilerWidget::onCompilationFinished(const CompileResult& result)
{
    m_progressBar->hide();
    m_diagnosticsManager->addDiagnostics(result.diagnostics);
    
    if (result.success) {
        m_outputLog->appendPlainText(tr("✅ Compilation successful! (%1 ms)")
                                     );
        m_outputLog->appendPlainText(tr("   Output: %1 (%2)")
                                     
                                     ));
    } else {
        m_outputLog->appendPlainText(tr("❌ Compilation failed: %1")
                                     );
    }
}

void CompilerWidget::onBuildStarted()
{
    m_progressBar->show();
    m_progressBar->setValue(0);
    m_outputLog->appendPlainText(tr("=== Build Started ==="));
}

void CompilerWidget::onBuildProgress(const std::string& file, int current, int total)
{
    m_progressBar->setValue(current * 100 / total);
    m_progressBar->setFormat(tr("Building %1/%2 - %p%"));
}

void CompilerWidget::onBuildFinished(bool success, int errors, int warnings)
{
    m_progressBar->hide();
    
    std::string result = success ? "✅ Build Successful" : "❌ Build Failed";
    m_outputLog->appendPlainText(tr("=== %1 ==="));
    m_outputLog->appendPlainText(tr("   Errors: %1, Warnings: %2"));
}

} // namespace Compiler
} // namespace RawrXD

// Register metatype for signal/
// // (RawrXD::Compiler::Diagnostic)
// // (RawrXD::Compiler::CompileOptions)
// // (RawrXD::Compiler::CompileResult)

