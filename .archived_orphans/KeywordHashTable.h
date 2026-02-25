#pragma once
// KeywordHashTable.h — Language keyword sets for syntax highlighting
// Zero Qt. Pure C++20.

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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
    Rust
};

class KeywordHashTable {
    std::unordered_map<Language, std::unordered_set<std::wstring>> keywordSets;

public:
    KeywordHashTable();

    /// Populate keyword set for the given language
    void initializeLanguage(Language lang);

    /// Check if \a word is a keyword in \a lang
    bool isKeyword(Language lang, const std::wstring& word) const;

    /// Return all keywords for \a lang (unordered)
    std::vector<std::wstring> getKeywords(Language lang) const;

    /// Dynamically add a keyword
    void addKeyword(Language lang, const std::wstring& keyword);

    /// Dynamically remove a keyword
    void removeKeyword(Language lang, const std::wstring& keyword);
};

} // namespace RawrXD
