// missing_features_batch3.hpp - Third batch of missing features
// Zero external dependencies, under 3000 lines
// Features: AI Completion, IntelliSense, Semantic Highlighting, Document Links,
//           Hover Provider, Signature Help, Rename Refactoring, Extract Method,
//           Organize Imports, Code Lens, Inlay Hints, Color Decorations,
//           Bookmarks, Todo Comments, Spell Checker, Clipboard History, and more

#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <regex>
#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>

namespace rawrxd {

// ============================================================================
// SECTION 1: AI CODE COMPLETION ENGINE (Local)
// ============================================================================

struct AICompletion {
    std::string text;
    std::string displayText;
    float score = 0.0f;
    std::string source;  // "local", "copilot", "codestral"
    int tokens = 0;
    bool isInline = false;  // Ghost text vs dropdown
    std::vector<std::string> alternatives;
};

struct AIContext {
    std::string beforeCursor;
    std::string afterCursor;
    std::string fileName;
    std::string language;
    std::vector<std::string> recentLines;
    std::vector<std::string> importedSymbols;
    std::string functionSignature;
    int indentLevel = 0;
    bool inComment = false;
    bool inString = false;
};

class AICompletionEngine {
public:
    bool enabled = true;
    bool inlineEnabled = true;
    int maxSuggestions = 5;
    int contextLines = 50;
    int maxTokens = 256;
    float temperature = 0.2f;
    
    std::string modelPath;
    std::string modelName = "local-7b";
    
    std::vector<AICompletion> completions;
    int currentCompletionIndex = 0;
    bool isLoading = false;
    
    std::function<void(const std::vector<AICompletion>&)> onComplete;
    
    // Main completion entry point
    void getCompletions(const AIContext& context) {
        if (!enabled) return;
        
        isLoading = true;
        completions.clear();
        
        // Build prompt
        std::string prompt = buildPrompt(context);
        
        // Try local patterns first (instant)
        auto localCompletions = getLocalCompletions(context);
        
        completions = localCompletions;
        
        // Sort by score
        std::sort(completions.begin(), completions.end(),
            [](const AICompletion& a, const AICompletion& b) {
                return a.score > b.score;
            });
        
        // Limit results
        if (completions.size() > (size_t)maxSuggestions) {
            completions.resize(maxSuggestions);
        }
        
        isLoading = false;
        
        if (onComplete) onComplete(completions);
    }
    
    // Inline (ghost text) completion
    std::string getInlineCompletion(const AIContext& context) {
        if (!inlineEnabled) return "";
        
        auto patterns = getPatternCompletions(context);
        
        if (!patterns.empty() && patterns[0].score > 0.8f) {
            return patterns[0].text;
        }
        
        return "";
    }
    
    void acceptCompletion(std::string& text, int& cursorPos) {
        if (currentCompletionIndex >= 0 && currentCompletionIndex < (int)completions.size()) {
            const auto& comp = completions[currentCompletionIndex];
            text.insert(cursorPos, comp.text);
            cursorPos += (int)comp.text.length();
            completions.clear();
        }
    }
    
    void nextCompletion() {
        if (!completions.empty()) {
            currentCompletionIndex = (currentCompletionIndex + 1) % (int)completions.size();
        }
    }
    
    void prevCompletion() {
        if (!completions.empty()) {
            currentCompletionIndex = (currentCompletionIndex == 0) ? 
                (int)completions.size() - 1 : currentCompletionIndex - 1;
        }
    }
    
    void dismiss() {
        completions.clear();
        currentCompletionIndex = 0;
    }
    
private:
    std::string buildPrompt(const AIContext& ctx) {
        std::string prompt;
        prompt += "// File: " + ctx.fileName + "\n";
        prompt += "// Language: " + ctx.language + "\n\n";
        for (const auto& line : ctx.recentLines) {
            prompt += line + "\n";
        }
        prompt += ctx.beforeCursor;
        return prompt;
    }
    
    std::vector<AICompletion> getLocalCompletions(const AIContext& ctx) {
        std::vector<AICompletion> results;
        
        if (ctx.language == "cpp" || ctx.language == "c") {
            auto cpp = getCppCompletions(ctx);
            results.insert(results.end(), cpp.begin(), cpp.end());
        } else if (ctx.language == "python") {
            auto py = getPythonCompletions(ctx);
            results.insert(results.end(), py.begin(), py.end());
        } else if (ctx.language == "javascript" || ctx.language == "typescript") {
            auto js = getJavaScriptCompletions(ctx);
            results.insert(results.end(), js.begin(), js.end());
        }
        
        auto patterns = getPatternCompletions(ctx);
        results.insert(results.end(), patterns.begin(), patterns.end());
        
        return results;
    }
    
    std::vector<AICompletion> getCppCompletions(const AIContext& ctx) {
        std::vector<AICompletion> results;
        
        static const std::vector<std::pair<std::string, std::string>> patterns = {
            {"std::cout << ", " << std::endl;"},
            {"std::vector<", "> v;"},
            {"for (int i = 0; i < ", "; ++i) {"},
            {"if (", ") {"},
            {"return ", ";"},
            {"#include <", ">"},
            {"namespace ", " {"},
        };
        
        for (const auto& [prefix, suffix] : patterns) {
            if (ctx.beforeCursor.length() >= prefix.length() &&
                ctx.beforeCursor.substr(ctx.beforeCursor.length() - prefix.length()) == prefix) {
                AICompletion c;
                c.text = suffix;
                c.displayText = prefix + "..." + suffix;
                c.score = 0.9f;
                c.source = "pattern";
                c.isInline = true;
                results.push_back(c);
            }
        }
        
        if (ctx.beforeCursor.find(")") != std::string::npos &&
            ctx.beforeCursor.back() == '{') {
            AICompletion c;
            c.text = "\n    $0\n}";
            c.displayText = "function body";
            c.score = 0.85f;
            c.source = "snippet";
            results.push_back(c);
        }
        
        return results;
    }
    
    std::vector<AICompletion> getPythonCompletions(const AIContext& ctx) {
        std::vector<AICompletion> results;
        
        static const std::vector<std::pair<std::string, std::string>> patterns = {
            {"print(", ")"},
            {"import ", ""},
            {"from ", " import "},
            {"for ", " in "},
            {"if ", ":"},
            {"def ", "(self):"},
            {"class ", ":"},
            {"with open(", ") as f:"},
        };
        
        for (const auto& [prefix, suffix] : patterns) {
            if (endsWith(ctx.beforeCursor, prefix)) {
                AICompletion c;
                c.text = suffix;
                c.displayText = prefix + suffix;
                c.score = 0.9f;
                c.source = "pattern";
                results.push_back(c);
            }
        }
        
        if (ctx.beforeCursor.find("def (") != std::string::npos) {
            AICompletion c;
            c.text = "self, ";
            c.displayText = "self, ...";
            c.score = 0.95f;
            c.source = "pattern";
            results.push_back(c);
        }
        
        return results;
    }
    
    std::vector<AICompletion> getJavaScriptCompletions(const AIContext& ctx) {
        std::vector<AICompletion> results;
        
        static const std::vector<std::pair<std::string, std::string>> patterns = {
            {"console.log(", ")"},
            {"import ", " from ''"},
            {"export ", " default "},
            {"async ", "function "},
            {"await ", ""},
            {"=> ", "{}"},
            {"function ", "() {}"},
        };
        
        for (const auto& [prefix, suffix] : patterns) {
            if (endsWith(ctx.beforeCursor, prefix)) {
                AICompletion c;
                c.text = suffix;
                c.displayText = prefix + suffix;
                c.score = 0.9f;
                c.source = "pattern";
                results.push_back(c);
            }
        }
        
        return results;
    }
    
    std::vector<AICompletion> getPatternCompletions(const AIContext& ctx) {
        std::vector<AICompletion> results;
        
        static const std::map<char, char> closing = {
            {'(', ')'}, {'[', ']'}, {'{', '}'}, {'"', '"'}, {'\'', '\''}
        };
        
        if (!ctx.beforeCursor.empty()) {
            char last = ctx.beforeCursor.back();
            auto it = closing.find(last);
            if (it != closing.end()) {
                if (ctx.afterCursor.empty() || ctx.afterCursor[0] != it->second) {
                    AICompletion c;
                    c.text = std::string(1, it->second);
                    c.displayText = std::string(1, it->second);
                    c.score = 0.99f;
                    c.source = "bracket";
                    c.isInline = true;
                    results.push_back(c);
                }
            }
        }
        
        std::string trimmed = ctx.beforeCursor;
        while (!trimmed.empty() && std::isspace((unsigned char)trimmed.back())) trimmed.pop_back();
        
        if (!trimmed.empty() && (trimmed.back() == '{' || trimmed.back() == ':')) {
            AICompletion c;
            c.text = "\n" + std::string(ctx.indentLevel * 4 + 4, ' ');
            c.displayText = "new line";
            c.score = 0.95f;
            c.source = "indent";
            c.isInline = true;
            results.push_back(c);
        }
        
        return results;
    }
    
    bool endsWith(const std::string& str, const std::string& suffix) {
        if (suffix.length() > str.length()) return false;
        return str.substr(str.length() - suffix.length()) == suffix;
    }
};

// ============================================================================
// SECTION 2: INTELLISENSE ENGINE
// ============================================================================

struct IntelliSenseItem {
    std::string label;
    std::string kind;  // class, function, variable, keyword, snippet, etc.
    std::string detail;
    std::string documentation;
    std::string insertText;
    std::string sortText;
    std::string filterText;
    int score = 0;
    bool deprecated = false;
    bool preselect = false;
};

class IntelliSenseEngine {
public:
    std::vector<IntelliSenseItem> items;
    std::vector<IntelliSenseItem*> filteredItems;
    std::string prefix;
    size_t selectedIndex = 0;
    bool visible = false;
    int triggerLine = 0;
    int triggerColumn = 0;
    
    std::string language;
    std::string filePath;
    std::vector<std::string> documentLines;
    std::map<std::string, std::vector<IntelliSenseItem>> cache;
    
    bool autoTrigger = true;
    int minTriggerLength = 1;
    bool showSnippets = true;
    bool showKeywords = true;
    bool showLocalSymbols = true;
    
    std::function<void(const IntelliSenseItem&)> onAccept;
    
    void trigger(int line, int column, const std::string& triggerChar = "") {
        triggerLine = line;
        triggerColumn = column;
        
        if (line >= 0 && line < (int)documentLines.size()) {
            const auto& ln = documentLines[line];
            int end = column;
            int start = end;
            while (start > 0 && (std::isalnum((unsigned char)ln[start-1]) || ln[start-1] == '_')) start--;
            prefix = ln.substr(start, end - start);
        }
        
        gatherCompletions(line, column);
        filterCompletions();
        
        visible = !filteredItems.empty();
        selectedIndex = 0;
    }
    
    void gatherCompletions(int line, int column) {
        items.clear();
        if (showKeywords) addKeywords();
        if (showSnippets) addSnippets();
        if (showLocalSymbols) addLocalSymbols();
        addContextCompletions(line, column);
    }
    
    void filterCompletions() {
        filteredItems.clear();
        
        for (auto& item : items) {
            if (matchesPrefix(item, prefix)) {
                item.score = calculateScore(item, prefix);
                filteredItems.push_back(&item);
            }
        }
        
        std::sort(filteredItems.begin(), filteredItems.end(),
            [](IntelliSenseItem* a, IntelliSenseItem* b) {
                if (a->preselect != b->preselect) return a->preselect;
                return a->score > b->score;
            });
    }
    
    void addCharacters(const std::string& chars) {
        prefix += chars;
        filterCompletions();
    }
    
    void backspace() {
        if (!prefix.empty()) {
            prefix.pop_back();
            filterCompletions();
        }
        if (prefix.empty() && minTriggerLength > 0) {
            hide();
        }
    }
    
    IntelliSenseItem* getSelected() {
        if (selectedIndex < filteredItems.size()) {
            return filteredItems[selectedIndex];
        }
        return nullptr;
    }
    
    void selectNext() {
        if (!filteredItems.empty()) {
            selectedIndex = (selectedIndex + 1) % filteredItems.size();
        }
    }
    
    void selectPrev() {
        if (!filteredItems.empty()) {
            selectedIndex = (selectedIndex == 0) ? filteredItems.size() - 1 : selectedIndex - 1;
        }
    }
    
    void accept() {
        if (auto* item = getSelected()) {
            if (onAccept) onAccept(*item);
        }
        hide();
    }
    
    void hide() {
        visible = false;
        items.clear();
        filteredItems.clear();
        prefix.clear();
    }
    
private:
    void addKeywords() {
        static const std::map<std::string, std::vector<std::string>> keywords = {
            {"cpp", {"auto", "break", "case", "catch", "class", "const", "constexpr",
                     "continue", "default", "delete", "do", "else", "enum", "explicit",
                     "extern", "false", "for", "friend", "goto", "if", "inline",
                     "namespace", "new", "nullptr", "operator", "private", "protected",
                     "public", "return", "sizeof", "static", "struct", "switch",
                     "template", "this", "throw", "true", "try", "typedef", "typename",
                     "union", "using", "virtual", "void", "volatile", "while"}},
            {"python", {"False", "None", "True", "and", "as", "assert", "async", "await",
                        "break", "class", "continue", "def", "del", "elif", "else",
                        "except", "finally", "for", "from", "global", "if", "import",
                        "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
                        "return", "try", "while", "with", "yield"}},
            {"javascript", {"async", "await", "break", "case", "catch", "class", "const",
                            "continue", "debugger", "default", "delete", "do", "else",
                            "export", "extends", "false", "finally", "for", "function",
                            "if", "import", "in", "instanceof", "let", "new", "null",
                            "return", "static", "super", "switch", "this", "throw",
                            "true", "try", "typeof", "undefined", "var", "void",
                            "while", "with", "yield"}},
            {"rust", {"as", "async", "await", "break", "const", "continue", "crate",
                      "dyn", "else", "enum", "extern", "false", "fn", "for", "if",
                      "impl", "in", "let", "loop", "match", "mod", "move", "mut",
                      "pub", "ref", "return", "self", "Self", "static", "struct",
                      "super", "trait", "true", "type", "unsafe", "use", "where", "while"}},
            {"go", {"break", "case", "chan", "const", "continue", "default", "defer",
                    "else", "fallthrough", "for", "func", "go", "goto", "if", "import",
                    "interface", "map", "package", "range", "return", "select", "struct",
                    "switch", "type", "var"}}
        };
        
        auto it = keywords.find(language);
        if (it != keywords.end()) {
            for (const auto& kw : it->second) {
                IntelliSenseItem item;
                item.label = kw;
                item.kind = "keyword";
                item.insertText = kw;
                item.sortText = "zzz_" + kw;
                items.push_back(item);
            }
        }
    }
    
    void addSnippets() {
        static const std::map<std::string, std::vector<std::pair<std::string, std::string>>> snippets = {
            {"cpp", {{"for", "for (int ${1:i} = 0; $1 < ${2:n}; $1++) {\n\t$0\n}"},
                     {"fori", "for (int ${1:i} = 0; $1 < ${2:count}; ++$1) {\n\t$0\n}"},
                     {"main", "int main(int argc, char* argv[]) {\n\t$0\n\treturn 0;\n}"},
                     {"class", "class ${1:ClassName} {\npublic:\n\t$1();\n\t~$1();\nprivate:\n\t$0\n};"},
                     {"if", "if (${1:condition}) {\n\t$0\n}"},
                     {"ife", "if (${1:condition}) {\n\t$2\n} else {\n\t$0\n}"},
                     {"while", "while (${1:condition}) {\n\t$0\n}"}}},
            {"python", {{"def", "def ${1:function}(${2:params}):\n\t$0"},
                        {"class", "class ${1:Name}:\n\tdef __init__(self):\n\t\t$0"},
                        {"for", "for ${1:item} in ${2:iterable}:\n\t$0"},
                        {"if", "if ${1:condition}:\n\t$0"},
                        {"main", "if __name__ == '__main__':\n\t$0"}}},
            {"javascript", {{"log", "console.log($0);"},
                            {"func", "function ${1:name}(${2:params}) {\n\t$0\n}"},
                            {"arrow", "const ${1:name} = (${2:params}) => {\n\t$0\n};"},
                            {"async", "async function ${1:name}(${2:params}) {\n\t$0\n}"},
                            {"class", "class ${1:Name} {\n\tconstructor($2) {\n\t\t$0\n\t}\n}"}}}
        };
        
        auto it = snippets.find(language);
        if (it != snippets.end()) {
            for (const auto& [label, snippet] : it->second) {
                IntelliSenseItem item;
                item.label = label;
                item.kind = "snippet";
                item.insertText = snippet;
                item.detail = "Snippet";
                item.sortText = "yyy_" + label;
                items.push_back(item);
            }
        }
    }
    
    void addLocalSymbols() {
        std::regex identRe(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
        std::set<std::string> seen;
        
        for (size_t lineNum = 0; lineNum < documentLines.size(); lineNum++) {
            const auto& line = documentLines[lineNum];
            std::smatch match;
            std::string::const_iterator searchStart(line.cbegin());
            
            while (std::regex_search(searchStart, line.cend(), match, identRe)) {
                std::string name = match[1].str();
                
                if (seen.find(name) == seen.end() && !isKeyword(name)) {
                    seen.insert(name);
                    
                    IntelliSenseItem item;
                    item.label = name;
                    item.kind = detectSymbolKind(line, match.position());
                    item.insertText = name;
                    item.detail = "line " + std::to_string(lineNum + 1);
                    item.sortText = "xxx_" + name;
                    items.push_back(item);
                }
                
                searchStart = match.suffix().first;
            }
        }
    }
    
    void addContextCompletions(int line, int column) {
        if (line < 0 || line >= (int)documentLines.size()) return;
        
        const auto& currentLine = documentLines[line];
        
        if (column > 0 && currentLine[column - 1] == '.') {
            addMemberCompletions(currentLine, column);
        }
        
        if (currentLine.find("import ") != std::string::npos ||
            currentLine.find("#include") != std::string::npos ||
            currentLine.find("from ") != std::string::npos) {
            addImportCompletions(currentLine);
        }
    }
    
    void addMemberCompletions(const std::string& line, int column) {
        // Placeholder for LSP-driven member completions
    }
    
    void addImportCompletions(const std::string& line) {
        static const std::map<std::string, std::vector<std::string>> commonImports = {
            {"cpp", {"<iostream>", "<vector>", "<string>", "<map>", "<set>",
                     "<algorithm>", "<memory>", "<thread>", "<chrono>", "<fstream>"}},
            {"python", {"os", "sys", "json", "re", "datetime", "collections",
                        "typing", "pathlib", "subprocess", "asyncio"}},
            {"javascript", {"react", "lodash", "axios", "express", "fs", "path"}}
        };
        
        auto it = commonImports.find(language);
        if (it != commonImports.end()) {
            for (const auto& imp : it->second) {
                IntelliSenseItem item;
                item.label = imp;
                item.kind = "module";
                item.insertText = imp;
                item.sortText = "www_" + imp;
                items.push_back(item);
            }
        }
    }
    
    bool matchesPrefix(const IntelliSenseItem& item, const std::string& pref) {
        if (pref.empty()) return true;
        
        std::string lowerItem = item.label;
        std::string lowerPref = pref;
        std::transform(lowerItem.begin(), lowerItem.end(), lowerItem.begin(), ::tolower);
        std::transform(lowerPref.begin(), lowerPref.end(), lowerPref.begin(), ::tolower);
        
        return lowerItem.find(lowerPref) == 0;
    }
    
    int calculateScore(const IntelliSenseItem& item, const std::string& pref) {
        int score = 0;
        
        if (item.label == pref) score += 1000;
        else if (item.label.find(pref) == 0) score += 800;
        else {
            std::string lower = item.label;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(pref) == 0) score += 700;
        }
        
        if (item.kind == "function" || item.kind == "method") score += 50;
        else if (item.kind == "class" || item.kind == "struct") score += 40;
        else if (item.kind == "variable") score += 30;
        else if (item.kind == "keyword") score += 10;
        else if (item.kind == "snippet") score += 20;
        
        if (item.preselect) score += 500;
        
        return score;
    }
    
    bool isKeyword(const std::string& word) {
        static std::set<std::string> keywords = {
            "if", "else", "for", "while", "do", "switch", "case", "break",
            "continue", "return", "class", "struct", "void", "int", "char",
            "bool", "true", "false", "null", "this", "new", "delete", "def",
            "import", "from", "as", "async", "await", "function", "const", "let"
        };
        return keywords.count(word) > 0;
    }
    
    std::string detectSymbolKind(const std::string& line, size_t pos) {
        std::string before = line.substr(0, pos);
        
        if (before.find("class ") != std::string::npos) return "class";
        if (before.find("struct ") != std::string::npos) return "struct";
        if (before.find("def ") != std::string::npos) return "function";
        if (before.find("function ") != std::string::npos) return "function";
        if (before.find("fn ") != std::string::npos) return "function";
        if (before.find("func ") != std::string::npos) return "function";
        if (before.find("const ") != std::string::npos) return "constant";
        if (before.find("let ") != std::string::npos) return "variable";
        if (before.find("var ") != std::string::npos) return "variable";
        
        return "identifier";
    }
};

// ============================================================================
// SECTION 3: SEMANTIC HIGHLIGHTING
// ============================================================================

struct SemanticToken {
    int line;
    int startColumn;
    int length;
    std::string tokenType;
    std::vector<std::string> modifiers;
    uint32_t color;
};

class SemanticHighlighter {
public:
    std::vector<SemanticToken> tokens;
    std::map<std::string, uint32_t> typeColors;
    
    void setColors(const std::map<std::string, uint32_t>& colors) {
        typeColors = colors;
    }
    
    std::vector<SemanticToken> highlight(const std::vector<std::string>& lines,
                                          const std::string& language) {
        tokens.clear();
        
        typeColors["class"]      = 0xFF4EC9B0;
        typeColors["function"]   = 0xFFDCDCAA;
        typeColors["variable"]   = 0xFF9CDCFE;
        typeColors["parameter"]  = 0xFF9CDCFE;
        typeColors["property"]   = 0xFF4FC1FF;
        typeColors["enum"]       = 0xFF4FC1FF;
        typeColors["enumMember"] = 0xFF4FC1FF;
        typeColors["type"]       = 0xFF4EC9B0;
        typeColors["macro"]      = 0xFF569CD6;
        typeColors["method"]     = 0xFFDCDCAA;
        typeColors["namespace"]  = 0xFF4EC9B0;
        typeColors["comment"]    = 0xFF6A9955;
        typeColors["string"]     = 0xFFCE9178;
        typeColors["keyword"]    = 0xFF569CD6;
        typeColors["number"]     = 0xFFB5CEA8;
        
        SymbolTable table = buildSymbolTable(lines, language);
        
        for (const auto& sym : table.symbols) {
            SemanticToken token;
            token.line = sym.line;
            token.startColumn = sym.column;
            token.length = (int)sym.name.length();
            token.tokenType = sym.kind;
            
            if (sym.isDefinition) token.modifiers.push_back("definition");
            if (sym.isDeclaration) token.modifiers.push_back("declaration");
            if (sym.isReadonly) token.modifiers.push_back("readonly");
            
            auto it = typeColors.find(token.tokenType);
            token.color = (it != typeColors.end()) ? it->second : 0xFFFFFFFF;
            
            tokens.push_back(token);
        }
        
        return tokens;
    }
    
private:
    struct SymbolEntry {
        std::string name;
        std::string kind;
        int line;
        int column;
        int scopeLevel;
        bool isDefinition = false;
        bool isDeclaration = false;
        bool isReadonly = false;
    };
    
    struct SymbolTable {
        std::vector<SymbolEntry> symbols;
    };
    
    SymbolTable buildSymbolTable(const std::vector<std::string>& lines,
                                  const std::string& language) {
        if (language == "cpp" || language == "c") return buildCppSymbolTable(lines);
        if (language == "python") return buildPythonSymbolTable(lines);
        if (language == "javascript" || language == "typescript") return buildJsSymbolTable(lines);
        return {};
    }
    
    SymbolTable buildCppSymbolTable(const std::vector<std::string>& lines) {
        SymbolTable table;
        
        std::regex classRe(R"(\bclass\s+(\w+))");
        std::regex structRe(R"(\bstruct\s+(\w+))");
        std::regex enumRe(R"(\benum\s+(?:class\s+)?(\w+))");
        std::regex funcRe(R"(\b(\w+)\s*\([^)]*\)\s*(?:const\s*)?\{)");
        std::regex namespaceRe(R"(\bnamespace\s+(\w+))");
        std::regex macroRe(R"(^\s*#define\s+(\w+))");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            const auto& line = lines[lineNum];
            std::smatch match;
            
            auto addEntry = [&](const std::string& name, const std::string& kind, size_t col) {
                SymbolEntry entry;
                entry.name = name;
                entry.kind = kind;
                entry.line = (int)lineNum;
                entry.column = (int)col;
                entry.isDefinition = true;
                table.symbols.push_back(entry);
            };
            
            if (std::regex_search(line, match, classRe))
                addEntry(match[1].str(), "class", match.position(1));
            if (std::regex_search(line, match, structRe))
                addEntry(match[1].str(), "struct", match.position(1));
            if (std::regex_search(line, match, enumRe))
                addEntry(match[1].str(), "enum", match.position(1));
            if (std::regex_search(line, match, namespaceRe))
                addEntry(match[1].str(), "namespace", match.position(1));
            if (std::regex_search(line, match, macroRe))
                addEntry(match[1].str(), "macro", match.position(1));
            
            std::string::const_iterator searchStart(line.cbegin());
            while (std::regex_search(searchStart, line.cend(), match, funcRe)) {
                SymbolEntry entry;
                entry.name = match[1].str();
                entry.kind = "function";
                entry.line = (int)lineNum;
                entry.column = (int)(match.position(1) + (searchStart - line.cbegin()));
                entry.isDefinition = true;
                table.symbols.push_back(entry);
                searchStart = match.suffix().first;
            }
        }
        
        return table;
    }
    
    SymbolTable buildPythonSymbolTable(const std::vector<std::string>& lines) {
        SymbolTable table;
        std::regex classRe(R"(^\s*class\s+(\w+))");
        std::regex funcRe(R"(^\s*def\s+(\w+))");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            const auto& line = lines[lineNum];
            std::smatch match;
            
            if (std::regex_search(line, match, classRe)) {
                SymbolEntry e; e.name = match[1]; e.kind = "class";
                e.line = (int)lineNum; e.column = (int)match.position(1);
                table.symbols.push_back(e);
            }
            if (std::regex_search(line, match, funcRe)) {
                SymbolEntry e; e.name = match[1]; e.kind = "function";
                e.line = (int)lineNum; e.column = (int)match.position(1);
                table.symbols.push_back(e);
            }
        }
        return table;
    }
    
    SymbolTable buildJsSymbolTable(const std::vector<std::string>& lines) {
        SymbolTable table;
        std::regex classRe(R"(^\s*class\s+(\w+))");
        std::regex funcRe(R"(\bfunction\s+(\w+))");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            const auto& line = lines[lineNum];
            std::smatch match;
            
            if (std::regex_search(line, match, classRe)) {
                SymbolEntry e; e.name = match[1]; e.kind = "class";
                e.line = (int)lineNum; e.column = (int)match.position(1);
                table.symbols.push_back(e);
            }
            if (std::regex_search(line, match, funcRe)) {
                SymbolEntry e; e.name = match[1]; e.kind = "function";
                e.line = (int)lineNum; e.column = (int)match.position(1);
                table.symbols.push_back(e);
            }
        }
        return table;
    }
};

// ============================================================================
// SECTION 4: DOCUMENT LINKS
// ============================================================================

struct DocumentLink {
    std::string file;
    int line;
    int startColumn;
    int endColumn;
    std::string target;
    std::string tooltip;
};

class DocumentLinkProvider {
public:
    std::vector<DocumentLink> links;
    
    std::vector<DocumentLink> findLinks(const std::vector<std::string>& lines,
                                         const std::string& filePath) {
        links.clear();
        std::string language = detectLanguage(filePath);
        
        if (language == "cpp" || language == "c") findCppLinks(lines, filePath);
        else if (language == "python") findPythonLinks(lines, filePath);
        else if (language == "javascript" || language == "typescript") findJsLinks(lines, filePath);
        else if (language == "markdown") findMarkdownLinks(lines, filePath);
        
        return links;
    }
    
    DocumentLink* getLinkAt(int line, int column) {
        for (auto& link : links) {
            if (link.line == line &&
                column >= link.startColumn && column <= link.endColumn) {
                return &link;
            }
        }
        return nullptr;
    }
    
private:
    std::string detectLanguage(const std::string& path) {
        size_t dot = path.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = path.substr(dot);
            if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "cpp";
            if (ext == ".c" || ext == ".h") return "c";
            if (ext == ".py") return "python";
            if (ext == ".js" || ext == ".mjs") return "javascript";
            if (ext == ".ts" || ext == ".tsx") return "typescript";
            if (ext == ".md") return "markdown";
        }
        return "plaintext";
    }
    
    void findCppLinks(const std::vector<std::string>& lines, const std::string& filePath) {
        std::regex includeRe(R"(#include\s*[<"]([^>"]+)[>"])");
        std::string baseDir = filePath.substr(0, filePath.find_last_of("/\\"));
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::smatch match;
            if (std::regex_search(lines[lineNum], match, includeRe)) {
                DocumentLink link;
                link.file = filePath;
                link.line = (int)lineNum;
                link.startColumn = (int)match.position(1);
                link.endColumn = (int)(match.position(1) + match[1].length());
                link.target = baseDir + "/" + match[1].str();
                link.tooltip = "Go to " + match[1].str();
                links.push_back(link);
            }
        }
    }
    
    void findPythonLinks(const std::vector<std::string>& lines, const std::string& filePath) {
        std::regex importRe(R"(import\s+(\w+))");
        std::regex fromRe(R"(from\s+([\w.]+)\s+import)");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::smatch match;
            if (std::regex_search(lines[lineNum], match, importRe)) {
                DocumentLink link;
                link.file = filePath;
                link.line = (int)lineNum;
                link.startColumn = (int)match.position(1);
                link.endColumn = (int)(match.position(1) + match[1].length());
                link.target = match[1].str() + ".py";
                link.tooltip = "Go to " + match[1].str();
                links.push_back(link);
            }
            if (std::regex_search(lines[lineNum], match, fromRe)) {
                DocumentLink link;
                link.file = filePath;
                link.line = (int)lineNum;
                link.startColumn = (int)match.position(1);
                link.endColumn = (int)(match.position(1) + match[1].length());
                link.target = match[1].str();
                std::replace(link.target.begin(), link.target.end(), '.', '/');
                link.target += ".py";
                link.tooltip = "Go to " + match[1].str();
                links.push_back(link);
            }
        }
    }
    
    void findJsLinks(const std::vector<std::string>& lines, const std::string& filePath) {
        std::regex importRe(R"(import\s+.*from\s+['"]([^'"]+)['"])");
        std::regex requireRe(R"(require\s*\(\s*['"]([^'"]+)['"]\s*\))");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::smatch match;
            if (std::regex_search(lines[lineNum], match, importRe)) {
                DocumentLink link;
                link.file = filePath;
                link.line = (int)lineNum;
                link.startColumn = (int)match.position(1);
                link.endColumn = (int)(match.position(1) + match[1].length());
                link.target = resolveModule(match[1].str(), filePath);
                link.tooltip = "Go to " + match[1].str();
                links.push_back(link);
            }
            if (std::regex_search(lines[lineNum], match, requireRe)) {
                DocumentLink link;
                link.file = filePath;
                link.line = (int)lineNum;
                link.startColumn = (int)match.position(1);
                link.endColumn = (int)(match.position(1) + match[1].length());
                link.target = resolveModule(match[1].str(), filePath);
                link.tooltip = "Go to " + match[1].str();
                links.push_back(link);
            }
        }
    }
    
    void findMarkdownLinks(const std::vector<std::string>& lines, const std::string& filePath) {
        std::regex linkRe(R"(\[([^\]]*)\]\(([^)]+)\))");
        std::string baseDir = filePath.substr(0, filePath.find_last_of("/\\"));
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::string::const_iterator searchStart(lines[lineNum].cbegin());
            std::smatch match;
            
            while (std::regex_search(searchStart, lines[lineNum].cend(), match, linkRe)) {
                DocumentLink link;
                link.file = filePath;
                link.line = (int)lineNum;
                link.startColumn = (int)(match.position(2) + (searchStart - lines[lineNum].cbegin()));
                link.endColumn = link.startColumn + (int)match[2].length();
                link.target = baseDir + "/" + match[2].str();
                link.tooltip = "Go to " + match[2].str();
                links.push_back(link);
                searchStart = match.suffix().first;
            }
        }
    }
    
    std::string resolveModule(const std::string& module, const std::string& filePath) {
        std::string baseDir = filePath.substr(0, filePath.find_last_of("/\\"));
        if (!module.empty() && module[0] == '.') {
            return baseDir + "/" + module + (module.find('.') == std::string::npos ? ".js" : "");
        }
        return "node_modules/" + module;
    }
};

// ============================================================================
// SECTION 5: HOVER PROVIDER
// ============================================================================

struct HoverInfo {
    std::string contents;
    std::string kind;  // markdown, plaintext
    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    std::string source;
};

class HoverProvider {
public:
    HoverInfo getHover(const std::vector<std::string>& lines, int line, int column,
                        const std::string& language) {
        HoverInfo info;
        
        if (line < 0 || line >= (int)lines.size()) return info;
        
        const auto& currentLine = lines[line];
        int start = column, end = column;
        
        while (start > 0 && (std::isalnum((unsigned char)currentLine[start-1]) || currentLine[start-1] == '_')) start--;
        while (end < (int)currentLine.length() && (std::isalnum((unsigned char)currentLine[end]) || currentLine[end] == '_')) end++;
        
        std::string word = currentLine.substr(start, end - start);
        if (word.empty()) return info;
        
        if (language == "cpp" || language == "c") info = getCppHover(lines, line, start, word);
        else if (language == "python") info = getPythonHover(lines, line, start, word);
        else if (language == "javascript" || language == "typescript") info = getJsHover(lines, line, start, word);
        
        info.startLine = line; info.startColumn = start;
        info.endLine = line;   info.endColumn = end;
        
        return info;
    }
    
private:
    HoverInfo getCppHover(const std::vector<std::string>& lines, int line, int col, const std::string& word) {
        HoverInfo info;
        
        static const std::map<std::string, std::string> cppDocs = {
            {"std::vector",  "```cpp\ntemplate<class T>\nclass vector;\n```\n\nDynamic array container."},
            {"std::string",  "```cpp\nusing string = std::basic_string<char>;\n```\n\nSequence of characters."},
            {"std::map",     "```cpp\ntemplate<class Key, class T>\nclass map;\n```\n\nSorted key-value pairs."},
            {"std::cout",    "```cpp\nextern std::ostream cout;\n```\n\nStandard output stream."},
            {"nullptr",      "The null pointer literal."},
            {"auto",         "Type deduction specifier."},
            {"constexpr",    "Specifies compile-time constant."},
        };
        
        auto it = cppDocs.find(word);
        if (it != cppDocs.end()) {
            info.contents = it->second;
            info.kind = "markdown";
            info.source = "cpp-reference";
            return info;
        }
        
        for (int i = line; i >= 0; i--) {
            if (lines[i].find(word + "(") != std::string::npos) {
                info.contents = "```cpp\n" + lines[i] + "\n```\n\n*Defined at line " + std::to_string(i + 1) + "*";
                info.kind = "markdown";
                info.source = "local";
                return info;
            }
        }
        
        info.contents = "**" + word + "**";
        info.kind = "markdown";
        return info;
    }
    
    HoverInfo getPythonHover(const std::vector<std::string>& lines, int line, int col, const std::string& word) {
        HoverInfo info;
        
        static const std::map<std::string, std::string> pyDocs = {
            {"print", "```python\nprint(*objects, sep=' ', end='\\n')\n```\n\nPrint to text stream."},
            {"len",   "```python\nlen(s) -> int\n```\n\nReturn length of object."},
            {"range", "```python\nrange(stop) | range(start, stop[, step])\n```\n\nReturn range object."},
            {"str",   "```python\nstr(object='') -> str\n```\n\nString conversion."},
            {"int",   "```python\nint(x=0) -> int\n```\n\nInteger conversion."},
            {"list",  "```python\nlist(iterable=()) -> list\n```\n\nCreate list."},
            {"dict",  "```python\ndict(**kwargs) -> dict\n```\n\nCreate dictionary."},
        };
        
        auto it = pyDocs.find(word);
        if (it != pyDocs.end()) {
            info.contents = it->second;
            info.kind = "markdown";
            info.source = "python-docs";
            return info;
        }
        
        for (int i = line; i >= 0; i--) {
            if (lines[i].find("def " + word) != std::string::npos) {
                info.contents = "```python\n" + lines[i] + "\n```\n\n*Defined at line " + std::to_string(i + 1) + "*";
                info.kind = "markdown";
                return info;
            }
        }
        
        info.contents = "**" + word + "**";
        info.kind = "markdown";
        return info;
    }
    
    HoverInfo getJsHover(const std::vector<std::string>& lines, int line, int col, const std::string& word) {
        HoverInfo info;
        
        static const std::map<std::string, std::string> jsDocs = {
            {"console",     "Browser debugging console object."},
            {"Promise",     "```javascript\nnew Promise(executor)\n```\n\nAsync operation result."},
            {"async",       "Declares async function returning a Promise."},
            {"await",       "Pauses execution until Promise resolves."},
            {"fetch",       "```javascript\nfetch(resource, options)\n```\n\nFetch network resource."},
        };
        
        auto it = jsDocs.find(word);
        if (it != jsDocs.end()) {
            info.contents = it->second;
            info.kind = "markdown";
            info.source = "mdn";
            return info;
        }
        
        info.contents = "**" + word + "**";
        info.kind = "markdown";
        return info;
    }
};

// ============================================================================
// SECTION 6: SIGNATURE HELP
// ============================================================================

struct ParameterInfo {
    std::string label;
    std::string documentation;
};

struct SignatureInfo {
    std::string label;
    std::string documentation;
    std::vector<ParameterInfo> parameters;
    int activeParameter = 0;
};

class SignatureHelpProvider {
public:
    std::vector<SignatureInfo> signatures;
    int activeSignature = 0;
    bool visible = false;
    
    std::vector<SignatureInfo> getSignatures(const std::vector<std::string>& lines,
                                              int line, int column,
                                              const std::string& language) {
        signatures.clear();
        if (line < 0 || line >= (int)lines.size()) return signatures;
        
        std::string functionName = findFunctionCall(lines, line, column);
        if (functionName.empty()) return signatures;
        
        signatures = lookupSignatures(functionName, language);
        
        if (!signatures.empty()) {
            visible = true;
            activeSignature = 0;
            signatures[0].activeParameter = getActiveParameter(lines, line, column);
        }
        
        return signatures;
    }
    
    SignatureInfo* getCurrentSignature() {
        if (activeSignature >= 0 && activeSignature < (int)signatures.size())
            return &signatures[activeSignature];
        return nullptr;
    }
    
    void nextSignature() {
        if (!signatures.empty()) activeSignature = (activeSignature + 1) % (int)signatures.size();
    }
    
    void prevSignature() {
        if (!signatures.empty()) activeSignature = (activeSignature == 0) ? (int)signatures.size() - 1 : activeSignature - 1;
    }
    
    void hide() { visible = false; signatures.clear(); }
    
private:
    std::string findFunctionCall(const std::vector<std::string>& lines, int line, int column) {
        int depth = 0;
        for (int l = line; l >= 0 && l >= line - 5; l--) {
            const auto& ln = lines[l];
            int start = (l == line) ? column - 1 : (int)ln.length() - 1;
            for (int c = start; c >= 0; c--) {
                if (ln[c] == ')') depth++;
                else if (ln[c] == '(') {
                    if (depth == 0) {
                        int nameEnd = c, nameStart = nameEnd;
                        while (nameStart > 0 && (std::isalnum((unsigned char)ln[nameStart-1]) || ln[nameStart-1] == '_'))
                            nameStart--;
                        return ln.substr(nameStart, nameEnd - nameStart);
                    }
                    depth--;
                }
            }
        }
        return "";
    }
    
    std::vector<SignatureInfo> lookupSignatures(const std::string& func, const std::string& lang) {
        static const std::map<std::string, std::vector<SignatureInfo>> pySignatures = {
            {"print", {{"print(*objects, sep=' ', end='\\n', file=sys.stdout, flush=False)",
                        "Print to text stream.",
                        {{"objects", "Objects to print"}, {"sep", "Separator"}, {"end", "Ending"}, {"file", "Stream"}, {"flush", "Flush?"}}}}},
            {"range", {{"range(stop)", "Range 0..stop", {{"stop", "End value"}}},
                       {"range(start, stop[, step])", "Range with step", {{"start", "Start"}, {"stop", "End"}, {"step", "Step"}}}}},
            {"len",   {{"len(s)", "Return length.", {{"s", "Object"}}}}},
        };
        
        static const std::map<std::string, std::vector<SignatureInfo>> jsSignatures = {
            {"console.log", {{"console.log(...data)", "Output to console.", {{"data", "Data to log"}}}}},
            {"fetch",       {{"fetch(resource)", "Fetch resource.", {{"resource", "URL"}}},
                             {"fetch(resource, options)", "Fetch with options.", {{"resource", "URL"}, {"options", "Options"}}}}},
        };
        
        if (lang == "python") {
            auto it = pySignatures.find(func);
            if (it != pySignatures.end()) return it->second;
        } else if (lang == "javascript" || lang == "typescript") {
            auto it = jsSignatures.find(func);
            if (it != jsSignatures.end()) return it->second;
        }
        
        return {};
    }
    
    int getActiveParameter(const std::vector<std::string>& lines, int line, int column) {
        int paramIndex = 0;
        int parenDepth = 0;
        
        for (int l = line; l >= std::max(0, line - 5); l--) {
            const auto& ln = lines[l];
            int start = (l == line) ? column : (int)ln.length();
            
            for (int c = start - 1; c >= 0; c--) {
                char ch = ln[c];
                if (ch == '(') { parenDepth++; if (parenDepth == 1) return paramIndex; }
                else if (ch == ')') { parenDepth--; }
                else if (ch == ',' && parenDepth == 1) paramIndex++;
            }
        }
        
        return paramIndex;
    }
};

// ============================================================================
// SECTION 7: RENAME REFACTORING
// ============================================================================

struct RenameLocation {
    std::string file;
    int line;
    int column;
    int length;
    std::string oldText;
    std::string newText;
};

class RenameRefactoring {
public:
    std::string oldName;
    std::string newName;
    std::vector<RenameLocation> locations;
    bool valid = false;
    
    bool prepareRename(const std::vector<std::string>& lines, int line, int column,
                       std::string& placeholder, int& rangeStart, int& rangeEnd) {
        if (line < 0 || line >= (int)lines.size()) return false;
        
        const auto& ln = lines[line];
        int start = column, end = column;
        
        while (start > 0 && (std::isalnum((unsigned char)ln[start-1]) || ln[start-1] == '_')) start--;
        while (end < (int)ln.length() && (std::isalnum((unsigned char)ln[end]) || ln[end] == '_')) end++;
        
        if (start == end) return false;
        
        oldName = ln.substr(start, end - start);
        placeholder = oldName;
        rangeStart = start;
        rangeEnd = end;
        return true;
    }
    
    std::vector<RenameLocation> findReferences(const std::vector<std::string>& lines,
                                                const std::string& file, int line, int column) {
        locations.clear();
        
        for (size_t l = 0; l < lines.size(); l++) {
            const auto& ln = lines[l];
            size_t pos = 0;
            
            while ((pos = ln.find(oldName, pos)) != std::string::npos) {
                bool validStart = (pos == 0 || (!std::isalnum((unsigned char)ln[pos-1]) && ln[pos-1] != '_'));
                bool validEnd   = (pos + oldName.length() >= ln.length() ||
                                   (!std::isalnum((unsigned char)ln[pos + oldName.length()]) && ln[pos + oldName.length()] != '_'));
                
                if (validStart && validEnd) {
                    RenameLocation loc;
                    loc.file = file;
                    loc.line = (int)l;
                    loc.column = (int)pos;
                    loc.length = (int)oldName.length();
                    loc.oldText = oldName;
                    loc.newText = newName;
                    locations.push_back(loc);
                }
                
                pos += oldName.length();
            }
        }
        
        return locations;
    }
    
    bool applyRename(std::vector<std::string>& lines) {
        if (!valid || newName.empty()) return false;
        
        std::sort(locations.begin(), locations.end(),
            [](const RenameLocation& a, const RenameLocation& b) {
                if (a.line != b.line) return a.line > b.line;
                return a.column > b.column;
            });
        
        for (const auto& loc : locations) {
            if (loc.line >= 0 && loc.line < (int)lines.size()) {
                lines[loc.line].replace(loc.column, loc.length, newName);
            }
        }
        
        return true;
    }
    
    int getChangeCount() const { return (int)locations.size(); }
};

// ============================================================================
// SECTION 8: EXTRACT METHOD/VARIABLE
// ============================================================================

struct ExtractedCode {
    std::string extractedCode;
    std::string declaration;
    std::vector<std::string> parameters;
    std::vector<std::pair<int, int>> usages;
};

class ExtractRefactoring {
public:
    ExtractedCode extractMethod(const std::vector<std::string>& lines,
                                  int startLine, int startCol,
                                  int endLine, int endCol,
                                  const std::string& language) {
        ExtractedCode result;
        
        for (int l = startLine; l <= endLine; l++) {
            if (l < 0 || l >= (int)lines.size()) continue;
            const auto& ln = lines[l];
            int s = (l == startLine) ? startCol : 0;
            int e = (l == endLine)   ? endCol   : (int)ln.length();
            if (l > startLine) result.extractedCode += "\n";
            result.extractedCode += ln.substr(s, e - s);
        }
        
        result.parameters = findUsedVariables(result.extractedCode, lines, startLine, language);
        
        std::string methodName = "extractedMethod";
        std::string paramList;
        for (size_t i = 0; i < result.parameters.size(); i++) {
            if (i > 0) paramList += ", ";
            paramList += result.parameters[i];
        }
        
        if (language == "cpp" || language == "c") {
            result.declaration = "void " + methodName + "(" + paramList + ") {\n    " + result.extractedCode + "\n}";
        } else if (language == "python") {
            result.declaration = "def " + methodName + "(" + paramList + "):\n    " + result.extractedCode + "\n";
        } else {
            result.declaration = "function " + methodName + "(" + paramList + ") {\n    " + result.extractedCode + "\n}";
        }
        
        return result;
    }
    
    ExtractedCode extractVariable(const std::vector<std::string>& lines,
                                    int line, int startCol, int endCol,
                                    const std::string& language) {
        ExtractedCode result;
        if (line < 0 || line >= (int)lines.size()) return result;
        
        std::string expression = lines[line].substr(startCol, endCol - startCol);
        result.extractedCode = expression;
        
        std::string varName = "extracted";
        if (language == "cpp" || language == "c") result.declaration = "auto " + varName + " = " + expression + ";";
        else if (language == "python") result.declaration = varName + " = " + expression;
        else result.declaration = "const " + varName + " = " + expression + ";";
        
        result.usages = findExpressionUsages(lines, expression, line);
        return result;
    }
    
private:
    std::vector<std::string> findUsedVariables(const std::string& code,
                                                 const std::vector<std::string>& lines,
                                                 int contextLine,
                                                 const std::string& language) {
        std::vector<std::string> vars;
        std::set<std::string> seen;
        std::regex identRe(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
        std::smatch match;
        std::string::const_iterator searchStart(code.cbegin());
        
        while (std::regex_search(searchStart, code.cend(), match, identRe)) {
            std::string name = match[1].str();
            if (!isKeyword(name, language) && seen.find(name) == seen.end()) {
                for (int l = 0; l < contextLine && l < (int)lines.size(); l++) {
                    if (lines[l].find(name + " =") != std::string::npos ||
                        lines[l].find(name + "(") != std::string::npos) {
                        vars.push_back(name);
                        seen.insert(name);
                        break;
                    }
                }
            }
            searchStart = match.suffix().first;
        }
        return vars;
    }
    
    std::vector<std::pair<int, int>> findExpressionUsages(const std::vector<std::string>& lines,
                                                           const std::string& expr, int startLine) {
        std::vector<std::pair<int, int>> usages;
        for (size_t l = startLine; l < lines.size(); l++) {
            size_t pos = 0;
            while ((pos = lines[l].find(expr, pos)) != std::string::npos) {
                usages.push_back({(int)l, (int)pos});
                pos += expr.length();
            }
        }
        return usages;
    }
    
    bool isKeyword(const std::string& word, const std::string& lang) {
        static std::set<std::string> keywords = {
            "if", "else", "for", "while", "do", "switch", "case", "break",
            "continue", "return", "class", "struct", "void", "int", "char",
            "bool", "true", "false", "null", "this", "new", "delete", "def",
            "import", "from", "as", "async", "await", "function", "const",
            "let", "var", "auto", "static", "public", "private", "protected"
        };
        return keywords.count(word) > 0;
    }
};

// ============================================================================
// SECTION 9: ORGANIZE IMPORTS
// ============================================================================

struct ImportStatement {
    std::string fullStatement;
    std::string module;
    std::vector<std::string> symbols;
    int line;
    bool isUsed = false;
};

class OrganizeImports {
public:
    std::vector<ImportStatement> imports;
    std::set<std::string> usedSymbols;
    
    std::vector<std::string> organize(const std::vector<std::string>& lines,
                                       const std::string& language) {
        imports.clear();
        usedSymbols.clear();
        
        imports = parseImports(lines, language);
        usedSymbols = findUsedSymbols(lines, language);
        
        for (auto& imp : imports) {
            for (const auto& sym : imp.symbols) {
                if (usedSymbols.count(sym)) { imp.isUsed = true; break; }
            }
        }
        
        return generateOrganizedImports(language);
    }
    
private:
    std::vector<ImportStatement> parseImports(const std::vector<std::string>& lines,
                                                const std::string& language) {
        std::vector<ImportStatement> result;
        std::regex cppIncludeRe(R"(#include\s*[<"]([^>"]+)[>"])");
        std::regex pyImportRe(R"(import\s+(\w+))");
        std::regex pyFromRe(R"(from\s+([\w.]+)\s+import\s+(.+))");
        std::regex jsImportRe(R"(import\s+(?:\{([^}]+)\}|(\w+))\s+from\s+['"]([^'"]+)['"])");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            const auto& line = lines[lineNum];
            std::smatch match;
            
            if ((language == "cpp" || language == "c") && std::regex_search(line, match, cppIncludeRe)) {
                ImportStatement imp;
                imp.fullStatement = line;
                imp.module = match[1].str();
                imp.line = (int)lineNum;
                imp.symbols.push_back(imp.module);
                result.push_back(imp);
            } else if (language == "python") {
                if (std::regex_search(line, match, pyImportRe)) {
                    ImportStatement imp;
                    imp.fullStatement = line;
                    imp.module = match[1].str();
                    imp.line = (int)lineNum;
                    imp.symbols.push_back(match[1].str());
                    result.push_back(imp);
                } else if (std::regex_search(line, match, pyFromRe)) {
                    ImportStatement imp;
                    imp.fullStatement = line;
                    imp.module = match[1].str();
                    imp.line = (int)lineNum;
                    std::stringstream ss(match[2].str());
                    std::string sym;
                    while (std::getline(ss, sym, ',')) {
                        sym.erase(0, sym.find_first_not_of(" \t"));
                        sym.erase(sym.find_last_not_of(" \t") + 1);
                        imp.symbols.push_back(sym);
                    }
                    result.push_back(imp);
                }
            } else if ((language == "javascript" || language == "typescript") && std::regex_search(line, match, jsImportRe)) {
                ImportStatement imp;
                imp.fullStatement = line;
                imp.module = match[3].str();
                imp.line = (int)lineNum;
                if (match[1].matched) {
                    std::stringstream ss(match[1].str());
                    std::string sym;
                    while (std::getline(ss, sym, ',')) {
                        sym.erase(0, sym.find_first_not_of(" \t"));
                        sym.erase(sym.find_last_not_of(" \t") + 1);
                        imp.symbols.push_back(sym);
                    }
                } else {
                    imp.symbols.push_back(match[2].str());
                }
                result.push_back(imp);
            }
        }
        return result;
    }
    
    std::set<std::string> findUsedSymbols(const std::vector<std::string>& lines, const std::string& language) {
        std::set<std::string> symbols;
        std::regex identRe(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
        
        // Skip import section
        size_t startLine = 0;
        for (size_t i = 0; i < lines.size(); i++) {
            if (isImportLine(lines[i], language)) startLine = i + 1;
            else if (!lines[i].empty()) break;
        }
        
        for (size_t i = startLine; i < lines.size(); i++) {
            std::smatch match;
            std::string::const_iterator searchStart(lines[i].cbegin());
            while (std::regex_search(searchStart, lines[i].cend(), match, identRe)) {
                symbols.insert(match[1].str());
                searchStart = match.suffix().first;
            }
        }
        return symbols;
    }
    
    bool isImportLine(const std::string& line, const std::string& language) {
        if (language == "cpp" || language == "c") return line.find("#include") == 0;
        if (language == "python") return line.find("import ") == 0 || line.find("from ") == 0;
        if (language == "javascript" || language == "typescript")
            return line.find("import ") == 0 || line.find("require(") != std::string::npos;
        return false;
    }
    
    std::vector<std::string> generateOrganizedImports(const std::string& language) {
        std::vector<ImportStatement*> used;
        for (auto& imp : imports) {
            if (imp.isUsed) used.push_back(&imp);
        }
        
        std::sort(used.begin(), used.end(),
            [](ImportStatement* a, ImportStatement* b) { return a->module < b->module; });
        
        std::vector<std::string> result;
        for (auto* imp : used) result.push_back(imp->fullStatement);
        return result;
    }
};

// ============================================================================
// SECTION 10: CODE LENS
// ============================================================================

struct CodeLens {
    std::string file;
    int line;
    int startColumn;
    int endColumn;
    std::string command;
    std::string title;
    std::string arguments;
    bool resolved = false;
};

class CodeLensProvider {
public:
    std::vector<CodeLens> lenses;
    
    std::vector<CodeLens> provideCodeLenses(const std::vector<std::string>& lines,
                                              const std::string& language,
                                              const std::string& filePath) {
        lenses.clear();
        addReferenceCountLenses(lines, language, filePath);
        addTestLenses(lines, language, filePath);
        addGitLenses(lines, filePath);
        return lenses;
    }
    
    CodeLens* getLensAt(int line) {
        for (auto& lens : lenses) {
            if (lens.line == line) return &lens;
        }
        return nullptr;
    }
    
private:
    void addReferenceCountLenses(const std::vector<std::string>& lines,
                                   const std::string& language, const std::string& filePath) {
        std::regex classRe(R"(\bclass\s+(\w+))");
        std::regex funcRe(R"(\b(def|function|fn|func)\s+(\w+)\s*\()");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::smatch match;
            if (std::regex_search(lines[lineNum], match, classRe) ||
                std::regex_search(lines[lineNum], match, funcRe)) {
                CodeLens lens;
                lens.file = filePath;
                lens.line = (int)lineNum;
                lens.command = "references";
                lens.title = "0 references";
                lenses.push_back(lens);
            }
        }
    }
    
    void addTestLenses(const std::vector<std::string>& lines,
                        const std::string& language, const std::string& filePath) {
        if (filePath.find("test") == std::string::npos) return;
        
        std::regex testRe(R"((TEST|test_|def test_)|(it|describe)\s*\()");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            if (std::regex_search(lines[lineNum], testRe)) {
                CodeLens runLens;
                runLens.file = filePath; runLens.line = (int)lineNum;
                runLens.command = "test.run"; runLens.title = "Run Test";
                lenses.push_back(runLens);
                
                CodeLens dbgLens;
                dbgLens.file = filePath; dbgLens.line = (int)lineNum;
                dbgLens.command = "test.debug"; dbgLens.title = "Debug Test";
                lenses.push_back(dbgLens);
            }
        }
    }
    
    void addGitLenses(const std::vector<std::string>& lines, const std::string& filePath) {
        CodeLens lens;
        lens.file = filePath; lens.line = 0;
        lens.command = "git.blame"; lens.title = "Show Blame";
        lenses.push_back(lens);
    }
};

// ============================================================================
// SECTION 11: INLAY HINTS
// ============================================================================

struct InlayHint {
    std::string file;
    int line;
    int column;
    std::string text;
    std::string kind;  // type, parameter
    std::string tooltip;
    bool paddingLeft = false;
    bool paddingRight = false;
    uint32_t color = 0xFF808080;
};

class InlayHintsProvider {
public:
    bool enabled = true;
    bool showTypeHints = true;
    bool showParameterHints = true;
    std::vector<InlayHint> hints;
    
    std::vector<InlayHint> provideHints(const std::vector<std::string>& lines,
                                         const std::string& language,
                                         const std::string& filePath) {
        hints.clear();
        if (!enabled) return hints;
        if (showTypeHints)     addTypeHints(lines, language, filePath);
        if (showParameterHints) addParameterHints(lines, language, filePath);
        return hints;
    }
    
private:
    void addTypeHints(const std::vector<std::string>& lines,
                       const std::string& language, const std::string& filePath) {
        if (language == "cpp" || language == "c") {
            std::regex autoRe(R"(\bauto\s+(\w+)\s*=\s*([^;]+);)");
            for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
                std::smatch match;
                if (std::regex_search(lines[lineNum], match, autoRe)) {
                    InlayHint hint;
                    hint.file = filePath;
                    hint.line = (int)lineNum;
                    hint.column = (int)(match.position(1) + match[1].length());
                    hint.text = ": " + inferCppType(match[2].str());
                    hint.kind = "type";
                    hint.paddingLeft = true;
                    hints.push_back(hint);
                }
            }
        }
    }
    
    void addParameterHints(const std::vector<std::string>& lines,
                            const std::string& language, const std::string& filePath) {
        std::regex callRe(R"((\w+)\s*\(([^)]+)\))");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::smatch match;
            std::string::const_iterator searchStart(lines[lineNum].cbegin());
            
            while (std::regex_search(searchStart, lines[lineNum].cend(), match, callRe)) {
                std::string funcName = match[1].str();
                std::string args = match[2].str();
                
                if (args.find("=") == std::string::npos) {
                    auto paramNames = getFunctionParams(funcName, language);
                    auto argList = splitArgs(args);
                    
                    for (size_t i = 0; i < argList.size() && i < paramNames.size(); i++) {
                        if (!argList[i].empty() && argList[i][0] != '"') {
                            InlayHint hint;
                            hint.file = filePath;
                            hint.line = (int)lineNum;
                            hint.column = (int)(match.position(2) + (searchStart - lines[lineNum].cbegin()));
                            for (size_t j = 0; j < i; j++) hint.column += (int)argList[j].length() + 2;
                            hint.text = paramNames[i] + ":";
                            hint.kind = "parameter";
                            hint.paddingRight = true;
                            hints.push_back(hint);
                        }
                    }
                }
                searchStart = match.suffix().first;
            }
        }
    }
    
    std::string inferCppType(const std::string& expr) {
        if (expr.find("\"") != std::string::npos) return "std::string";
        if (expr.find("std::vector") != std::string::npos) return "std::vector";
        if (expr.find("std::map") != std::string::npos) return "std::map";
        if (expr.find("std::make_shared") != std::string::npos) return "std::shared_ptr";
        if (expr.find("std::make_unique") != std::string::npos) return "std::unique_ptr";
        if (expr.find("true") != std::string::npos || expr.find("false") != std::string::npos) return "bool";
        if (expr.find(".") != std::string::npos) return "double";
        if (expr.find_first_of("0123456789") != std::string::npos) return "int";
        return "auto";
    }
    
    std::vector<std::string> getFunctionParams(const std::string& func, const std::string& lang) {
        static const std::map<std::string, std::vector<std::string>> params = {
            {"print", {"objects", "sep", "end", "file", "flush"}},
            {"range", {"start", "stop", "step"}},
            {"len",   {"s"}},
        };
        auto it = params.find(func);
        return it != params.end() ? it->second : std::vector<std::string>();
    }
    
    std::vector<std::string> splitArgs(const std::string& args) {
        std::vector<std::string> result;
        std::string current;
        int depth = 0;
        for (char c : args) {
            if (c == ',' && depth == 0) { result.push_back(current); current.clear(); }
            else { if (c == '(' || c == '[' || c == '{') depth++; else if (c == ')' || c == ']' || c == '}') depth--; current += c; }
        }
        if (!current.empty()) result.push_back(current);
        return result;
    }
};

// ============================================================================
// SECTION 12: COLOR DECORATIONS
// ============================================================================

struct ColorDecoration {
    std::string file;
    int line;
    int startColumn;
    int endColumn;
    uint32_t color;
    std::string format;
};

class ColorDecorationProvider {
public:
    std::vector<ColorDecoration> decorations;
    
    std::vector<ColorDecoration> findColors(const std::vector<std::string>& lines,
                                             const std::string& filePath) {
        decorations.clear();
        
        std::regex hexRe(R"(#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})(?:[0-9a-fA-F]{2})?)");
        std::regex rgbRe(R"(rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)(?:\s*,\s*[\d.]+)?\s*\))");
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            const auto& line = lines[lineNum];
            std::smatch match;
            std::string::const_iterator searchStart(line.cbegin());
            
            while (std::regex_search(searchStart, line.cend(), match, hexRe)) {
                ColorDecoration dec;
                dec.file = filePath;
                dec.line = (int)lineNum;
                dec.startColumn = (int)(match.position() + (searchStart - line.cbegin()));
                dec.endColumn = dec.startColumn + (int)match.length();
                dec.color = parseHexColor(match[1].str());
                dec.format = "hex";
                decorations.push_back(dec);
                searchStart = match.suffix().first;
            }
            
            searchStart = line.cbegin();
            while (std::regex_search(searchStart, line.cend(), match, rgbRe)) {
                ColorDecoration dec;
                dec.file = filePath;
                dec.line = (int)lineNum;
                dec.startColumn = (int)(match.position() + (searchStart - line.cbegin()));
                dec.endColumn = dec.startColumn + (int)match.length();
                dec.color = (255u << 24) | ((uint32_t)std::stoi(match[3].str()) << 16) |
                            ((uint32_t)std::stoi(match[2].str()) << 8) | (uint32_t)std::stoi(match[1].str());
                dec.format = "rgb";
                decorations.push_back(dec);
                searchStart = match.suffix().first;
            }
        }
        
        return decorations;
    }
    
    ColorDecoration* getColorAt(int line, int column) {
        for (auto& dec : decorations) {
            if (dec.line == line && column >= dec.startColumn && column <= dec.endColumn)
                return &dec;
        }
        return nullptr;
    }
    
    std::string formatColor(uint32_t color, const std::string& format) {
        uint8_t r = color & 0xFF;
        uint8_t g = (color >> 8) & 0xFF;
        uint8_t b = (color >> 16) & 0xFF;
        uint8_t a = (color >> 24) & 0xFF;
        
        char buf[16];
        if (format == "hex") {
            if (a == 255) snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
            else          snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", r, g, b, a);
            return buf;
        } else if (format == "rgb") {
            return "rgb(" + std::to_string(r) + ", " + std::to_string(g) + ", " + std::to_string(b) + ")";
        }
        return "";
    }
    
private:
    uint32_t parseHexColor(const std::string& hex) {
        std::string h = hex;
        if (h.length() == 3) {
            h = std::string(2, h[0]) + std::string(2, h[1]) + std::string(2, h[2]);
        }
        uint32_t r = std::stoul(h.substr(0, 2), nullptr, 16);
        uint32_t g = std::stoul(h.substr(2, 2), nullptr, 16);
        uint32_t b = std::stoul(h.substr(4, 2), nullptr, 16);
        return (255u << 24) | (b << 16) | (g << 8) | r;
    }
};

// ============================================================================
// SECTION 13: BOOKMARKS
// ============================================================================

struct Bookmark {
    std::string file;
    int line;
    int column;
    std::string label;
    std::string color;
    std::string annotation;
    std::chrono::system_clock::time_point created;
};

class BookmarkManager {
public:
    std::vector<Bookmark> bookmarks;
    
    void addBookmark(const std::string& file, int line, int column,
                     const std::string& label = "", const std::string& color = "blue") {
        Bookmark bm;
        bm.file = file; bm.line = line; bm.column = column;
        bm.label = label.empty() ? "Line " + std::to_string(line + 1) : label;
        bm.color = color;
        bm.created = std::chrono::system_clock::now();
        bookmarks.push_back(bm);
    }
    
    void removeBookmark(const std::string& file, int line) {
        bookmarks.erase(
            std::remove_if(bookmarks.begin(), bookmarks.end(),
                [&](const Bookmark& bm) { return bm.file == file && bm.line == line; }),
            bookmarks.end());
    }
    
    void toggleBookmark(const std::string& file, int line) {
        if (hasBookmark(file, line)) removeBookmark(file, line);
        else addBookmark(file, line, 0);
    }
    
    bool hasBookmark(const std::string& file, int line) {
        return std::any_of(bookmarks.begin(), bookmarks.end(),
            [&](const Bookmark& bm) { return bm.file == file && bm.line == line; });
    }
    
    Bookmark* getBookmark(const std::string& file, int line) {
        for (auto& bm : bookmarks) {
            if (bm.file == file && bm.line == line) return &bm;
        }
        return nullptr;
    }
    
    std::vector<Bookmark*> getBookmarksForFile(const std::string& file) {
        std::vector<Bookmark*> result;
        for (auto& bm : bookmarks) {
            if (bm.file == file) result.push_back(&bm);
        }
        return result;
    }
    
    Bookmark* getNextBookmark(const std::string& file, int currentLine) {
        for (auto& bm : bookmarks) {
            if (bm.file == file && bm.line > currentLine) return &bm;
        }
        for (auto& bm : bookmarks) {
            if (bm.file == file) return &bm;
        }
        return nullptr;
    }
    
    Bookmark* getPrevBookmark(const std::string& file, int currentLine) {
        for (auto it = bookmarks.rbegin(); it != bookmarks.rend(); ++it) {
            if (it->file == file && it->line < currentLine) return &(*it);
        }
        for (auto it = bookmarks.rbegin(); it != bookmarks.rend(); ++it) {
            if (it->file == file) return &(*it);
        }
        return nullptr;
    }
    
    void clearAllBookmarks() { bookmarks.clear(); }
    void clearFileBookmarks(const std::string& file) {
        bookmarks.erase(
            std::remove_if(bookmarks.begin(), bookmarks.end(),
                [&](const Bookmark& bm) { return bm.file == file; }),
            bookmarks.end());
    }
    
    void setAnnotation(const std::string& file, int line, const std::string& annotation) {
        if (auto* bm = getBookmark(file, line)) bm->annotation = annotation;
    }
};

// ============================================================================
// SECTION 14: TODO COMMENTS PARSER
// ============================================================================

struct TodoItem {
    std::string file;
    int line;
    int column;
    std::string text;
    std::string type;  // TODO, FIXME, HACK, XXX, NOTE
    std::string priority;
    std::string author;
    std::string description;
};

class TodoParser {
public:
    std::vector<TodoItem> todos;
    std::map<std::string, std::vector<TodoItem*>> byType;
    std::map<std::string, std::vector<TodoItem*>> byFile;
    
    std::vector<TodoItem> parseFile(const std::vector<std::string>& lines,
                                     const std::string& filePath) {
        std::vector<TodoItem> fileTodos;
        std::regex todoRe(R"((//|#|;)\s*(TODO|FIXME|HACK|XXX|NOTE|BUG)(?:\(([^)]+)\))?:?\s*(.+))",
                          std::regex_constants::icase);
        
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++) {
            std::smatch match;
            if (std::regex_search(lines[lineNum], match, todoRe)) {
                TodoItem item;
                item.file = filePath;
                item.line = (int)lineNum;
                item.column = (int)match.position(2);
                item.type = match[2].str();
                std::transform(item.type.begin(), item.type.end(), item.type.begin(), ::toupper);
                item.author = match[3].matched ? match[3].str() : "";
                item.text = match[4].str();
                item.priority = "medium";
                item.description = item.type + ": " + item.text;
                
                todos.push_back(item);
                fileTodos.push_back(item);
                byType[item.type].push_back(&todos.back());
                byFile[filePath].push_back(&todos.back());
            }
        }
        return fileTodos;
    }
    
    void clear() { todos.clear(); byType.clear(); byFile.clear(); }
    
    std::vector<TodoItem*> getTodosByType(const std::string& type) {
        auto it = byType.find(type);
        return it != byType.end() ? it->second : std::vector<TodoItem*>();
    }
    
    std::vector<TodoItem*> getTodosByFile(const std::string& file) {
        auto it = byFile.find(file);
        return it != byFile.end() ? it->second : std::vector<TodoItem*>();
    }
    
    int getCount() const { return (int)todos.size(); }
};

// ============================================================================
// SECTION 15: SPELL CHECKER
// ============================================================================

struct SpellingError {
    std::string word;
    int line;
    int column;
    int length;
    std::vector<std::string> suggestions;
};

class SpellChecker {
public:
    std::set<std::string> dictionary;
    std::set<std::string> userDictionary;
    std::set<std::string> ignoredWords;
    std::vector<SpellingError> errors;
    
    bool enabled = true;
    bool checkComments = true;
    bool checkStrings = true;
    bool checkIdentifiers = false;
    
    void addWord(const std::string& word) {
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        userDictionary.insert(lower);
    }
    
    void ignoreWord(const std::string& word) {
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        ignoredWords.insert(lower);
    }
    
    std::vector<SpellingError> check(const std::vector<std::string>& lines,
                                       const std::string& language) {
        errors.clear();
        if (!enabled) return errors;
        for (size_t lineNum = 0; lineNum < lines.size(); lineNum++)
            checkLine(lines[lineNum], (int)lineNum, language);
        return errors;
    }
    
    bool isCorrectWord(const std::string& word) {
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (dictionary.count(lower) || userDictionary.count(lower) || ignoredWords.count(lower))
            return true;
        return false;
    }
    
    std::vector<std::string> getSuggestions(const std::string& word) {
        std::vector<std::string> suggestions;
        std::string lower = word;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        for (const auto& dictWord : dictionary) {
            if (editDistance(lower, dictWord) <= 2) suggestions.push_back(dictWord);
            if (suggestions.size() >= 10) break;
        }
        return suggestions;
    }
    
private:
    void checkLine(const std::string& line, int lineNum, const std::string& language) {
        std::regex wordRe(R"(\b([a-zA-Z]{4,})\b)");  // 4+ char words only
        std::smatch match;
        std::string::const_iterator searchStart(line.cbegin());
        
        while (std::regex_search(searchStart, line.cend(), match, wordRe)) {
            std::string word = match[1].str();
            int column = (int)(match.position() + (searchStart - line.cbegin()));
            
            bool shouldCheck = (checkComments && isCommentContext(line, column)) ||
                               (checkStrings  && isStringContext(line, column))  ||
                               checkIdentifiers;
            
            if (shouldCheck && !isCorrectWord(word)) {
                SpellingError error;
                error.word = word; error.line = lineNum; error.column = column;
                error.length = (int)word.length();
                error.suggestions = getSuggestions(word);
                errors.push_back(error);
            }
            
            searchStart = match.suffix().first;
        }
    }
    
    bool isCommentContext(const std::string& line, int column) {
        size_t p = line.find("//");
        if (p != std::string::npos && column > (int)p) return true;
        p = line.find("#");
        if (p != std::string::npos && column >= (int)p) return true;
        return false;
    }
    
    bool isStringContext(const std::string& line, int column) {
        bool inString = false;
        for (int i = 0; i < column && i < (int)line.length(); i++) {
            if (line[i] == '"' || line[i] == '\'') inString = !inString;
        }
        return inString;
    }
    
    int editDistance(const std::string& a, const std::string& b) {
        std::vector<std::vector<int>> dp(a.length() + 1, std::vector<int>(b.length() + 1));
        for (size_t i = 0; i <= a.length(); i++) dp[i][0] = (int)i;
        for (size_t j = 0; j <= b.length(); j++) dp[0][j] = (int)j;
        for (size_t i = 1; i <= a.length(); i++) {
            for (size_t j = 1; j <= b.length(); j++) {
                dp[i][j] = (a[i-1] == b[j-1]) ? dp[i-1][j-1] :
                    1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
        return dp[a.length()][b.length()];
    }
};

// ============================================================================
// SECTION 16: CLIPBOARD HISTORY
// ============================================================================

struct ClipboardEntry {
    std::string text;
    std::chrono::system_clock::time_point time;
    std::string source;
    bool isCut = false;
    std::string preview;
};

class ClipboardHistory {
public:
    std::deque<ClipboardEntry> history;
    size_t maxSize = 50;
    size_t currentIndex = 0;
    
    void push(const std::string& text, bool isCut = false, const std::string& source = "") {
        ClipboardEntry entry;
        entry.text = text;
        entry.time = std::chrono::system_clock::now();
        entry.isCut = isCut;
        entry.source = source;
        entry.preview = text.substr(0, 100);
        if (text.length() > 100) entry.preview += "...";
        
        auto it = std::find_if(history.begin(), history.end(),
            [&](const ClipboardEntry& e) { return e.text == text; });
        if (it != history.end()) history.erase(it);
        
        history.push_front(entry);
        while (history.size() > maxSize) history.pop_back();
        currentIndex = 0;
    }
    
    std::string getCurrent() { return history.empty() ? "" : history[currentIndex].text; }
    
    std::string getNext() {
        if (history.empty()) return "";
        currentIndex = (currentIndex + 1) % history.size();
        return history[currentIndex].text;
    }
    
    std::string getPrev() {
        if (history.empty()) return "";
        currentIndex = (currentIndex == 0) ? history.size() - 1 : currentIndex - 1;
        return history[currentIndex].text;
    }
    
    std::string getAt(size_t index) { return index < history.size() ? history[index].text : ""; }
    
    void clear() { history.clear(); currentIndex = 0; }
    size_t size() const { return history.size(); }
    
    std::vector<ClipboardEntry*> getRecent(int count = 10) {
        std::vector<ClipboardEntry*> result;
        for (size_t i = 0; i < history.size() && i < (size_t)count; i++)
            result.push_back(&history[i]);
        return result;
    }
    
    void removeAt(size_t index) {
        if (index < history.size()) {
            history.erase(history.begin() + index);
            if (!history.empty() && currentIndex >= history.size())
                currentIndex = history.size() - 1;
        }
    }
};

// ============================================================================
// MAIN INTEGRATION CLASS - BATCH 3
// ============================================================================

class IDEFeatures3 {
public:
    AICompletionEngine      aiCompletion;
    IntelliSenseEngine      intellisense;
    SemanticHighlighter     semanticHighlighter;
    DocumentLinkProvider    documentLinks;
    HoverProvider           hoverProvider;
    SignatureHelpProvider   signatureHelp;
    RenameRefactoring       renameRefactor;
    ExtractRefactoring      extractRefactor;
    OrganizeImports         organizeImports;
    CodeLensProvider        codeLens;
    InlayHintsProvider      inlayHints;
    ColorDecorationProvider colorDecorations;
    BookmarkManager         bookmarks;
    TodoParser              todoParser;
    SpellChecker            spellChecker;
    ClipboardHistory        clipboard;
    
    void initialize() {
        loadDefaultDictionary();
    }
    
    void loadFile(const std::string& path, const std::vector<std::string>& lines) {
        std::string language = detectLanguage(path);
        
        documentLinks.findLinks(lines, path);
        colorDecorations.findColors(lines, path);
        todoParser.parseFile(lines, path);
        
        intellisense.documentLines = lines;
        intellisense.language = language;
        intellisense.filePath = path;
    }
    
    std::string detectLanguage(const std::string& path) {
        size_t dot = path.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = path.substr(dot);
            if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return "cpp";
            if (ext == ".c" || ext == ".h" || ext == ".hpp") return "c";
            if (ext == ".py") return "python";
            if (ext == ".js" || ext == ".mjs") return "javascript";
            if (ext == ".ts" || ext == ".tsx") return "typescript";
            if (ext == ".rs") return "rust";
            if (ext == ".go") return "go";
            if (ext == ".java") return "java";
            if (ext == ".md") return "markdown";
            if (ext == ".json") return "json";
        }
        return "plaintext";
    }
    
private:
    void loadDefaultDictionary() {
        std::vector<std::string> commonWords = {
            "the", "be", "to", "of", "and", "in", "that", "have", "it",
            "for", "not", "on", "with", "as", "you", "do", "at", "this",
            "but", "by", "from", "they", "we", "say", "or", "an", "will",
            "one", "all", "would", "there", "their", "what", "so", "up",
            "out", "if", "about", "who", "get", "which", "go", "when",
            "make", "can", "like", "time", "just", "know", "take", "into",
            "year", "good", "some", "could", "them", "see", "other", "than",
            "then", "now", "look", "only", "come", "after", "use", "two",
            "how", "work", "first", "well", "way", "even", "new", "want",
            "because", "any", "these", "give", "day", "most",
            // Programming terms
            "function", "variable", "class", "method", "property", "interface",
            "module", "package", "import", "export", "return", "string", "number",
            "boolean", "array", "object", "null", "undefined", "value", "type",
            "parameter", "argument", "callback", "async", "await", "promise",
            "error", "exception", "debug", "compile", "runtime", "static",
            "public", "private", "protected", "const", "let", "void"
        };
        
        for (const auto& word : commonWords) spellChecker.addWord(word);
    }
};

} // namespace rawrxd
