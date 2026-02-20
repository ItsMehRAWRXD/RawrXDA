// ============================================================================
// language_plugin.cpp — Pluginable Language Support Implementation
// ============================================================================
// Built-in language descriptors + DLL plugin loader + lexer factory.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "ide/language_plugin.h"
#include "RawrXD_Lexer.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Language {

// ============================================================================
// PluginLexer — Wraps C-ABI lexer as native Lexer
// ============================================================================
PluginLexer::PluginLexer(CLexerHandle handle,
                          LanguagePlugin_Lex_fn lexFn,
                          LanguagePlugin_LexStateful_fn lexStatefulFn,
                          LanguagePlugin_DestroyLexer_fn destroyFn,
                          const std::unordered_map<int32_t, TokenType>& tokenMap)
    : m_handle(handle)
    , m_lexFn(lexFn)
    , m_lexStatefulFn(lexStatefulFn)
    , m_destroyFn(destroyFn)
    , m_tokenMap(tokenMap)
{
}

PluginLexer::~PluginLexer() {
    if (m_handle && m_destroyFn) {
        m_destroyFn(m_handle);
    }
}

void PluginLexer::lex(const std::wstring& text, std::vector<Token>& outTokens) {
    if (!m_handle || !m_lexFn) return;

    static constexpr int MAX_TOKENS = 4096;
    CLexToken cTokens[MAX_TOKENS];
    int count = m_lexFn(m_handle, text.c_str(), static_cast<int>(text.size()),
                         cTokens, MAX_TOKENS);
    
    outTokens.reserve(outTokens.size() + count);
    for (int i = 0; i < count; ++i) {
        Token tok;
        auto mapIt = m_tokenMap.find(cTokens[i].type);
        tok.type = (mapIt != m_tokenMap.end()) ? mapIt->second : TokenType::Default;
        tok.start = cTokens[i].start;
        tok.length = cTokens[i].length;
        outTokens.push_back(tok);
    }
}

int PluginLexer::lexStateful(const std::wstring& text, int startState,
                              std::vector<Token>& outTokens) {
    if (!m_handle || !m_lexStatefulFn) {
        lex(text, outTokens);
        return 0;
    }

    static constexpr int MAX_TOKENS = 4096;
    CLexToken cTokens[MAX_TOKENS];
    int resultState = m_lexStatefulFn(m_handle, text.c_str(),
                                       static_cast<int>(text.size()),
                                       startState, cTokens, MAX_TOKENS);
    
    // Find actual count — the fn returns new state, tokens stored in buffer
    // Convention: returnVal = (newState << 16) | tokenCount
    int tokenCount = resultState & 0xFFFF;
    int newState = (resultState >> 16) & 0xFFFF;
    
    outTokens.reserve(outTokens.size() + tokenCount);
    for (int i = 0; i < tokenCount; ++i) {
        Token tok;
        auto mapIt = m_tokenMap.find(cTokens[i].type);
        tok.type = (mapIt != m_tokenMap.end()) ? mapIt->second : TokenType::Default;
        tok.start = cTokens[i].start;
        tok.length = cTokens[i].length;
        outTokens.push_back(tok);
    }
    
    return newState;
}

// ============================================================================
// Singleton
// ============================================================================
LanguageRegistry& LanguageRegistry::Instance() {
    static LanguageRegistry instance;
    return instance;
}

LanguageRegistry::~LanguageRegistry() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
void LanguageRegistry::Initialize() {
    if (m_initialized.load()) return;
    registerBuiltins();
    m_initialized.store(true);
}

void LanguageRegistry::Shutdown() {
    if (!m_initialized.load()) return;
    UnloadAllPlugins();
    m_initialized.store(false);
}

// ============================================================================
// Registration
// ============================================================================
void LanguageRegistry::RegisterProvider(std::unique_ptr<ILanguageProvider> provider) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& desc = provider->GetDescriptor();
    std::string id = desc.id;
    m_languages[id] = desc;
    
    // Register extension mappings
    for (const auto& ext : desc.fileExtensions) {
        m_extMap[ext] = id;
    }
    for (const auto& fn : desc.fileNames) {
        m_filenameMap[fn] = id;
    }
    
    m_providers[id] = std::move(provider);
}

void LanguageRegistry::RegisterDescriptor(const LanguageDescriptor& desc) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_languages[desc.id] = desc;
    
    for (const auto& ext : desc.fileExtensions) {
        m_extMap[ext] = desc.id;
    }
    for (const auto& fn : desc.fileNames) {
        m_filenameMap[fn] = desc.id;
    }
}

void LanguageRegistry::UnregisterLanguage(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_languages.erase(id);
    m_providers.erase(id);
    
    // Clean up extension maps
    for (auto it = m_extMap.begin(); it != m_extMap.end(); ) {
        if (it->second == id) it = m_extMap.erase(it);
        else ++it;
    }
    for (auto it = m_filenameMap.begin(); it != m_filenameMap.end(); ) {
        if (it->second == id) it = m_filenameMap.erase(it);
        else ++it;
    }
}

// ============================================================================
// Plugin Loading
// ============================================================================
bool LanguageRegistry::LoadPlugin(const std::string& dllPath) {
#ifdef _WIN32
    HMODULE hMod = LoadLibraryA(dllPath.c_str());
    if (!hMod) return false;
    
    LoadedPlugin plugin;
    plugin.path = dllPath;
    plugin.hModule = hMod;
    
    plugin.fnGetInfo = reinterpret_cast<LanguagePlugin_GetInfo_fn>(
        GetProcAddress(hMod, "LanguagePlugin_GetInfo"));
    plugin.fnGetLanguages = reinterpret_cast<LanguagePlugin_GetLanguages_fn>(
        GetProcAddress(hMod, "LanguagePlugin_GetLanguages"));
    plugin.fnCreateLexer = reinterpret_cast<LanguagePlugin_CreateLexer_fn>(
        GetProcAddress(hMod, "LanguagePlugin_CreateLexer"));
    plugin.fnLex = reinterpret_cast<LanguagePlugin_Lex_fn>(
        GetProcAddress(hMod, "LanguagePlugin_Lex"));
    plugin.fnLexStateful = reinterpret_cast<LanguagePlugin_LexStateful_fn>(
        GetProcAddress(hMod, "LanguagePlugin_LexStateful"));
    plugin.fnDestroyLexer = reinterpret_cast<LanguagePlugin_DestroyLexer_fn>(
        GetProcAddress(hMod, "LanguagePlugin_DestroyLexer"));
    plugin.fnFormat = reinterpret_cast<LanguagePlugin_Format_fn>(
        GetProcAddress(hMod, "LanguagePlugin_Format"));
    plugin.fnShutdown = reinterpret_cast<LanguagePlugin_Shutdown_fn>(
        GetProcAddress(hMod, "LanguagePlugin_Shutdown"));
    
    // Minimum required: GetInfo + GetLanguages + Lex
    if (!plugin.fnGetInfo || !plugin.fnGetLanguages || !plugin.fnLex) {
        FreeLibrary(hMod);
        return false;
    }
    
    auto* info = plugin.fnGetInfo();
    if (info) plugin.name = info->pluginName;
    
    // Load language descriptors from plugin
    CLanguageDescriptor cDescs[32];
    int count = plugin.fnGetLanguages(cDescs, 32);
    
    for (int i = 0; i < count; ++i) {
        LanguageDescriptor desc;
        desc.id = cDescs[i].id;
        desc.name = cDescs[i].name;
        desc.version = cDescs[i].version ? cDescs[i].version : "1.0";
        desc.isBuiltin = false;
        desc.pluginSource = dllPath;
        desc.supportedFeatures = static_cast<LanguageFeature>(cDescs[i].features);
        
        // Parse comma-separated lists
        auto split = [](const char* str, char delim) -> std::vector<std::string> {
            std::vector<std::string> out;
            if (!str) return out;
            std::istringstream ss(str);
            std::string item;
            while (std::getline(ss, item, delim)) {
                while (!item.empty() && item[0] == ' ') item.erase(0, 1);
                while (!item.empty() && item.back() == ' ') item.pop_back();
                if (!item.empty()) out.push_back(item);
            }
            return out;
        };
        
        desc.fileExtensions = split(cDescs[i].fileExtensions, ',');
        desc.fileNames = split(cDescs[i].fileNames, ',');
        
        // Comment style
        if (cDescs[i].lineComment) desc.comments.lineComment = cDescs[i].lineComment;
        if (cDescs[i].blockCommentStart) desc.comments.blockStart = cDescs[i].blockCommentStart;
        if (cDescs[i].blockCommentEnd) desc.comments.blockEnd = cDescs[i].blockCommentEnd;
        
        // Parse keyword groups (semicolon-separated groups, comma-separated words)
        if (cDescs[i].keywords) {
            std::istringstream kss(cDescs[i].keywords);
            std::string group;
            int groupIdx = 0;
            while (std::getline(kss, group, ';')) {
                KeywordSet ks;
                ks.category = "keywords_" + std::to_string(groupIdx++);
                ks.tokenType = TokenType::Keyword;
                ks.words = split(group.c_str(), ',');
                if (!ks.words.empty()) desc.keywords.push_back(ks);
            }
        }
        
        RegisterDescriptor(desc);
    }
    
    m_plugins.push_back(std::move(plugin));
    return true;
#else
    (void)dllPath;
    return false;
#endif
}

void LanguageRegistry::UnloadPlugin(const std::string& name) {
    auto it = std::find_if(m_plugins.begin(), m_plugins.end(),
                            [&](const LoadedPlugin& p) { return p.name == name; });
    if (it == m_plugins.end()) return;
    
    if (it->fnShutdown) it->fnShutdown();
#ifdef _WIN32
    if (it->hModule) FreeLibrary(static_cast<HMODULE>(it->hModule));
#endif
    m_plugins.erase(it);
}

void LanguageRegistry::UnloadAllPlugins() {
    for (auto& p : m_plugins) {
        if (p.fnShutdown) p.fnShutdown();
#ifdef _WIN32
        if (p.hModule) FreeLibrary(static_cast<HMODULE>(p.hModule));
#endif
    }
    m_plugins.clear();
}

std::vector<std::string> LanguageRegistry::GetLoadedPlugins() const {
    std::vector<std::string> names;
    for (const auto& p : m_plugins) names.push_back(p.name);
    return names;
}

// ============================================================================
// Discovery
// ============================================================================
std::vector<LanguageDescriptor> LanguageRegistry::GetAllLanguages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<LanguageDescriptor> out;
    for (const auto& [id, desc] : m_languages) {
        out.push_back(desc);
    }
    return out;
}

const LanguageDescriptor* LanguageRegistry::FindById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_languages.find(id);
    return (it != m_languages.end()) ? &it->second : nullptr;
}

const LanguageDescriptor* LanguageRegistry::FindByExtension(const std::string& ext) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    auto it = m_extMap.find(lowerExt);
    if (it == m_extMap.end()) return nullptr;
    
    auto langIt = m_languages.find(it->second);
    return (langIt != m_languages.end()) ? &langIt->second : nullptr;
}

const LanguageDescriptor* LanguageRegistry::FindByFilename(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_filenameMap.find(filename);
    if (it == m_filenameMap.end()) return nullptr;
    
    auto langIt = m_languages.find(it->second);
    return (langIt != m_languages.end()) ? &langIt->second : nullptr;
}

std::string LanguageRegistry::DetectLanguage(const std::string& filePath,
                                              const std::string& firstLine) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. Try filename match (for files like Makefile, Dockerfile, etc.)
    std::string filename = filePath;
    auto lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) filename = filePath.substr(lastSlash + 1);
    
    auto fnIt = m_filenameMap.find(filename);
    if (fnIt != m_filenameMap.end()) return fnIt->second;
    
    // 2. Try extension match
    auto dotPos = filename.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = filename.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        auto extIt = m_extMap.find(ext);
        if (extIt != m_extMap.end()) return extIt->second;
    }
    
    // 3. Try first-line pattern (shebang)
    if (!firstLine.empty()) {
        for (const auto& [id, desc] : m_languages) {
            if (!desc.firstLinePattern.empty()) {
                std::regex rx(desc.firstLinePattern);
                if (std::regex_search(firstLine, rx)) return id;
            }
        }
    }
    
    return "plaintext";
}

// ============================================================================
// Lexer Factory
// ============================================================================
std::unique_ptr<Lexer> LanguageRegistry::CreateLexer(const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. Check built-in providers
    auto provIt = m_providers.find(languageId);
    if (provIt != m_providers.end()) {
        return provIt->second->CreateLexer();
    }
    
    // 2. Check plugins
    for (auto& plugin : m_plugins) {
        if (!plugin.fnCreateLexer) continue;
        
        CLexerHandle handle = plugin.fnCreateLexer(languageId.c_str());
        if (!handle) continue;
        
        // Build token type mapping from descriptor
        std::unordered_map<int32_t, TokenType> tokenMap;
        // Default mappings
        tokenMap[0] = TokenType::Default;
        tokenMap[1] = TokenType::Keyword;
        tokenMap[2] = TokenType::Instruction;
        tokenMap[3] = TokenType::Register;
        tokenMap[4] = TokenType::Number;
        tokenMap[5] = TokenType::String;
        tokenMap[6] = TokenType::Comment;
        tokenMap[7] = TokenType::Operator;
        tokenMap[8] = TokenType::Preprocessor;
        tokenMap[9] = TokenType::Label;
        tokenMap[10] = TokenType::Directive;
        tokenMap[11] = TokenType::Type;
        tokenMap[12] = TokenType::Function;
        tokenMap[13] = TokenType::Variable;
        
        // Add custom token type mappings from language descriptor
        auto langIt = m_languages.find(languageId);
        if (langIt != m_languages.end()) {
            for (const auto& ct : langIt->second.customTokenTypes) {
                tokenMap[ct.id] = ct.baseFallback;
            }
        }
        
        return std::make_unique<PluginLexer>(
            handle, plugin.fnLex, plugin.fnLexStateful,
            plugin.fnDestroyLexer, tokenMap);
    }
    
    return nullptr;
}

// ============================================================================
// Feature Query
// ============================================================================
bool LanguageRegistry::SupportsFeature(const std::string& languageId,
                                        LanguageFeature f) const {
    auto* desc = FindById(languageId);
    if (!desc) return false;
    return hasFeature(desc->supportedFeatures, f);
}

LanguageFeature LanguageRegistry::GetFeatures(const std::string& languageId) const {
    auto* desc = FindById(languageId);
    return desc ? desc->supportedFeatures : LanguageFeature::None;
}

CommentStyle LanguageRegistry::GetCommentStyle(const std::string& languageId) const {
    auto* desc = FindById(languageId);
    return desc ? desc->comments : CommentStyle{};
}

// ============================================================================
// Formatting
// ============================================================================
void LanguageRegistry::SetExternalFormatter(const std::string& languageId,
                                             FormatCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_formatters[languageId] = std::move(cb);
}

std::string LanguageRegistry::FormatCode(const std::string& code,
                                          const std::string& languageId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 1. Check external formatter
    auto fmtIt = m_formatters.find(languageId);
    if (fmtIt != m_formatters.end()) {
        return fmtIt->second(code, languageId);
    }
    
    // 2. Check provider
    auto provIt = m_providers.find(languageId);
    if (provIt != m_providers.end()) {
        return provIt->second->FormatCode(code);
    }
    
    // 3. Check plugins
    for (auto& plugin : m_plugins) {
        if (!plugin.fnFormat) continue;
        char buf[65536];
        int result = plugin.fnFormat(languageId.c_str(), code.c_str(), buf, sizeof(buf));
        if (result > 0) return std::string(buf, result);
    }
    
    return code; // Unchanged
}

// ============================================================================
// Built-in Language Descriptors
// ============================================================================
void LanguageRegistry::registerBuiltins() {
    // ── MASM (already has MASMLexer) ──
    {
        LanguageDescriptor desc;
        desc.id = "masm";
        desc.name = "MASM Assembly";
        desc.version = "1.0";
        desc.fileExtensions = {".asm", ".inc", ".masm"};
        desc.comments.lineComment = ";";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::CommentToggle | LanguageFeature::BracketMatching;
        desc.autoClosingPairs = {{"(",")"}, {"[","]"}, {"\"","\""}};
        desc.surroundingPairs = {{"(",")"}, {"[","]"}, {"\"","\""}};
        desc.indentation.increasePattern = R"(^\s*(proc|macro|if|\.if)\b)";
        desc.indentation.decreasePattern = R"(^\s*(endp|endm|endif|\.endif)\b)";
        desc.keywords.push_back({"instructions",
            {"mov","add","sub","mul","div","push","pop","call","ret","jmp",
             "je","jne","jz","jnz","jg","jge","jl","jle","cmp","test",
             "and","or","xor","not","shl","shr","lea","nop","int","rep",
             "movsb","stosb","lodsb","scasb","cmpsb"},
            TokenType::Instruction});
        desc.keywords.push_back({"registers",
            {"eax","ebx","ecx","edx","esi","edi","esp","ebp",
             "rax","rbx","rcx","rdx","rsi","rdi","rsp","rbp",
             "r8","r9","r10","r11","r12","r13","r14","r15",
             "al","ah","bl","bh","cl","ch","dl","dh",
             "ax","bx","cx","dx","si","di","sp","bp",
             "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7"},
            TokenType::Register});
        desc.keywords.push_back({"directives",
            {".data",".code",".const",".model",".stack","proc","endp",
             "macro","endm","struct","ends","segment","assume",
             "include","includelib","extern","public","proto",
             "db","dw","dd","dq","dt","byte","word","dword","qword"},
            TokenType::Directive});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── C++ ──
    {
        LanguageDescriptor desc;
        desc.id = "cpp";
        desc.name = "C++";
        desc.version = "1.0";
        desc.fileExtensions = {".cpp", ".cxx", ".cc", ".c", ".h", ".hpp", ".hxx", ".hh", ".inl"};
        desc.comments = {"//", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::Folding | LanguageFeature::Indentation
            | LanguageFeature::AutoComplete | LanguageFeature::BracketMatching
            | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}, {"<",">"}};
        desc.surroundingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.folding.startPattern = R"(\{)";
        desc.folding.endPattern = R"(\})";
        desc.indentation.increasePattern = R"(\{[^}]*$)";
        desc.indentation.decreasePattern = R"(^\s*\})";
        desc.keywords.push_back({"keywords",
            {"alignas","alignof","and","and_eq","asm","auto","bitand","bitor",
             "bool","break","case","catch","char","char8_t","char16_t","char32_t",
             "class","compl","concept","const","consteval","constexpr","constinit",
             "const_cast","continue","co_await","co_return","co_yield",
             "decltype","default","delete","do","double","dynamic_cast",
             "else","enum","explicit","export","extern","false","float","for",
             "friend","goto","if","inline","int","long","mutable","namespace",
             "new","noexcept","not","not_eq","nullptr","operator","or","or_eq",
             "private","protected","public","register","reinterpret_cast",
             "requires","return","short","signed","sizeof","static","static_assert",
             "static_cast","struct","switch","template","this","thread_local",
             "throw","true","try","typedef","typeid","typename","union","unsigned",
             "using","virtual","void","volatile","wchar_t","while","xor","xor_eq"},
            TokenType::Keyword});
        desc.keywords.push_back({"types",
            {"int8_t","int16_t","int32_t","int64_t","uint8_t","uint16_t","uint32_t",
             "uint64_t","size_t","ptrdiff_t","intptr_t","uintptr_t",
             "string","wstring","vector","map","unordered_map","set","list","deque",
             "array","pair","tuple","optional","variant","any","shared_ptr",
             "unique_ptr","weak_ptr","function","mutex","atomic","thread"},
            TokenType::Type});
        desc.keywords.push_back({"preprocessor",
            {"#include","#define","#undef","#ifdef","#ifndef","#if","#else","#elif",
             "#endif","#pragma","#error","#warning","#line"},
            TokenType::Preprocessor});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Python ──
    {
        LanguageDescriptor desc;
        desc.id = "python";
        desc.name = "Python";
        desc.version = "1.0";
        desc.fileExtensions = {".py", ".pyw", ".pyi"};
        desc.fileNames = {"SConstruct", "SConscript"};
        desc.firstLinePattern = R"(^#!.*\bpython[23]?\b)";
        desc.comments = {"#", "\"\"\"", "\"\"\""};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::Folding | LanguageFeature::Indentation
            | LanguageFeature::AutoComplete | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.surroundingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.indentation.increasePattern = R"(:\s*$)";
        desc.indentation.decreasePattern = R"(^\s*(return|pass|break|continue|raise)\b)";
        desc.keywords.push_back({"keywords",
            {"False","None","True","and","as","assert","async","await","break",
             "class","continue","def","del","elif","else","except","finally",
             "for","from","global","if","import","in","is","lambda","nonlocal",
             "not","or","pass","raise","return","try","while","with","yield"},
            TokenType::Keyword});
        desc.keywords.push_back({"builtins",
            {"print","len","range","int","str","float","list","dict","set","tuple",
             "bool","type","isinstance","issubclass","id","hash","repr","input",
             "open","enumerate","zip","map","filter","sorted","reversed",
             "abs","min","max","sum","any","all","round","super","property",
             "staticmethod","classmethod","getattr","setattr","hasattr","delattr"},
            TokenType::Function});
        desc.keywords.push_back({"types",
            {"int","float","str","bool","list","dict","set","tuple","bytes",
             "bytearray","memoryview","complex","frozenset","range","type",
             "object","NoneType","Exception","BaseException"},
            TokenType::Type});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── JavaScript ──
    {
        LanguageDescriptor desc;
        desc.id = "javascript";
        desc.name = "JavaScript";
        desc.version = "1.0";
        desc.fileExtensions = {".js", ".mjs", ".cjs", ".jsx"};
        desc.mimeTypes = {"text/javascript", "application/javascript"};
        desc.comments = {"//", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::Folding | LanguageFeature::Indentation
            | LanguageFeature::AutoComplete | LanguageFeature::BracketMatching
            | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}, {"`","`"}};
        desc.indentation.increasePattern = R"(\{[^}]*$)";
        desc.indentation.decreasePattern = R"(^\s*\})";
        desc.keywords.push_back({"keywords",
            {"break","case","catch","class","const","continue","debugger","default",
             "delete","do","else","export","extends","false","finally","for",
             "function","if","import","in","instanceof","let","new","null","of",
             "return","super","switch","this","throw","true","try","typeof",
             "undefined","var","void","while","with","yield","async","await"},
            TokenType::Keyword});
        desc.keywords.push_back({"builtins",
            {"console","window","document","Math","JSON","Date","Array","Object",
             "String","Number","Boolean","Symbol","Map","Set","WeakMap","WeakSet",
             "Promise","Proxy","Reflect","RegExp","Error","TypeError","parseInt",
             "parseFloat","isNaN","isFinite","setTimeout","setInterval","fetch"},
            TokenType::Function});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── TypeScript ──
    {
        LanguageDescriptor desc;
        desc.id = "typescript";
        desc.name = "TypeScript";
        desc.version = "1.0";
        desc.fileExtensions = {".ts", ".tsx", ".mts", ".cts"};
        desc.comments = {"//", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::Folding | LanguageFeature::Indentation
            | LanguageFeature::AutoComplete | LanguageFeature::BracketMatching
            | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}, {"`","`"}, {"<",">"}};
        desc.indentation.increasePattern = R"(\{[^}]*$)";
        desc.indentation.decreasePattern = R"(^\s*\})";
        desc.keywords.push_back({"keywords",
            {"abstract","any","as","asserts","async","await","bigint","boolean",
             "break","case","catch","class","const","continue","debugger","declare",
             "default","delete","do","else","enum","export","extends","false",
             "finally","for","from","function","get","global","if","implements",
             "import","in","infer","instanceof","interface","is","keyof","let",
             "module","namespace","never","new","null","number","object","of",
             "override","package","private","protected","public","readonly",
             "require","return","satisfies","set","static","string","super",
             "switch","symbol","this","throw","true","try","type","typeof",
             "undefined","unique","unknown","using","var","void","while","with","yield"},
            TokenType::Keyword});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Rust ──
    {
        LanguageDescriptor desc;
        desc.id = "rust";
        desc.name = "Rust";
        desc.version = "1.0";
        desc.fileExtensions = {".rs"};
        desc.comments = {"//", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle
            | LanguageFeature::BracketMatching | LanguageFeature::Folding
            | LanguageFeature::Indentation;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.indentation.increasePattern = R"(\{[^}]*$)";
        desc.indentation.decreasePattern = R"(^\s*\})";
        desc.keywords.push_back({"keywords",
            {"as","async","await","break","const","continue","crate","dyn","else",
             "enum","extern","false","fn","for","if","impl","in","let","loop",
             "match","mod","move","mut","pub","ref","return","self","Self",
             "static","struct","super","trait","true","type","union","unsafe",
             "use","where","while","yield"},
            TokenType::Keyword});
        desc.keywords.push_back({"types",
            {"i8","i16","i32","i64","i128","isize","u8","u16","u32","u64","u128",
             "usize","f32","f64","bool","char","str","String","Vec","Box","Rc",
             "Arc","Cell","RefCell","Option","Result","HashMap","HashSet",
             "BTreeMap","BTreeSet","VecDeque","LinkedList","BinaryHeap"},
            TokenType::Type});
        desc.keywords.push_back({"builtins",
            {"println","eprintln","print","eprint","format","write","writeln",
             "vec","todo","unimplemented","unreachable","panic","assert",
             "assert_eq","assert_ne","debug_assert","cfg","include","include_str"},
            TokenType::Function});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Go ──
    {
        LanguageDescriptor desc;
        desc.id = "go";
        desc.name = "Go";
        desc.version = "1.0";
        desc.fileExtensions = {".go"};
        desc.comments = {"//", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle
            | LanguageFeature::BracketMatching | LanguageFeature::Folding;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"`","`"}};
        desc.indentation.increasePattern = R"(\{[^}]*$)";
        desc.indentation.decreasePattern = R"(^\s*\})";
        desc.keywords.push_back({"keywords",
            {"break","case","chan","const","continue","default","defer","else",
             "fallthrough","for","func","go","goto","if","import","interface",
             "map","package","range","return","select","struct","switch","type","var"},
            TokenType::Keyword});
        desc.keywords.push_back({"types",
            {"bool","byte","complex64","complex128","error","float32","float64",
             "int","int8","int16","int32","int64","rune","string","uint","uint8",
             "uint16","uint32","uint64","uintptr","any","comparable"},
            TokenType::Type});
        desc.keywords.push_back({"builtins",
            {"append","cap","close","complex","copy","delete","imag","len",
             "make","new","panic","print","println","real","recover"},
            TokenType::Function});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── JSON ──
    {
        LanguageDescriptor desc;
        desc.id = "json";
        desc.name = "JSON";
        desc.version = "1.0";
        desc.fileExtensions = {".json", ".jsonl", ".jsonc", ".geojson"};
        desc.fileNames = {".eslintrc", ".prettierrc", ".babelrc", "tsconfig.json",
                          "package.json", "composer.json"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::Folding
            | LanguageFeature::BracketMatching;
        desc.autoClosingPairs = {{"{","}"}, {"[","]"}, {"\"","\""}};
        desc.folding.startPattern = R"(\{|\[)";
        desc.folding.endPattern = R"(\}|\])";
        desc.keywords.push_back({"literals",
            {"true","false","null"},
            TokenType::Keyword});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Markdown ──
    {
        LanguageDescriptor desc;
        desc.id = "markdown";
        desc.name = "Markdown";
        desc.version = "1.0";
        desc.fileExtensions = {".md", ".markdown", ".mdown", ".mkd"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::Folding;
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── YAML ──
    {
        LanguageDescriptor desc;
        desc.id = "yaml";
        desc.name = "YAML";
        desc.version = "1.0";
        desc.fileExtensions = {".yml", ".yaml"};
        desc.comments.lineComment = "#";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::Folding
            | LanguageFeature::CommentToggle | LanguageFeature::Indentation;
        desc.autoClosingPairs = {{"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.indentation.increasePattern = R"(:\s*$)";
        desc.keywords.push_back({"literals",
            {"true","false","null","yes","no","on","off"},
            TokenType::Keyword});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── TOML ──
    {
        LanguageDescriptor desc;
        desc.id = "toml";
        desc.name = "TOML";
        desc.version = "1.0";
        desc.fileExtensions = {".toml"};
        desc.fileNames = {"Cargo.toml", "pyproject.toml"};
        desc.comments.lineComment = "#";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle
            | LanguageFeature::BracketMatching;
        desc.autoClosingPairs = {{"[","]"}, {"\"","\""}, {"'","'"}};
        desc.keywords.push_back({"literals",
            {"true","false"},
            TokenType::Keyword});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Shell / Bash ──
    {
        LanguageDescriptor desc;
        desc.id = "shellscript";
        desc.name = "Shell Script";
        desc.version = "1.0";
        desc.fileExtensions = {".sh", ".bash", ".zsh", ".fish"};
        desc.fileNames = {".bashrc", ".zshrc", ".profile", ".bash_profile"};
        desc.firstLinePattern = R"(^#!.*\b(ba|z|fi)?sh\b)";
        desc.comments.lineComment = "#";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}, {"`","`"}};
        desc.keywords.push_back({"keywords",
            {"if","then","else","elif","fi","case","esac","for","while","until",
             "do","done","in","function","select","time","coproc","readonly",
             "declare","local","export","set","unset","shift","source","alias",
             "return","exit","break","continue","eval","exec","trap"},
            TokenType::Keyword});
        desc.keywords.push_back({"builtins",
            {"echo","printf","read","cd","pwd","pushd","popd","dirs","test",
             "true","false","getopts","hash","type","ulimit","umask","wait",
             "kill","jobs","bg","fg","suspend","logout","history","let",
             "bind","builtin","caller","command","compgen","complete"},
            TokenType::Function});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── PowerShell ──
    {
        LanguageDescriptor desc;
        desc.id = "powershell";
        desc.name = "PowerShell";
        desc.version = "1.0";
        desc.fileExtensions = {".ps1", ".psm1", ".psd1"};
        desc.comments = {"#", "<#", "#>"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::CommentToggle | LanguageFeature::BracketMatching;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.keywords.push_back({"keywords",
            {"Begin","Break","Catch","Class","Continue","Data","Define","Do",
             "DynamicParam","Else","ElseIf","End","Enum","Exit","Filter",
             "Finally","For","ForEach","From","Function","Hidden","If","In",
             "InlineScript","Parallel","Param","Process","Return","Sequence",
             "Static","Switch","Throw","Trap","Try","Until","Using","Var",
             "While","Workflow"},
            TokenType::Keyword});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Dockerfile ──
    {
        LanguageDescriptor desc;
        desc.id = "dockerfile";
        desc.name = "Dockerfile";
        desc.version = "1.0";
        desc.fileExtensions = {".dockerfile"};
        desc.fileNames = {"Dockerfile", "Dockerfile.dev", "Dockerfile.prod"};
        desc.comments.lineComment = "#";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle;
        desc.keywords.push_back({"instructions",
            {"FROM","RUN","CMD","LABEL","MAINTAINER","EXPOSE","ENV","ADD","COPY",
             "ENTRYPOINT","VOLUME","USER","WORKDIR","ARG","ONBUILD","STOPSIGNAL",
             "HEALTHCHECK","SHELL"},
            TokenType::Keyword});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── SQL ──
    {
        LanguageDescriptor desc;
        desc.id = "sql";
        desc.name = "SQL";
        desc.version = "1.0";
        desc.fileExtensions = {".sql", ".ddl", ".dml"};
        desc.comments = {"--", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"'","'"}, {"\"","\""}};
        desc.keywords.push_back({"keywords",
            {"SELECT","FROM","WHERE","AND","OR","NOT","INSERT","INTO","VALUES",
             "UPDATE","SET","DELETE","CREATE","TABLE","ALTER","DROP","INDEX",
             "VIEW","JOIN","INNER","LEFT","RIGHT","OUTER","FULL","CROSS",
             "ON","AS","UNION","ALL","DISTINCT","ORDER","BY","GROUP","HAVING",
             "LIMIT","OFFSET","EXISTS","IN","BETWEEN","LIKE","IS","NULL",
             "TRUE","FALSE","CASE","WHEN","THEN","ELSE","END","BEGIN",
             "COMMIT","ROLLBACK","TRANSACTION","PRIMARY","KEY","FOREIGN",
             "REFERENCES","CONSTRAINT","DEFAULT","CHECK","UNIQUE","NOT",
             "AUTO_INCREMENT","CASCADE","TRIGGER","PROCEDURE","FUNCTION"},
            TokenType::Keyword});
        desc.keywords.push_back({"types",
            {"INT","INTEGER","SMALLINT","BIGINT","FLOAT","DOUBLE","DECIMAL",
             "NUMERIC","REAL","CHAR","VARCHAR","TEXT","BLOB","DATE","TIME",
             "TIMESTAMP","DATETIME","BOOLEAN","SERIAL","UUID","JSON","JSONB",
             "ARRAY","BYTEA","MONEY","INTERVAL","POINT","LINE","POLYGON"},
            TokenType::Type});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── CMake ──
    {
        LanguageDescriptor desc;
        desc.id = "cmake";
        desc.name = "CMake";
        desc.version = "1.0";
        desc.fileExtensions = {".cmake"};
        desc.fileNames = {"CMakeLists.txt"};
        desc.comments.lineComment = "#";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"\"","\""}, {"{","}"}};
        desc.keywords.push_back({"commands",
            {"cmake_minimum_required","project","add_executable","add_library",
             "target_link_libraries","target_include_directories","find_package",
             "include","set","option","if","elseif","else","endif","foreach",
             "endforeach","while","endwhile","function","endfunction","macro",
             "endmacro","return","message","install","configure_file",
             "add_custom_command","add_custom_target","add_subdirectory",
             "file","string","list","get_filename_component","execute_process",
             "add_definitions","add_compile_options","target_compile_features",
             "target_compile_definitions","target_compile_options"},
            TokenType::Function});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Makefile ──
    {
        LanguageDescriptor desc;
        desc.id = "makefile";
        desc.name = "Makefile";
        desc.version = "1.0";
        desc.fileNames = {"Makefile", "makefile", "GNUmakefile"};
        desc.fileExtensions = {".mk", ".mak"};
        desc.comments.lineComment = "#";
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::CommentToggle;
        desc.keywords.push_back({"directives",
            {"include","-include","sinclude","override","export","unexport",
             "define","endef","ifdef","ifndef","ifeq","ifneq","else","endif",
             ".PHONY",".SUFFIXES",".DEFAULT",".PRECIOUS",".SECONDARY",
             ".INTERMEDIATE",".DELETE_ON_ERROR",".IGNORE",".SILENT"},
            TokenType::Directive});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── XML / HTML ──
    {
        LanguageDescriptor desc;
        desc.id = "html";
        desc.name = "HTML";
        desc.version = "1.0";
        desc.fileExtensions = {".html", ".htm", ".xhtml", ".xml", ".svg"};
        desc.comments = {"", "<!--", "-->"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::Folding
            | LanguageFeature::BracketMatching | LanguageFeature::AutoComplete;
        desc.autoClosingPairs = {{"<",">"}, {"\"","\""}, {"'","'"}};
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── CSS ──
    {
        LanguageDescriptor desc;
        desc.id = "css";
        desc.name = "CSS";
        desc.version = "1.0";
        desc.fileExtensions = {".css", ".scss", ".sass", ".less"};
        desc.comments = {"", "/*", "*/"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::Folding
            | LanguageFeature::BracketMatching | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"{","}"}, {"(",")"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.folding.startPattern = R"(\{)";
        desc.folding.endPattern = R"(\})";
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Lua ──
    {
        LanguageDescriptor desc;
        desc.id = "lua";
        desc.name = "Lua";
        desc.version = "1.0";
        desc.fileExtensions = {".lua"};
        desc.comments = {"--", "--[[", "]]"};
        desc.supportedFeatures = LanguageFeature::Lexing | LanguageFeature::StatefulLexing
            | LanguageFeature::CommentToggle;
        desc.autoClosingPairs = {{"(",")"}, {"{","}"}, {"[","]"}, {"\"","\""}, {"'","'"}};
        desc.keywords.push_back({"keywords",
            {"and","break","do","else","elseif","end","false","for","function",
             "goto","if","in","local","nil","not","or","repeat","return",
             "then","true","until","while"},
            TokenType::Keyword});
        desc.keywords.push_back({"builtins",
            {"print","type","tostring","tonumber","error","pcall","xpcall",
             "require","dofile","loadfile","load","select","rawget","rawset",
             "rawlen","rawequal","pairs","ipairs","next","setmetatable",
             "getmetatable","assert","collectgarbage","unpack","table","string",
             "math","io","os","coroutine","debug","package","utf8"},
            TokenType::Function});
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
    
    // ── Plaintext (fallback) ──
    {
        LanguageDescriptor desc;
        desc.id = "plaintext";
        desc.name = "Plain Text";
        desc.version = "1.0";
        desc.fileExtensions = {".txt", ".text", ".log"};
        desc.supportedFeatures = LanguageFeature::None;
        desc.isBuiltin = true;
        RegisterDescriptor(desc);
    }
}

} // namespace Language
} // namespace RawrXD
