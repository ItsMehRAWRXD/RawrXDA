#pragma once
/**
 * @file completion_provider.h
 * @brief IntelliSense completion and signature help
 * Batch 3 - Item 39: Completion provider
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <mutex>

namespace RawrXD::LSP {

enum class CompletionItemKind {
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
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25
};

enum class InsertTextFormat {
    PlainText = 1,
    Snippet = 2
};

struct CompletionItem {
    std::string label;
    CompletionItemKind kind;
    std::optional<std::string> detail;
    std::optional<std::string> documentation;
    std::optional<std::string> deprecated;
    bool preselect = false;
    std::optional<std::string> sortText;
    std::optional<std::string> filterText;
    std::string insertText;
    InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;
    std::optional<std::string> commitCharacters;
    std::optional<std::string> command;
    std::optional<std::string> data;
};

struct CompletionContext {
    enum TriggerKind {
        Invoked = 1,
        TriggerCharacter = 2,
        TriggerForIncompleteCompletions = 3
    };
    TriggerKind triggerKind;
    std::optional<std::string> triggerCharacter;
};

struct SignatureInformation {
    std::string label;
    std::optional<std::string> documentation;
    struct ParameterInfo {
        std::string label;
        std::optional<std::string> documentation;
    };
    std::vector<ParameterInfo> parameters;
    std::optional<uint32_t> activeParameter;
};

struct SignatureHelp {
    std::vector<SignatureInformation> signatures;
    std::optional<uint32_t> activeSignature;
    std::optional<uint32_t> activeParameter;
};

class CompletionProvider {
public:
    CompletionProvider();
    ~CompletionProvider();

    // Configuration
    void setTriggerCharacters(const std::vector<std::string>& characters);
    std::vector<std::string> getTriggerCharacters() const;

    // Completion
    std::vector<CompletionItem> provideCompletion(const std::string& uri,
                                                      const std::string& content,
                                                      uint32_t line,
                                                      uint32_t column,
                                                      const CompletionContext& context);
    std::optional<CompletionItem> resolveCompletion(const CompletionItem& item);

    // Signature help
    std::optional<SignatureHelp> provideSignatureHelp(const std::string& uri,
                                                         const std::string& content,
                                                         uint32_t line,
                                                         uint32_t column);

    // Registration
    void registerCompletionSource(std::function<std::vector<CompletionItem>(
        const std::string& uri,
        const std::string& prefix)> source);

    // C++ specific
    std::vector<CompletionItem> getCppKeywords();
    std::vector<CompletionItem> getCppTypes();
    std::vector<CompletionItem> getCppSnippets();

private:
    std::vector<std::string> m_triggerCharacters;
    std::vector<std::function<std::vector<CompletionItem>(
        const std::string&, const std::string&)>> m_sources;
    mutable std::mutex m_mutex;

    std::string getWordAtPosition(const std::string& content,
                                  uint32_t line,
                                  uint32_t column);
    std::vector<std::string> getContext(const std::string& content,
                                          uint32_t line);
    bool isAfterDot(const std::string& content,
                    uint32_t line,
                    uint32_t column);
    bool isAfterArrow(const std::string& content,
                      uint32_t line,
                      uint32_t column);
    bool isAfterScope(const std::string& content,
                      uint32_t line,
                      uint32_t column);
};

// Global provider
CompletionProvider& getCompletionProvider();

// Utility
std::string completionItemKindToString(CompletionItemKind kind);

} // namespace RawrXD::LSP
