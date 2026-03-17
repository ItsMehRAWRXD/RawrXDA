#pragma once
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

namespace RawrXD {

enum class Language {
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
    std::unordered_map<Language, std::unordered_set<std::wstring>> keywordSets;

public:
    KeywordHashTable();
    ~KeywordHashTable() = default;

    // Initialize keywords for a specific language
    void initializeLanguage(Language lang);

    // Check if a word is a keyword in the given language
    bool isKeyword(Language lang, const std::wstring& word) const;

    // Get all keywords for a language
    std::vector<std::wstring> getKeywords(Language lang) const;

    // Add custom keywords for a language
    void addKeyword(Language lang, const std::wstring& keyword);

    // Remove keyword
    void removeKeyword(Language lang, const std::wstring& keyword);
};

} // namespace RawrXD