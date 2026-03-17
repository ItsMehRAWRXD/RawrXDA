#pragma once
#include <windows.h>
#include <string>
#include <memory>

// Forward declarations
namespace RawrXD {
    class AIIntegrationHub;
}

struct GenerationConfig;

namespace RawrXD {

class GUIMainEnhanced {
public:
    GUIMainEnhanced();
    ~GUIMainEnhanced();
    
    bool initialize(HINSTANCE hInstance);
    int run();
    void shutdown();
    
private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void createControls(HWND parent);
    void handleCommand(WORD id, WORD notifyCode);
    void sendChatMessage();
    void executeAgentCommand(const std::string& cmd);
    void appendToChatOutput(const std::string& text);
    void updateAgentConfig();
    void handleResize();
    
    HINSTANCE m_hInstance = NULL;
    HWND m_mainWindow = NULL;
    HWND m_editorWindow = NULL;
    HWND m_statusBar = NULL;
    HWND m_chatOutput = NULL;
    HWND m_chatInput = NULL;
    
    HWND m_maxModeCheck = NULL;
    HWND m_deepThinkCheck = NULL;
    HWND m_deepResearchCheck = NULL;
    HWND m_noRefusalCheck = NULL;
    
    bool m_maxMode = false;
    bool m_deepThinking = false;
    bool m_deepResearch = false;
    bool m_noRefusal = false;
    
    std::shared_ptr<AIIntegrationHub> m_hub;
};

} // namespace RawrXD
