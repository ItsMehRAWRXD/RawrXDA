// ============================================================================
// Win32IDE_AIBackend.cpp - AI Backend Verification and Testing
// Ollama connectivity verification, model availability check, inference test
// ============================================================================

#include "Win32IDE.h"
#include "ModelConnection.h"
#include "IDELogger.h"
#include <thread>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

// ============================================================================
// AI Backend Verification
// ============================================================================

bool Win32IDE::VerifyAIBackend() {
    LOG_INFO("Verifying AI backend connectivity...");
    
    try {
        // Test Ollama connection with timeout
        auto conn = std::make_unique<ModelConnection>();
        
        if (!conn) {
            LOG_ERROR("AI Backend: Failed to create ModelConnection");
            m_aiAvailable = false;
            PostMessage(m_hwndMain, WM_AI_BACKEND_FAILED, 0, 
                       reinterpret_cast<LPARAM>("Connection initialization failed"));
            return false;
        }
        
        // Async verification to avoid blocking UI
        std::thread([this, connPtr = std::move(conn)]() {
            try {
                // Check connection first
                if (!connPtr->checkConnection()) {
                    LOG_ERROR("AI Backend: Cannot connect to Ollama");
                    m_aiAvailable = false;
                    PostMessage(m_hwndMain, WM_AI_BACKEND_FAILED, 0,
                               reinterpret_cast<LPARAM>("Cannot connect to Ollama"));
                    return;
                }
                
                // Get available models
                auto models = connPtr->getAvailableModels();
                bool hasModels = !models.empty();
                
                m_aiAvailable = hasModels;
                
                if (hasModels) {
                    LOG_INFO("AI Backend verified: " + std::to_string(models.size()) + " models available");
                    std::string modelList;
                    for (const auto& model : models) {
                        if (!modelList.empty()) modelList += ", ";
                        modelList += model;
                    }
                    PostMessage(m_hwndMain, WM_AI_BACKEND_VERIFIED, 
                               static_cast<WPARAM>(models.size()), 
                               reinterpret_cast<LPARAM>(new std::string(modelList)));
                } else {
                    LOG_ERROR("AI Backend: No models available");
                    PostMessage(m_hwndMain, WM_AI_BACKEND_FAILED, 0,
                               reinterpret_cast<LPARAM>("No models loaded"));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("AI Backend verification failed: " + std::string(e.what()));
                m_aiAvailable = false;
                PostMessage(m_hwndMain, WM_AI_BACKEND_FAILED, 0,
                           reinterpret_cast<LPARAM>(e.what()));
            }
        }).detach();
        
        return true;  // Verification started
        
    } catch (const std::exception& e) {
        LOG_ERROR("AI Backend verification exception: " + std::string(e.what()));
        m_aiAvailable = false;
        return false;
    }
}

// ============================================================================
// Debug Test AI
// ============================================================================

void Win32IDE::runDebugTestAI() {
    if (!m_aiAvailable) {
        MessageBoxA(m_hwndMain, 
                   "AI Backend not available.\n\nCheck:\n1. Ollama is running\n2. Models are loaded\n3. Network connectivity", 
                   "AI Backend Offline", MB_OK | MB_ICONWARNING);
        return;
    }
    
    try {
        auto conn = std::make_unique<ModelConnection>();
        
        if (!conn) {
            MessageBoxA(m_hwndMain, "Failed to create connection", "Error", MB_OK | MB_ICONERROR);
            return;
        }
        
        // Quick test prompt
        const char* testPrompt = "Say 'RawrXD AI Backend Operational' and nothing else.";
        
        LOG_INFO("Running AI backend debug test...");
        
        // Send test request with callback
        conn->sendPrompt(
            "mistral",  // Default model, can be configurable
            testPrompt,
            {},  // No context
            [this](const std::string& response) {
                // Response callback
                LOG_INFO("AI Test response: " + response.substr(0, 100));
                MessageBoxA(m_hwndMain, response.c_str(), "AI Test Success", MB_OK);
            },
            [this](const std::string& error) {
                // Error callback
                LOG_ERROR("AI Test failed: " + error);
                MessageBoxA(m_hwndMain, ("Test failed: " + error).c_str(), "AI Test Failed", MB_OK | MB_ICONERROR);
            },
            []() {
                // Complete callback
                LOG_INFO("AI Test completed");
            }
        );
            
    } catch (const std::exception& e) {
        LOG_ERROR("Debug test exception: " + std::string(e.what()));
        MessageBoxA(m_hwndMain, e.what(), "AI Test Exception", MB_OK | MB_ICONERROR);
    }
}

// ============================================================================
// AI Backend Status Handler
// ============================================================================

void Win32IDE::onAIBackendVerified(bool success, const std::string& status) {
    m_aiAvailable = success;
    
    LOG_INFO(std::string("AI Backend status: ") + (success ? "Online" : "Offline") + " - " + status);
    
    // Update status bar
    if (m_hStatusBar) {
        const char* statusText = success ? "AI: Online" : "AI: Offline";
        SendMessageA(m_hStatusBar, SB_SETTEXT, 3, 
                    reinterpret_cast<LPARAM>(statusText));
    }
    
    // Enable/disable menu item
    updateToolsMenu();
}

// ============================================================================
// Update Tools Menu
// ============================================================================

void Win32IDE::updateToolsMenu() {
    HMENU hMenu = GetMenu(m_hwndMain);
    if (!hMenu) return;
    
    // Find Tools menu (typically the 3rd menu, index 2)
    int toolsMenuIndex = -1;
    int menuCount = GetMenuItemCount(hMenu);
    
    for (int i = 0; i < menuCount; i++) {
        char menuText[256] = { 0 };
        if (GetMenuStringA(hMenu, i, menuText, sizeof(menuText) - 1, MF_BYPOSITION)) {
            if (strstr(menuText, "Tools") || strstr(menuText, "tools")) {
                toolsMenuIndex = i;
                break;
            }
        }
    }
    
    if (toolsMenuIndex < 0) {
        LOG_WARNING("Tools menu not found");
        return;
    }
    
    HMENU hTools = GetSubMenu(hMenu, toolsMenuIndex);
    if (!hTools) return;
    
    // Check if the menu item already exists
    int itemCount = GetMenuItemCount(hTools);
    bool itemExists = false;
    
    for (int i = 0; i < itemCount; i++) {
        UINT cmdId = GetMenuItemID(hTools, i);
        if (cmdId == IDM_DEBUG_TEST_AI) {
            itemExists = true;
            break;
        }
    }
    
    // Insert menu item if it doesn't exist
    if (!itemExists) {
        InsertMenuA(hTools, -1, MF_BYPOSITION | MF_STRING, IDM_DEBUG_TEST_AI, 
                   "Test AI Backend\tCtrl+Shift+T");
    }
    
    // Set state based on availability
    EnableMenuItem(hTools, IDM_DEBUG_TEST_AI, 
                  MF_BYCOMMAND | (m_aiAvailable ? MF_ENABLED : MF_GRAYED));
    
    DrawMenuBar(m_hwndMain);
}

// ============================================================================
// Deferred Heavy Init Integration
// ============================================================================
// This function is called from within deferredHeavyInitBody in Win32IDE_Core.cpp
// to initialize AI backend asynchronously without blocking the UI

void Win32IDE::initializeAIBackend() {
    LOG_INFO("Starting AI backend initialization...");
    
    // Start verification in background
    if (!VerifyAIBackend()) {
        LOG_WARNING("AI backend verification async task failed to start");
        // Continue anyway - don't block IDE startup
    }
    
    // Update menus to reflect current state
    updateToolsMenu();
}
