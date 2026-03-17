// ==============================================================================
// Win32IDE_AmphibiousIntegration.h
// Pure Win32 IDE with RawrXD Amphibious ML System Integration
// No Qt, No external dependencies - Just Win32 + MASM + Winsock2
// ==============================================================================

#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <memory>
#include <functional>
#include <fstream>
#include <sstream>
#include <json/json.h>

// Remove Qt-related definitions
#undef QT_VERSION
#undef QT_CORE_LIB

// Winsock2 for network I/O
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

// Win32 GUI Libraries
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

// External MASM functions
extern "C" {
    // Amphibious ML System
    uint32_t Titan_PerformDMA(void* pSource, void* pDest, size_t size);
    void Titan_ExecuteComputeKernel(void* pContext, void* pPatch);
    
    // Win32IDE Bridge
    int Win32IDE_InitializeML(HWND hEditor, HWND hStatusBar, const char* modelPath);
    int Win32IDE_StartInference(HWND hEditor, const char* selectedCode, const char* userPrompt, void* outputBuffer);
    int Win32IDE_StreamTokenToEditor(HWND hEditor, const char* token, size_t tokenLen, int isDone);
    int Win32IDE_CommitTelemetry(const char* filePath, uint32_t tokenCount, double durationMs, int success);
    int Win32IDE_CancelInference(HWND hEditor, const char* originalText);
}

// ==============================================================================
// Win32IDE_MLIntegration Class
// Core integration between IDE and RawrXD Amphibious ML system
// ==============================================================================
class Win32IDE_MLIntegration {
public:
    enum class OperationStatus {
        Idle,
        Initializing,
        Inferencing,
        Streaming,
        Committed,
        Canceled,
        Error
    };

    struct StreamingMetrics {
        uint32_t tokensGenerated;
        double durationMs;
        uint64_t startTime;
        uint64_t endTime;
        bool success;
        std::string errorMessage;
    };

    using TokenCallback = std::function<void(const std::string&, bool isDone)>;
    using StatusCallback = std::function<void(OperationStatus)>;

    Win32IDE_MLIntegration(HWND hEditor, HWND hStatusBar);
    ~Win32IDE_MLIntegration();

    // Initialization
    bool Initialize(const std::string& modelPath = "");
    bool IsInitialized() const { return m_initialized; }

    // Inference management
    bool StartInference(const std::string& selectedCode, const std::string& userPrompt);
    bool CancelInference();
    bool IsInferencing() const { return m_status == OperationStatus::Inferencing; }

    // Callbacks
    void SetTokenCallback(TokenCallback callback) { m_tokenCallback = callback; }
    void SetStatusCallback(StatusCallback callback) { m_statusCallback = callback; }

    // Telemetry
    const StreamingMetrics& GetMetrics() const { return m_metrics; }
    bool WriteTelemetry(const std::string& outputPath);

    // Inference streaming (called from network thread)
    void OnTokenReceived(const std::string& token);
    void OnStreamComplete();
    void OnStreamError(const std::string& errorMsg);

private:
    // Window handles
    HWND m_hEditor;
    HWND m_hStatusBar;

    // State
    OperationStatus m_status;
    bool m_initialized;
    std::string m_modelPath;
    std::string m_selectedCode;
    std::string m_userPrompt;
    std::string m_originalText;

    // Streaming
    std::string m_accumulatedOutput;
    std::vector<std::string> m_tokenBuffer;
    std::mutex m_streamMutex;

    // Metrics
    StreamingMetrics m_metrics;

    // Callbacks
    TokenCallback m_tokenCallback;
    StatusCallback m_statusCallback;

    // Helper methods
    void SetStatus(OperationStatus status);
    void UpdateStatusBar(const std::string& message);
    void AppendTokenToEditor(const std::string& token);
    Json::Value BuildTelemetryJSON();
    std::string QueryPerformanceCounterMs();
};

// ==============================================================================
// Win32IDE_Window Class
// Main IDE window with ML integration
// ==============================================================================
class Win32IDE_Window {
public:
    Win32IDE_Window();
    ~Win32IDE_Window();

    bool Create(const std::string& title = "RawrXD Win32IDE + Amphibious ML", 
                int width = 1200, int height = 800);
    bool Show(int nCmdShow = SW_SHOW);
    int Run();
    void Close();

    // ML Operations
    bool RequestInference(const std::string& prompt);
    bool CancelInference();

private:
    // Window
    HWND m_hWindow;
    HWND m_hEditorControl;
    HWND m_hStatusBar;
    HWND m_hPromptInput;
    HWND m_hOutputPane;

    // ML Integration
    std::unique_ptr<Win32IDE_MLIntegration> m_mlIntegration;

    // Window class
    static WNDCLASSA m_wndClass;
    static const char* m_className;

    // Static callback
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Control creation
    void CreateControls();
    void CreateEditorControl();
    void CreateStatusBar();
    void CreatePromptInput();
    void CreateOutputPane();

    // Event handlers
    void OnExecuteButton();
    void OnCancelButton();
    void OnTokenReceived(const std::string& token);
    void OnStatusChanged(Win32IDE_MLIntegration::OperationStatus status);

    // Layout
    void OnSize(int cx, int cy);
    void LayoutControls();
};

// ==============================================================================
// Inference Thread Manager
// Handles async token streaming from Winsock2 llama.cpp connection
// ==============================================================================
class InferenceThreadManager {
public:
    InferenceThreadManager(Win32IDE_MLIntegration* mlIntegration);
    ~InferenceThreadManager();

    bool StartInference(const std::string& prompt);
    void Stop();
    bool IsRunning() const { return m_running; }

private:
    Win32IDE_MLIntegration* m_mlIntegration;
    std::thread m_thread;
    bool m_running;
    std::mutex m_mutex;

    void InferenceWorker(const std::string& prompt);
    bool ConnectToLLMServer();
    bool SendInferenceRequest(SOCKET sock, const std::string& prompt);
    void ReceiveTokenStream(SOCKET sock);
};

// ==============================================================================
// Telemetry Manager
// Produces structured JSON telemetry for promotion gates
// ==============================================================================
class TelemetryManager {
public:
    enum class EventType {
        InferenceStart,
        InferenceComplete,
        StreamingError,
        ValidationPass,
        ValidationFail,
        EditorCommit
    };

    TelemetryManager(const std::string& outputDir);
    
    void LogEvent(EventType type, const Json::Value& details);
    void WriteFinalReport(const std::string& filename = "promotion_gate.json");

private:
    std::string m_outputDir;
    std::vector<Json::Value> m_events;
    std::mutex m_eventMutex;
};

#endif // WIN32IDE_AMPHIBIOUS_INTEGRATION_H
