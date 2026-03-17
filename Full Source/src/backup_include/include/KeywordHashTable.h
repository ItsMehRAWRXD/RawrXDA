#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace RawrXD {

// NOTE: RawrXD also has a `namespace Language` (language plugins/registry).
// This enum must not be named `Language` or it collides with that namespace.
enum class KeywordLanguage {
    C,
    Cpp,
    Assembly,
    MASM,
    Python,
    Java,
    JavaScript,
    TypeScript,
    Go,
    Rust,
    // Add more as needed
};

class KeywordHashTable {
private:
    std::unordered_map<KeywordLanguage, std::unordered_set<std::wstring>> keywordSets;

public:
    KeywordHashTable();
    ~KeywordHashTable() = default;

    // Initialize keywords for a specific language
    void initializeLanguage(KeywordLanguage lang);

    // Check if a word is a keyword in the given language
    bool isKeyword(KeywordLanguage lang, const std::wstring& word) const;

    // Get all keywords for a language
    std::vector<std::wstring> getKeywords(KeywordLanguage lang) const;

    // Add custom keywords for a language
    void addKeyword(KeywordLanguage lang, const std::wstring& keyword);

    // Remove keyword
    void removeKeyword(KeywordLanguage lang, const std::wstring& keyword);
};

} // namespace RawrXD
