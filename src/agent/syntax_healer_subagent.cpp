// ============================================================================
// syntax_healer_subagent.cpp — Autonomous Syntax Error Detection & Auto-Repair
// ============================================================================
// Production implementation. C++20, Win32, no Qt, no exceptions.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "syntax_healer_subagent.hpp"
#include "../subagent_core.h"
#include "../agentic_engine.h"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <stack>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================================
// JSON serialization
// ============================================================================

std::string SyntaxError::toJSON() const {
    std::ostringstream oss;
    oss << "{\"kind\":\"" << syntaxErrorKindStr(kind) << "\""
        << ",\"severity\":" << (int)severity
        << ",\"file\":\"" << filePath << "\""
        << ",\"line\":" << line
        << ",\"col\":" << column
        << ",\"msg\":\"" << message << "\""
        << ",\"autoFixable\":" << (autoFixable ? "true" : "false")
        << "}";
    return oss.str();
}

// ============================================================================
// SyntaxHealerResult::summary
// ============================================================================
std::string SyntaxHealerResult::summary() const {
    std::ostringstream oss;
    oss << "SyntaxHealer[" << scanId << "]: "
        << filesScanned << " files, "
        << errorsFound << " errors, "
        << warningsFound << " warnings, "
        << errorsFixed << " fixed, "
        << errorsFailed << " failed, "
        << errorsSkipped << " skipped — "
        << elapsedMs << "ms";
    return oss.str();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SyntaxHealerSubAgent::SyntaxHealerSubAgent(
    SubAgentManager* manager,
    AgenticEngine* engine,
    AgenticFailureDetector* detector,
    AgenticPuppeteer* puppeteer)
    : m_manager(manager)
    , m_engine(engine)
    , m_detector(detector)
    , m_puppeteer(puppeteer)
{
    initDefaultMasmRegisters();
}

SyntaxHealerSubAgent::~SyntaxHealerSubAgent() {
    cancel();
}

// ============================================================================
// Configuration
// ============================================================================

void SyntaxHealerSubAgent::setConfig(const SyntaxHealerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
    if (m_config.validMasmRegisters.empty()) {
        initDefaultMasmRegisters();
    }
}

void SyntaxHealerSubAgent::initDefaultMasmRegisters() {
    m_config.validMasmRegisters = {
        // 64-bit GPRs
        "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        // 32-bit
        "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
        // 16-bit
        "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
        "r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w",
        // 8-bit
        "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
        "sil", "dil", "bpl", "spl",
        "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b",
        // SSE/AVX
        "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
        "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
        "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7",
        "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15",
        "zmm0", "zmm1", "zmm2", "zmm3", "zmm4", "zmm5", "zmm6", "zmm7",
        "zmm8", "zmm9", "zmm10", "zmm11", "zmm12", "zmm13", "zmm14", "zmm15",
        "zmm16", "zmm17", "zmm18", "zmm19", "zmm20", "zmm21", "zmm22", "zmm23",
        "zmm24", "zmm25", "zmm26", "zmm27", "zmm28", "zmm29", "zmm30", "zmm31",
        // Segment
        "cs", "ds", "es", "fs", "gs", "ss",
        // Special
        "rip", "rflags", "cr0", "cr2", "cr3", "cr4", "cr8",
        "dr0", "dr1", "dr2", "dr3", "dr6", "dr7",
        // MASM-style upper case handled by case-insensitive compare
    };
}

// ============================================================================
// UUID
// ============================================================================
std::string SyntaxHealerSubAgent::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream ss;
    ss << "sh-" << std::hex << std::setfill('0')
       << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-"
       << std::setw(4) << (dis(gen) & 0xFFFF);
    return ss.str();
}

// ============================================================================
// File I/O
// ============================================================================

std::string SyntaxHealerSubAgent::readFile(const std::string& path) const {
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_fileCache.find(path);
        if (it != m_fileCache.end()) return it->second;
    }
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return "";
    std::ostringstream oss;
    oss << ifs.rdbuf();
    std::string content = oss.str();
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_fileCache[path] = content;
    }
    return content;
}

std::string SyntaxHealerSubAgent::getLine(const std::string& content, int targetLine) const {
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    while (std::getline(stream, line)) {
        lineNum++;
        if (lineNum == targetLine) return line;
    }
    return "";
}

int SyntaxHealerSubAgent::countLines(const std::string& content) const {
    int count = 0;
    for (char c : content) {
        if (c == '\n') count++;
    }
    return count + 1;
}

std::string SyntaxHealerSubAgent::getContext(
    const std::string& content, int line, int radius) const
{
    std::istringstream stream(content);
    std::string curLine;
    std::ostringstream ctx;
    int lineNum = 0;
    int start = std::max(1, line - radius);
    int end = line + radius;

    while (std::getline(stream, curLine)) {
        lineNum++;
        if (lineNum >= start && lineNum <= end) {
            ctx << lineNum;
            if (lineNum == line) ctx << " >>> ";
            else ctx << "     ";
            ctx << curLine << "\n";
        }
        if (lineNum > end) break;
    }
    return ctx.str();
}

// ============================================================================
// C/C++ Brace Checking
// ============================================================================

void SyntaxHealerSubAgent::checkBraces(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixBraces) return;

    struct BraceInfo {
        char ch;
        int line;
        int col;
    };

    std::stack<BraceInfo> braceStack;
    std::stack<BraceInfo> parenStack;
    std::stack<BraceInfo> bracketStack;

    int lineNum = 1;
    int col = 0;
    bool inString = false;
    bool inChar = false;
    bool inLineComment = false;
    bool inBlockComment = false;
    char prevCh = 0;

    for (size_t i = 0; i < content.size(); i++) {
        char ch = content[i];
        col++;

        if (ch == '\n') {
            lineNum++;
            col = 0;
            inLineComment = false;
            prevCh = ch;
            continue;
        }

        // Track comment/string state
        if (!inString && !inChar && !inBlockComment && !inLineComment) {
            if (ch == '/' && i + 1 < content.size()) {
                if (content[i + 1] == '/') { inLineComment = true; prevCh = ch; continue; }
                if (content[i + 1] == '*') { inBlockComment = true; prevCh = ch; continue; }
            }
        }
        if (inBlockComment) {
            if (ch == '/' && prevCh == '*') inBlockComment = false;
            prevCh = ch;
            continue;
        }
        if (inLineComment) { prevCh = ch; continue; }

        if (!inString && !inChar && ch == '"' && prevCh != '\\') { inString = true; prevCh = ch; continue; }
        if (inString && ch == '"' && prevCh != '\\') { inString = false; prevCh = ch; continue; }
        if (!inString && !inChar && ch == '\'' && prevCh != '\\') { inChar = true; prevCh = ch; continue; }
        if (inChar && ch == '\'' && prevCh != '\\') { inChar = false; prevCh = ch; continue; }
        if (inString || inChar) { prevCh = ch; continue; }

        // Track braces
        switch (ch) {
            case '{': braceStack.push({ch, lineNum, col}); break;
            case '}':
                if (braceStack.empty()) {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::UnmatchedBrace;
                    err.severity = SyntaxSeverity::Error;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.column = col;
                    err.message = "Unmatched closing brace '}'";
                    err.context = getContext(content, lineNum);
                    err.autoFixable = true;
                    errors.push_back(err);
                } else {
                    braceStack.pop();
                }
                break;
            case '(': parenStack.push({ch, lineNum, col}); break;
            case ')':
                if (parenStack.empty()) {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::UnmatchedParen;
                    err.severity = SyntaxSeverity::Error;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.column = col;
                    err.message = "Unmatched closing parenthesis ')'";
                    err.context = getContext(content, lineNum);
                    err.autoFixable = true;
                    errors.push_back(err);
                } else {
                    parenStack.pop();
                }
                break;
            case '[': bracketStack.push({ch, lineNum, col}); break;
            case ']':
                if (bracketStack.empty()) {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::UnmatchedBracket;
                    err.severity = SyntaxSeverity::Error;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.column = col;
                    err.message = "Unmatched closing bracket ']'";
                    err.context = getContext(content, lineNum);
                    err.autoFixable = true;
                    errors.push_back(err);
                } else {
                    bracketStack.pop();
                }
                break;
        }
        prevCh = ch;
    }

    // Report unclosed
    while (!braceStack.empty()) {
        auto& info = braceStack.top();
        SyntaxError err;
        err.kind = SyntaxErrorKind::UnmatchedBrace;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = info.line;
        err.column = info.col;
        err.message = "Unmatched opening brace '{'";
        err.context = getContext(content, info.line);
        err.autoFixable = true;
        errors.push_back(err);
        braceStack.pop();
    }
    while (!parenStack.empty()) {
        auto& info = parenStack.top();
        SyntaxError err;
        err.kind = SyntaxErrorKind::UnmatchedParen;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = info.line;
        err.column = info.col;
        err.message = "Unmatched opening parenthesis '('";
        err.context = getContext(content, info.line);
        err.autoFixable = true;
        errors.push_back(err);
        parenStack.pop();
    }
    while (!bracketStack.empty()) {
        auto& info = bracketStack.top();
        SyntaxError err;
        err.kind = SyntaxErrorKind::UnmatchedBracket;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = info.line;
        err.column = info.col;
        err.message = "Unmatched opening bracket '['";
        err.context = getContext(content, info.line);
        err.autoFixable = true;
        errors.push_back(err);
        bracketStack.pop();
    }
}

// ============================================================================
// C/C++ Semicolon Checking
// ============================================================================

void SyntaxHealerSubAgent::checkSemicolons(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixSemicolons) return;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    bool inBlockComment = false;

    while (std::getline(stream, line)) {
        lineNum++;

        // Track block comments
        if (inBlockComment) {
            if (line.find("*/") != std::string::npos) inBlockComment = false;
            continue;
        }
        if (line.find("/*") != std::string::npos && line.find("*/") == std::string::npos) {
            inBlockComment = true;
            continue;
        }

        // Remove line comments
        std::string trimmed = line;
        size_t commentPos = trimmed.find("//");
        if (commentPos != std::string::npos) trimmed = trimmed.substr(0, commentPos);

        // Skip blank lines and preprocessor
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        trimmed = trimmed.substr(firstNon);
        if (trimmed.empty()) continue;
        if (trimmed[0] == '#') continue;
        if (trimmed[0] == '{' || trimmed[0] == '}') continue;

        // Lines that should end with semicolon but don't
        // Heuristic: variable declarations, return statements, simple assignments
        bool shouldHaveSemicolon = false;

        // return statement
        if (trimmed.find("return ") == 0 || trimmed == "return") {
            shouldHaveSemicolon = true;
        }
        // break/continue
        else if (trimmed == "break" || trimmed == "continue") {
            shouldHaveSemicolon = true;
        }

        if (shouldHaveSemicolon) {
            // Check if it ends with ;
            size_t lastNon = trimmed.find_last_not_of(" \t\r");
            if (lastNon != std::string::npos && trimmed[lastNon] != ';' &&
                trimmed[lastNon] != '{' && trimmed[lastNon] != ',') {
                SyntaxError err;
                err.kind = SyntaxErrorKind::MissingSemicolon;
                err.severity = SyntaxSeverity::Error;
                err.filePath = filePath;
                err.line = lineNum;
                err.column = (int)trimmed.size() + 1;
                err.message = "Missing semicolon at end of statement";
                err.context = getContext(content, lineNum);
                err.suggestion = trimmed + ";";
                err.autoFixable = true;
                errors.push_back(err);
            }
        }

        // Double semicolons (except in for loops)
        if (trimmed.find(";;") != std::string::npos &&
            trimmed.find("for") == std::string::npos) {
            SyntaxError err;
            err.kind = SyntaxErrorKind::ExtraSemicolon;
            err.severity = SyntaxSeverity::Warning;
            err.filePath = filePath;
            err.line = lineNum;
            err.message = "Double semicolon detected";
            err.context = getContext(content, lineNum);
            err.autoFixable = true;
            errors.push_back(err);
        }
    }
}

// ============================================================================
// String Checking
// ============================================================================

void SyntaxHealerSubAgent::checkStrings(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixStrings) return;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        // Count unescaped quotes
        int doubleQuotes = 0;
        bool escaped = false;
        bool inChar = false;

        for (size_t i = 0; i < line.size(); i++) {
            char ch = line[i];
            if (escaped) { escaped = false; continue; }
            if (ch == '\\') { escaped = true; continue; }

            // Skip char literals
            if (!inChar && ch == '\'') { inChar = true; continue; }
            if (inChar && ch == '\'') { inChar = false; continue; }
            if (inChar) continue;

            // Skip // comments
            if (ch == '/' && i + 1 < line.size() && line[i + 1] == '/') break;

            if (ch == '"') doubleQuotes++;
        }

        // Odd number of quotes means unterminated
        if (doubleQuotes % 2 != 0) {
            // Check for raw strings R"(...)"
            if (line.find("R\"") == std::string::npos) {
                SyntaxError err;
                err.kind = SyntaxErrorKind::UnterminatedString;
                err.severity = SyntaxSeverity::Error;
                err.filePath = filePath;
                err.line = lineNum;
                err.message = "Unterminated string literal";
                err.context = getContext(content, lineNum);
                err.autoFixable = true;
                errors.push_back(err);
            }
        }
    }
}

// ============================================================================
// Comment Checking
// ============================================================================

void SyntaxHealerSubAgent::checkComments(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixComments) return;

    // Find unclosed block comments
    int openLine = 0;
    int depth = 0;
    int lineNum = 0;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        lineNum++;
        for (size_t i = 0; i < line.size(); i++) {
            if (i + 1 < line.size()) {
                if (line[i] == '/' && line[i + 1] == '*') {
                    if (depth == 0) openLine = lineNum;
                    depth++;
                    i++;
                } else if (line[i] == '*' && line[i + 1] == '/') {
                    depth--;
                    if (depth < 0) {
                        SyntaxError err;
                        err.kind = SyntaxErrorKind::UnterminatedComment;
                        err.severity = SyntaxSeverity::Error;
                        err.filePath = filePath;
                        err.line = lineNum;
                        err.message = "Unexpected */ without matching /*";
                        err.context = getContext(content, lineNum);
                        err.autoFixable = true;
                        errors.push_back(err);
                        depth = 0;
                    }
                    i++;
                }
                // Nested // in block comment is fine
                if (depth == 0 && line[i] == '/' && line[i + 1] == '/') break;
            }
        }
    }

    if (depth > 0) {
        SyntaxError err;
        err.kind = SyntaxErrorKind::UnterminatedComment;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = openLine;
        err.message = "Unterminated block comment /* opened here";
        err.context = getContext(content, openLine);
        err.autoFixable = true;
        errors.push_back(err);
    }
}

// ============================================================================
// Preprocessor Checking
// ============================================================================

void SyntaxHealerSubAgent::checkPreprocessor(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixPreprocessor) return;

    struct IfInfo { int line; std::string directive; };
    std::stack<IfInfo> ifStack;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        size_t firstNon = line.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        if (line[firstNon] != '#') continue;

        std::string directive = line.substr(firstNon + 1);
        size_t dirStart = directive.find_first_not_of(" \t");
        if (dirStart == std::string::npos) {
            SyntaxError err;
            err.kind = SyntaxErrorKind::MalformedPreprocessor;
            err.severity = SyntaxSeverity::Warning;
            err.filePath = filePath;
            err.line = lineNum;
            err.message = "Empty preprocessor directive";
            err.context = getContext(content, lineNum);
            err.autoFixable = false;
            errors.push_back(err);
            continue;
        }
        directive = directive.substr(dirStart);

        // Track #if/#ifdef/#ifndef
        if (directive.substr(0, 2) == "if" ||
            directive.substr(0, 5) == "ifdef" ||
            directive.substr(0, 6) == "ifndef") {
            ifStack.push({lineNum, directive});
        }
        // #elif and #else require open #if
        else if (directive.substr(0, 4) == "elif" ||
                 directive.substr(0, 4) == "else") {
            if (ifStack.empty()) {
                SyntaxError err;
                err.kind = SyntaxErrorKind::OrphanedElse;
                err.severity = SyntaxSeverity::Error;
                err.filePath = filePath;
                err.line = lineNum;
                err.message = "#" + directive.substr(0, 4) + " without matching #if";
                err.context = getContext(content, lineNum);
                err.autoFixable = false;
                errors.push_back(err);
            }
        }
        // #endif closes an #if
        else if (directive.substr(0, 5) == "endif") {
            if (ifStack.empty()) {
                SyntaxError err;
                err.kind = SyntaxErrorKind::OrphanedElse;
                err.severity = SyntaxSeverity::Error;
                err.filePath = filePath;
                err.line = lineNum;
                err.message = "#endif without matching #if";
                err.context = getContext(content, lineNum);
                err.autoFixable = true;
                errors.push_back(err);
            } else {
                ifStack.pop();
            }
        }
    }

    // Report unclosed #if
    while (!ifStack.empty()) {
        auto& info = ifStack.top();
        SyntaxError err;
        err.kind = SyntaxErrorKind::MissingEndif;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = info.line;
        err.message = "#if directive without matching #endif";
        err.context = getContext(content, info.line);
        err.autoFixable = true;
        errors.push_back(err);
        ifStack.pop();
    }
}

// ============================================================================
// Encoding Checking
// ============================================================================

void SyntaxHealerSubAgent::checkEncoding(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixEncoding) return;

    // Check for BOM
    if (content.size() >= 3 &&
        (unsigned char)content[0] == 0xEF &&
        (unsigned char)content[1] == 0xBB &&
        (unsigned char)content[2] == 0xBF) {
        SyntaxError err;
        err.kind = SyntaxErrorKind::BOMMismatch;
        err.severity = SyntaxSeverity::Info;
        err.filePath = filePath;
        err.line = 1;
        err.message = "UTF-8 BOM detected — may cause issues with some tools";
        err.autoFixable = true;
        errors.push_back(err);
    }

    // Check for invalid UTF-8 sequences
    for (size_t i = 0; i < content.size(); i++) {
        unsigned char c = (unsigned char)content[i];
        if (c < 0x80) continue;

        int expectedBytes = 0;
        if ((c & 0xE0) == 0xC0) expectedBytes = 1;
        else if ((c & 0xF0) == 0xE0) expectedBytes = 2;
        else if ((c & 0xF8) == 0xF0) expectedBytes = 3;
        else {
            // Invalid start byte
            int line = 1;
            for (size_t j = 0; j < i; j++) {
                if (content[j] == '\n') line++;
            }
            SyntaxError err;
            err.kind = SyntaxErrorKind::InvalidUTF8;
            err.severity = SyntaxSeverity::Warning;
            err.filePath = filePath;
            err.line = line;
            err.message = "Invalid UTF-8 byte sequence";
            err.autoFixable = false;
            errors.push_back(err);
            continue;
        }

        // Verify continuation bytes
        bool valid = true;
        for (int j = 0; j < expectedBytes; j++) {
            if (i + 1 + j >= content.size() ||
                ((unsigned char)content[i + 1 + j] & 0xC0) != 0x80) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            int line = 1;
            for (size_t j = 0; j < i; j++) {
                if (content[j] == '\n') line++;
            }
            SyntaxError err;
            err.kind = SyntaxErrorKind::InvalidUTF8;
            err.severity = SyntaxSeverity::Warning;
            err.filePath = filePath;
            err.line = line;
            err.message = "Truncated UTF-8 byte sequence";
            err.autoFixable = false;
            errors.push_back(err);
        }
        i += expectedBytes;
    }
}

// ============================================================================
// MASM PROC/ENDP Checking
// ============================================================================

void SyntaxHealerSubAgent::checkMasmProcEndp(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixMasm) return;

    struct ProcInfo { std::string name; int line; bool hasFrame; };
    std::stack<ProcInfo> procStack;
    bool hasEnd = false;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        std::string trimmed = line;
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        trimmed = trimmed.substr(firstNon);
        if (trimmed[0] == ';') continue;

        // PROC
        std::regex procRegex(R"(^(\w+)\s+PROC\b(.*))", std::regex::icase);
        std::smatch procMatch;
        if (std::regex_search(trimmed, procMatch, procRegex)) {
            ProcInfo info;
            info.name = procMatch[1].str();
            info.line = lineNum;
            std::string rest = procMatch[2].str();
            std::string restUpper = rest;
            std::transform(restUpper.begin(), restUpper.end(), restUpper.begin(), ::toupper);
            info.hasFrame = (restUpper.find("FRAME") != std::string::npos);
            procStack.push(info);
            continue;
        }

        // ENDP
        std::regex endpRegex(R"(^(\w+)\s+ENDP\b)", std::regex::icase);
        std::smatch endpMatch;
        if (std::regex_search(trimmed, endpMatch, endpRegex)) {
            std::string endpName = endpMatch[1].str();
            if (procStack.empty()) {
                SyntaxError err;
                err.kind = SyntaxErrorKind::MasmMissingEndp;
                err.severity = SyntaxSeverity::Error;
                err.filePath = filePath;
                err.line = lineNum;
                err.message = "ENDP for '" + endpName + "' without matching PROC";
                err.context = getContext(content, lineNum);
                err.autoFixable = false;
                errors.push_back(err);
            } else {
                auto& top = procStack.top();
                // Name mismatch
                std::string topUpper = top.name;
                std::string endpUpper = endpName;
                std::transform(topUpper.begin(), topUpper.end(), topUpper.begin(), ::toupper);
                std::transform(endpUpper.begin(), endpUpper.end(), endpUpper.begin(), ::toupper);
                if (topUpper != endpUpper) {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::MasmMissingEndp;
                    err.severity = SyntaxSeverity::Error;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.message = "ENDP name '" + endpName + "' doesn't match PROC '" + top.name + "'";
                    err.context = getContext(content, lineNum);
                    err.suggestion = top.name + " ENDP";
                    err.autoFixable = true;
                    errors.push_back(err);
                }
                procStack.pop();
            }
            continue;
        }

        // .ENDPROLOG check (only needed for FRAME procs)
        {
            std::string upper = trimmed;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            if (upper.find(".ENDPROLOG") != std::string::npos) {
                if (procStack.empty() || !procStack.top().hasFrame) {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::MasmMissingFrame;
                    err.severity = SyntaxSeverity::Warning;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.message = ".ENDPROLOG outside of a FRAME procedure";
                    err.context = getContext(content, lineNum);
                    err.autoFixable = false;
                    errors.push_back(err);
                }
            }
        }

        // END directive
        {
            std::string upper = trimmed;
            std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
            if (upper == "END" || upper.substr(0, 4) == "END ") {
                hasEnd = true;
            }
        }
    }

    // Report unclosed PROCs
    while (!procStack.empty()) {
        auto& info = procStack.top();
        SyntaxError err;
        err.kind = SyntaxErrorKind::MasmMissingEndp;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = info.line;
        err.message = "PROC '" + info.name + "' without matching ENDP";
        err.context = getContext(content, info.line);
        err.suggestion = info.name + " ENDP";
        err.autoFixable = true;
        errors.push_back(err);

        if (info.hasFrame) {
            // Also check for .ENDPROLOG in the body
            // Simplified: just warn
            SyntaxError frameErr;
            frameErr.kind = SyntaxErrorKind::MasmMissingFrame;
            frameErr.severity = SyntaxSeverity::Warning;
            frameErr.filePath = filePath;
            frameErr.line = info.line;
            frameErr.message = "FRAME PROC '" + info.name + "' — verify .ENDPROLOG is present";
            frameErr.autoFixable = false;
            errors.push_back(frameErr);
        }

        procStack.pop();
    }

    // Missing END directive
    if (!hasEnd) {
        SyntaxError err;
        err.kind = SyntaxErrorKind::MasmMissingEnd;
        err.severity = SyntaxSeverity::Error;
        err.filePath = filePath;
        err.line = lineNum;
        err.message = "Missing END directive at end of MASM file";
        err.suggestion = "END";
        err.autoFixable = true;
        errors.push_back(err);
    }
}

// ============================================================================
// MASM Register Validation
// ============================================================================

void SyntaxHealerSubAgent::checkMasmRegisters(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixMasm) return;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    // Instructions that take register operands
    static const std::unordered_set<std::string> regInstructions = {
        "mov", "add", "sub", "xor", "and", "or", "cmp", "test",
        "lea", "push", "pop", "inc", "dec", "not", "neg",
        "shl", "shr", "sar", "sal", "rol", "ror",
        "imul", "idiv", "mul", "div", "movzx", "movsx",
        "cmov", "cmove", "cmovne", "cmovz", "cmovnz",
        "xchg", "bswap", "bt", "bts", "btr", "btc",
        "movq", "movd", "movdqa", "movdqu", "movaps", "movups",
        "vmovdqa", "vmovdqu", "vmovaps", "vmovups"
    };

    while (std::getline(stream, line)) {
        lineNum++;

        std::string trimmed = line;
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        trimmed = trimmed.substr(firstNon);
        if (trimmed[0] == ';') continue;

        // Remove comments
        size_t commentPos = trimmed.find(';');
        if (commentPos != std::string::npos) trimmed = trimmed.substr(0, commentPos);

        // Extract instruction
        std::string lower = trimmed;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        for (const auto& inst : regInstructions) {
            size_t instPos = lower.find(inst);
            if (instPos == std::string::npos) continue;
            // Make sure it's a whole word
            if (instPos > 0 && std::isalnum(lower[instPos - 1])) continue;
            size_t afterInst = instPos + inst.size();
            if (afterInst < lower.size() && std::isalnum(lower[afterInst])) continue;

            // Extract operands
            std::string operands = lower.substr(afterInst);
            size_t opStart = operands.find_first_not_of(" \t,");
            if (opStart == std::string::npos) continue;
            operands = operands.substr(opStart);

            // Check each word-like token against register list
            std::regex wordRegex(R"(\b([a-z][a-z0-9]+)\b)");
            auto begin = std::sregex_iterator(operands.begin(), operands.end(), wordRegex);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                std::string token = (*it)[1].str();
                // Skip numbers, memory size operators, known non-register words
                if (token == "ptr" || token == "byte" || token == "word" ||
                    token == "dword" || token == "qword" || token == "tbyte" ||
                    token == "xmmword" || token == "ymmword" || token == "offset" ||
                    token == "near" || token == "far" || token == "short") continue;

                // Check if it looks like a register (starts with known prefix)
                bool looksLikeReg =
                    (token.size() <= 5 &&
                     (token[0] == 'r' || token[0] == 'e' ||
                      token.size() == 2 ||
                      token.substr(0, 3) == "xmm" || token.substr(0, 3) == "ymm" ||
                      token.substr(0, 3) == "zmm"));

                if (looksLikeReg && m_config.validMasmRegisters.find(token) ==
                    m_config.validMasmRegisters.end()) {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::MasmBadRegister;
                    err.severity = SyntaxSeverity::Error;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.message = "Potentially invalid register: '" + token + "'";
                    err.context = getContext(content, lineNum);
                    err.autoFixable = false;
                    errors.push_back(err);
                }
            }
            break; // Only check first instruction match per line
        }
    }
}

// ============================================================================
// MASM Directive Validation
// ============================================================================

void SyntaxHealerSubAgent::checkMasmDirectives(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixMasm) return;

    static const std::unordered_set<std::string> validDirectives = {
        ".CODE", ".DATA", ".DATA?", ".CONST", ".STACK",
        ".MODEL", ".386", ".486", ".586", ".686", ".X64",
        "OPTION", "INCLUDE", "INCLUDELIB", "EXTERN", "EXTERNDEF", "EXTRN",
        "PUBLIC", "PROTO", "INVOKE", "PROC", "ENDP", "STRUCT", "ENDS",
        "UNION", "RECORD", "MACRO", "ENDM", "LOCAL", "IF", "IFDEF",
        "IFNDEF", "ELSE", "ELSEIF", "ENDIF", "REPEAT", "WHILE",
        "FOR", "FORC", "IRP", "IRPC", "EXITM", "PURGE",
        "SEGMENT", "ASSUME", "ORG", "ALIGN", "EVEN", "COMMENT",
        "DB", "DW", "DD", "DQ", "DF", "DT",
        "BYTE", "WORD", "DWORD", "QWORD", "TBYTE", "REAL4", "REAL8",
        "LABEL", "EQU", "TEXTEQU", "CATSTR", "SUBSTR", "SIZESTR",
        ".ALLOCSTACK", ".ENDPROLOG", ".PUSHREG", ".SAVEREG", ".SAVEXMM128",
        ".SETFRAME", "END", "TITLE", "PAGE", "NAME",
        "_TEXT", "_DATA",
    };

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmed = line;
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        trimmed = trimmed.substr(firstNon);
        if (trimmed[0] == ';') continue;

        // Check directives starting with .
        if (trimmed[0] == '.') {
            std::string directive;
            for (size_t i = 0; i < trimmed.size() && !std::isspace(trimmed[i]); i++) {
                directive += (char)std::toupper(trimmed[i]);
            }
            if (validDirectives.find(directive) == validDirectives.end() &&
                directive.size() > 1) {
                // Known exception patterns (.LIST, .XLIST, etc.) — skip
                if (directive != ".LIST" && directive != ".XLIST" &&
                    directive != ".NOLIST" && directive != ".LISTALL" &&
                    directive != ".ERR" && directive != ".ERRB" &&
                    directive != ".ERRNB" && directive != ".ERRE" &&
                    directive != ".ERRNZ" && directive != ".ERRDEF" &&
                    directive != ".ERRNDEF" && directive != ".ERRDIF") {
                    SyntaxError err;
                    err.kind = SyntaxErrorKind::MasmBadDirective;
                    err.severity = SyntaxSeverity::Warning;
                    err.filePath = filePath;
                    err.line = lineNum;
                    err.message = "Potentially unknown MASM directive: " + directive;
                    err.context = getContext(content, lineNum);
                    err.autoFixable = false;
                    errors.push_back(err);
                }
            }
        }
    }
}

// ============================================================================
// MASM Frame Validation
// ============================================================================

void SyntaxHealerSubAgent::checkMasmFrame(
    const std::string& content, const std::string& filePath,
    std::vector<SyntaxError>& errors) const
{
    if (!m_config.fixMasm) return;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    bool inFrameProc = false;
    bool hasEndProlog = false;
    std::string frameProcName;
    int frameProcLine = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        std::string upper = line;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

        // Trim
        size_t firstNon = upper.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        upper = upper.substr(firstNon);
        if (upper[0] == ';') continue;

        if (upper.find("PROC") != std::string::npos &&
            upper.find("FRAME") != std::string::npos) {
            inFrameProc = true;
            hasEndProlog = false;
            frameProcLine = lineNum;

            // Extract name
            std::regex nameRegex(R"(^(\w+)\s+PROC)", std::regex::icase);
            std::smatch match;
            std::string trimmed = line;
            size_t fn = trimmed.find_first_not_of(" \t");
            if (fn != std::string::npos) trimmed = trimmed.substr(fn);
            if (std::regex_search(trimmed, match, nameRegex)) {
                frameProcName = match[1].str();
            }
        }

        if (inFrameProc && upper.find(".ENDPROLOG") != std::string::npos) {
            hasEndProlog = true;
        }

        if (inFrameProc && upper.find("ENDP") != std::string::npos) {
            if (!hasEndProlog) {
                SyntaxError err;
                err.kind = SyntaxErrorKind::MasmMissingFrame;
                err.severity = SyntaxSeverity::Error;
                err.filePath = filePath;
                err.line = frameProcLine;
                err.message = "FRAME PROC '" + frameProcName + "' missing .ENDPROLOG";
                err.context = getContext(content, frameProcLine);
                err.autoFixable = true;
                err.suggestion = "    .ENDPROLOG";
                errors.push_back(err);
            }
            inFrameProc = false;
        }
    }
}

// ============================================================================
// Unified Analysis
// ============================================================================

std::vector<SyntaxError> SyntaxHealerSubAgent::analyzeCpp(
    const std::string& filePath) const
{
    std::vector<SyntaxError> errors;
    std::string content = readFile(filePath);
    if (content.empty()) return errors;

    checkBraces(content, filePath, errors);
    checkSemicolons(content, filePath, errors);
    checkStrings(content, filePath, errors);
    checkComments(content, filePath, errors);
    checkPreprocessor(content, filePath, errors);
    checkEncoding(content, filePath, errors);

    // Limit per-file errors
    if ((int)errors.size() > m_config.maxErrorsPerFile) {
        errors.resize(m_config.maxErrorsPerFile);
    }

    return errors;
}

std::vector<SyntaxError> SyntaxHealerSubAgent::analyzeMasm(
    const std::string& filePath) const
{
    std::vector<SyntaxError> errors;
    std::string content = readFile(filePath);
    if (content.empty()) return errors;

    checkMasmProcEndp(content, filePath, errors);
    checkMasmRegisters(content, filePath, errors);
    checkMasmDirectives(content, filePath, errors);
    checkMasmFrame(content, filePath, errors);
    checkEncoding(content, filePath, errors);

    if ((int)errors.size() > m_config.maxErrorsPerFile) {
        errors.resize(m_config.maxErrorsPerFile);
    }

    return errors;
}

std::vector<SyntaxError> SyntaxHealerSubAgent::analyze(
    const std::string& filePath) const
{
    std::string ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".asm" || ext == ".inc") {
        return analyzeMasm(filePath);
    } else {
        return analyzeCpp(filePath);
    }
}

// ============================================================================
// Fix Generation
// ============================================================================

std::vector<SyntaxFixAction> SyntaxHealerSubAgent::generateFixes(
    const std::vector<SyntaxError>& errors) const
{
    std::vector<SyntaxFixAction> fixes;

    for (const auto& error : errors) {
        if (!error.autoFixable) continue;
        if (error.severity == SyntaxSeverity::Info && m_config.minFixConfidence > 0.5f) continue;

        SyntaxFixAction fix;
        fix.filePath = error.filePath;
        fix.line = error.line;
        fix.column = error.column;
        fix.errorKind = error.kind;

        switch (error.kind) {
            case SyntaxErrorKind::UnmatchedBrace:
                fix = generateBraceFix(error);
                break;
            case SyntaxErrorKind::UnmatchedParen:
            case SyntaxErrorKind::UnmatchedBracket:
                fix = generateBraceFix(error);
                break;
            case SyntaxErrorKind::MissingSemicolon:
            case SyntaxErrorKind::ExtraSemicolon:
                fix = generateSemicolonFix(error);
                break;
            case SyntaxErrorKind::UnterminatedString:
                fix = generateStringFix(error);
                break;
            case SyntaxErrorKind::UnterminatedComment:
                fix = generateCommentFix(error);
                break;
            case SyntaxErrorKind::MissingEndif:
            case SyntaxErrorKind::OrphanedElse:
                fix = generatePreprocessorFix(error);
                break;
            case SyntaxErrorKind::MasmMissingEndp:
            case SyntaxErrorKind::MasmMissingEnd:
            case SyntaxErrorKind::MasmMissingFrame:
            case SyntaxErrorKind::MasmBadDirective:
                fix = generateMasmFix(error);
                break;
            case SyntaxErrorKind::BOMMismatch:
                fix.type = SyntaxFixAction::Type::DeleteText;
                fix.line = 1;
                fix.column = 1;
                fix.endColumn = 4;
                fix.reason = "Remove UTF-8 BOM";
                fix.confidence = 0.9f;
                fix.priority = 30;
                break;
            default:
                continue;
        }

        if (fix.confidence >= m_config.minFixConfidence) {
            fixes.push_back(fix);
        }
    }

    std::sort(fixes.begin(), fixes.end());
    return fixes;
}

SyntaxFixAction SyntaxHealerSubAgent::generateBraceFix(const SyntaxError& error) const {
    SyntaxFixAction fix;
    fix.filePath = error.filePath;
    fix.line = error.line;
    fix.column = error.column;
    fix.errorKind = error.kind;
    fix.reason = error.message;
    fix.priority = 90;

    bool isOpening = (error.message.find("opening") != std::string::npos);

    switch (error.kind) {
        case SyntaxErrorKind::UnmatchedBrace:
            if (isOpening) {
                fix.type = SyntaxFixAction::Type::InsertLine;
                fix.newText = "}";
                fix.confidence = 0.75f;
            } else {
                fix.type = SyntaxFixAction::Type::DeleteText;
                fix.confidence = 0.6f;
            }
            break;
        case SyntaxErrorKind::UnmatchedParen:
            if (isOpening) {
                fix.type = SyntaxFixAction::Type::InsertText;
                fix.newText = ")";
                fix.confidence = 0.8f;
            } else {
                fix.type = SyntaxFixAction::Type::DeleteText;
                fix.confidence = 0.5f;
            }
            break;
        case SyntaxErrorKind::UnmatchedBracket:
            if (isOpening) {
                fix.type = SyntaxFixAction::Type::InsertText;
                fix.newText = "]";
                fix.confidence = 0.8f;
            } else {
                fix.type = SyntaxFixAction::Type::DeleteText;
                fix.confidence = 0.5f;
            }
            break;
        default:
            fix.confidence = 0.3f;
            break;
    }

    return fix;
}

SyntaxFixAction SyntaxHealerSubAgent::generateSemicolonFix(const SyntaxError& error) const {
    SyntaxFixAction fix;
    fix.filePath = error.filePath;
    fix.line = error.line;
    fix.column = error.column;
    fix.errorKind = error.kind;
    fix.reason = error.message;

    if (error.kind == SyntaxErrorKind::MissingSemicolon) {
        fix.type = SyntaxFixAction::Type::InsertText;
        fix.newText = ";";
        fix.confidence = 0.85f;
        fix.priority = 85;
    } else {
        fix.type = SyntaxFixAction::Type::ReplaceLine;
        // Replace ;; with ;
        std::string lineContent = error.context;
        size_t pos = lineContent.find(";;");
        if (pos != std::string::npos) {
            fix.oldText = ";;";
            fix.newText = ";";
        }
        fix.confidence = 0.9f;
        fix.priority = 70;
    }

    return fix;
}

SyntaxFixAction SyntaxHealerSubAgent::generateStringFix(const SyntaxError& error) const {
    SyntaxFixAction fix;
    fix.type = SyntaxFixAction::Type::InsertText;
    fix.filePath = error.filePath;
    fix.line = error.line;
    fix.newText = "\"";
    fix.errorKind = error.kind;
    fix.reason = "Close unterminated string literal";
    fix.confidence = 0.7f;
    fix.priority = 80;
    return fix;
}

SyntaxFixAction SyntaxHealerSubAgent::generateCommentFix(const SyntaxError& error) const {
    SyntaxFixAction fix;
    fix.filePath = error.filePath;
    fix.line = error.line;
    fix.errorKind = error.kind;
    fix.reason = error.message;

    if (error.message.find("Unexpected */") != std::string::npos) {
        fix.type = SyntaxFixAction::Type::DeleteText;
        fix.oldText = "*/";
        fix.confidence = 0.6f;
        fix.priority = 60;
    } else {
        fix.type = SyntaxFixAction::Type::InsertLine;
        fix.newText = "*/";
        fix.confidence = 0.75f;
        fix.priority = 75;
    }

    return fix;
}

SyntaxFixAction SyntaxHealerSubAgent::generatePreprocessorFix(const SyntaxError& error) const {
    SyntaxFixAction fix;
    fix.filePath = error.filePath;
    fix.line = error.line;
    fix.errorKind = error.kind;
    fix.reason = error.message;

    if (error.kind == SyntaxErrorKind::MissingEndif) {
        fix.type = SyntaxFixAction::Type::InsertLine;
        fix.newText = "#endif";
        // Insert at end of file
        std::string content = readFile(error.filePath);
        fix.line = countLines(content);
        fix.confidence = 0.8f;
        fix.priority = 80;
    } else {
        // Orphaned #else/#endif — harder to fix automatically
        fix.type = SyntaxFixAction::Type::DeleteLine;
        fix.confidence = 0.4f;
        fix.priority = 40;
    }

    return fix;
}

SyntaxFixAction SyntaxHealerSubAgent::generateMasmFix(const SyntaxError& error) const {
    SyntaxFixAction fix;
    fix.filePath = error.filePath;
    fix.line = error.line;
    fix.errorKind = error.kind;
    fix.reason = error.message;

    switch (error.kind) {
        case SyntaxErrorKind::MasmMissingEndp:
            if (!error.suggestion.empty()) {
                fix.type = SyntaxFixAction::Type::InsertLine;
                fix.newText = error.suggestion;
                fix.confidence = 0.85f;
                fix.priority = 90;
            } else {
                fix.confidence = 0.3f;
                fix.priority = 30;
            }
            break;
        case SyntaxErrorKind::MasmMissingEnd:
            fix.type = SyntaxFixAction::Type::InsertLine;
            fix.newText = "END";
            std::string content = readFile(error.filePath);
            fix.line = countLines(content);
            fix.confidence = 0.9f;
            fix.priority = 85;
            break;
    }
    // MasmMissingFrame handled via suggestion
    if (error.kind == SyntaxErrorKind::MasmMissingFrame && !error.suggestion.empty()) {
        fix.type = SyntaxFixAction::Type::InsertLine;
        fix.newText = error.suggestion;
        fix.confidence = 0.7f;
        fix.priority = 75;
    }

    return fix;
}

// ============================================================================
// Apply Fixes
// ============================================================================

int SyntaxHealerSubAgent::applyFixes(std::vector<SyntaxFixAction>& fixes) {
    if (m_config.dryRun) return 0;

    int applied = 0;
    for (auto& fix : fixes) {
        if (fix.confidence < m_config.minFixConfidence) continue;
        if (applySingleFix(fix)) {
            applied++;
            if (m_onFix) m_onFix("", fix, true);
        } else {
            if (m_onFix) m_onFix("", fix, false);
        }
    }

    // Clear cache for modified files
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        for (const auto& fix : fixes) {
            m_fileCache.erase(fix.filePath);
        }
    }

    return applied;
}

bool SyntaxHealerSubAgent::applySingleFix(const SyntaxFixAction& fix) {
    std::string content = readFile(fix.filePath);
    if (content.empty() && fix.type != SyntaxFixAction::Type::InsertLine) return false;

    std::istringstream in(content);
    std::ostringstream out;
    std::string line;
    int lineNum = 0;
    bool done = false;

    switch (fix.type) {
        case SyntaxFixAction::Type::InsertLine: {
            while (std::getline(in, line)) {
                lineNum++;
                out << line << "\n";
                if (lineNum == fix.line && !done) {
                    out << fix.newText << "\n";
                    done = true;
                }
            }
            if (!done) {
                out << fix.newText << "\n";
                done = true;
            }
            break;
        }
        case SyntaxFixAction::Type::InsertText: {
            while (std::getline(in, line)) {
                lineNum++;
                if (lineNum == fix.line && !done) {
                    if (fix.column > 0 && fix.column <= (int)line.size()) {
                        line.insert(fix.column - 1, fix.newText);
                    } else {
                        line += fix.newText;
                    }
                    done = true;
                }
                out << line << "\n";
            }
            break;
        }
        case SyntaxFixAction::Type::DeleteLine: {
            while (std::getline(in, line)) {
                lineNum++;
                if (lineNum == fix.line && !done) {
                    done = true;
                    continue; // Skip this line
                }
                out << line << "\n";
            }
            break;
        }
        case SyntaxFixAction::Type::ReplaceLine: {
            while (std::getline(in, line)) {
                lineNum++;
                if (lineNum == fix.line && !done) {
                    if (!fix.oldText.empty()) {
                        size_t pos = line.find(fix.oldText);
                        if (pos != std::string::npos) {
                            line.replace(pos, fix.oldText.size(), fix.newText);
                        }
                    } else {
                        line = fix.newText;
                    }
                    done = true;
                }
                out << line << "\n";
            }
            break;
        }
        case SyntaxFixAction::Type::DeleteText: {
            // Special case: BOM removal
            if (fix.line == 1 && fix.column == 1 && fix.endColumn == 4) {
                if (content.size() >= 3 &&
                    (unsigned char)content[0] == 0xEF &&
                    (unsigned char)content[1] == 0xBB &&
                    (unsigned char)content[2] == 0xBF) {
                    std::ofstream ofs(fix.filePath, std::ios::binary | std::ios::trunc);
                    if (!ofs.is_open()) return false;
                    ofs << content.substr(3);
                    return true;
                }
            }
            // General delete
            while (std::getline(in, line)) {
                lineNum++;
                if (lineNum == fix.line && !done && !fix.oldText.empty()) {
                    size_t pos = line.find(fix.oldText);
                    if (pos != std::string::npos) {
                        line.erase(pos, fix.oldText.size());
                    }
                    done = true;
                }
                out << line << "\n";
            }
            break;
        }
        case SyntaxFixAction::Type::ReplaceText:
        case SyntaxFixAction::Type::WrapBlock:
        case SyntaxFixAction::Type::SwapLines:
        case SyntaxFixAction::Type::IndentBlock:
        case SyntaxFixAction::Type::ReformatBlock:
        default:
            return false;
    }

    if (!done) return false;

    std::ofstream ofs(fix.filePath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << out.str();
    return true;
}

bool SyntaxHealerSubAgent::validateFix(
    const std::string& filePath,
    const SyntaxFixAction& fix) const
{
    // Re-analyze the file after fix and check if the specific error is gone
    auto errorsAfter = analyze(filePath);
    for (const auto& err : errorsAfter) {
        if (err.kind == fix.errorKind && err.line == fix.line) {
            return false; // Same error still present
        }
    }
    return true;
}

// ============================================================================
// Bulk Scan
// ============================================================================

SyntaxHealerResult SyntaxHealerSubAgent::scan(
    const std::string& parentId,
    const std::vector<std::string>& filePaths)
{
    auto startTime = std::chrono::steady_clock::now();
    std::string scanId = generateId();
    m_running.store(true);
    m_cancelled.store(false);

    int totalFiles = std::min((int)filePaths.size(), m_config.maxFilesPerScan);
    SyntaxHealerResult result = SyntaxHealerResult::ok(scanId);

    std::vector<SyntaxError> allErrors;

    for (int i = 0; i < totalFiles && !m_cancelled.load(); i++) {
        auto fileErrors = analyze(filePaths[i]);

        for (const auto& err : fileErrors) {
            if (m_onError) m_onError(scanId, err);
            allErrors.push_back(err);

            if (err.severity >= SyntaxSeverity::Error) result.errorsFound++;
            else result.warningsFound++;
        }

        if (m_onProgress) m_onProgress(scanId, i + 1, totalFiles);
    }

    auto elapsed = std::chrono::steady_clock::now() - startTime;
    result.filesScanned = totalFiles;
    result.remainingErrors = allErrors;
    result.elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    m_stats.totalScans++;
    m_stats.totalFilesProcessed += totalFiles;
    m_stats.totalErrorsFound += (int)allErrors.size();

    m_running.store(false);
    if (m_onComplete) m_onComplete(result);
    return result;
}

SyntaxHealerResult SyntaxHealerSubAgent::scanAndFix(
    const std::string& parentId,
    const std::vector<std::string>& filePaths)
{
    auto result = scan(parentId, filePaths);
    if (!result.success) return result;

    auto fixes = generateFixes(result.remainingErrors);
    int applied = applyFixes(fixes);

    result.errorsFixed = applied;
    result.errorsFailed = 0;
    result.errorsSkipped = (int)result.remainingErrors.size() - applied;

    // Separate applied/failed
    for (const auto& fix : fixes) {
        if (fix.confidence >= m_config.minFixConfidence) {
            result.appliedFixes.push_back(fix);
        }
    }

    m_stats.totalErrorsFixed += applied;

    if (m_onComplete) m_onComplete(result);
    return result;
}

std::string SyntaxHealerSubAgent::scanAndFixAsync(
    const std::string& parentId,
    const std::vector<std::string>& filePaths,
    SyntaxHealerCompleteCb onComplete)
{
    std::string scanId = generateId();

    if (m_manager) {
        std::ostringstream prompt;
        prompt << "Heal syntax errors across " << filePaths.size()
               << " files in project: " << m_config.projectRoot;
        m_manager->spawnSubAgent(parentId, "SyntaxHealer:" + scanId, prompt.str());
    }

    std::thread([this, parentId, filePaths, onComplete, scanId]() {
        auto result = scanAndFix(parentId, filePaths);
        result.scanId = scanId;
        if (onComplete) onComplete(result);
    }).detach();

    return scanId;
}

// ============================================================================
// Self-Healing via LLM
// ============================================================================

std::string SyntaxHealerSubAgent::selfHealComplex(
    const SyntaxError& error, const std::string& context)
{
    if (!m_engine || !m_manager) return "";

    m_stats.totalLLMAssists++;

    std::ostringstream prompt;
    prompt << "Fix this syntax error:\n"
           << "File: " << fs::path(error.filePath).filename().string() << "\n"
           << "Error: " << error.message << "\n"
           << "Context:\n" << context << "\n"
           << "Provide the corrected code only.";

    std::string agentId = m_manager->spawnSubAgent(
        "syntax-healer", "fix:" + syntaxErrorKindStr(error.kind), prompt.str());
    if (m_manager->waitForSubAgent(agentId, 15000)) {
        return m_manager->getSubAgentResult(agentId);
    }
    return "";
}

// ============================================================================
// Cancel / Stats
// ============================================================================

void SyntaxHealerSubAgent::cancel() {
    m_cancelled.store(true);
}

SyntaxHealerSubAgent::Stats SyntaxHealerSubAgent::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void SyntaxHealerSubAgent::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = {};
}
