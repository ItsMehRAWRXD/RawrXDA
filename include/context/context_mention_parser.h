// ============================================================================
// context_mention_parser.h — @-Mention Context Parser System
// ============================================================================
// Implements Cursor-like @-mention parsing for context injection:
//
//   @file <path>        — Include file contents
//   @symbol <name>      — Include symbol definition
//   @workspace           — Include workspace-level context
//   @web <query>        — Web search results
//   @terminal            — Recent terminal output
//   @diagnostics         — Current errors/warnings
//   @git                 — Git diff/status
//   @selection           — Current editor selection
//   @image <path>       — Image input (Vision)
//   @docs <query>       — Documentation search
//   @codebase <query>   — Semantic codebase search
//   @folder <path>      — Include folder structure
//   @definition <name>  — Go-to-definition result
//   @references <name>  — Find-all-references result
//   @custom:<plugin>    — Plugin-provided context
//
// Integrates with:
//   - Context::Indexer (symbol lookup)
//   - Context::SemanticStore (codebase search)
//   - LSP Bridge (diagnostics, definitions, references)
//   - Win32IDE (file reading, terminal, selection)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <cstdint>
#include <regex>

namespace RawrXD {
namespace Context {

// ============================================================================
// Mention types
// ============================================================================
enum class MentionType {
    File,           // @file path/to/file.cpp
    Symbol,         // @symbol MyClass::method
    Workspace,      // @workspace
    Web,            // @web search query
    Terminal,       // @terminal
    Diagnostics,    // @diagnostics
    Git,            // @git
    Selection,      // @selection
    Image,          // @image path/to/img.png
    Docs,           // @docs query
    Codebase,       // @codebase search query
    Folder,         // @folder src/utils
    Definition,     // @definition functionName
    References,     // @references MyClass
    Custom,         // @custom:pluginName args
    Unknown         // Unrecognized mention
};

// ============================================================================
// Parsed mention
// ============================================================================
struct ParsedMention {
    MentionType     type;
    std::string     rawText;        // Original text including @
    std::string     argument;       // Text after the mention type
    int             startOffset;    // Character offset in original text
    int             endOffset;      // End offset
    std::string     pluginName;     // For @custom: mentions
    
    // Resolved data (filled by resolver)
    bool            resolved;
    std::string     resolvedContent;
    std::string     resolvedLabel;      // Display label for context chip
    std::string     resolvedDetail;     // Tooltip detail
    int             tokenEstimate;      // Estimated tokens for this context
};

// ============================================================================
// Parse result
// ============================================================================
struct MentionParseResult {
    std::string                     cleanedText;    // Text with mentions removed
    std::vector<ParsedMention>      mentions;       // All parsed mentions
    int                             totalMentions;
    int                             resolvedMentions;
    int                             totalTokenEstimate;
    
    bool hasType(MentionType type) const {
        for (const auto& m : mentions) {
            if (m.type == type) return true;
        }
        return false;
    }
    
    std::vector<ParsedMention> getByType(MentionType type) const {
        std::vector<ParsedMention> result;
        for (const auto& m : mentions) {
            if (m.type == type) result.push_back(m);
        }
        return result;
    }
};

// ============================================================================
// Autocomplete suggestion for @-mention
// ============================================================================
struct MentionSuggestion {
    MentionType     type;
    std::string     label;          // What to show: "@file", "@symbol", etc.
    std::string     description;    // "Include file contents"
    std::string     insertText;     // What to insert when selected
    std::string     iconId;         // Icon for UI
    float           score;          // Relevance score
};

// ============================================================================
// Context resolver callbacks — provided by host (Win32IDE)
// ============================================================================
struct MentionResolverCallbacks {
    // File operations
    std::function<std::string(const std::string& path)>
        readFile;
    std::function<std::vector<std::string>(const std::string& pattern)>
        globFiles;
    std::function<std::string(const std::string& folderPath)>
        listFolder;
    
    // Symbol operations
    std::function<std::string(const std::string& symbolName)>
        findSymbolDefinition;
    std::function<std::string(const std::string& symbolName)>
        findSymbolReferences;
    
    // IDE state
    std::function<std::string()>
        getSelection;
    std::function<std::string()>
        getTerminalOutput;
    std::function<std::string()>
        getDiagnostics;
    std::function<std::string()>
        getGitStatus;
    
    // Search
    std::function<std::string(const std::string& query)>
        webSearch;
    std::function<std::string(const std::string& query)>
        docsSearch;
    std::function<std::string(const std::string& query)>
        codebaseSearch;
    
    // Image
    std::function<std::string(const std::string& imagePath)>
        loadImage;  // Returns base64-encoded image data
    
    // Workspace
    std::function<std::string()>
        getWorkspaceContext;
};

// ============================================================================
// Custom mention provider (for plugins)
// ============================================================================
struct CustomMentionProvider {
    std::string     name;           // Plugin name (after @custom:)
    std::string     description;    // Human-readable description
    std::string     iconId;         // Icon
    
    // Resolve callback
    std::function<std::string(const std::string& argument)>
        resolve;
    
    // Autocomplete callback
    std::function<std::vector<MentionSuggestion>(const std::string& partial)>
        autocomplete;
};

// ============================================================================
// ContextMentionParser — Main @-mention parsing engine
// ============================================================================
class ContextMentionParser {
public:
    ContextMentionParser();
    ~ContextMentionParser() = default;

    // ---- Configuration ----
    void SetResolverCallbacks(const MentionResolverCallbacks& callbacks);
    void SetMaxTokenBudget(int maxTokens);
    int  GetMaxTokenBudget() const { return m_maxTokenBudget; }

    // ---- Custom Provider Registration (Pluggable) ----
    void RegisterCustomProvider(const CustomMentionProvider& provider);
    void UnregisterCustomProvider(const std::string& name);
    std::vector<std::string> GetRegisteredProviders() const;

    // ---- Parsing ----
    MentionParseResult Parse(const std::string& input);
    
    // Parse without resolving (fast, for UI highlighting)
    MentionParseResult ParseSyntaxOnly(const std::string& input);

    // ---- Resolution (fill in resolved content) ----
    void Resolve(MentionParseResult& result);
    void ResolveOne(ParsedMention& mention);

    // ---- Autocomplete ----
    std::vector<MentionSuggestion> GetSuggestions(
        const std::string& partialInput,
        int cursorPosition
    );

    // ---- File Path Completion ----
    std::vector<std::string> CompleteFilePath(const std::string& partial);
    std::vector<std::string> CompleteSymbolName(const std::string& partial);

    // ---- Context Assembly ----
    // Combine resolved mentions into a context string for the LLM
    std::string AssembleContext(const MentionParseResult& result,
                                int maxTokens = 0);

    // ---- Token Estimation ----
    static int EstimateTokens(const std::string& text);

private:
    // Internal parsing
    std::vector<ParsedMention> findMentions(const std::string& input);
    MentionType classifyMention(const std::string& keyword);
    std::string extractArgument(const std::string& input, int afterKeyword);
    
    // Resolution per type
    void resolveFile(ParsedMention& m);
    void resolveSymbol(ParsedMention& m);
    void resolveWorkspace(ParsedMention& m);
    void resolveWeb(ParsedMention& m);
    void resolveTerminal(ParsedMention& m);
    void resolveDiagnostics(ParsedMention& m);
    void resolveGit(ParsedMention& m);
    void resolveSelection(ParsedMention& m);
    void resolveImage(ParsedMention& m);
    void resolveDocs(ParsedMention& m);
    void resolveCodebase(ParsedMention& m);
    void resolveFolder(ParsedMention& m);
    void resolveDefinition(ParsedMention& m);
    void resolveReferences(ParsedMention& m);
    void resolveCustom(ParsedMention& m);

    MentionResolverCallbacks  m_callbacks;
    int                       m_maxTokenBudget = 8192;
    
    mutable std::mutex        m_providerMutex;
    std::map<std::string, CustomMentionProvider> m_customProviders;
    
    // Regex patterns (compiled once)
    std::regex                m_mentionRegex;
};

} // namespace Context
} // namespace RawrXD
