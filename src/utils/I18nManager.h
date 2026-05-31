#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <cwchar>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace RawrXD {

class I18nManager {
public:
    static I18nManager& getInstance() {
        static I18nManager instance;
        return instance;
    }

    std::wstring translateW(const std::string& key) {
        std::string s = translate(key);
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (len <= 0) return L"";
        std::wstring ws(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], (int)ws.size());
        if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
        return ws;
    }

    void setLanguage(const std::string& langCode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentLang = langCode;
    }

    std::string getLanguage() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_currentLang;
    }

    void loadTranslation(const std::string& langCode, const std::map<std::string, std::string>& translations) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_translations[langCode] = translations;
    }

    std::string translate(const std::string& key) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Return key if no current translation set
        if (m_currentLang.empty()) return key;

        auto itLang = m_translations.find(m_currentLang);
        if (itLang != m_translations.end()) {
            auto itKey = itLang->second.find(key);
            if (itKey != itLang->second.end()) {
                return itKey->second;
            }
        }
        
        // Fallback to English
        if (m_currentLang != "en") {
            auto itEn = m_translations.find("en");
            if (itEn != m_translations.end()) {
                auto itKey = itEn->second.find(key);
                if (itKey != itEn->second.end()) {
                    return itKey->second;
                }
            }
        }
        
        return key; // Return key as fallback
    }

    void initializeDefaultTranslations() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // English
        m_translations["en"]["GEN_START"] = "Generating tokens...";
        m_translations["en"]["GEN_DONE"] = "Generated %d tokens in %dm %ds (%.2f tok/s)";
        m_translations["en"]["EST_TOK"] = "%d / %d tokens (%d%%)";
        m_translations["en"]["CUR_TOK"] = "%d tokens";
        
        // Chinese
        m_translations["zh"]["GEN_START"] = "正在生成令牌...";
        m_translations["zh"]["GEN_DONE"] = "已在 %dm %ds 内生成 %d 个令牌 (%.2f tok/s)";
        m_translations["zh"]["EST_TOK"] = "%d / %d 令牌 (%d%%)";
        m_translations["zh"]["CUR_TOK"] = "%d 令牌";
        
        // Spanish
        m_translations["es"]["GEN_START"] = "Generando tokens...";
        m_translations["es"]["GEN_DONE"] = "Generados %d tokens en %dm %ds (%.2f tok/s)";
        m_translations["es"]["EST_TOK"] = "%d / %d tokens (%d%%)";
        m_translations["es"]["CUR_TOK"] = "%d tokens";
    }

private:
    I18nManager() : m_currentLang("en") {}
    
    std::string m_currentLang;
    std::map<std::string, std::map<std::string, std::string>> m_translations;
    mutable std::mutex m_mutex;
};

// Global helper for short syntax: T("key")
inline std::string T(const std::string& key) {
    return I18nManager::getInstance().translate(key);
}

} // namespace RawrXD
