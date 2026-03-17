// ============================================================================
// voice_automation.h — Windows SAPI Voice Automation for RawrXD IDE
// Pure Win32 + COM, no external dependencies
// Features: Text-to-Speech, Speech Recognition, IDE Voice Commands
// ============================================================================

#pragma once

#ifndef RAWRXD_VOICE_AUTOMATION_H
#define RAWRXD_VOICE_AUTOMATION_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <sapi.h>
#include <sphelper.h>
#include <sperror.h>

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "sapi.lib")

// ============================================================================
// Forward Declarations
// ============================================================================
class Win32IDE;

// ============================================================================
// Voice Command Callback Type
// ============================================================================
using VoiceCommandCallback = std::function<void()>;
using VoiceCommandCallbackWithArg = std::function<void(const std::wstring&)>;

// ============================================================================
// Voice Command Entry
// ============================================================================
struct VoiceCommand {
    std::wstring phrase;
    std::wstring pattern;           // For commands with arguments (e.g., "go to line *")
    bool hasArgument;
    VoiceCommandCallback callback;
    VoiceCommandCallbackWithArg callbackWithArg;
};

// ============================================================================
// Speech Event Callback
// ============================================================================
enum class SpeechEventType : int {
    Started = 1,
    Ended = 2,
    Cancelled = 3,
    Error = 4,
    WordBoundary = 5,
    Recognition = 6,
    Hypothesis = 7
};

struct SpeechEvent {
    SpeechEventType type;
    std::wstring text;
    HRESULT errorCode;
    int wordPosition;
    int wordLength;
};

using SpeechEventCallback = std::function<void(const SpeechEvent&)>;

// ============================================================================
// Voice Statistics
// ============================================================================
struct VoiceStatistics {
    int64_t totalUtterances;
    int64_t totalRecognitions;
    int64_t recognitionErrors;
    int64_t commandsExecuted;
    double avgRecognitionConfidence;
    int64_t wordsSpoken;
    double totalSpeechTimeMs;
};

// ============================================================================
// Voice Automation Configuration
// ============================================================================
struct VoiceAutomationConfig {
    int volume;                     // 0-100
    int rate;                       // -10 to 10
    std::wstring preferredVoice;    // Voice name or empty for default
    bool autoStart;                 // Auto-start listening on init
    float recognitionConfidenceThreshold; // 0.0-1.0
    bool feedbackOnRecognition;     // Speak confirmation on recognition
    bool enableDictation;           // Enable free-form dictation
};

// ============================================================================
// VoiceAutomation Class - Main SAPI Integration
// ============================================================================
class VoiceAutomation {
public:
    VoiceAutomation();
    ~VoiceAutomation();

    // Disable copy/move
    VoiceAutomation(const VoiceAutomation&) = delete;
    VoiceAutomation& operator=(const VoiceAutomation&) = delete;
    VoiceAutomation(VoiceAutomation&&) = delete;
    VoiceAutomation& operator=(VoiceAutomation&&) = delete;

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool Initialize();
    bool Initialize(const VoiceAutomationConfig& config);
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // =========================================================================
    // Text-to-Speech (TTS)
    // =========================================================================
    bool Speak(const std::string& text);
    bool Speak(const std::wstring& text);
    bool SpeakAsync(const std::string& text);
    bool SpeakAsync(const std::wstring& text);
    bool StopSpeaking();
    bool IsSpeaking() const;
    
    void SetVolume(int volume);     // 0-100
    int GetVolume() const;
    void SetRate(int rate);         // -10 to 10
    int GetRate() const;

    // Voice enumeration
    std::vector<std::wstring> GetAvailableVoices() const;
    bool SetVoice(const std::wstring& voiceName);
    std::wstring GetCurrentVoice() const;

    // =========================================================================
    // Speech Recognition (SR)
    // =========================================================================
    bool StartListening();
    bool StopListening();
    bool IsListening() const { return m_listening.load(); }
    bool PauseListening();
    bool ResumeListening();

    // Command registration
    void AddCommand(const std::string& phrase, VoiceCommandCallback callback);
    void AddCommand(const std::wstring& phrase, VoiceCommandCallback callback);
    void AddCommandWithArg(const std::string& pattern, VoiceCommandCallbackWithArg callback);
    void AddCommandWithArg(const std::wstring& pattern, VoiceCommandCallbackWithArg callback);
    void RemoveCommand(const std::string& phrase);
    void RemoveCommand(const std::wstring& phrase);
    void ClearCommands();

    // =========================================================================
    // IDE Voice Commands
    // =========================================================================
    void RegisterIDECommands();
    void RegisterIDECommands(Win32IDE* ide);
    void UnregisterIDECommands();

    // =========================================================================
    // Events and Statistics
    // =========================================================================
    void SetEventCallback(SpeechEventCallback callback);
    void SetIDEHandle(Win32IDE* ide) { m_ide = ide; }
    VoiceStatistics GetStatistics() const;
    void ResetStatistics();

    // =========================================================================
    // SAPI Notification Handler (called from window proc)
    // =========================================================================
    void ProcessSAPINotification();
    
    // Get the notification window handle for SAPI events
    HWND GetNotificationWindow() const { return m_hwndNotify; }

private:
    // =========================================================================
    // Internal Initialization
    // =========================================================================
    bool InitializeCOM();
    bool InitializeTTS();
    bool InitializeSR();
    bool CreateNotificationWindow();
    bool BuildGrammar();
    bool RebuildGrammar();

    // =========================================================================
    // Speech Recognition Event Processing
    // =========================================================================
    void ProcessRecognitionEvent(ISpRecoResult* pResult);
    void ProcessHypothesisEvent(ISpRecoResult* pResult);
    void EmitEvent(const SpeechEvent& evt);
    void ExecuteCommand(const std::wstring& phrase, const std::wstring& argument);

    // =========================================================================
    // String Conversion Helpers
    // =========================================================================
    static std::wstring StringToWide(const std::string& str);
    static std::string WideToString(const std::wstring& wstr);

    // =========================================================================
    // Window Procedure for SAPI Notifications
    // =========================================================================
    static LRESULT CALLBACK NotifyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // COM state
    std::atomic<bool> m_comInitialized{false};
    std::atomic<bool> m_initialized{false};
    
    // SAPI Text-to-Speech interfaces
    ISpVoice* m_pVoice;
    
    // SAPI Speech Recognition interfaces
    ISpRecognizer* m_pRecognizer;
    ISpRecoContext* m_pRecoContext;
    ISpRecoGrammar* m_pGrammar;
    
    // Recognition state
    std::atomic<bool> m_listening{false};
    std::atomic<bool> m_paused{false};
    ULONGLONG m_grammarId;
    
    // Configuration
    VoiceAutomationConfig m_config;
    
    // Command registry
    std::unordered_map<std::wstring, VoiceCommand> m_commands;
    std::vector<VoiceCommand> m_patternCommands;  // Commands with wildcards
    mutable std::mutex m_commandMutex;
    
    // Event callback
    SpeechEventCallback m_eventCallback;
    mutable std::mutex m_callbackMutex;
    
    // Statistics
    VoiceStatistics m_stats;
    mutable std::mutex m_statsMutex;
    
    // Notification window
    HWND m_hwndNotify;
    static const UINT WM_SAPI_EVENT = WM_USER + 0x0500;
    
    // IDE reference
    Win32IDE* m_ide;
    
    // Speech queue for async operations
    std::queue<std::wstring> m_speechQueue;
    std::mutex m_speechQueueMutex;
    std::atomic<bool> m_speakingAsync{false};
    std::thread m_speechThread;
    std::atomic<bool> m_stopSpeechThread{false};
};

// ============================================================================
// Global Voice Automation Instance Access
// ============================================================================
VoiceAutomation& GetVoiceAutomation();
bool InitializeVoiceAutomation();
void ShutdownVoiceAutomation();

// ============================================================================
// IDE Voice Command IDs (for menu integration)
// ============================================================================
#define IDM_VOICE_TOGGLE_LISTENING  10300
#define IDM_VOICE_TOGGLE_TTS        10301
#define IDM_VOICE_SETTINGS          10302
#define IDM_VOICE_TTS_TEST          10303

#endif // RAWRXD_VOICE_AUTOMATION_H
