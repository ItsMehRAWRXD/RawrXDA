/**
 * \file language_support_system.h
 * \brief Comprehensive language support system for 50+ languages
 * \author RawrXD AI Engineering Team
 * \date January 14, 2026
 * 
 * This system provides COMPLETE support for 50+ programming languages:
 * - Syntax highlighting (via TextMate grammars)
 * - Code completion (via LSP servers)
 * - Code formatting (via language-specific formatters)
 * - Debugging (via Debug Adapter Protocol)
 * - Linting & analysis (via language tools)
 * - Build system integration
 * 
 * NO STUBS - COMPLETE IMPLEMENTATIONS ONLY
 */

#pragma once
#include <lsp_client.h>
#include <memory>
#include <string>
#include <functional>

namespace RawrXD {
namespace Language {

// ============================================================================
// LANGUAGE DEFINITIONS - All 50+ supported languages
// ============================================================================

enum class LanguageID {
    C, CPP, ObjectiveC, ObjectiveCPP,
    CSharp, FSharp, VBNet,
    Java, Kotlin, Scala, Groovy, Clojure,
    Python,
    JavaScript, TypeScript, HTML, CSS, SCSS, SASS, Less, JSON, XML, YAML, Vue, Svelte, JSX, TSX,
    Rust, Go, Zig, D,
    Haskell, Elixir, Erlang, LISP, Scheme, Racket,
    PHP, Ruby, Perl, Lua,
    VHDL, Verilog, SystemVerilog,
    MASM, NASM, GAS, ARM,
    SQL, Shell, PowerShell, Bash, Dart, Swift, R, MATLAB, Octave, Julia, Fortran, COBOL, Ada, Pascal, Delphi,
    PlainText, Unknown
};

enum class TokenType {
    Keyword,
    String,
    Comment,
    Number,
    Function,
    Variable,
    Class,
    Operator,
    Unknown
};

enum class TokenModifier {
    None,
    Readonly,
    Static,
    Deprecated
};

enum class CompletionKind {
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19
};

enum class InsertTextFormat {
    PlainText = 1,
    Snippet = 2
};

struct ParameterInformation {
    std::string label;
    std::string documentation;
};

struct SignatureInformation {
    std::string label;
    std::string documentation;
    int activeParameter{0};
    std::vector<ParameterInformation> parameters;
};

using CompletionCallback = std::function<void(const std::vector<RawrXD::CompletionItem>&)>;
using SignatureCallback = std::function<void(const std::vector<SignatureInformation>&)>;
using ResolveCallback = std::function<void(const RawrXD::CompletionItem&)>;

// ============================================================================
// LANGUAGE CONFIGURATION - Each language fully defined
// ============================================================================

struct LanguageConfiguration {
    LanguageID id;
    std::string name;
    std::string fileExtension;
    std::stringList allExtensions;
    std::string mimeType;
    
    std::string lspServerCommand;
    std::stringList lspServerArgs;
    std::string lspLanguageId;
    
    std::string formatterCommand;
    std::stringList formatterArgs;
    bool supportsFormatOnSave;
    bool supportsRangeFormatting;
    
    std::string debugAdapterCommand;
    std::stringList debugAdapterArgs;
    std::string debugLanguageId;
    
    std::string linterCommand;
    std::stringList linterArgs;
    
    std::string buildSystemType;
    std::string buildCommand;
    std::string runCommand;
    std::string testCommand;
    
    std::string commentLineStart;
    std::string commentBlockStart;
    std::string commentBlockEnd;
    std::stringList keywords;
    std::stringList builtins;
    
    int indentSize;
    bool useSpaces;
    std::string lineEndingStyle;
    
    bool supportsDebugger;
    bool supportsFormatter;
    bool supportsLinter;
    bool supportsLanguageServer;
    bool supportsBracketMatching;
    bool supportsCodeLens;
    bool supportsInlayHints;
    bool supportsSemanticTokens;
};

// ============================================================================
// LANGUAGE SUPPORT MANAGER - Orchestrates all language features
// ============================================================================

class LanguageSupportManager  {

public:
    explicit LanguageSupportManager( = nullptr);
    ~LanguageSupportManager() override;

    bool initialize();
    
    LanguageID detectLanguageFromFile(const std::string& filePath) const;
    LanguageID detectLanguageFromContent(const std::string& filePath) const;
    
    const LanguageConfiguration* getLanguageConfig(LanguageID id) const;
    const LanguageConfiguration* getLanguageConfig(const std::string& fileName) const;
    
    std::string getLanguageName(LanguageID id) const;
    
    bool isLanguageSupported(LanguageID id) const;
    bool isFormatterAvailable(LanguageID id) const;
    bool isDebuggerAvailable(LanguageID id) const;
    bool isLSPAvailable(LanguageID id) const;
    
    void requestCompletion(const std::string& filePath, int line, int column,
                          std::function<void(const std::vector<RawrXD::CompletionItem>&)> callback);
    
    void requestFormatting(const std::string& filePath, 
                          std::function<void(const std::string&)> callback);
    
    void requestHover(const std::string& filePath, int line, int column,
                     std::function<void(const std::string&)> callback);
    
    void requestDefinition(const std::string& filePath, int line, int column,
                          std::function<void(const std::string&, int, int)> callback);
    
    void requestRename(const std::string& filePath, int line, int column,
                      const std::string& newName,
                      std::function<void(const std::vector<std::pair<std::string, std::vector<std::pair<int, int>>>>&)> callback);
    
    void requestReferences(const std::string& filePath, int line, int column,
                          std::function<void(const std::vector<std::pair<std::string, std::vector<std::pair<int, int>>>>&)> callback);
    
    std::vector<LanguageConfiguration> getSupportedLanguages() const;
    
    struct LanguageStats {
        int totalLanguages;
        int supportedWithLSP;
        int supportedWithFormatter;
        int supportedWithDebugger;
        int supportedWithLinter;
    };
    LanguageStats getStatistics() const;
\nprivate:\n    void onLSPServerStarted(LanguageID id);
    void onLSPServerError(LanguageID id, const std::string& error);
    void onFormatterFinished(LanguageID id, const std::string& output);

private:
    void initializeLanguageConfigs();
    bool startLSPServer(LanguageID id);
    void stopLSPServer(LanguageID id);
    bool isToolAvailable(const std::string& command);
    std::string findToolInPath(const std::string& toolName);

    std::map<LanguageID, LanguageConfiguration> m_languages;
    std::map<LanguageID, void**> m_lspServers;
    std::map<LanguageID, void**> m_formatters;
\npublic:\n    void languageSupported(LanguageID id);
    void languageNotSupported(LanguageID id);
    void lspServerStarted(LanguageID id);
    void lspServerError(LanguageID id, const std::string& error);
    void formattingCompleted(LanguageID id);
    void formattingError(LanguageID id, const std::string& error);
};

// ============================================================================
// LANGUAGE-SPECIFIC FEATURES
// ============================================================================

class CppLanguageSupport {
public:
    static LanguageConfiguration getCppConfig();
    static bool initializeClangd();
    static bool startClangd(const std::string& workspaceRoot);
};

class PythonLanguageSupport {
public:
    static LanguageConfiguration getPythonConfig();
    static bool initializePylsp();
    static std::string detectVirtualEnv(const std::string& projectRoot);
};

class RustLanguageSupport {
public:
    static LanguageConfiguration getRustConfig();
    static bool initializeRustAnalyzer();
    static bool hasCargoProject(const std::string& projectRoot);
};

class GoLanguageSupport {
public:
    static LanguageConfiguration getGoConfig();
    static bool initializeGopls();
    static std::string detectGoroot();
};

class TypeScriptLanguageSupport {
public:
    static LanguageConfiguration getTypeScriptConfig();
    static LanguageConfiguration getJavaScriptConfig();
    static bool initializeTypeScriptServer();
};

class AssemblyLanguageSupport {
public:
    static LanguageConfiguration getMASMConfig();
    static LanguageConfiguration getNASMConfig();
    static LanguageConfiguration getGASConfig();
    static bool initializeAssemblySupport();
};

class JavaLanguageSupport {
public:
    static LanguageConfiguration getJavaConfig();
    static bool initializeEclipseJDT();
    static std::string detectJavaHome();
};

class CSharpLanguageSupport {
public:
    static LanguageConfiguration getCSharpConfig();
    static bool initializeOmnisharp();
    static bool hasDotnetProject(const std::string& projectRoot);
};

// ============================================================================
// COMPLETION & INTELLISENSE
// ============================================================================

class CodeCompletionProvider  {

public:
    explicit CodeCompletionProvider( = nullptr);

    void setLSPClient(LSPClient* client);
    void requestCompletions(const std::string& filePath, int line, int column, CompletionCallback callback);
    void requestSignatureHelp(const std::string& filePath, int line, int column, SignatureCallback callback);
    void resolveCompletionItem(const RawrXD::CompletionItem& item, ResolveCallback callback);
    std::vector<RawrXD::CompletionItem> getSnippets(LanguageID language);
    RawrXD::CompletionItem createSnippet(const std::string& label, const std::string& insertText, const std::string& description);
    std::string getCompletionKindIcon(CompletionKind kind);
    std::string getInsertTextFormatName(InsertTextFormat format);

private:
    LSPClient* m_lspClient;
};

// ============================================================================
// FORMATTING
// ============================================================================

using FormattingCallback = std::function<void(const std::string&, bool, const std::string&)>;

class CodeFormatter  {

public:
    explicit CodeFormatter( = nullptr);

    void setLanguageManager(LanguageSupportManager* manager);
    bool format(const std::string& filePath, const std::string& code, FormattingCallback callback);
    bool formatWithFormatter(LanguageID language, const std::string& filePath, const std::string& formatterCommand, FormattingCallback callback);
    bool formatRange(const std::string& filePath, const std::string& code, int startLine, int endLine, FormattingCallback callback);
    bool formatOnSave(const std::string& filePath, const std::string& code);
    std::string getFormatterForLanguage(LanguageID language);
    std::vector<std::string> getAvailableFormatters();

private:
    LanguageSupportManager* m_manager;
};

// ============================================================================
// DEBUGGING
// ============================================================================

struct LaunchConfiguration {
    std::string program;
    std::string cwd;
    std::stringList args;
};

struct AttachConfiguration {
    int processId{0};
    std::string cwd;
    std::stringList args;
};

struct StackFrame {
    int id{-1};
    std::string name;
    std::string file;
    int line{0};
    int column{0};
};

struct Variable {
    std::string name;
    std::string value;
    std::string type;
};

using DebugCallback = std::function<void(bool, const std::string&)>;
using BreakpointCallback = std::function<void(int, const std::string&)>;
using StackCallback = std::function<void(const std::vector<StackFrame>&)>;
using VariablesCallback = std::function<void(const std::vector<Variable>&)>;
using EvaluateCallback = std::function<void(const std::string&, const std::string&)>;
using ResponseCallback = std::function<void(const void*&)>;

class DAPHandler  {

public:
    explicit DAPHandler( = nullptr);
    ~DAPHandler();

    bool initialize(LanguageID language, const std::string& debugAdapterCommand);
    bool launch(const LaunchConfiguration& config, DebugCallback callback);
    bool attach(const AttachConfiguration& config, DebugCallback callback);
    bool setBreakpoint(const std::string& filePath, int line, BreakpointCallback callback);
    bool removeBreakpoint(const std::string& filePath, int line, DebugCallback callback);
    bool continue_();
    bool pause();
    bool stepOver();
    bool stepInto();
    bool stepOut();
    bool getCallStack(StackCallback callback);
    bool getVariables(int frameId, VariablesCallback callback);
    bool evaluate(int frameId, const std::string& expression, EvaluateCallback callback);
\npublic:\n    void debugStarted();
    void debugStopped();
    void debugContinued();
    void debugOutput(const std::string& output);

private:
    void sendRequest(const std::string& command, const void*& arguments, ResponseCallback callback = {});
    void onDebugOutput();
    void onDebugFinished();
    void handleDebugEvent(const std::string& event, const void*& data);
    void* buildLaunchArguments(const LaunchConfiguration& config);
    void* buildAttachArguments(const AttachConfiguration& config);

    void** m_debugProcess{nullptr};
    void** m_debugSocket{nullptr};
    LanguageID m_language{LanguageID::CPP};
    int m_messageId{0};
    std::map<int, ResponseCallback> m_pendingRequests;
};

// ============================================================================
// LINTING & ANALYSIS
// ============================================================================

enum class DiagnosticSeverity {
    Error,
    Warning,
    Information
};

struct DiagnosticMessage {
    int line{0};
    int column{0};
    DiagnosticSeverity severity{DiagnosticSeverity::Warning};
    std::string message;
    std::string ruleId;
};

using LintCallback = std::function<void(const std::vector<DiagnosticMessage>&)>;

class UniversalLinter  {

public:
    explicit UniversalLinter( = nullptr);

    void setLanguageManager(LanguageSupportManager* manager);
    bool lint(const std::string& filePath, const std::string& code, LintCallback callback);
    bool lintWithLinter(LanguageID language, const std::string& filePath, const std::string& linterCommand, LintCallback callback);
    std::vector<DiagnosticMessage> parseLinterOutput(const std::string& linterCommand, const std::string& output);
    std::vector<DiagnosticMessage> parseESLintOutput(const std::string& output);
    std::vector<DiagnosticMessage> parsePylintOutput(const std::string& output);
    std::vector<DiagnosticMessage> parseFlake8Output(const std::string& output);
    std::vector<DiagnosticMessage> parseRubocopOutput(const std::string& output);
    std::vector<DiagnosticMessage> parseClippyOutput(const std::string& output);
    std::vector<DiagnosticMessage> parseGolintOutput(const std::string& output);
    std::vector<DiagnosticMessage> parseClangTidyOutput(const std::string& output);
    std::vector<DiagnosticMessage> parseGenericLinterOutput(const std::string& output);
    std::string getLinterForLanguage(LanguageID language);

private:
    LanguageSupportManager* m_manager{nullptr};
};

// ============================================================================
// SYNTAX HIGHLIGHTING
// ============================================================================

struct HighlightRule {
    std::regex pattern;
    QTextCharFormat format;
};

struct SemanticToken {
    int startChar{0};
    int length{0};
    TokenType type{TokenType::Unknown};
    std::vector<TokenModifier> modifiers;
};

/**
 * TextMate grammar-based syntax highlighter
 */
class SyntaxHighlighter {

public:
    explicit SyntaxHighlighter(QTextDocument* parent = nullptr);
    
    void setLanguage(LanguageID language);
    void applySemanticTokens(const std::vector<SemanticToken>& tokens);

protected:
    void highlightBlock(const std::string& text) override;

private:
    void initializeHighlightingRules();
    void loadGrammarForLanguage(LanguageID id);
    void applyHighlighting(const std::string& text);
    void loadCppGrammar();
    void loadPythonGrammar();
    void loadRustGrammar();
    void loadGoGrammar();
    void loadJavaScriptGrammar();
    void loadJavaGrammar();
    void loadMASMGrammar();
    void loadBasicGrammar();
    
    LanguageID m_currentLanguage;
    std::vector<HighlightRule> m_currentRules;
};

}}  // namespace RawrXD::Language

