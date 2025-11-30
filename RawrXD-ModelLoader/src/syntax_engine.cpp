#include "../include/syntax_engine.h"
#include <cctype>

static bool isWordChar(char c){ return std::isalnum((unsigned char)c) || c=='_' || c=='$'; }

void GenericLanguagePlugin::lex(std::string_view text, std::vector<SyntaxToken>& out) {
    unsigned i = 0; unsigned n = (unsigned)text.size();
    while(i < n) {
        if(std::isspace((unsigned char)text[i])) { ++i; continue; }
        unsigned start = i;
        while(i < n && isWordChar(text[i])) ++i;
        if(start == i) { ++i; continue; }
        SyntaxToken tk; tk.start = start; tk.length = i - start; tk.type = 0;
        bool allDigits = true; bool allAlpha = true;
        for(unsigned k=start; k<i; ++k){ char c = text[k]; if(!std::isdigit((unsigned char)c)) allDigits=false; if(!std::isalpha((unsigned char)c)) allAlpha=false; }
        if(allDigits) tk.type = 1; else if(allAlpha) tk.type = 2;
        out.push_back(tk);
    }
}

CppLanguagePlugin::CppLanguagePlugin() {
    const char* kw[] = {
        "auto","break","case","catch","class","const","constexpr","continue","decltype","default","delete","do","else","enum","explicit","export","extern","for","friend","goto","if","inline","namespace","new","noexcept","operator","private","protected","public","return","sizeof","static","struct","switch","template","this","throw","try","typedef","typeid","typename","union","using","virtual","volatile","while"};
    for(auto s: kw) m_keywords.insert(s);
}

void CppLanguagePlugin::lex(std::string_view text, std::vector<SyntaxToken>& out) {
    unsigned i=0,n=(unsigned)text.size();
    while(i<n){
        char c = text[i];
        if(std::isspace((unsigned char)c)) { ++i; continue; }
        // Line comment
        if(c=='/' && i+1<n && text[i+1]=='/') {
            unsigned start=i; i+=2; while(i<n && text[i] != '\n') ++i;
            out.push_back({start, i-start, 5});
            continue;
        }
        // String literal (simple)
        if(c=='"') {
            unsigned start=i; ++i; while(i<n && text[i] != '"') { if(text[i]=='\\' && i+1<n) i+=2; else ++i; }
            if(i<n) ++i; // consume closing quote
            out.push_back({start, i-start, 4});
            continue;
        }
        // Word / identifier / number / keyword
        if(isWordChar(c)) {
            unsigned start=i; while(i<n && isWordChar(text[i])) ++i;
            std::string word(text.substr(start, i-start));
            SyntaxToken tk{start, (unsigned)(i-start), 2};
            bool allDigits=true; for(char wc: word){ if(!std::isdigit((unsigned char)wc)){ allDigits=false; break; } }
            if(allDigits) tk.type=1; else if(m_keywords.count(word)) tk.type=3;
            out.push_back(tk);
            continue;
        }
        ++i; // punctuation / other
    }
}

PowerShellLanguagePlugin::PowerShellLanguagePlugin() {
    const char* kw[] = {
        "function","param","begin","process","end","if","else","elseif","switch","for","foreach","while","do","return","break","continue","try","catch","finally","throw"};
    for(auto s: kw) m_keywords.insert(s);
}

void PowerShellLanguagePlugin::lex(std::string_view text, std::vector<SyntaxToken>& out) {
    unsigned i=0,n=(unsigned)text.size();
    while(i<n){
        char c=text[i];
        if(std::isspace((unsigned char)c)) { ++i; continue; }
        // Comment (# until EOL)
        if(c=='#') { unsigned start=i; while(i<n && text[i] != '\n') ++i; out.push_back({start, i-start, 5}); continue; }
        // String literal (supports single and double quotes minimal)
        if(c=='"' || c=='\''){ unsigned start=i; char quote=c; ++i; while(i<n && text[i]!=quote){ if(text[i]=='`' && i+1<n) i+=2; else ++i; } if(i<n) ++i; out.push_back({start,i-start,4}); continue; }
        if(isWordChar(c)) { unsigned start=i; while(i<n && isWordChar(text[i])) ++i; std::string word(text.substr(start,i-start)); SyntaxToken tk{start,(unsigned)(i-start),2}; bool allDigits=true; for(char wc: word){ if(!std::isdigit((unsigned char)wc)){ allDigits=false; break; } } if(allDigits) tk.type=1; else if(m_keywords.count(word)) tk.type=3; out.push_back(tk); continue; }
        ++i;
    }
}

SyntaxEngine::SyntaxEngine() : m_lang(&m_fallback) {}

void SyntaxEngine::setLanguage(LanguagePluginBase* lang) { m_lang = lang ? lang : &m_fallback; }

void SyntaxEngine::tokenize(std::string_view text, std::vector<SyntaxToken>& outTokens) {
    outTokens.clear(); if(!m_lang) m_lang = &m_fallback; m_lang->lex(text, outTokens);
}
