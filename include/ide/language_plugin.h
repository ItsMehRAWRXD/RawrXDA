// ============================================================================
// language_plugin.h — Pluginable Language Support System
// ============================================================================
// Extends the Lexer framework with dynamic language providers (DLL plugins).
//
// Existing: Abstract Lexer base (RawrXD_Lexer.h), MASMLexer.
// Added:    LanguageRegistry singleton, LanguageDescriptor, C-ABI plugin
//           contract, extensible TokenType mapping, grammar support.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>

// Include base Lexer definition (required for PluginLexer inheritance)
#include "RawrXD_Lexer.h"

namespace RawrXD {
namespace Language {

// ============================================================================
// Extended Token Type Mapping
// ============================================================================
// Allows plugins to define custom token types beyond the base 14
struct ExtendedTokenType {
    int32_t                 id;             // Plugin-assigned ID (>= 100)
    char                    name[64];       // Semantic name, e.g. "decorator"
    uint32_t                foregroundARGB;  // Suggested color
    uint32_t                backgroundARGB;
    bool                    bold;
    bool                    italic;
    bool                    underline;
    TokenType               baseFallback;   // Map to base TokenType for non-aware renderers
};

// ============================================================================
// Language Feature Flags
// ============================================================================
enum class LanguageFeature : uint32_t {
    None                = 0x00000000,
    Lexing              = 0x00000001,   // Basic syntax highlighting
    StatefulLexing      = 0x00000002,   // Multi-line state tracking
    Folding             = 0x00000004,   // Code folding
    Indentation         = 0x00000008,   // Smart indentation rules
    AutoComplete        = 0x00000010,   // Basic keyword completion
    BracketMatching     = 0x00000020,   // Bracket/paren matching
    CommentToggle       = 0x00000040,   // Line/block comment toggling
    Snippets            = 0x00000080,   // Snippet insertion
    Diagnostics         = 0x00000100,   // Error/warning detection
    Formatting          = 0x00000200,   // Code formatting
    GotoDefinition      = 0x00000400,   // Navigation to definitions
    FindReferences      = 0x00000800,   // Find all references
    Hover               = 0x00001000,   // Hover information
    SignatureHelp       = 0x00002000,   // Function signature help
    SemanticTokens      = 0x00004000,   // Semantic token provider
    All                 = 0xFFFFFFFF,
};

inline LanguageFeature operator|(LanguageFeature a, LanguageFeature b) {
    return static_cast<LanguageFeature>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline LanguageFeature operator&(LanguageFeature a, LanguageFeature b) {
    return static_cast<LanguageFeature>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasFeature(LanguageFeature flags, LanguageFeature f) {
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(f)) != 0;
}

// ============================================================================
// Commenting Style
// ============================================================================
struct CommentStyle {
    std::string lineComment;    // e.g. "//"
    std::string blockStart;     // e.g. "/*"
    std::string blockEnd;       // e.g. "*/"
};

// ============================================================================
// Indentation Rules
// ============================================================================
struct IndentationRules {
    std::string increasePattern; // Regex: lines after which indent increases
    std::string decreasePattern; // Regex: lines that should outdent
    std::string unindentPattern; // Regex: lines that should fully unindent
};

// ============================================================================
// Auto-Closing Pair
// ============================================================================
struct AutoClosingPair {
    std::string open;   // e.g. "("
    std::string close;  // e.g. ")"
};

// ============================================================================
// Folding Region Markers
// ============================================================================
struct FoldingMarkers {
    std::string startPattern;   // Regex for fold start
    std::string endPattern;     // Regex for fold end
};

// ============================================================================
// Keyword Set (for registration and completion)
// ============================================================================
struct KeywordSet {
    std::string               category;   // e.g. "control", "type", "builtin"
    std::vector<std::string>  words;
    TokenType                 tokenType;  // How to highlight these
};

// ============================================================================
// Language Descriptor — Full description of a language
// ============================================================================
struct LanguageDescriptor {
    std::string                         id;                 // e.g. "rust", "go"
    std::string                         name;               // Display name
    std::string                         version;            // "1.0"
    std::vector<std::string>            fileExtensions;     // {".rs"}
    std::vector<std::string>            fileNames;          // {"Makefile"}
    std::vector<std::string>            mimeTypes;          // {"text/x-rust"}
    std::string                         firstLinePattern;   // Regex for shebang
    
    CommentStyle                        comments;
    IndentationRules                    indentation;
    std::vector<AutoClosingPair>        autoClosingPairs;
    std::vector<AutoClosingPair>        surroundingPairs;
    FoldingMarkers                      folding;
    
    std::vector<KeywordSet>             keywords;
    std::vector<ExtendedTokenType>      customTokenTypes;
    
    LanguageFeature                     supportedFeatures;
    
    bool                                isBuiltin = false;
    std::string                         pluginSource;       // DLL path if from plugin
};

// ============================================================================
// Language Provider Interface (internal C++ interface)
// ============================================================================
class ILanguageProvider {
public:
    virtual ~ILanguageProvider() = default;
    
    virtual const LanguageDescriptor& GetDescriptor() const = 0;
    
    // Create a Lexer for this language
    virtual std::unique_ptr<Lexer> CreateLexer() = 0;
    
    // Optional: completion keywords
    virtual std::vector<std::string> GetCompletionKeywords() { return {}; }
    
    // Optional: format code
    virtual std::string FormatCode(const std::string& code) { return code; }
    
    // Optional: folding ranges (line pairs)
    virtual std::vector<std::pair<int,int>> GetFoldingRanges(const std::string& code) { return {}; }
    
    // Optional: indentation for line
    virtual int GetIndentLevel(const std::string& line, int prevIndent) { return prevIndent; }
    
    // Optional: diagnostics
    struct Diagnostic {
        int line, col;
        int severity;       // 0=error, 1=warning, 2=info
        std::string message;
    };
    virtual std::vector<Diagnostic> GetDiagnostics(const std::string& code) { return {}; }
};

// ============================================================================
// C-ABI Plugin Contract (for external DLLs)
// ============================================================================

// C-struct mirror of LanguageDescriptor (DLL boundary safe)
struct CLanguageDescriptor {
    const char*     id;
    const char*     name;
    const char*     version;
    const char*     fileExtensions;     // Comma-separated: ".rs,.rlib"
    const char*     fileNames;          // Comma-separated
    const char*     lineComment;
    const char*     blockCommentStart;
    const char*     blockCommentEnd;
    const char*     keywords;           // Semicolon-separated groups: "if,else,for;int,float,str"
    uint32_t        features;           // LanguageFeature bitmask
};

// C-ABI token used across DLL boundary
struct CLexToken {
    int32_t     type;       // Maps to TokenType ordinal or custom ID
    int32_t     start;
    int32_t     length;
};

// C-ABI plugin info
struct CLanguagePluginInfo {
    const char*     pluginName;
    const char*     pluginVersion;
    const char*     author;
    int32_t         languageCount;
};

// C-ABI lexer handle
typedef void* CLexerHandle;

extern "C" {
    // DLL exports
    typedef CLanguagePluginInfo* (*LanguagePlugin_GetInfo_fn)();
    typedef int     (*LanguagePlugin_GetLanguages_fn)(CLanguageDescriptor* outDescs, int maxCount);
    typedef CLexerHandle (*LanguagePlugin_CreateLexer_fn)(const char* languageId);
    typedef int     (*LanguagePlugin_Lex_fn)(CLexerHandle handle, const wchar_t* text, int textLen,
                                             CLexToken* outTokens, int maxTokens);
    typedef int     (*LanguagePlugin_LexStateful_fn)(CLexerHandle handle, const wchar_t* text,
                                                      int textLen, int startState,
                                                      CLexToken* outTokens, int maxTokens);
    typedef void    (*LanguagePlugin_DestroyLexer_fn)(CLexerHandle handle);
    typedef int     (*LanguagePlugin_Format_fn)(const char* languageId, const char* code,
                                                 char* outBuf, int outBufSize);
    typedef void    (*LanguagePlugin_Shutdown_fn)();
}

// ============================================================================
// Plugin-Backed Lexer — wraps C-ABI lexer as a Lexer subclass
// ============================================================================
class PluginLexer : public Lexer {
public:
    PluginLexer(CLexerHandle handle,
                LanguagePlugin_Lex_fn lexFn,
                LanguagePlugin_LexStateful_fn lexStatefulFn,
                LanguagePlugin_DestroyLexer_fn destroyFn,
                const std::unordered_map<int32_t, TokenType>& tokenMap);
    ~PluginLexer() override;
    
    void lex(const std::wstring& text, std::vector<Token>& outTokens) override;
    int lexStateful(const std::wstring& text, int startState,
                    std::vector<Token>& outTokens) override;

private:
    CLexerHandle                                m_handle;
    LanguagePlugin_Lex_fn                       m_lexFn;
    LanguagePlugin_LexStateful_fn               m_lexStatefulFn;
    LanguagePlugin_DestroyLexer_fn              m_destroyFn;
    std::unordered_map<int32_t, TokenType>      m_tokenMap;
};

// ============================================================================
// Language Registry — Singleton managing all language providers
// ============================================================================
class LanguageRegistry {
public:
    static LanguageRegistry& Instance();
    ~LanguageRegistry();
    
    // ── Lifecycle ──
    void Initialize();
    void Shutdown();
    
    // ── Built-in Registration ──
    void RegisterProvider(std::unique_ptr<ILanguageProvider> provider);
    void RegisterDescriptor(const LanguageDescriptor& desc);
    void UnregisterLanguage(const std::string& id);
    
    // ── Plugin Loading ──
    bool LoadPlugin(const std::string& dllPath);
    void UnloadPlugin(const std::string& name);
    void UnloadAllPlugins();
    std::vector<std::string> GetLoadedPlugins() const;
    
    // ── Discovery ──
    std::vector<LanguageDescriptor> GetAllLanguages() const;
    const LanguageDescriptor* FindById(const std::string& id) const;
    const LanguageDescriptor* FindByExtension(const std::string& ext) const;
    const LanguageDescriptor* FindByFilename(const std::string& filename) const;
    std::string DetectLanguage(const std::string& filePath,
                                const std::string& firstLine = "") const;
    
    // ── Lexer Factory ──
    std::unique_ptr<Lexer> CreateLexer(const std::string& languageId);
    
    // ── Feature Query ──
    bool SupportsFeature(const std::string& languageId, LanguageFeature f) const;
    LanguageFeature GetFeatures(const std::string& languageId) const;
    
    // ── Comment Style ──
    CommentStyle GetCommentStyle(const std::string& languageId) const;
    
    // ── Formatting Callback ──
    typedef std::function<std::string(const std::string& code, const std::string& lang)>
        FormatCallback;
    void SetExternalFormatter(const std::string& languageId, FormatCallback cb);
    std::string FormatCode(const std::string& code, const std::string& languageId);
    
private:
    LanguageRegistry() = default;
    void registerBuiltins();
    
    struct LoadedPlugin {
        std::string                         path;
        std::string                         name;
        void*                               hModule = nullptr;
        LanguagePlugin_GetInfo_fn           fnGetInfo = nullptr;
        LanguagePlugin_GetLanguages_fn      fnGetLanguages = nullptr;
        LanguagePlugin_CreateLexer_fn       fnCreateLexer = nullptr;
        LanguagePlugin_Lex_fn              fnLex = nullptr;
        LanguagePlugin_LexStateful_fn       fnLexStateful = nullptr;
        LanguagePlugin_DestroyLexer_fn      fnDestroyLexer = nullptr;
        LanguagePlugin_Format_fn            fnFormat = nullptr;
        LanguagePlugin_Shutdown_fn          fnShutdown = nullptr;
    };
    
    mutable std::mutex                                          m_mutex;
    std::map<std::string, LanguageDescriptor>                   m_languages;      // id → desc
    std::map<std::string, std::unique_ptr<ILanguageProvider>>   m_providers;      // id → provider
    std::map<std::string, std::string>                          m_extMap;         // ".cpp" → "cpp"
    std::map<std::string, std::string>                          m_filenameMap;    // "Makefile" → "makefile"
    std::vector<LoadedPlugin>                                   m_plugins;
    std::map<std::string, FormatCallback>                       m_formatters;
    std::atomic<bool>                                           m_initialized{false};
};

} // namespace Language
} // namespace RawrXD
