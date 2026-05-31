// ollama_chat_integration.cpp - Chat panel integration implementation
#include "ollama_chat_integration.h"
#include <algorithm>

namespace RawrXD::Extensions::Ollama {

OllamaChatIntegration::OllamaChatIntegration(HWND chatWindow, OllamaModelProvider* provider)
    : m_chatWindow(chatWindow)
    , m_provider(provider)
    , m_modelDropdown(nullptr)
    , m_modeDropdown(nullptr)
    , m_currentMode(CAP_MAX) {
}

OllamaChatIntegration::~OllamaChatIntegration() {
    if (m_modelDropdown) {
        RemoveWindowSubclass(m_modelDropdown, SubclassProc, reinterpret_cast<DWORD_PTR>(this));
    }
    if (m_modeDropdown) {
        RemoveWindowSubclass(m_modeDropdown, SubclassProc, reinterpret_cast<DWORD_PTR>(this));
    }
}

bool OllamaChatIntegration::Initialize() {
    CreateControls();
    RefreshModels();
    
    // Get current model and mode from provider
    m_currentModel = m_provider->GetCurrentModel();
    m_currentMode = m_provider->GetCurrentMode(m_chatWindow);
    
    // Set initial selections
    if (m_modelDropdown) {
        for (int i = 0; i < m_availableModels.size(); ++i) {
            if (m_availableModels[i].id == m_currentModel.id) {
                ComboBox_SetCurSel(m_modelDropdown, i);
                break;
            }
        }
    }
    
    if (m_modeDropdown) {
        ComboBox_SetCurSel(m_modeDropdown, static_cast<int>(m_currentMode));
    }
    
    return true;
}

void OllamaChatIntegration::Update() {
    // Refresh model list if needed
    if (m_provider->IsConnected()) {
        RefreshModels();
    }
}

void OllamaChatIntegration::RefreshModels() {
    m_availableModels = m_provider->DiscoverModels(false);
    PopulateModelDropdown();
}

ModelInfo OllamaChatIntegration::GetSelectedModel() const {
    return m_currentModel;
}

uint32_t OllamaChatIntegration::GetSelectedMode() const {
    return m_currentMode;
}

void OllamaChatIntegration::CreateControls() {
    RECT clientRect;
    GetClientRect(m_chatWindow, &clientRect);
    
    // Create model dropdown
    m_modelDropdown = CreateWindowEx(0, WC_COMBOBOX, L"", 
                                    WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                    10, 10, 200, 200, 
                                    m_chatWindow, 
                                    reinterpret_cast<HMENU>(MODEL_DROPDOWN_ID),
                                    GetModuleHandle(nullptr), nullptr);
    
    if (m_modelDropdown) {
        SetWindowSubclass(m_modelDropdown, SubclassProc, 
                         reinterpret_cast<DWORD_PTR>(this), 0);
    }
    
    // Create mode dropdown
    m_modeDropdown = CreateWindowEx(0, WC_COMBOBOX, L"", 
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                                   220, 10, 120, 200, 
                                   m_chatWindow, 
                                   reinterpret_cast<HMENU>(MODE_DROPDOWN_ID),
                                   GetModuleHandle(nullptr), nullptr);
    
    if (m_modeDropdown) {
        SetWindowSubclass(m_modeDropdown, SubclassProc, 
                         reinterpret_cast<DWORD_PTR>(this), 0);
        PopulateModeDropdown();
    }
}

void OllamaChatIntegration::PopulateModelDropdown() {
    if (!m_modelDropdown) return;
    
    ComboBox_ResetContent(m_modelDropdown);
    
    for (const auto& model : m_availableModels) {
        std::wstring wideName = UTF8ToWide(model.displayName);
        ComboBox_AddString(m_modelDropdown, wideName.c_str());
    }
    
    // Select current model
    for (int i = 0; i < m_availableModels.size(); ++i) {
        if (m_availableModels[i].id == m_currentModel.id) {
            ComboBox_SetCurSel(m_modelDropdown, i);
            break;
        }
    }
}

void OllamaChatIntegration::PopulateModeDropdown() {
    if (!m_modeDropdown) return;
    
    ComboBox_ResetContent(m_modeDropdown);
    
    const wchar_t* modeNames[] = {
        L"Agent",
        L"Ask",
        L"Plan",
        L"MAX"
    };
    
    for (int i = 0; i < 4; ++i) {
        ComboBox_AddString(m_modeDropdown, modeNames[i]);
    }
    
    ComboBox_SetCurSel(m_modeDropdown, static_cast<int>(m_currentMode));
}

void OllamaChatIntegration::OnModelChanged() {
    if (!m_modelDropdown) return;
    
    int selected = ComboBox_GetCurSel(m_modelDropdown);
    if (selected >= 0 && selected < m_availableModels.size()) {
        m_currentModel = m_availableModels[selected];
        m_provider->SetCurrentModel(m_currentModel.id);
    }
}

void OllamaChatIntegration::OnModeChanged() {
    if (!m_modeDropdown) return;
    
    int selected = ComboBox_GetCurSel(m_modeDropdown);
    if (selected >= 0 && selected < 4) {
        m_currentMode = static_cast<uint32_t>(selected);
        m_provider->SetCurrentMode(m_chatWindow, m_currentMode);
    }
}

LRESULT OllamaChatIntegration::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND: {
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                if (LOWORD(wParam) == MODEL_DROPDOWN_ID) {
                    OnModelChanged();
                    return 0;
                } else if (LOWORD(wParam) == MODE_DROPDOWN_ID) {
                    OnModeChanged();
                    return 0;
                }
            }
            break;
        }
    }
    
    return DefSubclassProc(m_chatWindow, msg, wParam, lParam);
}

LRESULT CALLBACK OllamaChatIntegration::SubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, 
                                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    OllamaChatIntegration* integration = reinterpret_cast<OllamaChatIntegration*>(dwRefData);
    
    if (integration) {
        return integration->HandleMessage(msg, wParam, lParam);
    }
    
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

} // namespace RawrXD::Extensions::Ollama
