// ==============================================================================
// Win32IDE_AmphibiousIntegration.cpp
// Win32IDE properly integrated with RawrXD Amphibious ML System
// ==============================================================================

#include "Win32IDE_AmphibiousIntegration.h"
#include <algorithm>
#include <chrono>
#include <iostream>

// Globals
WNDCLASSA Win32IDE_Window::m_wndClass = {};
const char* Win32IDE_Window::m_className = "Win32IDE_AmphibiousML_Class";

// ==============================================================================
// Win32IDE_MLIntegration Implementation
// ==============================================================================

Win32IDE_MLIntegration::Win32IDE_MLIntegration(HWND hEditor, HWND hStatusBar)
    : m_hEditor(hEditor)
    , m_hStatusBar(hStatusBar)
    , m_status(OperationStatus::Idle)
    , m_initialized(false)
{
    memset(&m_metrics, 0, sizeof(m_metrics));
}

Win32IDE_MLIntegration::~Win32IDE_MLIntegration() {
    if (IsInferencing()) {
        CancelInference();
    }
}

bool Win32IDE_MLIntegration::Initialize(const std::string& modelPath) {
    m_modelPath = modelPath.empty() ? "" : modelPath;
    
    // Call MASM bridge to initialize ML runtime
    int result = Win32IDE_InitializeML(m_hEditor, m_hStatusBar, 
                                       m_modelPath.empty() ? nullptr : m_modelPath.c_str());
    
    if (result == 0) {
        m_initialized = true;
        SetStatus(OperationStatus::Idle);
        UpdateStatusBar("ML Runtime Ready");
        return true;
    }
    
    UpdateStatusBar("ML Runtime Failed to Initialize");
    return false;
}

bool Win32IDE_MLIntegration::StartInference(const std::string& selectedCode, const std::string& userPrompt) {
    if (!m_initialized) {
        OnStreamError("ML Runtime not initialized");
        return false;
    }
    
    if (IsInferencing()) {
        OnStreamError("Inference already in progress");
        return false;
    }
    
    SetStatus(OperationStatus::Inferencing);
    
    m_selectedCode = selectedCode;
    m_userPrompt = userPrompt;
    m_originalText = selectedCode; // For potential revert
    m_accumulatedOutput.clear();
    m_tokenBuffer.clear();
    
    // Get current time for metrics
    LARGE_INTEGER freq, start;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    m_metrics.startTime = start.QuadPart;
    m_metrics.tokensGenerated = 0;
    m_metrics.success = false;
    
    // Allocate output buffer
    void* outputBuffer = malloc(STREAM_BUFFER_SIZE);
    
    // Launch inference via MASM bridge
    int result = Win32IDE_StartInference(
        m_hEditor,
        selectedCode.c_str(),
        userPrompt.c_str(),
        outputBuffer
    );
    
    free(outputBuffer);
    
    if (result != 0) {
        SetStatus(OperationStatus::Error);
        OnStreamError("Failed to start inference");
        return false;
    }
    
    SetStatus(OperationStatus::Streaming);
    UpdateStatusBar("Streaming tokens from ML model...");
    return true;
}

bool Win32IDE_MLIntegration::CancelInference() {
    if (!IsInferencing()) {
        return false;
    }
    
    // Revert editor text
    Win32IDE_CancelInference(m_hEditor, m_originalText.c_str());
    
    SetStatus(OperationStatus::Canceled);
    UpdateStatusBar("Inference canceled, changes reverted");
    
    return true;
}

void Win32IDE_MLIntegration::OnTokenReceived(const std::string& token) {
    {
        std::lock_guard<std::mutex> lock(m_streamMutex);
        m_accumulatedOutput += token;
        m_tokenBuffer.push_back(token);
        m_metrics.tokensGenerated++;
    }
    
    // Stream token to editor
    Win32IDE_StreamTokenToEditor(m_hEditor, token.c_str(), token.length(), 0);
    
    // Callback to UI
    if (m_tokenCallback) {
        m_tokenCallback(token, false);
    }
}

void Win32IDE_MLIntegration::OnStreamComplete() {
    // Final token
    Win32IDE_StreamTokenToEditor(m_hEditor, "", 0, 1);
    
    LARGE_INTEGER freq, end;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&end);
    m_metrics.endTime = end.QuadPart;
    m_metrics.durationMs = (double)(m_metrics.endTime - m_metrics.startTime) / 
                          (double)freq.QuadPart * 1000.0;
    m_metrics.success = true;
    
    SetStatus(OperationStatus::Committed);
    UpdateStatusBar(std::string("Complete: ") + std::to_string(m_metrics.tokensGenerated) + 
                   " tokens in " + std::to_string((int)m_metrics.durationMs) + "ms");
    
    if (m_tokenCallback) {
        m_tokenCallback("", true);
    }
}

void Win32IDE_MLIntegration::OnStreamError(const std::string& errorMsg) {
    m_metrics.success = false;
    m_metrics.errorMessage = errorMsg;
    
    SetStatus(OperationStatus::Error);
    UpdateStatusBar(std::string("Error: ") + errorMsg);
    
    if (m_statusCallback) {
        m_statusCallback(OperationStatus::Error);
    }
}

void Win32IDE_MLIntegration::SetStatus(OperationStatus status) {
    m_status = status;
    if (m_statusCallback) {
        m_statusCallback(status);
    }
}

void Win32IDE_MLIntegration::UpdateStatusBar(const std::string& message) {
    if (m_hStatusBar) {
        SendMessageA(m_hStatusBar, SB_SETTEXT, 0, (LPARAM)message.c_str());
    }
}

void Win32IDE_MLIntegration::AppendTokenToEditor(const std::string& token) {
    if (!m_hEditor) return;
    
    // Move cursor to end
    SendMessageA(m_hEditor, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    
    // Insert token
    SendMessageA(m_hEditor, EM_REPLACESEL, 0, (LPARAM)token.c_str());
}

bool Win32IDE_MLIntegration::WriteTelemetry(const std::string& outputPath) {
    Json::Value telemetry = BuildTelemetryJSON();
    
    std::ofstream file(outputPath);
    if (!file.is_open()) {
        return false;
    }
    
    file << telemetry.toStyledString();
    file.close();
    
    return true;
}

Json::Value Win32IDE_MLIntegration::BuildTelemetryJSON() {
    Json::Value root;
    
    root["event"] = "inference_cycle";
    root["success"] = m_metrics.success;
    root["timestamp"] = (Json::Value::Int64)m_metrics.startTime;
    
    Json::Value metrics;
    metrics["tokens_generated"] = m_metrics.tokensGenerated;
    metrics["duration_ms"] = m_metrics.durationMs;
    metrics["tokens_per_second"] = m_metrics.tokensGenerated / (m_metrics.durationMs / 1000.0);
    
    root["metrics"] = metrics;
    
    if (!m_metrics.errorMessage.empty()) {
        root["error"] = m_metrics.errorMessage;
    }
    
    return root;
}

// ==============================================================================
// Inference Thread Manager Implementation
// ==============================================================================

InferenceThreadManager::InferenceThreadManager(Win32IDE_MLIntegration* mlIntegration)
    : m_mlIntegration(mlIntegration)
    , m_running(false)
{
}

InferenceThreadManager::~InferenceThreadManager() {
    Stop();
}

bool InferenceThreadManager::StartInference(const std::string& prompt) {
    if (m_running) {
        return false;
    }
    
    m_running = true;
    m_thread = std::thread(&InferenceThreadManager::InferenceWorker, this, prompt);
    
    return true;
}

void InferenceThreadManager::Stop() {
    m_running = false;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void InferenceThreadManager::InferenceWorker(const std::string& prompt) {
    SOCKET sock = SOCKET_ERROR;
    
    try {
        if (!ConnectToLLMServer()) {
            m_mlIntegration->OnStreamError("Failed to connect to LLM server (127.0.0.1:8080)");
            return;
        }
        
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        
        if (!SendInferenceRequest(sock, prompt)) {
            m_mlIntegration->OnStreamError("Failed to send inference request");
            return;
        }
        
        ReceiveTokenStream(sock);
        m_mlIntegration->OnStreamComplete();
    }
    catch (const std::exception& e) {
        m_mlIntegration->OnStreamError(std::string("Exception: ") + e.what());
    }
    
    if (sock != SOCKET_ERROR) {
        closesocket(sock);
    }
    
    m_running = false;
}

bool InferenceThreadManager::ConnectToLLMServer() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }
    
    return true;
}

bool InferenceThreadManager::SendInferenceRequest(SOCKET sock, const std::string& prompt) {
    // Build JSON request for llama.cpp /v1/chat/completions
    Json::Value request;
    request["model"] = "local";
    request["stream"] = true;
    request["max_tokens"] = 2048;
    
    Json::Value messages(Json::arrayValue);
    Json::Value systemMsg;
    systemMsg["role"] = "system";
    systemMsg["content"] = "You are a helpful code assistant. Generate code improvements.";
    messages.append(systemMsg);
    
    Json::Value userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = prompt;
    messages.append(userMsg);
    
    request["messages"] = messages;
    
    std::string jsonBody = request.toStyledString();
    
    // Build HTTP request
    std::string httpRequest = "POST /v1/chat/completions HTTP/1.1\r\n";
    httpRequest += "Host: 127.0.0.1:8080\r\n";
    httpRequest += "Content-Type: application/json\r\n";
    httpRequest += std::string("Content-Length: ") + std::to_string(jsonBody.length()) + "\r\n";
    httpRequest += "Connection: close\r\n";
    httpRequest += "\r\n";
    httpRequest += jsonBody;
    
    // Send request
    int result = send(sock, httpRequest.c_str(), (int)httpRequest.length(), 0);
    
    return result != SOCKET_ERROR;
}

void InferenceThreadManager::ReceiveTokenStream(SOCKET sock) {
    char buffer[4096];
    std::string accumulated;
    
    while (m_running) {
        int bytesRecv = recv(sock, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesRecv == 0 || bytesRecv == SOCKET_ERROR) {
            break;
        }
        
        buffer[bytesRecv] = '\0';
        accumulated += buffer;
        
        // Parse streaming JSON chunks and extract tokens
        size_t pos = 0;
        while ((pos = accumulated.find("\"content\":\"", pos)) != std::string::npos) {
            pos += 11;
            size_t endPos = accumulated.find("\"", pos);
            
            if (endPos != std::string::npos) {
                std::string token = accumulated.substr(pos, endPos - pos);
                
                // Handle escape sequences
                size_t escapePos;
                while ((escapePos = token.find("\\n")) != std::string::npos) {
                    token.replace(escapePos, 2, "\n");
                }
                
                m_mlIntegration->OnTokenReceived(token);
                pos = endPos + 1;
            }
            else {
                break;
            }
        }
    }
}

// ==============================================================================
// Win32IDE_Window Implementation
// ==============================================================================

Win32IDE_Window::Win32IDE_Window()
    : m_hWindow(nullptr)
    , m_hEditorControl(nullptr)
    , m_hStatusBar(nullptr)
    , m_hPromptInput(nullptr)
    , m_hOutputPane(nullptr)
{
}

Win32IDE_Window::~Win32IDE_Window() {
    if (m_hWindow) {
        DestroyWindow(m_hWindow);
    }
}

bool Win32IDE_Window::Create(const std::string& title, int width, int height) {
    // Register window class
    m_wndClass.lpfnWndProc = WindowProcStatic;
    m_wndClass.lpszClassName = m_className;
    m_wndClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
    m_wndClass.hIcon = LoadIconA(nullptr, IDI_APPLICATION);
    m_wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    m_wndClass.style = CS_VREDRAW | CS_HREDRAW;
    
    if (!RegisterClassA(&m_wndClass)) {
        return false;
    }
    
    // Create window
    m_hWindow = CreateWindowExA(
        0,
        m_className,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        GetModuleHandleA(nullptr),
        this
    );
    
    if (!m_hWindow) {
        return false;
    }
    
    // Create controls
    CreateControls();
    
    // Initialize ML integration
    m_mlIntegration = std::make_unique<Win32IDE_MLIntegration>(m_hEditorControl, m_hStatusBar);
    
    return true;
}

bool Win32IDE_Window::Show(int nCmdShow) {
    if (!m_hWindow) {
        return false;
    }
    
    ShowWindow(m_hWindow, nCmdShow);
    UpdateWindow(m_hWindow);
    
    return true;
}

int Win32IDE_Window::Run() {
    MSG msg;
    
    while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    return (int)msg.wParam;
}

void Win32IDE_Window::Close() {
    if (m_hWindow) {
        DestroyWindow(m_hWindow);
        m_hWindow = nullptr;
    }
}

void Win32IDE_Window::CreateControls() {
    RECT clientRect;
    GetClientRect(m_hWindow, &clientRect);
    
    // Editor control
    m_hEditorControl = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        10,
        10,
        clientRect.right - 20,
        clientRect.bottom - 100,
        m_hWindow,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
    
    // Status bar
    m_hStatusBar = CreateWindowExA(
        0,
        STATUSCLASSNAMEA,
        "Ready",
        WS_CHILD | WS_VISIBLE | SBT_NOBORDERS,
        0,
        clientRect.bottom - 20,
        clientRect.right,
        20,
        m_hWindow,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
    
    // Prompt input
    m_hPromptInput = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        "EDIT",
        "",
        WS_CHILD | WS_VISIBLE,
        10,
        clientRect.bottom - 70,
        clientRect.right - 120,
        25,
        m_hWindow,
        nullptr,
        GetModuleHandleA(nullptr),
        nullptr
    );
    
    // Execute button
    CreateWindowExA(
        0,
        "BUTTON",
        "Execute Inference",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        clientRect.right - 100,
        clientRect.bottom - 70,
        90,
        25,
        m_hWindow,
        (HMENU)1001,
        GetModuleHandleA(nullptr),
        nullptr
    );
}

LRESULT CALLBACK Win32IDE_Window::WindowProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Win32IDE_Window* pThis;
    
    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<Win32IDE_Window*>(pCreate->lpCreateParams);
        SetWindowLongA(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else {
        pThis = reinterpret_cast<Win32IDE_Window*>(GetWindowLongA(hwnd, GWLP_USERDATA));
    }
    
    if (pThis) {
        return pThis->WindowProc(hwnd, msg, wParam, lParam);
    }
    
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

LRESULT Win32IDE_Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        
        case WM_COMMAND:
            if (LOWORD(wParam) == 1001) {
                OnExecuteButton();
            }
            return 0;
        
        case WM_SIZE:
            if (m_mlIntegration) {
                LayoutControls();
            }
            return 0;
    }
    
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

void Win32IDE_Window::OnExecuteButton() {
    if (!m_mlIntegration->IsInitialized()) {
        m_mlIntegration->Initialize();
    }
    
    // Get selected code from editor
    int selStart, selEnd;
    SendMessageA(m_hEditorControl, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    
    char selectedCode[4096] = {0};
    SendMessageA(m_hEditorControl, WM_GETTEXT, sizeof(selectedCode), (LPARAM)selectedCode);
    
    // Get prompt from input
    char prompt[1024] = {0};
    GetWindowTextA(m_hPromptInput, prompt, sizeof(prompt));
    
    // Start inference
    m_mlIntegration->StartInference(selectedCode, prompt);
}

void Win32IDE_Window::LayoutControls() {
    RECT clientRect;
    GetClientRect(m_hWindow, &clientRect);
    
    // Resize editor
    MoveWindow(m_hEditorControl, 10, 10, clientRect.right - 20, clientRect.bottom - 100, TRUE);
    
    // Resize prompt input
    MoveWindow(m_hPromptInput, 10, clientRect.bottom - 70, clientRect.right - 120, 25, TRUE);
    
    // Resize status bar
    MoveWindow(m_hStatusBar, 0, clientRect.bottom - 20, clientRect.right, 20, TRUE);
}

// ==============================================================================
// Main Entry Point
// ==============================================================================

int main() {
    // Initialize COM
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    
    Win32IDE_Window window;
    
    if (!window.Create("RawrXD Win32IDE + Amphibious ML System", 1200, 800)) {
        return 1;
    }
    
    window.Show(SW_SHOW);
    
    int result = window.Run();
    
    CoUninitialize();
    
    return result;
}
