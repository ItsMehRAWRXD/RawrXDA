// ============================================================================
// context_mention_parser.cpp — @-Mention Context Parser Implementation
// ============================================================================
// Full implementation of @-mention parsing, resolution, autocomplete,
// and context assembly for Cursor-like context injection.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "context/context_mention_parser.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace RawrXD {
namespace Context {

// ============================================================================
// Constructor
// ============================================================================
ContextMentionParser::ContextMentionParser()
    // Match @keyword or @keyword:subkey followed by argument
    // Pattern: @(word)(:(word))?\s*(.*?)(?=@|\n|$)
    : m_mentionRegex(R"(@([a-zA-Z_][a-zA-Z0-9_]*)(?::([a-zA-Z_][a-zA-Z0-9_]*))?(?:\s+([^\n@]*))?)",
                     std::regex::ECMAScript)
{
}

// ============================================================================
// Configuration
// ============================================================================
void ContextMentionParser::SetResolverCallbacks(const MentionResolverCallbacks& callbacks) {
    m_callbacks = callbacks;
}

void ContextMentionParser::SetMaxTokenBudget(int maxTokens) {
    m_maxTokenBudget = maxTokens;
}

// ============================================================================
// Custom Provider Registration
// ============================================================================
void ContextMentionParser::RegisterCustomProvider(const CustomMentionProvider& provider) {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    m_customProviders[provider.name] = provider;
}

void ContextMentionParser::UnregisterCustomProvider(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    m_customProviders.erase(name);
}

std::vector<std::string> ContextMentionParser::GetRegisteredProviders() const {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    std::vector<std::string> names;
    for (const auto& [name, _] : m_customProviders) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// Mention Classification
// ============================================================================
MentionType ContextMentionParser::classifyMention(const std::string& keyword) {
    // Case-insensitive comparison
    std::string lower = keyword;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "file" || lower == "f")            return MentionType::File;
    if (lower == "symbol" || lower == "sym" || lower == "s")
                                                     return MentionType::Symbol;
    if (lower == "workspace" || lower == "ws")       return MentionType::Workspace;
    if (lower == "web" || lower == "search")         return MentionType::Web;
    if (lower == "terminal" || lower == "term")      return MentionType::Terminal;
    if (lower == "diagnostics" || lower == "diag" || lower == "errors")
                                                     return MentionType::Diagnostics;
    if (lower == "git" || lower == "diff")           return MentionType::Git;
    if (lower == "selection" || lower == "sel")       return MentionType::Selection;
    if (lower == "image" || lower == "img")          return MentionType::Image;
    if (lower == "docs" || lower == "doc")           return MentionType::Docs;
    if (lower == "codebase" || lower == "code")      return MentionType::Codebase;
    if (lower == "folder" || lower == "dir")         return MentionType::Folder;
    if (lower == "definition" || lower == "def")     return MentionType::Definition;
    if (lower == "references" || lower == "refs")    return MentionType::References;
    if (lower == "custom")                           return MentionType::Custom;
    
    // Check custom providers
    {
        std::lock_guard<std::mutex> lock(m_providerMutex);
        if (m_customProviders.find(lower) != m_customProviders.end()) {
            return MentionType::Custom;
        }
    }
    
    return MentionType::Unknown;
}

// ============================================================================
// Find Mentions in text
// ============================================================================
std::vector<ParsedMention> ContextMentionParser::findMentions(const std::string& input) {
    std::vector<ParsedMention> mentions;
    
    std::sregex_iterator it(input.begin(), input.end(), m_mentionRegex);
    std::sregex_iterator end;
    
    for (; it != end; ++it) {
        const std::smatch& match = *it;
        
        ParsedMention m;
        m.rawText = match[0].str();
        m.startOffset = static_cast<int>(match.position());
        m.endOffset = m.startOffset + static_cast<int>(m.rawText.size());
        
        std::string keyword = match[1].str();
        std::string subkey = match.size() > 2 ? match[2].str() : "";
        std::string arg = match.size() > 3 ? match[3].str() : "";
        
        // Trim argument
        while (!arg.empty() && std::isspace(static_cast<unsigned char>(arg.back()))) {
            arg.pop_back();
        }
        
        m.type = classifyMention(keyword);
        m.argument = arg;
        m.resolved = false;
        m.tokenEstimate = 0;
        
        // Handle @custom:pluginName
        if (m.type == MentionType::Custom) {
            if (!subkey.empty()) {
                m.pluginName = subkey;
            } else {
                // The keyword itself might be a custom provider name
                std::string lower = keyword;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::lock_guard<std::mutex> lock(m_providerMutex);
                if (m_customProviders.find(lower) != m_customProviders.end()) {
                    m.pluginName = lower;
                }
            }
        }
        
        mentions.push_back(std::move(m));
    }
    
    return mentions;
}

// ============================================================================
// Parse — Full parse with resolution
// ============================================================================
MentionParseResult ContextMentionParser::Parse(const std::string& input) {
    MentionParseResult result;
    result.mentions = findMentions(input);
    result.totalMentions = static_cast<int>(result.mentions.size());
    
    // Build cleaned text (mentions removed)
    result.cleanedText = input;
    // Remove mentions in reverse order to preserve offsets
    for (auto it = result.mentions.rbegin(); it != result.mentions.rend(); ++it) {
        result.cleanedText.erase(it->startOffset,
                                  it->endOffset - it->startOffset);
    }
    // Trim excessive whitespace
    while (result.cleanedText.find("  ") != std::string::npos) {
        auto pos = result.cleanedText.find("  ");
        result.cleanedText.erase(pos, 1);
    }
    
    // Resolve all mentions
    Resolve(result);
    
    // Count resolved and estimate tokens
    result.resolvedMentions = 0;
    result.totalTokenEstimate = 0;
    for (const auto& m : result.mentions) {
        if (m.resolved) result.resolvedMentions++;
        result.totalTokenEstimate += m.tokenEstimate;
    }
    
    return result;
}

// ============================================================================
// ParseSyntaxOnly — Fast parse without resolution
// ============================================================================
MentionParseResult ContextMentionParser::ParseSyntaxOnly(const std::string& input) {
    MentionParseResult result;
    result.mentions = findMentions(input);
    result.totalMentions = static_cast<int>(result.mentions.size());
    result.cleanedText = input;
    result.resolvedMentions = 0;
    result.totalTokenEstimate = 0;
    return result;
}

// ============================================================================
// Resolve all mentions
// ============================================================================
void ContextMentionParser::Resolve(MentionParseResult& result) {
    for (auto& m : result.mentions) {
        ResolveOne(m);
    }
}

void ContextMentionParser::ResolveOne(ParsedMention& mention) {
    switch (mention.type) {
        case MentionType::File:         resolveFile(mention); break;
        case MentionType::Symbol:       resolveSymbol(mention); break;
        case MentionType::Workspace:    resolveWorkspace(mention); break;
        case MentionType::Web:          resolveWeb(mention); break;
        case MentionType::Terminal:     resolveTerminal(mention); break;
        case MentionType::Diagnostics:  resolveDiagnostics(mention); break;
        case MentionType::Git:          resolveGit(mention); break;
        case MentionType::Selection:    resolveSelection(mention); break;
        case MentionType::Image:        resolveImage(mention); break;
        case MentionType::Docs:         resolveDocs(mention); break;
        case MentionType::Codebase:     resolveCodebase(mention); break;
        case MentionType::Folder:       resolveFolder(mention); break;
        case MentionType::Definition:   resolveDefinition(mention); break;
        case MentionType::References:   resolveReferences(mention); break;
        case MentionType::Custom:       resolveCustom(mention); break;
        case MentionType::Unknown:
            mention.resolvedLabel = "Unknown mention";
            mention.resolvedDetail = "Unrecognized @-mention: " + mention.rawText;
            break;
    }
}

// ============================================================================
// Resolution per type
// ============================================================================
void ContextMentionParser::resolveFile(ParsedMention& m) {
    if (!m_callbacks.readFile) {
        m.resolvedContent = "[File resolver not configured]";
        return;
    }
    
    std::string content = m_callbacks.readFile(m.argument);
    if (content.empty()) {
        m.resolvedContent = "[File not found: " + m.argument + "]";
        m.resolvedLabel = m.argument + " (not found)";
        return;
    }
    
    m.resolved = true;
    m.resolvedContent = "--- File: " + m.argument + " ---\n" + content + "\n---\n";
    m.resolvedLabel = m.argument;
    m.resolvedDetail = std::to_string(content.size()) + " bytes";
    m.tokenEstimate = EstimateTokens(content);
}

void ContextMentionParser::resolveSymbol(ParsedMention& m) {
    if (!m_callbacks.findSymbolDefinition) {
        m.resolvedContent = "[Symbol resolver not configured]";
        return;
    }
    
    std::string def = m_callbacks.findSymbolDefinition(m.argument);
    if (def.empty()) {
        m.resolvedContent = "[Symbol not found: " + m.argument + "]";
        m.resolvedLabel = m.argument + " (not found)";
        return;
    }
    
    m.resolved = true;
    m.resolvedContent = "--- Symbol: " + m.argument + " ---\n" + def + "\n---\n";
    m.resolvedLabel = m.argument;
    m.resolvedDetail = "Symbol definition";
    m.tokenEstimate = EstimateTokens(def);
}

void ContextMentionParser::resolveWorkspace(ParsedMention& m) {
    if (!m_callbacks.getWorkspaceContext) {
        m.resolvedContent = "[Workspace resolver not configured]";
        return;
    }
    
    std::string ctx = m_callbacks.getWorkspaceContext();
    m.resolved = true;
    m.resolvedContent = "--- Workspace Context ---\n" + ctx + "\n---\n";
    m.resolvedLabel = "Workspace";
    m.resolvedDetail = "Workspace-level context";
    m.tokenEstimate = EstimateTokens(ctx);
}

void ContextMentionParser::resolveWeb(ParsedMention& m) {
    if (!m_callbacks.webSearch) {
        m.resolvedContent = "[Web search not configured]";
        return;
    }
    
    std::string results = m_callbacks.webSearch(m.argument);
    m.resolved = !results.empty();
    m.resolvedContent = "--- Web: " + m.argument + " ---\n" + results + "\n---\n";
    m.resolvedLabel = "Web: " + m.argument;
    m.resolvedDetail = "Web search results";
    m.tokenEstimate = EstimateTokens(results);
}

void ContextMentionParser::resolveTerminal(ParsedMention& m) {
    if (!m_callbacks.getTerminalOutput) {
        m.resolvedContent = "[Terminal not configured]";
        return;
    }
    
    std::string output = m_callbacks.getTerminalOutput();
    m.resolved = true;
    m.resolvedContent = "--- Terminal Output ---\n" + output + "\n---\n";
    m.resolvedLabel = "Terminal";
    m.resolvedDetail = std::to_string(output.size()) + " bytes";
    m.tokenEstimate = EstimateTokens(output);
}

void ContextMentionParser::resolveDiagnostics(ParsedMention& m) {
    if (!m_callbacks.getDiagnostics) {
        m.resolvedContent = "[Diagnostics not configured]";
        return;
    }
    
    std::string diags = m_callbacks.getDiagnostics();
    m.resolved = true;
    m.resolvedContent = "--- Diagnostics ---\n" + diags + "\n---\n";
    m.resolvedLabel = "Diagnostics";
    m.resolvedDetail = "Current errors/warnings";
    m.tokenEstimate = EstimateTokens(diags);
}

void ContextMentionParser::resolveGit(ParsedMention& m) {
    if (!m_callbacks.getGitStatus) {
        m.resolvedContent = "[Git not configured]";
        return;
    }
    
    std::string status = m_callbacks.getGitStatus();
    m.resolved = true;
    m.resolvedContent = "--- Git Status ---\n" + status + "\n---\n";
    m.resolvedLabel = "Git";
    m.resolvedDetail = "Git diff/status";
    m.tokenEstimate = EstimateTokens(status);
}

void ContextMentionParser::resolveSelection(ParsedMention& m) {
    if (!m_callbacks.getSelection) {
        m.resolvedContent = "[Selection not configured]";
        return;
    }
    
    std::string sel = m_callbacks.getSelection();
    if (sel.empty()) {
        m.resolvedContent = "[No active selection]";
        m.resolvedLabel = "Selection (empty)";
        return;
    }
    
    m.resolved = true;
    m.resolvedContent = "--- Selection ---\n" + sel + "\n---\n";
    m.resolvedLabel = "Selection";
    m.resolvedDetail = std::to_string(sel.size()) + " chars selected";
    m.tokenEstimate = EstimateTokens(sel);
}

void ContextMentionParser::resolveImage(ParsedMention& m) {
    if (!m_callbacks.loadImage) {
        m.resolvedContent = "[Image loader not configured]";
        return;
    }
    
    std::string data = m_callbacks.loadImage(m.argument);
    if (data.empty()) {
        m.resolvedContent = "[Image not found: " + m.argument + "]";
        m.resolvedLabel = m.argument + " (not found)";
        return;
    }
    
    m.resolved = true;
    m.resolvedContent = data; // Base64-encoded for multimodal
    m.resolvedLabel = m.argument;
    m.resolvedDetail = "Image (" + std::to_string(data.size()) + " bytes b64)";
    m.tokenEstimate = 85; // Typical vision token cost per image tile
}

void ContextMentionParser::resolveDocs(ParsedMention& m) {
    if (!m_callbacks.docsSearch) {
        m.resolvedContent = "[Docs search not configured]";
        return;
    }
    
    std::string results = m_callbacks.docsSearch(m.argument);
    m.resolved = !results.empty();
    m.resolvedContent = "--- Docs: " + m.argument + " ---\n" + results + "\n---\n";
    m.resolvedLabel = "Docs: " + m.argument;
    m.resolvedDetail = "Documentation search results";
    m.tokenEstimate = EstimateTokens(results);
}

void ContextMentionParser::resolveCodebase(ParsedMention& m) {
    if (!m_callbacks.codebaseSearch) {
        m.resolvedContent = "[Codebase search not configured]";
        return;
    }
    
    std::string results = m_callbacks.codebaseSearch(m.argument);
    m.resolved = !results.empty();
    m.resolvedContent = "--- Codebase: " + m.argument + " ---\n" + results + "\n---\n";
    m.resolvedLabel = "Codebase: " + m.argument;
    m.resolvedDetail = "Semantic code search results";
    m.tokenEstimate = EstimateTokens(results);
}

void ContextMentionParser::resolveFolder(ParsedMention& m) {
    if (!m_callbacks.listFolder) {
        m.resolvedContent = "[Folder listing not configured]";
        return;
    }
    
    std::string listing = m_callbacks.listFolder(m.argument);
    m.resolved = !listing.empty();
    m.resolvedContent = "--- Folder: " + m.argument + " ---\n" + listing + "\n---\n";
    m.resolvedLabel = m.argument;
    m.resolvedDetail = "Folder structure";
    m.tokenEstimate = EstimateTokens(listing);
}

void ContextMentionParser::resolveDefinition(ParsedMention& m) {
    if (!m_callbacks.findSymbolDefinition) {
        m.resolvedContent = "[Definition lookup not configured]";
        return;
    }
    
    std::string def = m_callbacks.findSymbolDefinition(m.argument);
    m.resolved = !def.empty();
    m.resolvedContent = "--- Definition: " + m.argument + " ---\n" + def + "\n---\n";
    m.resolvedLabel = "def:" + m.argument;
    m.resolvedDetail = "Symbol definition";
    m.tokenEstimate = EstimateTokens(def);
}

void ContextMentionParser::resolveReferences(ParsedMention& m) {
    if (!m_callbacks.findSymbolReferences) {
        m.resolvedContent = "[References lookup not configured]";
        return;
    }
    
    std::string refs = m_callbacks.findSymbolReferences(m.argument);
    m.resolved = !refs.empty();
    m.resolvedContent = "--- References: " + m.argument + " ---\n" + refs + "\n---\n";
    m.resolvedLabel = "refs:" + m.argument;
    m.resolvedDetail = "All references";
    m.tokenEstimate = EstimateTokens(refs);
}

void ContextMentionParser::resolveCustom(ParsedMention& m) {
    std::lock_guard<std::mutex> lock(m_providerMutex);
    
    auto it = m_customProviders.find(m.pluginName);
    if (it == m_customProviders.end()) {
        m.resolvedContent = "[Custom provider not found: " + m.pluginName + "]";
        m.resolvedLabel = m.pluginName + " (not found)";
        return;
    }
    
    if (!it->second.resolve) {
        m.resolvedContent = "[Custom provider has no resolver: " + m.pluginName + "]";
        return;
    }
    
    std::string result = it->second.resolve(m.argument);
    m.resolved = !result.empty();
    m.resolvedContent = "--- " + m.pluginName + ": " + m.argument + " ---\n" 
                      + result + "\n---\n";
    m.resolvedLabel = m.pluginName + ":" + m.argument;
    m.resolvedDetail = it->second.description;
    m.tokenEstimate = EstimateTokens(result);
}

// ============================================================================
// Autocomplete Suggestions
// ============================================================================
std::vector<MentionSuggestion> ContextMentionParser::GetSuggestions(
    const std::string& partialInput, int cursorPosition) {
    
    std::vector<MentionSuggestion> suggestions;
    
    // Find the @ closest before cursor
    int atPos = -1;
    for (int i = std::min(cursorPosition, (int)partialInput.size()) - 1; i >= 0; --i) {
        if (partialInput[i] == '@') { atPos = i; break; }
        if (std::isspace(static_cast<unsigned char>(partialInput[i])) && i < cursorPosition - 1) break;
    }
    if (atPos < 0) return suggestions;
    
    std::string partial = partialInput.substr(atPos + 1, cursorPosition - atPos - 1);
    std::transform(partial.begin(), partial.end(), partial.begin(), ::tolower);
    
    // Built-in mention types
    struct BuiltinSuggestion {
        const char* keyword;
        const char* description;
        const char* icon;
        MentionType type;
    };
    
    static const BuiltinSuggestion builtins[] = {
        {"file",        "Include file contents",          "file",       MentionType::File},
        {"symbol",      "Include symbol definition",      "symbol",     MentionType::Symbol},
        {"workspace",   "Include workspace context",      "workspace",  MentionType::Workspace},
        {"web",         "Web search results",             "globe",      MentionType::Web},
        {"terminal",    "Recent terminal output",         "terminal",   MentionType::Terminal},
        {"diagnostics", "Current errors/warnings",        "warning",    MentionType::Diagnostics},
        {"git",         "Git diff/status",                "git",        MentionType::Git},
        {"selection",   "Current editor selection",       "selection",  MentionType::Selection},
        {"image",       "Image input (Vision)",           "image",      MentionType::Image},
        {"docs",        "Documentation search",           "book",       MentionType::Docs},
        {"codebase",    "Semantic codebase search",       "search",     MentionType::Codebase},
        {"folder",      "Include folder structure",       "folder",     MentionType::Folder},
        {"definition",  "Go-to-definition result",        "definition", MentionType::Definition},
        {"references",  "Find-all-references result",     "references", MentionType::References},
    };
    
    for (const auto& b : builtins) {
        std::string kw = b.keyword;
        if (partial.empty() || kw.find(partial) == 0) {
            MentionSuggestion s;
            s.type = b.type;
            s.label = std::string("@") + b.keyword;
            s.description = b.description;
            s.insertText = std::string("@") + b.keyword + " ";
            s.iconId = b.icon;
            s.score = partial.empty() ? 0.5f : 
                      (float)partial.size() / (float)kw.size();
            suggestions.push_back(s);
        }
    }
    
    // Custom providers
    {
        std::lock_guard<std::mutex> lock(m_providerMutex);
        for (const auto& [name, provider] : m_customProviders) {
            if (partial.empty() || name.find(partial) == 0) {
                MentionSuggestion s;
                s.type = MentionType::Custom;
                s.label = "@" + name;
                s.description = provider.description;
                s.insertText = "@" + name + " ";
                s.iconId = provider.iconId;
                s.score = partial.empty() ? 0.3f :
                          (float)partial.size() / (float)name.size();
                suggestions.push_back(s);
            }
        }
    }
    
    // Sort by score descending
    std::sort(suggestions.begin(), suggestions.end(),
              [](const MentionSuggestion& a, const MentionSuggestion& b) {
                  return a.score > b.score;
              });
    
    return suggestions;
}

// ============================================================================
// File/Symbol Completion
// ============================================================================
std::vector<std::string> ContextMentionParser::CompleteFilePath(const std::string& partial) {
    if (!m_callbacks.globFiles) return {};
    std::string pattern = partial + "*";
    return m_callbacks.globFiles(pattern);
}

std::vector<std::string> ContextMentionParser::CompleteSymbolName(const std::string& partial) {
    // Delegate to codebase search as symbol fuzzy match
    if (!m_callbacks.codebaseSearch) return {};
    std::string results = m_callbacks.codebaseSearch(partial);
    
    // Parse results into individual symbol names
    std::vector<std::string> symbols;
    std::istringstream iss(results);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) symbols.push_back(line);
        if (symbols.size() >= 20) break;
    }
    return symbols;
}

// ============================================================================
// Context Assembly
// ============================================================================
std::string ContextMentionParser::AssembleContext(const MentionParseResult& result,
                                                   int maxTokens) {
    if (maxTokens <= 0) maxTokens = m_maxTokenBudget;
    
    std::ostringstream ss;
    int tokensUsed = 0;
    
    // Priority order: Selection > File > Symbol > Definition > Diagnostics > others
    static const MentionType priority[] = {
        MentionType::Selection,
        MentionType::File,
        MentionType::Symbol,
        MentionType::Definition,
        MentionType::Diagnostics,
        MentionType::Git,
        MentionType::Terminal,
        MentionType::References,
        MentionType::Folder,
        MentionType::Workspace,
        MentionType::Codebase,
        MentionType::Web,
        MentionType::Docs,
        MentionType::Image,
        MentionType::Custom
    };
    
    for (auto pType : priority) {
        for (const auto& m : result.mentions) {
            if (m.type != pType || !m.resolved) continue;
            
            int cost = m.tokenEstimate;
            if (tokensUsed + cost > maxTokens) {
                // Truncate content to fit budget
                int remaining = maxTokens - tokensUsed;
                if (remaining > 100) { // Worth including partially
                    int charLimit = remaining * 4; // ~4 chars per token
                    std::string truncated = m.resolvedContent.substr(0, charLimit);
                    truncated += "\n... [truncated to fit context budget]\n";
                    ss << truncated;
                    tokensUsed = maxTokens;
                }
                continue;
            }
            
            ss << m.resolvedContent << "\n";
            tokensUsed += cost;
        }
    }
    
    return ss.str();
}

// ============================================================================
// Token Estimation
// ============================================================================
int ContextMentionParser::EstimateTokens(const std::string& text) {
    if (text.empty()) return 0;
    // Rough estimate: ~4 characters per token for English
    // This matches GPT-style tokenizers approximately
    return static_cast<int>(text.size() / 4) + 1;
}

} // namespace Context
} // namespace RawrXD
