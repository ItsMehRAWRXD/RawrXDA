#include "gui_main.h"
#include "../include/ai_integration_hub.h"
#include "agentic_engine.h"
#include <commctrl.h>
#include <richedit.h>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {

// Control IDs
#define IDC_EDITOR 1001
#define IDC_CHAT_OUTPUT 1002
#define IDC_CHAT_INPUT 1003
#define IDC_STATUS 1004
#define IDC_MAXMODE 1005
#define IDC_DEEPTHINK 1006
#define IDC_DEEPRESEARCH 1007
#define IDC_NOREFUSAL 1008
#define ID_FILE_NEW 2001
#define ID_FILE_OPEN 2002
#define ID_FILE_SAVE 2003
#define ID_AGENT_PLAN 2010
#define ID_AGENT_BUGREPORT 2011
#define ID_AGENT_SUGGEST 2012
#define ID_AGENT_REACTSERVER 2013

GUIMainEnhanced::GUIMainEnhanced() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    LoadLibrary(TEXT("Msftedit.dll"));
}

GUIMainEnhanced::~GUIMainEnhanced() {
    shutdown();
}

LRESULT CALLBACK GUIMainEnhanced::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GUIMainEnhanced* pThis = nullptr;
    
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (GUIMainEnhanced*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (GUIMainEnhanced*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (pThis) {
        return pThis->handleMessage(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT GUIMainEnhanced::handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            createControls(hwnd);
            return 0;
            
        case WM_SIZE:
            handleResize();
            return 0;
            
        case WM_COMMAND:
            handleCommand(LOWORD(wParam), HIWORD(wParam));
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool GUIMainEnhanced::initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    
    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = TEXT("RawrXD_GUI_Enhanced");
    
    if (!RegisterClassEx(&wc)) return false;
    
    // Create main window
    m_mainWindow = CreateWindowEx(
        0, TEXT("RawrXD_GUI_Enhanced"), TEXT("RawrXD Agentic IDE"),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
        NULL, NULL, hInstance, this
    );
    
    if (!m_mainWindow) return false;
    
    // Initialize AIIntegrationHub
    m_hub = std::make_shared<RawrXD::AIIntegrationHub>();
    m_hub->initialize();
    
    ShowWindow(m_mainWindow, SW_SHOW);
    UpdateWindow(m_mainWindow);
    
    return true;
}

void GUIMainEnhanced::createControls(HWND parent) {
    // Status Bar
    m_statusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
        WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, parent, (HMENU)IDC_STATUS,
        m_hInstance, NULL);
    
    // Editor (RichEdit)
    m_editorWindow = CreateWindowEx(0, TEXT("RICHEDIT50W"), TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        10, 50, 700, 600, parent, (HMENU)IDC_EDITOR, m_hInstance, NULL);
    
    // Chat Output
    m_chatOutput = CreateWindowEx(0, TEXT("RICHEDIT50W"), TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        720, 50, 450, 500, parent, (HMENU)IDC_CHAT_OUTPUT, m_hInstance, NULL);
    
    // Chat Input
    m_chatInput = CreateWindowEx(0, TEXT("EDIT"), TEXT(""),
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        720, 560, 450, 30, parent, (HMENU)IDC_CHAT_INPUT, m_hInstance, NULL);
    
    // Mode Checkboxes
    int yPos = 600;
    m_maxModeCheck = CreateWindowEx(0, TEXT("BUTTON"), TEXT("Max Mode"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        720, yPos, 150, 25, parent, (HMENU)IDC_MAXMODE, m_hInstance, NULL);
    
    m_deepThinkCheck = CreateWindowEx(0, TEXT("BUTTON"), TEXT("Deep Thinking"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        870, yPos, 150, 25, parent, (HMENU)IDC_DEEPTHINK, m_hInstance, NULL);
    
    yPos += 30;
    m_deepResearchCheck = CreateWindowEx(0, TEXT("BUTTON"), TEXT("Deep Research"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        720, yPos, 150, 25, parent, (HMENU)IDC_DEEPRESEARCH, m_hInstance, NULL);
    
    m_noRefusalCheck = CreateWindowEx(0, TEXT("BUTTON"), TEXT("No Refusal"),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        870, yPos, 150, 25, parent, (HMENU)IDC_NOREFUSAL, m_hInstance, NULL);
    
    // Send Button
    CreateWindowEx(0, TEXT("BUTTON"), TEXT("Send"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        1100, 560, 70, 30, parent, (HMENU)2020, m_hInstance, NULL);
}

void GUIMainEnhanced::handleCommand(WORD id, WORD notifyCode) {
    switch (id) {
        case 2020: // Send button
            sendChatMessage();
            break;
            
        case IDC_MAXMODE:
            m_maxMode = (BST_CHECKED == SendMessage(m_maxModeCheck, BM_GETCHECK, 0, 0));
            updateAgentConfig();
            break;
            
        case IDC_DEEPTHINK:
            m_deepThinking = (BST_CHECKED == SendMessage(m_deepThinkCheck, BM_GETCHECK, 0, 0));
            updateAgentConfig();
            break;
            
        case IDC_DEEPRESEARCH:
            m_deepResearch = (BST_CHECKED == SendMessage(m_deepResearchCheck, BM_GETCHECK, 0, 0));
            updateAgentConfig();
            break;
            
        case IDC_NOREFUSAL:
            m_noRefusal = (BST_CHECKED == SendMessage(m_noRefusalCheck, BM_GETCHECK, 0, 0));
            updateAgentConfig();
            break;
            
        case ID_AGENT_PLAN:
            executeAgentCommand("/plan ");
            break;
            
        case ID_AGENT_BUGREPORT:
            executeAgentCommand("/bugreport ");
            break;
            
        case ID_AGENT_SUGGEST:
            executeAgentCommand("/suggest ");
            break;
            
        case ID_AGENT_REACTSERVER:
            executeAgentCommand("/react-server ");
            break;
    }
}

void GUIMainEnhanced::sendChatMessage() {
    char buffer[1024];
    GetWindowTextA(m_chatInput, buffer, sizeof(buffer));
    std::string message(buffer);
    
    if (message.empty()) return;
    
    // Clear input
    SetWindowTextA(m_chatInput, "");
    
    // Add to output
    appendToChatOutput("You: " + message + "\r\n");
    
    // Process through hub
    if (m_hub) {
        std::string response = m_hub->chat(message);
        appendToChatOutput("Agent: " + response + "\r\n\r\n");
    }
}

void GUIMainEnhanced::executeAgentCommand(const std::string& cmd) {
    // Get selection from editor or use empty
    char buffer[4096];
    GETTEXTLENGTHEX gtl = {GTL_DEFAULT, CP_ACP};
    int len = SendMessageA(m_editorWindow, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    if (len > 0 && len < sizeof(buffer)) {
        GETTEXTEX gt = {len + 1, GT_DEFAULT, CP_ACP, NULL, NULL};
        SendMessageA(m_editorWindow, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)buffer);
        std::string code(buffer);
        
        appendToChatOutput("Executing: " + cmd + "\r\n");
        if (m_hub) {
            std::string result = m_hub->chat(cmd + code);
            appendToChatOutput("Result: " + result + "\r\n\r\n");
        }
    }
}

void GUIMainEnhanced::appendToChatOutput(const std::string& text) {
    GETTEXTLENGTHEX gtl = {GTL_DEFAULT, CP_ACP};
    int len = SendMessageA(m_chatOutput, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    
    CHARRANGE cr;
    cr.cpMin = len;
    cr.cpMax = len;
    SendMessage(m_chatOutput, EM_EXSETSEL, 0, (LPARAM)&cr);
    SendMessageA(m_chatOutput, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

void GUIMainEnhanced::updateAgentConfig() {
    if (!m_hub) return;
    
    GenerationConfig config;
    config.maxMode = m_maxMode;
    config.deepThinking = m_deepThinking;
    config.deepResearch = m_deepResearch;
    config.noRefusal = m_noRefusal;
    
    m_hub->updateAgentConfig(config);
    
    // Update status
    std::string status = "Modes: ";
    if (m_maxMode) status += "[MAX] ";
    if (m_deepThinking) status += "[THINK] ";
    if (m_deepResearch) status += "[RESEARCH] ";
    if (m_noRefusal) status += "[UNCENSORED] ";
    
    SetWindowTextA(m_statusBar, status.c_str());
}

void GUIMainEnhanced::handleResize() {
    // Simple layout adjustment on resize
    RECT rc;
    GetClientRect(m_mainWindow, &rc);
    
    // Status bar auto-sizes
    SendMessage(m_statusBar, WM_SIZE, 0, 0);
    
    // Resize editor and chat panels proportionally
    int editorWidth = (rc.right * 3) / 5;
    SetWindowPos(m_editorWindow, NULL, 10, 50, editorWidth - 20, rc.bottom - 100, SWP_NOZORDER);
    
    int chatX = editorWidth + 10;
    int chatWidth = rc.right - chatX - 10;
    SetWindowPos(m_chatOutput, NULL, chatX, 50, chatWidth, rc.bottom - 200, SWP_NOZORDER);
    SetWindowPos(m_chatInput, NULL, chatX, rc.bottom - 140, chatWidth, 30, SWP_NOZORDER);
}

int GUIMainEnhanced::run() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

void GUIMainEnhanced::shutdown() {
    if (m_hub) {
        m_hub->shutdown();
    }
    if (m_mainWindow) {
        DestroyWindow(m_mainWindow);
        m_mainWindow = NULL;
    }
}

} // namespace RawrXD
