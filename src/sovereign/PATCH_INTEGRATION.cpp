//=============================================================================
// INTEGRATION PATCH: How to wire Sovereign into agentic_engine.cpp
//
// File: src/ide/agentic_engine.cpp
// Changes: 2 locations
//=============================================================================

//=============================================================================
// CHANGE 1: Add include at top (after other #includes)
//=============================================================================

// BEFORE (line ~10):
/*
#include "agentic_engine.hpp"
#include "chat_interface.h"
#include "context_manager.h"
...
*/

// AFTER:
/*
#include "agentic_engine.hpp"
#include "chat_interface.h"
#include "context_manager.h"

#ifdef RAWRXD_WITH_SOVEREIGN
#include "sovereign/AgenticEngineSovereignHook.h"
#endif

...
*/

//=============================================================================
// CHANGE 2: Replace chat() method (search "std::string AgenticEngine::chat")
//=============================================================================

// BEFORE (existing implementation):
/*
std::string AgenticEngine::chat(const std::string& userMessage) {
    // Get model
    auto model = m_modelRouter->getModel(m_preferences.language);
    if (!model) {
        return "Error: model not loaded";
    }
    
    // Append to history
    Message msg{
        "user",
        userMessage,
        std::chrono::system_clock::now
    };
    m_chatHistory.push_back(msg);
    
    // Format prompt
    std::string prompt = buildPrompt(m_chatHistory);
    
    // Inference
    std::string response = model->infer(prompt);
    
    // Add assistant response to history
    m_chatHistory.push_back({
        "assistant",
        response,
        std::chrono::system_clock::now()
    });
    
    return response;
}
*/

// AFTER (wrapped with sovereign):
/*
std::string AgenticEngine::chat(const std::string& userMessage) {
#ifdef RAWRXD_WITH_SOVEREIGN
    // Initialize hook once
    static bool initialized = false;
    if (!initialized) {
        auto& hook = RawrXD::AgenticEngineSovereignHook::getInstance();
        hook.initialize();
        initialized = true;
    }
    
    // Process through sovereign + LLM pipeline
    auto& hook = RawrXD::AgenticEngineSovereignHook::getInstance();
    return hook.processWithSovereign(
        userMessage,
        [this](const std::string& prompt) {
            return this->originalChatImpl(prompt);
        }
    );
#else
    return this->originalChatImpl(userMessage);
#endif
}

// Renamed from original chat() to originalChatImpl()
std::string AgenticEngine::originalChatImpl(const std::string& userMessage) {
    // Get model
    auto model = m_modelRouter->getModel(m_preferences.language);
    if (!model) {
        return "Error: model not loaded";
    }
    
    // (rest of original chat() code unchanged)
    Message msg{"user", userMessage, std::chrono::system_clock::now()};
    m_chatHistory.push_back(msg);
    
    std::string prompt = buildPrompt(m_chatHistory);
    std::string response = model->infer(prompt);
    
    m_chatHistory.push_back({
        "assistant",
        response,
        std::chrono::system_clock::now()
    });
    
    return response;
}
*/

//=============================================================================
// ADD to agentic_engine.hpp header (public methods section)
//=============================================================================

// In the AgenticEngine class declaration, add:
/*
private:
    std::string originalChatImpl(const std::string& userMessage);
*/

//=============================================================================
// CHANGE 3 (optional): Add to IDE main window initialization
//
// File: src/ide/main.cpp or RawrXD_IDE_Win32.cpp in WinMain/MainWindow ctor
//=============================================================================

// After creating main window, add:

/*
#ifdef RAWRXD_WITH_SOVEREIGN
    #include "sovereign/RawrXD_SovereignStatusPanel.h"
    
    // Global panel pointer (scope carefully based on window lifetime)
    RawrXD::UI::SovereignStatusPanel* g_pSovereignPanel = nullptr;
#endif

// In wWinMain or MainWindow init:

#ifdef RAWRXD_WITH_SOVEREIGN
    g_pSovereignPanel = new RawrXD::UI::SovereignStatusPanel();
    
    // Create in right sidebar (adjust coords to your layout)
    g_pSovereignPanel->create(
        hMainWindow,
        1100,   // x position (right side)
        50,     // y position
        280,    // width
        350,    // height
        nullptr
    );
    
    // Auto-enable sovereign on startup
    g_pSovereignPanel->setSovereignEnabled(true);
    
    // Set timer for periodic refresh
    SetTimer(hMainWindow, 999, 1000, nullptr);  // 1 sec refresh
#endif
```
```cpp
// In WM_TIMER handler:

case WM_TIMER:
    if (wParam == 999) {
#ifdef RAWRXD_WITH_SOVEREIGN
        if (g_pSovereignPanel) {
            g_pSovereignPanel->refresh();
        }
#endif
    }
    break;

// In cleanup/shutdown:

#ifdef RAWRXD_WITH_SOVEREIGN
    if (g_pSovereignPanel) {
        delete g_pSovereignPanel;
        g_pSovereignPanel = nullptr;
    }
#endif
*/

//=============================================================================
// VERIFICATION: Compile & Link Check
//=============================================================================

// After applying changes, compile test:

/*
cl.exe /c src/sovereign/SovereignCoreWrapper.cpp \
         src/sovereign/AgenticEngineSovereignHook.cpp \
         src/sovereign/RawrXD_SovereignStatusPanel.cpp

link.exe /OUT:test.exe \
         src/ide/agentic_engine.obj \
         d:\rawrxd\build_out\sovereign_core.obj \
         SovereignCoreWrapper.obj \
         AgenticEngineSovereignHook.obj \
         RawrXD_SovereignStatusPanel.obj \
         kernel32.lib user32.lib gdi32.lib msvcrt.lib

// If successful:
//   - test.exe created
//   - No "unresolved external" errors
//   - Sovereign_Pipeline_Cycle, AcquireSovereignLock, etc. resolved
*/

//=============================================================================
// RUNTIME: Verify sovereign is active
//=============================================================================

// In IDE chat, user types:
// > "Hello"
//
// IDE responds with BOTH:
// 1. Standard LLM response
// 2. Sovereign cycle status appended

// Example output:
/*
User: Hello
Assistant: Hello! I'm RawrXD, your autonomous IDE. How can I help?

--- [SOVEREIGN AUTONOMOUS CYCLE] ---
Cycle #1 | Status: SYNCING | Heals: 0
Agents: A0(live)
--- [END AUTONOMOUS] ---

[Right sidebar shows:]
[SOVEREIGN AGENTIC CORE]
Cycle: 1 | Heals: 0
Status: SYNCING | Running: YES
Agents: 1 active

[Enable Sovereign] [Run Cycle]
*/
