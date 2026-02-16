// RawrXD Configuration Manager - Pure Win32/C++ Implementation
// Replaces: agentic_configuration.cpp and all Qt JSON/config components
// Zero Qt dependencies - just Win32 API + STL

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <algorithm>
#include <fstream>
#include <sstream>

// ============================================================================
// JSON PARSER (MINIMAL)
// ============================================================================

struct JsonValue {
    enum Type { STRING, NUMBER, BOOLEAN, OBJECT, ARRAY, NULL_TYPE };
    Type type;
    std::wstring str_value;
    double num_value;
    bool bool_value;
    std::map<std::wstring, std::shared_ptr<JsonValue>> obj_value;
    std::vector<std::shared_ptr<JsonValue>> arr_value;
    
    JsonValue() : type(NULL_TYPE), num_value(0), bool_value(false) {}
    
    static std::shared_ptr<JsonValue> Parse(const std::wstring& json) {
        size_t pos = 0;
        return ParseValue(json, pos);
    }
    
private:
    static std::shared_ptr<JsonValue> ParseValue(const std::wstring& json, size_t& pos) {
        auto value = std::make_shared<JsonValue>();
        SkipWhitespace(json, pos);
        
        if (pos >= json.length()) return value;
        
        wchar_t ch = json[pos];
        if (ch == L'"') {
            value->type = STRING;
            value->str_value = ParseString(json, pos);
        } else if (ch == L'{') {
            value->type = OBJECT;
            ParseObject(json, pos, value->obj_value);
        } else if (ch == L'[') {
            value->type = ARRAY;
            ParseArray(json, pos, value->arr_value);
        } else if (ch == L't' || ch == L'f') {
            value->type = BOOLEAN;
            value->bool_value = ParseBool(json, pos);
        } else if (iswdigit(ch) || ch == L'-') {
            value->type = NUMBER;
            value->num_value = ParseNumber(json, pos);
        }
        
        return value;
    }
    
    static void SkipWhitespace(const std::wstring& json, size_t& pos) {
        while (pos < json.length() && (json[pos] == L' ' || json[pos] == L'\t' || 
               json[pos] == L'\n' || json[pos] == L'\r')) pos++;
    }
    
    static std::wstring ParseString(const std::wstring& json, size_t& pos) {
        pos++; // Skip opening quote
        std::wstring result;
        while (pos < json.length() && json[pos] != L'"') {
            result += json[pos++];
        }
        if (pos < json.length()) pos++; // Skip closing quote
        return result;
    }
    
    static void ParseObject(const std::wstring& json, size_t& pos, 
                           std::map<std::wstring, std::shared_ptr<JsonValue>>& obj) {
        pos++; // Skip {
        SkipWhitespace(json, pos);
        
        while (pos < json.length() && json[pos] != L'}') {
            SkipWhitespace(json, pos);
            if (json[pos] == L'"') {
                std::wstring key = ParseString(json, pos);
                SkipWhitespace(json, pos);
                if (pos < json.length() && json[pos] == L':') {
                    pos++;
                    obj[key] = ParseValue(json, pos);
                    SkipWhitespace(json, pos);
                    if (pos < json.length() && json[pos] == L',') pos++;
                }
            } else {
                break;
            }
        }
        if (pos < json.length()) pos++; // Skip }
    }
    
    static void ParseArray(const std::wstring& json, size_t& pos,
                          std::vector<std::shared_ptr<JsonValue>>& arr) {
        pos++; // Skip [
        SkipWhitespace(json, pos);
        
        while (pos < json.length() && json[pos] != L']') {
            arr.push_back(ParseValue(json, pos));
            SkipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == L',') pos++;
        }
        if (pos < json.length()) pos++; // Skip ]
    }
    
    static bool ParseBool(const std::wstring& json, size_t& pos) {
        if (json.substr(pos, 4) == L"true") {
            pos += 4;
            return true;
        } else if (json.substr(pos, 5) == L"false") {
            pos += 5;
            return false;
        }
        return false;
    }
    
    static double ParseNumber(const std::wstring& json, size_t& pos) {
        std::wstring numStr;
        while (pos < json.length() && (iswdigit(json[pos]) || json[pos] == L'.' || json[pos] == L'-')) {
            numStr += json[pos++];
        }
        return _wtof(numStr.c_str());
    }
};

// ============================================================================
// CONFIGURATION MANAGER
// ============================================================================

enum class Environment {
    Development,
    Staging, 
    Production
};

class ConfigurationManager {
private:
    Environment m_environment;
    std::map<std::wstring, std::wstring> m_config;
    std::wstring m_configPath;
    std::wstring m_workspaceRoot;
    bool m_autoSave;
    
    // Default configuration values
    struct ConfigDefaults {
        static const std::map<std::wstring, std::wstring> Values;
    };
    
public:
    ConfigurationManager() 
        : m_environment(Environment::Development),
          m_autoSave(true) {
        LoadDefaults();
        InitializePaths();
    }
    
    ~ConfigurationManager() {
        if (m_autoSave) {
            SaveToFile();
        }
    }
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    void InitializeFromEnvironment(Environment env) {
        m_environment = env;
        
        std::wstring envName;
        switch (env) {
        case Environment::Development: envName = L"Development"; break;
        case Environment::Staging: envName = L"Staging"; break;
        case Environment::Production: envName = L"Production"; break;
        }
        
        LogInfo(L"ConfigurationManager initialized for environment: " + envName);
        ApplyEnvironmentOverrides();
    }
    
    bool LoadFromJsonFile(const std::wstring& filePath) {
        HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            LogWarning(L"Cannot open JSON file: " + filePath);
            return false;
        }
        
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize == INVALID_FILE_SIZE) {
            CloseHandle(hFile);
            return false;
        }
        
        std::vector<char> buffer(fileSize);
        DWORD bytesRead;
        bool success = ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL);
        CloseHandle(hFile);
        
        if (!success) return false;
        
        // Convert to wide string
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, NULL, 0);
        std::vector<wchar_t> wideBuffer(wideSize);
        MultiByteToWideChar(CP_UTF8, 0, buffer.data(), bytesRead, wideBuffer.data(), wideSize);
        
        std::wstring json(wideBuffer.data(), wideSize);
        return ParseJsonConfig(json);
    }
    
    bool SaveToFile(const std::wstring& filePath = L"") {
        std::wstring path = filePath.empty() ? m_configPath : filePath;
        if (path.empty()) {
            path = GetConfigDirectory() + L"\\rawrxd_config.json";
        }
        
        std::wstring json = GenerateJsonConfig();
        
        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0,
                                  NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            LogError(L"Cannot create config file: " + path);
            return false;
        }
        
        // Convert to UTF-8
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, NULL, 0, NULL, NULL);
        std::vector<char> utf8Buffer(utf8Size);
        WideCharToMultiByte(CP_UTF8, 0, json.c_str(), -1, utf8Buffer.data(), utf8Size, NULL, NULL);
        
        DWORD bytesWritten;
        bool success = WriteFile(hFile, utf8Buffer.data(), utf8Size - 1, &bytesWritten, NULL);
        CloseHandle(hFile);
        
        if (success) {
            LogInfo(L"Configuration saved to: " + path);
        }
        
        return success;
    }
    
    // ========================================================================
    // CONFIGURATION ACCESS
    // ========================================================================
    
    std::wstring GetString(const std::wstring& key, const std::wstring& defaultValue = L"") {
        auto it = m_config.find(key);
        return (it != m_config.end()) ? it->second : defaultValue;
    }
    
    int GetInt(const std::wstring& key, int defaultValue = 0) {
        std::wstring str = GetString(key);
        return str.empty() ? defaultValue : _wtoi(str.c_str());
    }
    
    double GetDouble(const std::wstring& key, double defaultValue = 0.0) {
        std::wstring str = GetString(key);
        return str.empty() ? defaultValue : _wtof(str.c_str());
    }
    
    bool GetBool(const std::wstring& key, bool defaultValue = false) {
        std::wstring str = GetString(key);
        if (str.empty()) return defaultValue;
        std::transform(str.begin(), str.end(), str.begin(), ::towlower);
        return str == L"true" || str == L"1" || str == L"yes";
    }
    
    void SetString(const std::wstring& key, const std::wstring& value) {
        m_config[key] = value;
        if (m_autoSave) SaveToFile();
    }
    
    void SetInt(const std::wstring& key, int value) {
        SetString(key, std::to_wstring(value));
    }
    
    void SetDouble(const std::wstring& key, double value) {
        SetString(key, std::to_wstring(value));
    }
    
    void SetBool(const std::wstring& key, bool value) {
        SetString(key, value ? L"true" : L"false");
    }
    
    // ========================================================================
    // SPECIALIZED GETTERS
    // ========================================================================
    
    std::wstring GetModelPath() {
        return GetString(L"model_path", GetWorkspaceRoot() + L"\\models");
    }
    
    std::wstring GetTempDirectory() {
        return GetString(L"temp_directory", GetConfigDirectory() + L"\\temp");
    }
    
    std::wstring GetLogDirectory() {
        return GetString(L"log_directory", GetConfigDirectory() + L"\\logs");
    }
    
    int GetMaxTokens() {
        return GetInt(L"max_tokens", 2048);
    }
    
    double GetTemperature() {
        return GetDouble(L"temperature", 0.8);
    }
    
    bool IsDebuggingEnabled() {
        return GetBool(L"debug_enabled", m_environment == Environment::Development);
    }
    
    // ========================================================================
    // ENVIRONMENT-SPECIFIC
    // ========================================================================
    
    Environment GetEnvironment() const {
        return m_environment;
    }
    
    bool IsProduction() const {
        return m_environment == Environment::Production;
    }
    
    bool IsDevelopment() const {
        return m_environment == Environment::Development;
    }
    
private:
    void LoadDefaults() {
        m_config = ConfigDefaults::Values;
    }
    
    void InitializePaths() {
        wchar_t buffer[MAX_PATH];
        
        // Get current directory as workspace root
        GetCurrentDirectoryW(MAX_PATH, buffer);
        m_workspaceRoot = buffer;
        
        // Set config path
        m_configPath = GetConfigDirectory() + L"\\rawrxd_config.json";
        
        // Ensure directories exist
        CreateDirectoryIfNeeded(GetConfigDirectory());
        CreateDirectoryIfNeeded(GetTempDirectory());
        CreateDirectoryIfNeeded(GetLogDirectory());
    }
    
    void ApplyEnvironmentOverrides() {
        switch (m_environment) {
        case Environment::Development:
            SetBool(L"debug_enabled", true);
            SetBool(L"verbose_logging", true);
            SetInt(L"max_memory_mb", 8192);
            break;
            
        case Environment::Staging:
            SetBool(L"debug_enabled", false);
            SetBool(L"verbose_logging", true);
            SetInt(L"max_memory_mb", 4096);
            break;
            
        case Environment::Production:
            SetBool(L"debug_enabled", false);
            SetBool(L"verbose_logging", false);
            SetInt(L"max_memory_mb", 2048);
            break;
        }
    }
    
    bool ParseJsonConfig(const std::wstring& json) {
        auto root = JsonValue::Parse(json);
        if (!root || root->type != JsonValue::OBJECT) {
            LogError(L"Invalid JSON configuration");
            return false;
        }
        
        for (const auto& pair : root->obj_value) {
            if (pair.second->type == JsonValue::STRING) {
                m_config[pair.first] = pair.second->str_value;
            } else if (pair.second->type == JsonValue::NUMBER) {
                m_config[pair.first] = std::to_wstring(pair.second->num_value);
            } else if (pair.second->type == JsonValue::BOOLEAN) {
                m_config[pair.first] = pair.second->bool_value ? L"true" : L"false";
            }
        }
        
        LogInfo(L"Loaded " + std::to_wstring(m_config.size()) + L" configuration values");
        return true;
    }
    
    std::wstring GenerateJsonConfig() {
        std::wstring json = L"{\n";
        bool first = true;
        
        for (const auto& pair : m_config) {
            if (!first) json += L",\n";
            json += L"  \"" + pair.first + L"\": \"" + pair.second + L"\"";
            first = false;
        }
        
        json += L"\n}";
        return json;
    }
    
    std::wstring GetConfigDirectory() {
        wchar_t buffer[MAX_PATH];
        if (GetEnvironmentVariableW(L"USERPROFILE", buffer, MAX_PATH)) {
            return std::wstring(buffer) + L"\\.rawrxd";
        }
        return m_workspaceRoot + L"\\.rawrxd";
    }
    
    std::wstring GetWorkspaceRoot() {
        return m_workspaceRoot;
    }
    
    void CreateDirectoryIfNeeded(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES) {
            CreateDirectoryW(path.c_str(), NULL);
        }
    }
    
    // Logging helpers
    void LogInfo(const std::wstring& msg) {
        OutputDebugStringW((L"[CONFIG] INFO: " + msg + L"\n").c_str());
    }
    
    void LogWarning(const std::wstring& msg) {
        OutputDebugStringW((L"[CONFIG] WARNING: " + msg + L"\n").c_str());
    }
    
    void LogError(const std::wstring& msg) {
        OutputDebugStringW((L"[CONFIG] ERROR: " + msg + L"\n").c_str());
    }
};

// ============================================================================
// DEFAULT CONFIGURATION VALUES
// ============================================================================

const std::map<std::wstring, std::wstring> ConfigurationManager::ConfigDefaults::Values = {
    // Model settings
    {L"model_path", L"./models"},
    {L"model_name", L"rawrxd-model.gguf"},
    {L"max_tokens", L"2048"},
    {L"temperature", L"0.8"},
    {L"top_p", L"0.95"},
    {L"top_k", L"50"},
    
    // Authentication & API keys
    {L"user_api_key", L"key_1bbe2f4d33423a095fc03d9f873eb4a161a680df099e82410be7bb19e65c319f"},
    {L"api_endpoint", L"http://localhost:23959"},
    {L"enable_extensions", L"true"},
    {L"amazon_q_enabled", L"false"},
    {L"github_copilot_enabled", L"false"},
    
    // Performance settings
    {L"num_threads", L"8"},
    {L"max_memory_mb", L"4096"},
    {L"cache_size_mb", L"1024"},
    
    // Interface settings
    {L"window_width", L"1200"},
    {L"window_height", L"800"},
    {L"theme", L"dark"},
    {L"font_size", L"12"},
    
    // Behavior settings
    {L"auto_save", L"true"},
    {L"debug_enabled", L"false"},
    {L"verbose_logging", L"false"},
    {L"stream_responses", L"true"},
    
    // Directories
    {L"temp_directory", L"./temp"},
    {L"log_directory", L"./logs"},
    {L"backup_directory", L"./backups"}
};

// ============================================================================
// C INTERFACE FOR DLL EXPORT
// ============================================================================

extern "C" {

__declspec(dllexport) ConfigurationManager* CreateConfigurationManager() {
    return new ConfigurationManager();
}

__declspec(dllexport) void DestroyConfigurationManager(ConfigurationManager* config) {
    delete config;
}

__declspec(dllexport) BOOL Config_LoadFromFile(ConfigurationManager* config, const wchar_t* filePath) {
    return config ? config->LoadFromJsonFile(filePath) : FALSE;
}

__declspec(dllexport) BOOL Config_SaveToFile(ConfigurationManager* config, const wchar_t* filePath) {
    return config ? config->SaveToFile(filePath ? filePath : L"") : FALSE;
}

__declspec(dllexport) const wchar_t* Config_GetString(ConfigurationManager* config, 
    const wchar_t* key, const wchar_t* defaultValue) {
    if (!config || !key) return defaultValue;
    
    static std::wstring result;
    result = config->GetString(key, defaultValue ? defaultValue : L"");
    return result.c_str();
}

__declspec(dllexport) void Config_SetString(ConfigurationManager* config, 
    const wchar_t* key, const wchar_t* value) {
    if (config && key && value) {
        config->SetString(key, value);
    }
}

__declspec(dllexport) int Config_GetInt(ConfigurationManager* config, const wchar_t* key, int defaultValue) {
    return config ? config->GetInt(key, defaultValue) : defaultValue;
}

__declspec(dllexport) void Config_SetInt(ConfigurationManager* config, const wchar_t* key, int value) {
    if (config && key) {
        config->SetInt(key, value);
    }
}

__declspec(dllexport) BOOL Config_GetBool(ConfigurationManager* config, const wchar_t* key, BOOL defaultValue) {
    return config ? (config->GetBool(key, defaultValue != FALSE) ? TRUE : FALSE) : defaultValue;
}

__declspec(dllexport) void Config_SetBool(ConfigurationManager* config, const wchar_t* key, BOOL value) {
    if (config && key) {
        config->SetBool(key, value != FALSE);
    }
}

} // extern "C"

// ============================================================================
// DLL ENTRY POINT  
// ============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        OutputDebugStringW(L"[RawrXD_Configuration] DLL loaded\n");
        break;
    case DLL_PROCESS_DETACH:
        OutputDebugStringW(L"[RawrXD_Configuration] DLL unloaded\n");
        break;
    }
    return TRUE;
}