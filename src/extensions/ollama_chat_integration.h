// ollama_chat_integration.h - Chat panel integration for Ollama model provider
#pragma once

#include "ollama_model_provider.h"
#include <windows.h>
#include <commctrl.h>

namespace RawrXD::Extensions::Ollama {

class OllamaChatIntegration {
private:
    HWND m_chatWindow;
    HWND m_modelDropdown;
    HWND m_modeDropdown;
    OllamaModelProvider* m_provider;
    
    std::vector<ModelInfo> m_availableModels;
    ModelInfo m_currentModel;
    uint32_t m_currentMode;
    
    static constexpr int MODEL_DROPDOWN_ID = 1001;
    static constexpr int MODE_DROPDOWN_ID = 1002;

public:
    OllamaChatIntegration(HWND chatWindow, OllamaModelProvider* provider);
    ~OllamaChatIntegration();

    bool Initialize();
    void Update();
    void RefreshModels();
    
    ModelInfo GetSelectedModel() const;
    uint32_t GetSelectedMode() const;
    
    // Message handling
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    void CreateControls();
    void PopulateModelDropdown();
    void PopulateModeDropdown();
    void OnModelChanged();
    void OnModeChanged();
    
    static LRESULT CALLBACK SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, 
                                        UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
};

} // namespace RawrXD::Extensions::Ollama
