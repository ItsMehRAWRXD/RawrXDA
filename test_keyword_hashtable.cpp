#include "KeywordHashTable.h"
#include <iostream>

int main() {
    RawrXD::KeywordHashTable table;
    
    // Test C++ keywords
    std::wcout << L"Testing C++ keywords:" << std::endl;
    std::vector<std::wstring> cppKeywords = table.getKeywords(RawrXD::Language::Cpp);
    for (const auto& kw : cppKeywords) {
        std::wcout << kw << L" ";
    }
    std::wcout << std::endl;
    
    // Test lookup
    bool isKw = table.isKeyword(RawrXD::Language::Cpp, L"int");
    std::wcout << L"'int' is keyword: " << (isKw ? L"yes" : L"no") << std::endl;
    
    isKw = table.isKeyword(RawrXD::Language::Cpp, L"notakeyword");
    std::wcout << L"'notakeyword' is keyword: " << (isKw ? L"yes" : L"no") << std::endl;
    
    return 0;
}