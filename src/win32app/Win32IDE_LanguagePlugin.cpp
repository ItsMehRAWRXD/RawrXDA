#include "Win32IDE.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::string lowerExt(const std::string& path) {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return {};
    }
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

IDEPlugin::TokenType classifyWord(const std::string& word,
                                  const std::unordered_map<std::string, IDEPlugin::TokenType>& keywords) {
    auto it = keywords.find(word);
    if (it != keywords.end()) {
        return it->second;
    }
    bool isNum = !word.empty() && std::all_of(word.begin(), word.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
    if (isNum) {
        return IDEPlugin::TokenType::Number;
    }
    return IDEPlugin::TokenType::Identifier;
}

}  // namespace

class LanguagePluginManager {
public:
    LanguagePluginManager() {
        registerLanguage("cpp", {".c", ".cc", ".cpp", ".cxx", ".h", ".hpp", ".hxx"},
                         {"int", "float", "double", "char", "bool", "void", "class", "struct", "namespace", "template", "return"});
        registerLanguage("python", {".py", ".pyw", ".pyi"},
                         {"def", "class", "import", "from", "if", "elif", "else", "for", "while", "return", "try", "except"});
    }

    std::vector<std::string> supportedLanguages() const {
        std::vector<std::string> out;
        out.reserve(m_languages.size());
        for (const auto& kv : m_languages) {
            out.push_back(kv.first);
        }
        std::sort(out.begin(), out.end());
        return out;
    }

    std::string detectLanguage(const std::string& filePath, const std::string&) const {
        const std::string ext = lowerExt(filePath);
        auto it = m_extToLang.find(ext);
        return it == m_extToLang.end() ? std::string("plaintext") : it->second;
    }

    std::vector<IDEPlugin::SyntaxToken> tokenize(const std::string& language, const std::string& code) const {
        std::vector<IDEPlugin::SyntaxToken> tokens;
        auto langIt = m_languages.find(language);
        const auto& keywords = (langIt != m_languages.end()) ? langIt->second : m_emptyMap;

        int tokenStart = -1;
        for (size_t i = 0; i <= code.size(); ++i) {
            const bool end = i == code.size();
            const char c = end ? '\0' : code[i];
            const bool isWordChar = !end && (std::isalnum(static_cast<unsigned char>(c)) || c == '_');
            if (isWordChar) {
                if (tokenStart < 0) {
                    tokenStart = static_cast<int>(i);
                }
                continue;
            }
            if (tokenStart >= 0) {
                const int len = static_cast<int>(i) - tokenStart;
                const std::string word = code.substr(static_cast<size_t>(tokenStart), static_cast<size_t>(len));
                IDEPlugin::SyntaxToken token;
                token.type = classifyWord(word, keywords);
                token.start = tokenStart;
                token.length = len;
                token.text = word;
                tokens.push_back(std::move(token));
                tokenStart = -1;
            }
        }

        return tokens;
    }

    std::vector<IDEPlugin::CompletionItem> completions(const std::string& language) const {
        std::vector<IDEPlugin::CompletionItem> out;
        auto it = m_languages.find(language);
        if (it == m_languages.end()) {
            return out;
        }

        out.reserve(it->second.size());
        for (const auto& kw : it->second) {
            IDEPlugin::CompletionItem item;
            item.label = kw.first;
            item.detail = language + " keyword";
            item.documentation = "";
            item.kind = IDEPlugin::CompletionItemKind::Keyword;
            item.insertText = kw.first;
            out.push_back(std::move(item));
        }
        return out;
    }

    std::vector<IDEPlugin::Diagnostic> diagnostics(const std::string&, const std::string& code) const {
        std::vector<IDEPlugin::Diagnostic> out;
        int line = 1;
        int col = 1;
        int balance = 0;

        for (char c : code) {
            if (c == '\n') {
                ++line;
                col = 1;
                continue;
            }
            if (c == '{') {
                ++balance;
            } else if (c == '}') {
                --balance;
                if (balance < 0) {
                    IDEPlugin::Diagnostic d;
                    d.source = "language-plugin";
                    d.line = line;
                    d.column = col;
                    d.message = "Unmatched closing brace";
                    d.severity = "error";
                    out.push_back(std::move(d));
                    balance = 0;
                }
            }
            ++col;
        }

        if (balance > 0) {
            IDEPlugin::Diagnostic d;
            d.source = "language-plugin";
            d.line = line;
            d.column = 1;
            d.message = "Unclosed block: missing }";
            d.severity = "warning";
            out.push_back(std::move(d));
        }

        return out;
    }

private:
    void registerLanguage(const std::string& id,
                          const std::vector<std::string>& extensions,
                          const std::vector<std::string>& keywords) {
        std::unordered_map<std::string, IDEPlugin::TokenType> map;
        for (const auto& kw : keywords) {
            map[kw] = IDEPlugin::TokenType::Keyword;
        }
        m_languages[id] = std::move(map);
        for (const auto& ext : extensions) {
            m_extToLang[ext] = id;
        }
    }

    std::unordered_map<std::string, std::unordered_map<std::string, IDEPlugin::TokenType>> m_languages;
    std::unordered_map<std::string, std::string> m_extToLang;
    const std::unordered_map<std::string, IDEPlugin::TokenType> m_emptyMap;
};

void Win32IDE::initLanguagePlugin() {
    if (!m_languageManager) {
        m_languageManager = std::make_unique<LanguagePluginManager>();
    }
}

void Win32IDE::updateSyntaxHighlighting() {
    if (!m_languageManager) {
        initLanguagePlugin();
    }
    applySyntaxColoring();
}

void Win32IDE::applySyntaxHighlighting(const std::vector<IDEPlugin::SyntaxToken>&) {
    applySyntaxColoring();
}

void Win32IDE::showCodeCompletions(int) {
    if (!m_languageManager) {
        initLanguagePlugin();
    }
    const std::string language = m_languageManager->detectLanguage(m_currentFile, getEditorText());
    const auto items = m_languageManager->completions(language);

    std::ostringstream oss;
    oss << "[Language] Completion candidates for " << language << ":\n";
    const size_t maxItems = std::min<size_t>(items.size(), 30);
    for (size_t i = 0; i < maxItems; ++i) {
        oss << "  - " << items[i].label << "\n";
    }
    if (items.size() > maxItems) {
        oss << "  ... (" << (items.size() - maxItems) << " more)\n";
    }
    appendToOutput(oss.str(), "Language", OutputSeverity::Info);
}

void Win32IDE::updateDiagnostics() {
    if (!m_languageManager) {
        initLanguagePlugin();
    }

    clearDiagnostics();
    const std::string code = getEditorText();
    const std::string language = m_languageManager->detectLanguage(m_currentFile, code);
    const auto diagnostics = m_languageManager->diagnostics(language, code);
    for (const auto& d : diagnostics) {
        addDiagnostic(d);
    }
}

void Win32IDE::clearDiagnostics() {
    appendToOutput("[Language] Diagnostics cleared.\n", "Problems", OutputSeverity::Debug);
}

void Win32IDE::addDiagnostic(const IDEPlugin::Diagnostic& diagnostic) {
    std::ostringstream oss;
    oss << "[" << diagnostic.severity << "] "
        << diagnostic.source << "(" << diagnostic.line << ":" << diagnostic.column << "): "
        << diagnostic.message << "\n";
    appendToOutput(oss.str(), "Problems", OutputSeverity::Warning);
}

bool Win32IDE::initializeLanguageServer(const std::string& filename) {
    const std::string ext = lowerExt(filename);
    LSPLanguage lang = LSPLanguage::Cpp;

    if (ext == ".py" || ext == ".pyw" || ext == ".pyi") {
        lang = LSPLanguage::Python;
    } else if (ext == ".js" || ext == ".jsx") {
        lang = LSPLanguage::TypeScript;
    } else if (ext == ".ts" || ext == ".tsx") {
        lang = LSPLanguage::TypeScript;
    } else if (ext == ".json") {
        lang = LSPLanguage::TypeScript;
    }

    return startLSPServer(lang);
}

std::vector<std::string> Win32IDE::getSupportedLanguages() const {
    if (!m_languageManager) {
        return {};
    }
    return m_languageManager->supportedLanguages();
}
