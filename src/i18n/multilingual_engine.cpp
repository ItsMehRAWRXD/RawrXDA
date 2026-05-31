// ============================================================================
// Multi-Language Support Engine — Internationalization and Localization
// Provides code translation, UI localization, and language detection
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>

namespace RawrXD::I18n {

enum class Language {
    ENGLISH,
    SPANISH,
    FRENCH,
    GERMAN,
    CHINESE,
    JAPANESE,
    KOREAN,
    RUSSIAN,
    PORTUGUESE,
    ITALIAN,
    ARABIC,
    HINDI,
    AUTO_DETECT
};

struct UIElement {
    std::string id;
    std::string defaultText;
    std::string context;
    std::map<Language, std::string> translations;
};

struct LanguagePattern {
    Language language;
    std::vector<std::string> keywords;
    double confidence;
};

class MultiLingualEngine {
public:
    explicit MultiLingualEngine(std::shared_ptr<SovereignInferenceClient> aiClient)
        : m_aiClient(aiClient) {
        InitializeDefaultTranslations();
    }

    std::string TranslateCodeComments(const std::string& code, Language targetLang) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return code; // Return original if AI unavailable
        }

        // Extract comments from code
        auto comments = ExtractComments(code);
        std::string translatedCode = code;
        
        for (const auto& comment : comments) {
            std::string prompt = "Translate this code comment to " + 
                                LanguageToString(targetLang) + ":\n" + comment.text;
            
            std::vector<ChatMessage> messages = {
                {"system", "You are a technical translator. Translate code comments accurately."},
                {"user", prompt}
            };

            auto result = m_aiClient->ChatSync(messages);
            
            if (result.success) {
                // Replace comment in code
                ReplaceComment(translatedCode, comment, result.response);
            }
        }
        
        return translatedCode;
    }

    void LocalizeUIElements(std::vector<UIElement>& elements, Language targetLang) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        for (auto& element : elements) {
            // Check if translation exists
            auto it = element.translations.find(targetLang);
            if (it == element.translations.end()) {
                // Generate translation
                std::string translated = TranslateText(element.defaultText, targetLang);
                element.translations[targetLang] = translated;
            }
        }
    }

    Language DetectLanguagePatterns(const std::string& content) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Simple keyword-based detection
        std::map<Language, int> scores;
        
        for (const auto& [lang, keywords] : m_languageKeywords) {
            int count = 0;
            for (const auto& keyword : keywords) {
                if (content.find(keyword) != std::string::npos) {
                    count++;
                }
            }
            scores[lang] = count;
        }
        
        // Find highest scoring language
        Language detected = Language::ENGLISH;
        int maxScore = 0;
        for (const auto& [lang, score] : scores) {
            if (score > maxScore) {
                maxScore = score;
                detected = lang;
            }
        }
        
        return detected;
    }

    std::string GetLocalizedString(const std::string& key, Language lang) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_translations.find(key);
        if (it != m_translations.end()) {
            auto langIt = it->second.find(lang);
            if (langIt != it->second.end()) {
                return langIt->second;
            }
        }
        
        return key; // Return key if translation not found
    }

    void AddTranslation(const std::string& key, Language lang, const std::string& translation) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_translations[key][lang] = translation;
    }

    std::vector<Language> GetSupportedLanguages() const {
        return {
            Language::ENGLISH,
            Language::SPANISH,
            Language::FRENCH,
            Language::GERMAN,
            Language::CHINESE,
            Language::JAPANESE,
            Language::KOREAN,
            Language::RUSSIAN,
            Language::PORTUGUESE,
            Language::ITALIAN
        };
    }

private:
    std::shared_ptr<SovereignInferenceClient> m_aiClient;
    mutable std::mutex m_mutex;
    std::map<std::string, std::map<Language, std::string>> m_translations;
    std::map<Language, std::vector<std::string>> m_languageKeywords;

    struct CommentInfo {
        std::string text;
        size_t startPos;
        size_t endPos;
        bool isBlockComment;
    };

    void InitializeDefaultTranslations() {
        // Initialize common UI strings
        m_translations["file_menu"][Language::ENGLISH] = "File";
        m_translations["file_menu"][Language::SPANISH] = "Archivo";
        m_translations["file_menu"][Language::FRENCH] = "Fichier";
        m_translations["file_menu"][Language::GERMAN] = "Datei";
        
        m_translations["edit_menu"][Language::ENGLISH] = "Edit";
        m_translations["edit_menu"][Language::SPANISH] = "Editar";
        m_translations["edit_menu"][Language::FRENCH] = "Édition";
        m_translations["edit_menu"][Language::GERMAN] = "Bearbeiten";
        
        m_translations["view_menu"][Language::ENGLISH] = "View";
        m_translations["view_menu"][Language::SPANISH] = "Ver";
        m_translations["view_menu"][Language::FRENCH] = "Affichage";
        m_translations["view_menu"][Language::GERMAN] = "Ansicht";
        
        // Initialize language keywords for detection
        m_languageKeywords[Language::SPANISH] = {"el", "la", "de", "en", "y", "que"};
        m_languageKeywords[Language::FRENCH] = {"le", "la", "de", "et", "est", "que"};
        m_languageKeywords[Language::GERMAN] = {"der", "die", "und", "zu", "den", "das"};
        m_languageKeywords[Language::CHINESE] = {"的", "是", "在", "和", "了", "有"};
        m_languageKeywords[Language::JAPANESE] = {"の", "は", "を", "が", "に", "と"};
    }

    std::vector<CommentInfo> ExtractComments(const std::string& code) {
        std::vector<CommentInfo> comments;
        
        // Extract // comments
        size_t pos = 0;
        while ((pos = code.find("//", pos)) != std::string::npos) {
            CommentInfo comment;
            comment.startPos = pos;
            size_t endPos = code.find('\n', pos);
            comment.endPos = endPos == std::string::npos ? code.length() : endPos;
            comment.text = code.substr(pos, comment.endPos - pos);
            comment.isBlockComment = false;
            comments.push_back(comment);
            pos = comment.endPos;
        }
        
        // Extract /* */ comments
        pos = 0;
        while ((pos = code.find("/*", pos)) != std::string::npos) {
            CommentInfo comment;
            comment.startPos = pos;
            size_t endPos = code.find("*/", pos);
            if (endPos != std::string::npos) {
                comment.endPos = endPos + 2;
                comment.text = code.substr(pos, comment.endPos - pos);
                comment.isBlockComment = true;
                comments.push_back(comment);
                pos = comment.endPos;
            } else {
                break;
            }
        }
        
        return comments;
    }

    void ReplaceComment(std::string& code, const CommentInfo& original, 
                       const std::string& translated) {
        // Replace the comment text while preserving delimiters
        if (original.isBlockComment) {
            code.replace(original.startPos, original.endPos - original.startPos,
                        "/* " + translated + " */");
        } else {
            code.replace(original.startPos, original.endPos - original.startPos,
                        "// " + translated);
        }
    }

    std::string TranslateText(const std::string& text, Language targetLang) {
        if (!m_aiClient || !m_aiClient->IsLoaded()) {
            return text;
        }

        std::string prompt = "Translate to " + LanguageToString(targetLang) + ": " + text;
        
        std::vector<ChatMessage> messages = {
            {"system", "You are a translator. Provide accurate translations."},
            {"user", prompt}
        };

        auto result = m_aiClient->ChatSync(messages);
        
        if (result.success) {
            return result.response;
        }
        
        return text;
    }

    std::string LanguageToString(Language lang) {
        switch (lang) {
            case Language::ENGLISH: return "English";
            case Language::SPANISH: return "Spanish";
            case Language::FRENCH: return "French";
            case Language::GERMAN: return "German";
            case Language::CHINESE: return "Chinese";
            case Language::JAPANESE: return "Japanese";
            case Language::KOREAN: return "Korean";
            case Language::RUSSIAN: return "Russian";
            case Language::PORTUGUESE: return "Portuguese";
            case Language::ITALIAN: return "Italian";
            case Language::ARABIC: return "Arabic";
            case Language::HINDI: return "Hindi";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::I18n
