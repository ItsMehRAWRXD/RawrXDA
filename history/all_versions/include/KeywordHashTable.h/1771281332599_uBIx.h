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
    std::unordered_map<Language, std::unordered_set<std::string>> keywordSets;

public:
    KeywordHashTable();
    ~KeywordHashTable() = default;

    // Initialize keywords for a specific language
    void initializeLanguage(Language lang);

    // Check if a word is a keyword in the given language
    bool isKeyword(Language lang, const std::string& word) const;

    // Get all keywords for a language
    std::vector<std::string> getKeywords(Language lang) const;

    // Add custom keywords for a language
    void addKeyword(Language lang, const std::string& keyword);

    // Remove keyword
    void removeKeyword(Language lang, const std::string& keyword);
};

} // namespace RawrXD