// ============================================================================
// symbol_linker_subagent.cpp — Autonomous Symbol Resolution & Cross-TU Linker
// ============================================================================
// Production implementation. C++20, Win32, no Qt, no exceptions.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "symbol_linker_subagent.hpp"
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
#include <thread>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

// ============================================================================
// SymbolEntry helpers
// ============================================================================

void SymbolEntry::computeHash() {
    // FNV-1a hash of name + kind + file + linkage
    uint64_t h = 14695981039346656037ULL;
    auto fnv = [&](const std::string& s) {
        for (char c : s) {
            h ^= (uint64_t)(unsigned char)c;
            h *= 1099511628211ULL;
        }
    };
    fnv(name);
    h ^= (uint64_t)kind;  h *= 1099511628211ULL;
    h ^= (uint64_t)linkage; h *= 1099511628211ULL;
    fnv(filePath);
    hash = h;
}

std::string SymbolEntry::toJSON() const {
    std::ostringstream oss;
    oss << "{\"name\":\"" << name << "\""
        << ",\"kind\":\"" << symbolKindStr(kind) << "\""
        << ",\"file\":\"" << filePath << "\""
        << ",\"line\":" << line
        << ",\"defined\":" << (isDefined ? "true" : "false")
        << ",\"externC\":" << (isExternC ? "true" : "false");
    if (!signature.empty()) oss << ",\"sig\":\"" << signature << "\"";
    if (!qualifiedName.empty()) oss << ",\"qualified\":\"" << qualifiedName << "\"";
    oss << "}";
    return oss.str();
}

std::string SymbolConflict::toJSON() const {
    std::ostringstream oss;
    oss << "{\"type\":" << (int)type
        << ",\"symbol\":\"" << symbolName << "\""
        << ",\"severity\":" << severity
        << ",\"desc\":\"" << description << "\""
        << ",\"count\":" << conflicting.size() << "}";
    return oss.str();
}

// ============================================================================
// SymbolLinkerResult::summary
// ============================================================================
std::string SymbolLinkerResult::summary() const {
    std::ostringstream oss;
    oss << "SymbolLinker[" << scanId << "]: "
        << filesScanned << " files, "
        << symbolsDefined << " defined, "
        << symbolsReferenced << " referenced, "
        << symbolsResolved << " resolved, "
        << symbolsUnresolved << " unresolved, "
        << conflictsFound << " conflicts, "
        << fixesApplied << " fixes, "
        << stubsGenerated << " stubs — "
        << elapsedMs << "ms";
    return oss.str();
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

SymbolLinkerSubAgent::SymbolLinkerSubAgent(
    SubAgentManager* manager,
    AgenticEngine* engine,
    AgenticFailureDetector* detector,
    AgenticPuppeteer* puppeteer)
    : m_manager(manager)
    , m_engine(engine)
    , m_detector(detector)
    , m_puppeteer(puppeteer)
{
}

SymbolLinkerSubAgent::~SymbolLinkerSubAgent() {
    cancel();
}

// ============================================================================
// Configuration
// ============================================================================

void SymbolLinkerSubAgent::setConfig(const SymbolLinkerConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

// ============================================================================
// UUID
// ============================================================================
std::string SymbolLinkerSubAgent::generateId() const {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream ss;
    ss << "sl-" << std::hex << std::setfill('0')
       << std::setw(8) << (dis(gen) & 0xFFFFFFFF) << "-"
       << std::setw(4) << (dis(gen) & 0xFFFF);
    return ss.str();
}

// ============================================================================
// File I/O
// ============================================================================

std::string SymbolLinkerSubAgent::readFile(const std::string& path) const {
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

std::string SymbolLinkerSubAgent::normalizePath(const std::string& path) const {
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (result.size() > 1 && result.back() == '/') result.pop_back();
    std::error_code ec;
    auto canonical = fs::weakly_canonical(result, ec);
    if (!ec) {
        result = canonical.string();
        std::replace(result.begin(), result.end(), '\\', '/');
    }
    return result;
}

bool SymbolLinkerSubAgent::isLibrarySymbol(const std::string& name) const {
    if (m_config.librarySymbols.count(name)) return true;

    // Win32 API patterns
    if (name.size() > 1 && name[0] == '_' && name[1] == '_') return true;
    static const char* win32Prefixes[] = {
        "GetProcAddress", "LoadLibrary", "CreateFile", "CloseHandle",
        "VirtualAlloc", "VirtualFree", "HeapAlloc", "HeapFree",
        "GetLastError", "SetLastError", "GetModuleHandle",
        "ExitProcess", "GetStdHandle", "WriteFile", "ReadFile",
        "GetTickCount", "QueryPerformanceCounter", "Sleep",
        "CreateThread", "WaitForSingleObject", "InitializeCriticalSection",
        "__security_cookie", "__report_rangecheckfailure", "__GSHandlerCheck",
        "__C_specific_handler", "_CxxThrowException", "__CxxFrameHandler4",
        "memcpy", "memset", "memmove", "strcmp", "strlen", "strcpy",
        "printf", "fprintf", "sprintf", "malloc", "free", "calloc", "realloc",
        nullptr
    };
    for (int i = 0; win32Prefixes[i]; i++) {
        if (name == win32Prefixes[i]) return true;
    }
    return false;
}

// ============================================================================
// C/C++ Symbol Extraction
// ============================================================================

SymbolKind SymbolLinkerSubAgent::classifyLine(
    const std::string& line, const std::string& trimmed,
    bool& isDef, bool& isExternC) const
{
    isDef = false;
    isExternC = false;

    if (trimmed.empty() || trimmed[0] == '/' || trimmed[0] == '#' || trimmed[0] == '*') {
        return SymbolKind::Unknown;
    }

    // extern "C"
    if (trimmed.find("extern \"C\"") != std::string::npos) {
        isExternC = true;
    }

    // Class/struct definition
    if (std::regex_search(trimmed, std::regex(R"(^(class|struct)\s+(\w+)\s*[\{:;])"))) {
        isDef = (trimmed.find('{') != std::string::npos) ||
                (trimmed.find(';') != std::string::npos && trimmed.find('(') == std::string::npos);
        if (trimmed.find('{') != std::string::npos) {
            return (trimmed.substr(0, 5) == "class") ? SymbolKind::Class : SymbolKind::Struct;
        }
        return SymbolKind::Unknown; // Forward declaration
    }

    // Enum
    if (std::regex_search(trimmed, std::regex(R"(^enum\s+(class\s+)?(\w+))"))) {
        isDef = (trimmed.find('{') != std::string::npos);
        return SymbolKind::Enum;
    }

    // Typedef
    if (trimmed.substr(0, 7) == "typedef") {
        isDef = true;
        return SymbolKind::Typedef;
    }

    // Namespace
    if (std::regex_search(trimmed, std::regex(R"(^namespace\s+(\w+))"))) {
        return SymbolKind::Namespace;
    }

    // Function definition (has both parentheses and opening brace or is followed by {)
    if (trimmed.find('(') != std::string::npos) {
        bool hasBody = (trimmed.find('{') != std::string::npos);
        bool isDecl = (trimmed.back() == ';');
        bool isStatic = (trimmed.find("static ") != std::string::npos ||
                         trimmed.find("static\t") != std::string::npos);
        bool isVirtual = (trimmed.find("virtual ") != std::string::npos);

        if (trimmed.find("operator") != std::string::npos) {
            isDef = hasBody;
            return SymbolKind::OperatorOverload;
        }

        // Constructor/destructor patterns
        std::smatch ctorMatch;
        if (std::regex_search(trimmed, ctorMatch, std::regex(R"((\w+)\s*::\s*~?\1\s*\()"))) {
            isDef = hasBody;
            return (trimmed.find('~') != std::string::npos) ?
                   SymbolKind::Destructor : SymbolKind::Constructor;
        }

        if (hasBody || !isDecl) {
            isDef = hasBody;
            if (isStatic) return SymbolKind::StaticFunc;
            if (isVirtual) return SymbolKind::VirtualFunc;
            return SymbolKind::Function;
        } else {
            isDef = false;
            return SymbolKind::Function; // Declaration
        }
    }

    // Variable definition/declaration
    if (trimmed.find('=') != std::string::npos && trimmed.back() == ';') {
        isDef = true;
        bool isStatic = (trimmed.find("static ") != std::string::npos);
        return isStatic ? SymbolKind::StaticVar : SymbolKind::Variable;
    }

    // extern declaration
    if (trimmed.find("extern ") == 0) {
        isDef = false;
        return SymbolKind::Variable;
    }

    return SymbolKind::Unknown;
}

void SymbolLinkerSubAgent::parseFunctionSignature(
    const std::string& line, SymbolEntry& entry) const
{
    // Extract: [return_type] name(params)
    std::regex funcRegex(R"((\w[\w\s\*&:<>,]*?)\s+(\w[\w:]*)\s*\(([^)]*)\))");
    std::smatch match;
    if (std::regex_search(line, match, funcRegex)) {
        entry.returnType = match[1].str();
        entry.name = match[2].str();
        entry.signature = match[0].str();

        // Parse param types
        std::string params = match[3].str();
        if (!params.empty() && params != "void") {
            std::istringstream pss(params);
            std::string param;
            while (std::getline(pss, param, ',')) {
                // Trim
                size_t start = param.find_first_not_of(" \t");
                size_t end = param.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    param = param.substr(start, end - start + 1);
                }
                // Extract type (everything before last word)
                size_t lastSpace = param.rfind(' ');
                if (lastSpace != std::string::npos) {
                    entry.paramTypes.push_back(param.substr(0, lastSpace));
                } else {
                    entry.paramTypes.push_back(param);
                }
            }
        }
    }
}

std::vector<SymbolEntry> SymbolLinkerSubAgent::extractSymbolsCpp(
    const std::string& filePath) const
{
    std::vector<SymbolEntry> symbols;
    std::string content = readFile(filePath);
    if (content.empty()) return symbols;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    std::string currentNamespace;
    std::string currentClass;
    bool inExternC = false;
    int braceDepth = 0;
    int externCDepth = -1;

    while (std::getline(stream, line)) {
        lineNum++;

        // Track brace depth
        for (char c : line) {
            if (c == '{') braceDepth++;
            if (c == '}') {
                braceDepth--;
                if (braceDepth == externCDepth) {
                    inExternC = false;
                    externCDepth = -1;
                }
            }
        }

        // Trim
        std::string trimmed = line;
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon != std::string::npos) trimmed = trimmed.substr(firstNon);

        bool isDef = false;
        bool isExtC = false;
        SymbolKind kind = classifyLine(line, trimmed, isDef, isExtC);
        if (kind == SymbolKind::Unknown) continue;

        if (isExtC) {
            inExternC = true;
            externCDepth = braceDepth;
        }

        SymbolEntry entry;
        entry.kind = kind;
        entry.filePath = filePath;
        entry.line = lineNum;
        entry.isDefined = isDef;
        entry.isExternC = inExternC || isExtC;

        // Extract name
        switch (kind) {
            case SymbolKind::Class:
            case SymbolKind::Struct: {
                std::regex re(R"((class|struct)\s+(?:__declspec\(\w+\)\s+)?(\w+))");
                std::smatch m;
                if (std::regex_search(trimmed, m, re)) {
                    entry.name = m[2].str();
                    currentClass = entry.name;
                }
                break;
            }
            case SymbolKind::Enum: {
                std::regex re(R"(enum\s+(?:class\s+)?(\w+))");
                std::smatch m;
                if (std::regex_search(trimmed, m, re)) entry.name = m[1].str();
                break;
            }
            case SymbolKind::Typedef: {
                // typedef ... name;
                std::regex re(R"(typedef\s+.+\s+(\w+)\s*;)");
                std::smatch m;
                if (std::regex_search(trimmed, m, re)) entry.name = m[1].str();
                break;
            }
            case SymbolKind::Namespace: {
                std::regex re(R"(namespace\s+(\w+))");
                std::smatch m;
                if (std::regex_search(trimmed, m, re)) {
                    entry.name = m[1].str();
                    currentNamespace = entry.name;
                }
                break;
            }
            case SymbolKind::Function:
            case SymbolKind::StaticFunc:
            case SymbolKind::VirtualFunc:
            case SymbolKind::OperatorOverload:
            case SymbolKind::Constructor:
            case SymbolKind::Destructor: {
                parseFunctionSignature(trimmed, entry);
                break;
            }
            case SymbolKind::Variable:
            case SymbolKind::StaticVar: {
                // Extract variable name from declaration
                std::regex re(R"((?:extern\s+|static\s+)?(?:const\s+)?[\w\s\*&:<>]+\s+(\w+)\s*[=;])");
                std::smatch m;
                if (std::regex_search(trimmed, m, re)) entry.name = m[1].str();
                break;
            }
            default:
                break;
        }

        if (entry.name.empty()) continue;

        // Build qualified name
        if (!currentNamespace.empty()) {
            entry.parentScope = currentNamespace;
            entry.qualifiedName = currentNamespace + "::" + entry.name;
        } else {
            entry.qualifiedName = entry.name;
        }

        // Linkage
        if (entry.isExternC) {
            entry.linkage = SymbolLinkage::External;
        } else if (kind == SymbolKind::StaticFunc || kind == SymbolKind::StaticVar) {
            entry.linkage = SymbolLinkage::Internal;
        } else if (trimmed.find("__declspec(dllexport)") != std::string::npos) {
            entry.linkage = SymbolLinkage::DllExport;
        } else if (trimmed.find("__declspec(dllimport)") != std::string::npos) {
            entry.linkage = SymbolLinkage::DllImport;
        } else if (trimmed.find("inline ") != std::string::npos) {
            entry.linkage = SymbolLinkage::Inline;
        } else {
            entry.linkage = SymbolLinkage::External;
        }

        entry.computeHash();
        symbols.push_back(entry);
    }

    return symbols;
}

// ============================================================================
// MASM Symbol Extraction
// ============================================================================

std::vector<SymbolEntry> SymbolLinkerSubAgent::extractSymbolsMasm(
    const std::string& filePath) const
{
    std::vector<SymbolEntry> symbols;
    std::string content = readFile(filePath);
    if (content.empty()) return symbols;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;

        std::string trimmed = line;
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        trimmed = trimmed.substr(firstNon);

        // Skip comments
        if (trimmed[0] == ';') continue;

        // PROC definitions: name PROC [frame]
        std::regex procRegex(R"(^(\w+)\s+PROC\b)", std::regex::icase);
        std::smatch procMatch;
        if (std::regex_search(trimmed, procMatch, procRegex)) {
            SymbolEntry entry;
            entry.name = procMatch[1].str();
            entry.kind = SymbolKind::MasmProc;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = true;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::External;
            entry.computeHash();
            symbols.push_back(entry);
            continue;
        }

        // PUBLIC declarations: PUBLIC name
        std::regex pubRegex(R"(^\s*PUBLIC\s+(\w+))", std::regex::icase);
        std::smatch pubMatch;
        if (std::regex_search(trimmed, pubMatch, pubRegex)) {
            SymbolEntry entry;
            entry.name = pubMatch[1].str();
            entry.kind = SymbolKind::MasmProc;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = false;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::AsmPublic;
            entry.computeHash();
            symbols.push_back(entry);
            continue;
        }

        // EXTERNDEF / EXTRN declarations: EXTERNDEF name:PROC
        std::regex extRegex(R"(^\s*(?:EXTERNDEF|EXTRN)\s+(\w+)\s*:\s*(\w+))", std::regex::icase);
        std::smatch extMatch;
        if (std::regex_search(trimmed, extMatch, extRegex)) {
            SymbolEntry entry;
            entry.name = extMatch[1].str();
            std::string type = extMatch[2].str();
            std::transform(type.begin(), type.end(), type.begin(), ::toupper);
            entry.kind = (type == "PROC") ? SymbolKind::MasmProc : SymbolKind::MasmExtern;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = false;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::AsmExtrn;
            entry.computeHash();
            symbols.push_back(entry);
            continue;
        }

        // Labels: name: or name::
        std::regex labelRegex(R"(^(\w+)\s*::?\s*$)");
        std::smatch labelMatch;
        if (std::regex_search(trimmed, labelMatch, labelRegex)) {
            SymbolEntry entry;
            entry.name = labelMatch[1].str();
            entry.kind = SymbolKind::Label;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = true;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::Internal;
            entry.computeHash();
            symbols.push_back(entry);
            continue;
        }

        // Data definitions: name TYPE value
        std::regex dataRegex(R"(^(\w+)\s+(DB|DW|DD|DQ|BYTE|WORD|DWORD|QWORD)\b)", std::regex::icase);
        std::smatch dataMatch;
        if (std::regex_search(trimmed, dataMatch, dataRegex)) {
            SymbolEntry entry;
            entry.name = dataMatch[1].str();
            entry.kind = SymbolKind::Variable;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = true;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::External;
            entry.computeHash();
            symbols.push_back(entry);
            continue;
        }

        // CALL references: call name
        std::regex callRegex(R"(\bcall\s+(\w+))", std::regex::icase);
        std::smatch callMatch;
        std::string searchStr = trimmed;
        while (std::regex_search(searchStr, callMatch, callRegex)) {
            SymbolEntry entry;
            entry.name = callMatch[1].str();
            entry.kind = SymbolKind::MasmProc;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = false;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::AsmExtrn;
            entry.computeHash();
            symbols.push_back(entry);
            searchStr = callMatch.suffix();
        }
    }

    return symbols;
}

// ============================================================================
// .def File Parsing
// ============================================================================

std::vector<SymbolEntry> SymbolLinkerSubAgent::extractSymbolsDef(
    const std::string& filePath) const
{
    std::vector<SymbolEntry> symbols;
    std::string content = readFile(filePath);
    if (content.empty()) return symbols;

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    bool inExports = false;

    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmed = line;
        size_t firstNon = trimmed.find_first_not_of(" \t");
        if (firstNon == std::string::npos) continue;
        trimmed = trimmed.substr(firstNon);

        // EXPORTS section
        if (trimmed == "EXPORTS" || trimmed == "exports") {
            inExports = true;
            continue;
        }
        if (trimmed == "LIBRARY" || trimmed == "library" ||
            trimmed == "NAME" || trimmed == "name") {
            inExports = false;
            continue;
        }

        if (!inExports) continue;
        if (trimmed[0] == ';') continue;

        // Parse export: name [=internalname] [@ordinal] [NONAME] [DATA] [PRIVATE]
        std::regex exportRegex(R"(^\s*(\w+))");
        std::smatch match;
        if (std::regex_search(trimmed, match, exportRegex)) {
            SymbolEntry entry;
            entry.name = match[1].str();
            entry.kind = SymbolKind::Function;
            entry.filePath = filePath;
            entry.line = lineNum;
            entry.isDefined = true;
            entry.isExternC = true;
            entry.linkage = SymbolLinkage::DllExport;

            if (trimmed.find("DATA") != std::string::npos) {
                entry.kind = SymbolKind::Variable;
            }

            entry.computeHash();
            symbols.push_back(entry);
        }
    }

    return symbols;
}

// ============================================================================
// Unified extraction
// ============================================================================

std::vector<SymbolEntry> SymbolLinkerSubAgent::extractSymbols(
    const std::string& filePath) const
{
    std::string ext = fs::path(filePath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".asm" || ext == ".inc") {
        return extractSymbolsMasm(filePath);
    } else if (ext == ".def") {
        return extractSymbolsDef(filePath);
    } else {
        return extractSymbolsCpp(filePath);
    }
}

// ============================================================================
// Name Demangling (simplified — MSVC undname)
// ============================================================================

std::string SymbolLinkerSubAgent::demangle(const std::string& mangledName) const {
    if (!isMangled(mangledName)) return mangledName;

#ifdef _WIN32
    // Use UnDecorateSymbolName from DbgHelp
    // For now, basic MSVC mangling patterns
    std::string name = mangledName;

    // MSVC mangling starts with ?
    if (name[0] == '?') {
        // Extract basic name: ?name@namespace@@...
        size_t at = name.find('@', 1);
        if (at != std::string::npos) {
            return name.substr(1, at - 1);
        }
    }
    // GCC/clang mangling starts with _Z
    if (name.size() > 2 && name[0] == '_' && name[1] == 'Z') {
        // _ZN<len>name<len>name...E
        size_t pos = 2;
        if (pos < name.size() && name[pos] == 'N') {
            pos++;
            std::string result;
            while (pos < name.size() && name[pos] != 'E') {
                int len = 0;
                while (pos < name.size() && std::isdigit(name[pos])) {
                    len = len * 10 + (name[pos] - '0');
                    pos++;
                }
                if (len > 0 && pos + len <= name.size()) {
                    if (!result.empty()) result += "::";
                    result += name.substr(pos, len);
                    pos += len;
                } else {
                    break;
                }
            }
            if (!result.empty()) return result;
        }
    }
#endif
    return mangledName;
}

bool SymbolLinkerSubAgent::isMangled(const std::string& name) const {
    if (name.empty()) return false;
    // MSVC: starts with ?
    if (name[0] == '?') return true;
    // GCC/clang: starts with _Z
    if (name.size() > 2 && name[0] == '_' && name[1] == 'Z') return true;
    return false;
}

// ============================================================================
// Symbol Table Building
// ============================================================================

void SymbolLinkerSubAgent::buildSymbolTable(
    const std::vector<std::string>& filePaths)
{
    std::lock_guard<std::mutex> lock(m_symMutex);
    m_symtab = SymbolTable{};

    for (const auto& path : filePaths) {
        if (m_cancelled.load()) break;

        auto symbols = extractSymbols(path);
        for (auto& sym : symbols) {
            if (sym.isDefined) {
                m_symtab.definitions[sym.name].push_back(sym);
            } else {
                m_symtab.references[sym.name].push_back(sym);
            }
        }

        m_stats.totalSymbolsExtracted += (int64_t)symbols.size();
    }
}

void SymbolLinkerSubAgent::resolveReferences() {
    std::lock_guard<std::mutex> lock(m_symMutex);

    m_symtab.resolvedRefs.clear();
    m_symtab.unresolvedRefs.clear();

    for (const auto& [name, refs] : m_symtab.references) {
        // Skip library/system symbols
        if (isLibrarySymbol(name)) {
            m_symtab.resolvedRefs.insert(name);
            continue;
        }

        // Try exact match
        if (m_symtab.definitions.count(name)) {
            m_symtab.resolvedRefs.insert(name);
            m_stats.totalRefsResolved++;
            continue;
        }

        // Try demangled match
        if (m_config.resolveMangled) {
            std::string demangled = demangle(name);
            if (demangled != name && m_symtab.definitions.count(demangled)) {
                m_symtab.resolvedRefs.insert(name);
                m_stats.totalRefsResolved++;
                continue;
            }

            // Try matching against demangled definitions
            bool found = false;
            for (const auto& [defName, defs] : m_symtab.definitions) {
                std::string defDemangled = demangle(defName);
                if (defDemangled == demangled || defDemangled == name) {
                    m_symtab.resolvedRefs.insert(name);
                    m_stats.totalRefsResolved++;
                    found = true;
                    break;
                }
            }
            if (found) continue;
        }

        m_symtab.unresolvedRefs.insert(name);
    }
}

std::vector<SymbolEntry> SymbolLinkerSubAgent::getUnresolved() const {
    std::lock_guard<std::mutex> lock(m_symMutex);
    std::vector<SymbolEntry> result;
    for (const auto& name : m_symtab.unresolvedRefs) {
        auto it = m_symtab.references.find(name);
        if (it != m_symtab.references.end() && !it->second.empty()) {
            result.push_back(it->second.front());
        }
    }
    return result;
}

std::vector<SymbolEntry> SymbolLinkerSubAgent::getDefined() const {
    std::lock_guard<std::mutex> lock(m_symMutex);
    std::vector<SymbolEntry> result;
    for (const auto& [name, defs] : m_symtab.definitions) {
        for (const auto& d : defs) result.push_back(d);
    }
    return result;
}

std::vector<SymbolEntry> SymbolLinkerSubAgent::findDefinitions(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_symMutex);
    auto it = m_symtab.definitions.find(name);
    if (it != m_symtab.definitions.end()) return it->second;
    return {};
}

std::vector<SymbolEntry> SymbolLinkerSubAgent::findReferences(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_symMutex);
    auto it = m_symtab.references.find(name);
    if (it != m_symtab.references.end()) return it->second;
    return {};
}

// ============================================================================
// Conflict Detection
// ============================================================================

std::vector<SymbolConflict> SymbolLinkerSubAgent::detectConflicts() const {
    std::lock_guard<std::mutex> lock(m_symMutex);
    std::vector<SymbolConflict> conflicts;

    for (const auto& [name, defs] : m_symtab.definitions) {
        if (defs.size() <= 1) continue;

        // Filter out forward declarations and inline functions
        std::vector<SymbolEntry> realDefs;
        for (const auto& d : defs) {
            if (!d.isForwardDecl && d.linkage != SymbolLinkage::Inline) {
                realDefs.push_back(d);
            }
        }

        if (realDefs.size() <= 1) continue;

        // Check for genuine duplicate definitions
        SymbolConflict conflict;
        conflict.symbolName = name;
        conflict.conflicting = realDefs;
        conflict.severity = 2;

        // Check if same kind
        bool allSameKind = true;
        for (size_t i = 1; i < realDefs.size(); i++) {
            if (realDefs[i].kind != realDefs[0].kind) {
                allSameKind = false;
                break;
            }
        }

        if (!allSameKind) {
            conflict.type = SymbolConflict::Type::TypeMismatch;
            conflict.description = "Symbol '" + name + "' defined as different types across TUs";
        } else {
            conflict.type = SymbolConflict::Type::DuplicateDefinition;
            conflict.description = "Symbol '" + name + "' defined in " +
                                   std::to_string(realDefs.size()) + " translation units";
        }

        conflicts.push_back(conflict);
    }

    return conflicts;
}

// ============================================================================
// Bulk Scan
// ============================================================================

SymbolLinkerResult SymbolLinkerSubAgent::scan(
    const std::string& parentId,
    const std::vector<std::string>& filePaths)
{
    auto startTime = std::chrono::steady_clock::now();
    std::string scanId = generateId();
    m_running.store(true);
    m_cancelled.store(false);

    int totalFiles = std::min((int)filePaths.size(), m_config.maxFilesPerScan);

    // Build symbol table
    std::vector<std::string> truncated(filePaths.begin(),
                                        filePaths.begin() + totalFiles);
    buildSymbolTable(truncated);

    for (int i = 0; i < totalFiles; i++) {
        if (m_onProgress) m_onProgress(scanId, i + 1, totalFiles);
    }

    // Resolve references
    resolveReferences();

    // Detect conflicts
    auto conflicts = detectConflicts();

    auto elapsed = std::chrono::steady_clock::now() - startTime;

    SymbolLinkerResult result = SymbolLinkerResult::ok(scanId);
    result.filesScanned = totalFiles;

    {
        std::lock_guard<std::mutex> lock(m_symMutex);
        int defCount = 0;
        for (const auto& [_, defs] : m_symtab.definitions) defCount += (int)defs.size();
        int refCount = 0;
        for (const auto& [_, refs] : m_symtab.references) refCount += (int)refs.size();

        result.symbolsDefined = defCount;
        result.symbolsReferenced = refCount;
        result.symbolsResolved = (int)m_symtab.resolvedRefs.size();
        result.symbolsUnresolved = (int)m_symtab.unresolvedRefs.size();
        for (const auto& name : m_symtab.unresolvedRefs) {
            result.unresolvedNames.push_back(name);
        }
    }

    result.conflictsFound = (int)conflicts.size();
    result.conflicts = conflicts;
    result.elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    m_stats.totalScans++;
    m_stats.totalFilesProcessed += totalFiles;
    m_stats.totalConflictsFound += (int)conflicts.size();

    m_running.store(false);
    if (m_onComplete) m_onComplete(result);
    return result;
}

// ============================================================================
// Scan + Auto-Fix
// ============================================================================

SymbolLinkerResult SymbolLinkerSubAgent::scanAndFix(
    const std::string& parentId,
    const std::vector<std::string>& filePaths)
{
    auto result = scan(parentId, filePaths);
    if (!result.success) return result;

    auto fixes = generateFixes();
    int applied = applyFixes(fixes);

    result.fixesApplied = applied;
    result.fixesFailed = (int)fixes.size() - applied;

    // Count stubs
    for (const auto& fix : fixes) {
        if (fix.type == SymbolFixAction::Type::GenerateStub) {
            result.stubsGenerated++;
        }
    }

    m_stats.totalFixesApplied += applied;

    if (m_onComplete) m_onComplete(result);
    return result;
}

// ============================================================================
// Async
// ============================================================================

std::string SymbolLinkerSubAgent::scanAndFixAsync(
    const std::string& parentId,
    const std::vector<std::string>& filePaths,
    SymbolLinkerCompleteCb onComplete)
{
    std::string scanId = generateId();

    if (m_manager) {
        std::ostringstream prompt;
        prompt << "Resolve symbols across " << filePaths.size()
               << " translation units in project: " << m_config.projectRoot;
        m_manager->spawnSubAgent(parentId, "SymbolLinker:" + scanId, prompt.str());
    }

    std::thread([this, parentId, filePaths, onComplete, scanId]() {
        auto result = scanAndFix(parentId, filePaths);
        result.scanId = scanId;
        if (onComplete) onComplete(result);
    }).detach();

    return scanId;
}

// ============================================================================
// Fix Generation
// ============================================================================

std::vector<SymbolFixAction> SymbolLinkerSubAgent::generateFixes() const {
    std::lock_guard<std::mutex> lock(m_symMutex);
    std::vector<SymbolFixAction> fixes;

    for (const auto& name : m_symtab.unresolvedRefs) {
        if (isLibrarySymbol(name)) continue;

        auto refIt = m_symtab.references.find(name);
        if (refIt == m_symtab.references.end() || refIt->second.empty()) continue;

        const auto& ref = refIt->second.front();

        // Generate appropriate fix based on symbol kind
        if (m_config.generateStubs) {
            SymbolFixAction fix;
            fix.type = SymbolFixAction::Type::GenerateStub;
            fix.symbolName = name;
            fix.newText = generateStub(ref);
            fix.reason = "Generate stub for unresolved " +
                         std::string(symbolKindStr(ref.kind)) + ": " + name;
            fix.priority = 70;

            // Determine target file for the stub
            // If it's a function referenced from a .cpp, create stubs in a _stubs.cpp
            std::string refDir = fs::path(ref.filePath).parent_path().string();
            fix.filePath = refDir + "/generated_stubs.cpp";

            fixes.push_back(fix);
        }

        if (m_config.generateExternDecls) {
            // If the reference is from ASM, generate EXTERNDEF
            if (ref.kind == SymbolKind::MasmProc || ref.kind == SymbolKind::MasmExtern) {
                if (m_config.supportMasm) {
                    SymbolFixAction fix;
                    fix.type = SymbolFixAction::Type::AddMasmExterndef;
                    fix.symbolName = name;
                    fix.filePath = ref.filePath;
                    fix.newText = generateMasmExterndef(ref);
                    fix.reason = "Add EXTERNDEF for unresolved MASM reference: " + name;
                    fix.priority = 60;
                    fixes.push_back(fix);
                }
            } else {
                SymbolFixAction fix;
                fix.type = SymbolFixAction::Type::AddExternDecl;
                fix.symbolName = name;
                fix.filePath = ref.filePath;
                fix.newText = generateExternDecl(ref);
                fix.reason = "Add extern declaration for unresolved reference: " + name;
                fix.priority = 50;
                fixes.push_back(fix);
            }
        }
    }

    std::sort(fixes.begin(), fixes.end());
    return fixes;
}

std::string SymbolLinkerSubAgent::generateStub(const SymbolEntry& symbol) const {
    std::ostringstream oss;

    switch (symbol.kind) {
        case SymbolKind::Function:
        case SymbolKind::MasmProc:
        case SymbolKind::ExternC: {
            if (symbol.isExternC) oss << "extern \"C\" ";

            // Return type
            std::string retType = symbol.returnType.empty() ? "void" : symbol.returnType;
            oss << retType << " ";

            // Calling convention for extern "C"
            if (symbol.isExternC) {
                oss << "__cdecl ";
            }

            oss << symbol.name << "(";

            // Parameters
            if (!symbol.paramTypes.empty()) {
                for (size_t i = 0; i < symbol.paramTypes.size(); i++) {
                    if (i > 0) oss << ", ";
                    oss << symbol.paramTypes[i] << " p" << i;
                }
            } else {
                oss << "void";
            }

            oss << ") {\n";
            oss << "    // STUB: auto-generated by SymbolLinkerSubAgent\n";
            if (retType != "void") {
                if (retType.find('*') != std::string::npos) {
                    oss << "    return nullptr;\n";
                } else if (retType == "bool") {
                    oss << "    return false;\n";
                } else {
                    oss << "    return (" << retType << ")0;\n";
                }
            }
            oss << "}\n";
            break;
        }

        case SymbolKind::Variable:
        case SymbolKind::StaticVar: {
            if (symbol.isExternC) oss << "extern \"C\" ";
            std::string type = symbol.returnType.empty() ? "int" : symbol.returnType;
            oss << type << " " << symbol.name << " = 0; // STUB\n";
            break;
        }

        default:
            oss << "// STUB: unresolved symbol '" << symbol.name
                << "' (kind: " << symbolKindStr(symbol.kind) << ")\n";
            break;
    }

    return oss.str();
}

std::string SymbolLinkerSubAgent::generateExternDecl(const SymbolEntry& symbol) const {
    std::ostringstream oss;

    if (symbol.isExternC || symbol.kind == SymbolKind::MasmProc) {
        oss << "extern \"C\" ";
    } else {
        oss << "extern ";
    }

    switch (symbol.kind) {
        case SymbolKind::Function:
        case SymbolKind::MasmProc:
        case SymbolKind::ExternC: {
            std::string retType = symbol.returnType.empty() ? "void" : symbol.returnType;
            oss << retType << " " << symbol.name << "(";
            if (!symbol.paramTypes.empty()) {
                for (size_t i = 0; i < symbol.paramTypes.size(); i++) {
                    if (i > 0) oss << ", ";
                    oss << symbol.paramTypes[i];
                }
            }
            oss << ");";
            break;
        }
        case SymbolKind::Variable: {
            std::string type = symbol.returnType.empty() ? "int" : symbol.returnType;
            oss << type << " " << symbol.name << ";";
            break;
        }
        default:
            oss << "// extern " << symbol.name << "; // kind: " << symbolKindStr(symbol.kind);
            break;
    }

    return oss.str();
}

std::string SymbolLinkerSubAgent::generateMasmExterndef(const SymbolEntry& symbol) const {
    std::string type = "PROC";
    if (symbol.kind == SymbolKind::Variable || symbol.kind == SymbolKind::MasmExtern) {
        type = "QWORD";
    }
    return "EXTERNDEF " + symbol.name + ":" + type;
}

std::string SymbolLinkerSubAgent::generateDefEntries(
    const std::vector<SymbolEntry>& symbols) const
{
    std::ostringstream oss;
    oss << "EXPORTS\n";
    for (const auto& sym : symbols) {
        if (sym.linkage == SymbolLinkage::Internal) continue;
        oss << "    " << sym.name;
        if (sym.kind == SymbolKind::Variable) oss << " DATA";
        oss << "\n";
    }
    return oss.str();
}

// ============================================================================
// Apply Fixes
// ============================================================================

int SymbolLinkerSubAgent::applyFixes(std::vector<SymbolFixAction>& fixes) {
    int applied = 0;

    for (auto& fix : fixes) {
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

bool SymbolLinkerSubAgent::applySingleFix(const SymbolFixAction& fix) {
    switch (fix.type) {
        case SymbolFixAction::Type::GenerateStub: {
            // Append to stubs file
            std::ofstream ofs(fix.filePath, std::ios::app);
            if (!ofs.is_open()) return false;
            ofs << "\n" << fix.newText << "\n";
            m_stats.totalStubsGenerated++;
            return true;
        }

        case SymbolFixAction::Type::AddExternDecl:
        case SymbolFixAction::Type::AddMasmExterndef:
        case SymbolFixAction::Type::AddExternC:
        case SymbolFixAction::Type::AddForwardDecl: {
            // Read existing content
            std::string content = readFile(fix.filePath);
            // Check if already present
            if (content.find(fix.newText) != std::string::npos) return true;

            // Find insertion point (after last extern/include)
            std::istringstream stream(content);
            std::string line;
            int lineNum = 0;
            int insertAfter = 0;
            while (std::getline(stream, line)) {
                lineNum++;
                std::string trimmed = line;
                size_t firstNon = trimmed.find_first_not_of(" \t");
                if (firstNon != std::string::npos) trimmed = trimmed.substr(firstNon);

                if (trimmed.find("#include") == 0 ||
                    trimmed.find("extern") == 0 ||
                    trimmed.find("EXTERNDEF") == 0 ||
                    trimmed.find("EXTRN") == 0) {
                    insertAfter = lineNum;
                }
            }

            // Insert
            std::istringstream in2(content);
            std::ostringstream out;
            lineNum = 0;
            bool inserted = false;
            while (std::getline(in2, line)) {
                lineNum++;
                out << line << "\n";
                if (lineNum == insertAfter && !inserted) {
                    out << fix.newText << "\n";
                    inserted = true;
                }
            }
            if (!inserted) {
                out << fix.newText << "\n";
            }

            std::ofstream ofs(fix.filePath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open()) return false;
            ofs << out.str();
            return true;
        }

        default:
            return false;
    }
}

// ============================================================================
// Self-Healing
// ============================================================================

std::string SymbolLinkerSubAgent::selfHealResolution(const SymbolEntry& unresolved) {
    if (!m_engine || !m_manager) return "";

    std::ostringstream prompt;
    prompt << "Unresolved symbol: " << unresolved.name << "\n"
           << "Kind: " << symbolKindStr(unresolved.kind) << "\n"
           << "Referenced from: " << unresolved.filePath << ":" << unresolved.line << "\n";
    if (!unresolved.signature.empty()) {
        prompt << "Signature: " << unresolved.signature << "\n";
    }
    prompt << "Generate a minimal correct implementation.";

    std::string agentId = m_manager->spawnSubAgent(
        "symbol-linker", "resolve:" + unresolved.name, prompt.str());
    if (m_manager->waitForSubAgent(agentId, 20000)) {
        return m_manager->getSubAgentResult(agentId);
    }
    return "";
}

// ============================================================================
// Cancel / Stats
// ============================================================================

void SymbolLinkerSubAgent::cancel() {
    m_cancelled.store(true);
}

SymbolLinkerSubAgent::Stats SymbolLinkerSubAgent::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_stats;
}

void SymbolLinkerSubAgent::resetStats() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats = {};
}
