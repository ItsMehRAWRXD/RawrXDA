// ============================================================================
// voice_automation.cpp — Windows SAPI Voice Automation for RawrXD IDE
// Pure Win32 + COM Implementation
// Complete TTS, Speech Recognition, and IDE Voice Command Integration
// ============================================================================

#include "voice_automation.h"
#include <comdef.h>
#include <cstdio>
#include <algorithm>
#include <cwctype>
#include <regex>

// Include for Win32IDE if available
#ifdef RAWRXD_HAS_WIN32IDE
#include "win32app/Win32IDE.h"
#endif

// Resource IDs from resource.h (duplicated for compile independence)
#ifndef ID_FILE_NEW
#define ID_FILE_NEW             1001
#define ID_FILE_OPEN            1002
#define ID_FILE_SAVE            1003
#define ID_FILE_SAVEAS          1004
#define ID_FILE_EXIT            1005
#define ID_FILE_CLOSE           1006
#define ID_EDIT_UNDO            2001
#define ID_EDIT_REDO            2002
#define ID_EDIT_CUT             2003
#define ID_EDIT_COPY            2004
#define ID_EDIT_PASTE           2005
#define ID_EDIT_SELECT_ALL      2006
#define ID_EDIT_FIND            2007
#define ID_EDIT_REPLACE         2008
#define ID_VIEW_TERMINAL        3003
#define ID_VIEW_SIDEBAR         3010
#define ID_VIEW_ZOOM_IN         3020
#define ID_VIEW_ZOOM_OUT        3021
#define ID_BUILD_COMPILE        7001
#define ID_BUILD_BUILD          7002
#define ID_BUILD_REBUILD        7003
#define ID_BUILD_CLEAN          7004
#define ID_BUILD_RUN            7005
#define ID_BUILD_DEBUG          7006
#define IDM_BUILD_SOLUTION      10400
#define IDM_BUILD_CLEAN         10401
#define IDM_BUILD_REBUILD       10402
#define IDM_AGENT_START_LOOP    4100
#define IDM_AGENT_STOP          4105
#define IDM_AGENT_VIEW_STATUS   4104
#define IDM_AI_MODE_MAX         4200
#define IDM_TERMINAL_TOGGLE     4000
#endif

// Custom WM_USER commands for IDE
#define WM_IDE_GOTO_LINE        (WM_USER + 100)
#define WM_IDE_AI_EXPLAIN       (WM_USER + 101)
#define WM_IDE_AI_REFACTOR      (WM_USER + 102)
#define WM_IDE_AI_FIX_ERRORS    (WM_USER + 103)
#define WM_IDE_AI_OPTIMIZE      (WM_USER + 104)
#define WM_IDE_RUN_TESTS        (WM_USER + 105)

// ============================================================================
// Debug Logging
// ============================================================================
inline void VoiceLogA(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    char out[1040];
    snprintf(out, sizeof(out), "[VoiceAutomation] %s\n", buf);
    OutputDebugStringA(out);
}

inline void VoiceLogW(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    vswprintf(buf, sizeof(buf)/sizeof(wchar_t), fmt, args);
    va_end(args);
    wchar_t out[1040];
    swprintf(out, sizeof(out)/sizeof(wchar_t), L"[VoiceAutomation] %s\n", buf);
    OutputDebugStringW(out);
}

#ifndef VOICE_LOG
#define VOICE_LOG(fmt, ...) VoiceLogA(fmt, ##__VA_ARGS__)
#endif

#ifndef VOICE_LOG_W
#define VOICE_LOG_W(fmt, ...) VoiceLogW(fmt, ##__VA_ARGS__)
#endif

// ============================================================================
// Global Instance
// ============================================================================
static VoiceAutomation* g_voiceAutomation = nullptr;
static std::mutex g_voiceAutomationMutex;

VoiceAutomation& GetVoiceAutomation() {
    std::lock_guard<std::mutex> lock(g_voiceAutomationMutex);
    if (!g_voiceAutomation) {
        g_voiceAutomation = new VoiceAutomation();
    }
    return *g_voiceAutomation;
}

bool InitializeVoiceAutomation() {
    return GetVoiceAutomation().Initialize();
}

void ShutdownVoiceAutomation() {
    std::lock_guard<std::mutex> lock(g_voiceAutomationMutex);
    if (g_voiceAutomation) {
        g_voiceAutomation->Shutdown();
        delete g_voiceAutomation;
        g_voiceAutomation = nullptr;
    }
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
VoiceAutomation::VoiceAutomation()
    : m_pVoice(nullptr)
    , m_pRecognizer(nullptr)
    , m_pRecoContext(nullptr)
    , m_pGrammar(nullptr)
    , m_grammarId(1)
    , m_hwndNotify(nullptr)
    , m_ide(nullptr)
{
    // Default configuration
    m_config.volume = 100;
    m_config.rate = 0;
    m_config.autoStart = false;
    m_config.recognitionConfidenceThreshold = 0.5f;
    m_config.feedbackOnRecognition = false;
    m_config.enableDictation = false;

    // Initialize statistics
    memset(&m_stats, 0, sizeof(m_stats));
}

VoiceAutomation::~VoiceAutomation() {
    Shutdown();
}

// ============================================================================
// COM Initialization
// ============================================================================
bool VoiceAutomation::InitializeCOM() {
    if (m_comInitialized.load()) {
        return true;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        VOICE_LOG("CoInitializeEx failed: 0x%08X", (unsigned)hr);
        return false;
    }

    m_comInitialized.store(true);
    VOICE_LOG("COM initialized successfully");
    return true;
}

// ============================================================================
// Notification Window for SAPI Events
// ============================================================================
bool VoiceAutomation::CreateNotificationWindow() {
    // Register window class
    static const wchar_t* NOTIFY_CLASS = L"RawrXD_VoiceNotify";
    static bool classRegistered = false;

    if (!classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = NotifyWndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = NOTIFY_CLASS;

        if (!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            if (err != ERROR_CLASS_ALREADY_EXISTS) {
                VOICE_LOG("RegisterClassEx failed: %lu", err);
                return false;
            }
        }
        classRegistered = true;
    }

    // Create message-only window
    m_hwndNotify = CreateWindowExW(
        0, NOTIFY_CLASS, L"VoiceNotify",
        0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr,
        GetModuleHandle(nullptr), this);

    if (!m_hwndNotify) {
        VOICE_LOG("CreateWindowEx failed: %lu", GetLastError());
        return false;
    }

    // Store 'this' pointer in window
    SetWindowLongPtrW(m_hwndNotify, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    VOICE_LOG("Notification window created: %p", m_hwndNotify);
    return true;
}

LRESULT CALLBACK VoiceAutomation::NotifyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    VoiceAutomation* pThis = reinterpret_cast<VoiceAutomation*>(
        GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_SAPI_EVENT && pThis) {
        pThis->ProcessSAPINotification();
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// ============================================================================
// Text-to-Speech Initialization
// ============================================================================
bool VoiceAutomation::InitializeTTS() {
    HRESULT hr = CoCreateInstance(
        CLSID_SpVoice, nullptr, CLSCTX_ALL,
        IID_ISpVoice, reinterpret_cast<void**>(&m_pVoice));

    if (FAILED(hr) || !m_pVoice) {
        VOICE_LOG("Failed to create ISpVoice: 0x%08X", (unsigned)hr);
        return false;
    }

    // Set initial volume and rate
    m_pVoice->SetVolume(static_cast<USHORT>(m_config.volume));
    m_pVoice->SetRate(m_config.rate);

    // Select preferred voice if specified
    if (!m_config.preferredVoice.empty()) {
        SetVoice(m_config.preferredVoice);
    }

    VOICE_LOG("TTS initialized successfully");
    return true;
}

// ============================================================================
// Speech Recognition Initialization
// ============================================================================
bool VoiceAutomation::InitializeSR() {
    HRESULT hr;

    // Create shared recognizer (uses default audio input)
    hr = CoCreateInstance(
        CLSID_SpSharedRecognizer, nullptr, CLSCTX_ALL,
        IID_ISpRecognizer, reinterpret_cast<void**>(&m_pRecognizer));

    if (FAILED(hr) || !m_pRecognizer) {
        VOICE_LOG("Failed to create ISpRecognizer: 0x%08X", (unsigned)hr);
        return false;
    }

    // Create recognition context
    hr = m_pRecognizer->CreateRecoContext(&m_pRecoContext);
    if (FAILED(hr) || !m_pRecoContext) {
        VOICE_LOG("Failed to create ISpRecoContext: 0x%08X", (unsigned)hr);
        return false;
    }

    // Set notification window
    hr = m_pRecoContext->SetNotifyWindowMessage(
        m_hwndNotify, WM_SAPI_EVENT, 0, 0);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to set notify window: 0x%08X", (unsigned)hr);
        return false;
    }

    // Set events of interest
    const ULONGLONG ullInterest =
        SPFEI(CYCPRULEWHEN_SOUND_START) |
        SPFEI(SPEI_SOUND_END) |
        SPFEI(SPEI_PHRASE_START) |
        SPFEI(SPEI_RECOGNITION) |
        SPFEI(SPEI_FALSE_RECOGNITION) |
        SPFEI(SPEI_HYPOTHESIS) |
        SPFEI(SPEI_INTERFERENCE) |
        SPFEI(SPEI_RECO_STATE_CHANGE);

    hr = m_pRecoContext->SetInterest(ullInterest, ullInterest);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to set interest: 0x%08X", (unsigned)hr);
        // Continue anyway, some events may still work
    }

    // Create grammar
    hr = m_pRecoContext->CreateGrammar(m_grammarId, &m_pGrammar);
    if (FAILED(hr) || !m_pGrammar) {
        VOICE_LOG("Failed to create grammar: 0x%08X", (unsigned)hr);
        return false;
    }

    VOICE_LOG("Speech recognition initialized successfully");
    return true;
}

// ============================================================================
// Build Grammar from Registered Commands
// ============================================================================
bool VoiceAutomation::BuildGrammar() {
    if (!m_pGrammar) {
        VOICE_LOG("No grammar object available");
        return false;
    }

    HRESULT hr;

    // Clear existing rules
    hr = m_pGrammar->ResetGrammar(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
    if (FAILED(hr)) {
        VOICE_LOG("Failed to reset grammar: 0x%08X", (unsigned)hr);
        return false;
    }

    // Get the top-level rule state
    SPSTATEHANDLE hTopRule;
    hr = m_pGrammar->GetRule(L"TopLevelRule", 0,
        SPRAF_TopLevel | SPRAF_Active | SPRAF_Dynamic, TRUE, &hTopRule);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to create top rule: 0x%08X", (unsigned)hr);
        return false;
    }

    // Add all registered commands
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);

        // Simple commands (exact phrase match)
        for (const auto& pair : m_commands) {
            const VoiceCommand& cmd = pair.second;
            if (!cmd.hasArgument) {
                // Create a path for this phrase
                hr = m_pGrammar->AddWordTransition(
                    hTopRule, nullptr,
                    cmd.phrase.c_str(), L" ",
                    SPWT_LEXICAL, 1.0f, nullptr);

                if (FAILED(hr)) {
                    VOICE_LOG_W(L"Failed to add phrase '%s': 0x%08X",
                        cmd.phrase.c_str(), (unsigned)hr);
                }
            }
        }

        // Pattern commands with wildcards
        // SAPI doesn't support true wildcards, so we use dictation for those
        // Instead, we'll create rules with optional number/word slots
        for (const auto& cmd : m_patternCommands) {
            // Parse pattern to extract static and wildcard parts
            // Pattern format: "go to line *" -> "go to line" + dictation
            std::wstring staticPart;
            size_t starPos = cmd.pattern.find(L'*');
            if (starPos != std::wstring::npos) {
                staticPart = cmd.pattern.substr(0, starPos);
                // Trim trailing space
                while (!staticPart.empty() && staticPart.back() == L' ') {
                    staticPart.pop_back();
                }
            } else {
                staticPart = cmd.pattern;
            }

            // Add the static part as a command
            // The argument will be captured via post-processing
            if (!staticPart.empty()) {
                hr = m_pGrammar->AddWordTransition(
                    hTopRule, nullptr,
                    staticPart.c_str(), L" ",
                    SPWT_LEXICAL, 1.0f, nullptr);

                if (FAILED(hr)) {
                    VOICE_LOG_W(L"Failed to add pattern '%s': 0x%08X",
                        staticPart.c_str(), (unsigned)hr);
                }
            }
        }
    }

    // Commit grammar changes
    hr = m_pGrammar->Commit(0);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to commit grammar: 0x%08X", (unsigned)hr);
        return false;
    }

    VOICE_LOG("Grammar built with %zu commands, %zu patterns",
        m_commands.size(), m_patternCommands.size());
    return true;
}

bool VoiceAutomation::RebuildGrammar() {
    if (!m_pGrammar) return false;

    // Deactivate, rebuild, reactivate
    bool wasListening = m_listening.load();
    if (wasListening) {
        m_pGrammar->SetGrammarState(SPGS_DISABLED);
    }

    bool result = BuildGrammar();

    if (wasListening && result) {
        HRESULT hr = m_pGrammar->SetGrammarState(SPGS_ENABLED);
        hr = m_pGrammar->SetRuleState(nullptr, nullptr, SPRS_ACTIVE);
        if (FAILED(hr)) {
            VOICE_LOG("Failed to reactivate grammar: 0x%08X", (unsigned)hr);
        }
    }

    return result;
}

// ============================================================================
// Main Initialization
// ============================================================================
bool VoiceAutomation::Initialize() {
    return Initialize(m_config);
}

bool VoiceAutomation::Initialize(const VoiceAutomationConfig& config) {
    if (m_initialized.load()) {
        VOICE_LOG("Already initialized");
        return true;
    }

    m_config = config;

    // Initialize COM
    if (!InitializeCOM()) {
        return false;
    }

    // Create notification window
    if (!CreateNotificationWindow()) {
        return false;
    }

    // Initialize TTS
    if (!InitializeTTS()) {
        return false;
    }

    // Initialize SR
    if (!InitializeSR()) {
        return false;
    }

    // Build initial grammar
    BuildGrammar();

    m_initialized.store(true);
    VOICE_LOG("Voice automation fully initialized");

    // Auto-start listening if configured
    if (m_config.autoStart) {
        StartListening();
    }

    return true;
}

// ============================================================================
// Shutdown
// ============================================================================
void VoiceAutomation::Shutdown() {
    if (!m_initialized.load()) {
        return;
    }

    VOICE_LOG("Shutting down voice automation...");

    // Stop speech thread
    m_stopSpeechThread.store(true);
    if (m_speechThread.joinable()) {
        m_speechThread.join();
    }

    // Stop listening
    StopListening();
    StopSpeaking();

    // Release SAPI interfaces
    if (m_pGrammar) {
        m_pGrammar->Release();
        m_pGrammar = nullptr;
    }

    if (m_pRecoContext) {
        m_pRecoContext->Release();
        m_pRecoContext = nullptr;
    }

    if (m_pRecognizer) {
        m_pRecognizer->Release();
        m_pRecognizer = nullptr;
    }

    if (m_pVoice) {
        m_pVoice->Release();
        m_pVoice = nullptr;
    }

    // Destroy notification window
    if (m_hwndNotify) {
        DestroyWindow(m_hwndNotify);
        m_hwndNotify = nullptr;
    }

    // Clear commands
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_commands.clear();
        m_patternCommands.clear();
    }

    // Uninitialize COM (only if we initialized it)
    if (m_comInitialized.load()) {
        CoUninitialize();
        m_comInitialized.store(false);
    }

    m_initialized.store(false);
    m_listening.store(false);

    VOICE_LOG("Voice automation shut down");
}

// ============================================================================
// Text-to-Speech Functions
// ============================================================================
bool VoiceAutomation::Speak(const std::string& text) {
    return Speak(StringToWide(text));
}

bool VoiceAutomation::Speak(const std::wstring& text) {
    if (!m_pVoice) {
        VOICE_LOG("TTS not initialized");
        return false;
    }

    // Stop any current speech first
    m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

    // Speak synchronously
    HRESULT hr = m_pVoice->Speak(text.c_str(), SPF_DEFAULT, nullptr);
    if (FAILED(hr)) {
        VOICE_LOG("Speak failed: 0x%08X", (unsigned)hr);
        return false;
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalUtterances++;
        m_stats.wordsSpoken += static_cast<int64_t>(
            std::count(text.begin(), text.end(), L' ') + 1);
    }

    // Emit event
    SpeechEvent evt;
    evt.type = SpeechEventType::Ended;
    evt.text = text;
    evt.errorCode = S_OK;
    EmitEvent(evt);

    return true;
}

bool VoiceAutomation::SpeakAsync(const std::string& text) {
    return SpeakAsync(StringToWide(text));
}

bool VoiceAutomation::SpeakAsync(const std::wstring& text) {
    if (!m_pVoice) {
        VOICE_LOG("TTS not initialized");
        return false;
    }

    // Speak asynchronously
    HRESULT hr = m_pVoice->Speak(text.c_str(), SPF_ASYNC, nullptr);
    if (FAILED(hr)) {
        VOICE_LOG("SpeakAsync failed: 0x%08X", (unsigned)hr);
        return false;
    }

    // Update statistics
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalUtterances++;
        m_stats.wordsSpoken += static_cast<int64_t>(
            std::count(text.begin(), text.end(), L' ') + 1);
    }

    // Emit started event
    SpeechEvent evt;
    evt.type = SpeechEventType::Started;
    evt.text = text;
    evt.errorCode = S_OK;
    EmitEvent(evt);

    return true;
}

bool VoiceAutomation::StopSpeaking() {
    if (!m_pVoice) {
        return false;
    }

    HRESULT hr = m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    if (FAILED(hr)) {
        VOICE_LOG("StopSpeaking failed: 0x%08X", (unsigned)hr);
        return false;
    }

    // Emit cancelled event
    SpeechEvent evt;
    evt.type = SpeechEventType::Cancelled;
    evt.errorCode = S_OK;
    EmitEvent(evt);

    return true;
}

bool VoiceAutomation::IsSpeaking() const {
    if (!m_pVoice) {
        return false;
    }

    SPVOICESTATUS status;
    HRESULT hr = m_pVoice->GetStatus(&status, nullptr);
    if (FAILED(hr)) {
        return false;
    }

    return status.dwRunningState == SPRS_IS_SPEAKING;
}

void VoiceAutomation::SetVolume(int volume) {
    m_config.volume = std::clamp(volume, 0, 100);
    if (m_pVoice) {
        m_pVoice->SetVolume(static_cast<USHORT>(m_config.volume));
    }
}

int VoiceAutomation::GetVolume() const {
    return m_config.volume;
}

void VoiceAutomation::SetRate(int rate) {
    m_config.rate = std::clamp(rate, -10, 10);
    if (m_pVoice) {
        m_pVoice->SetRate(m_config.rate);
    }
}

int VoiceAutomation::GetRate() const {
    return m_config.rate;
}

// ============================================================================
// Voice Enumeration and Selection
// ============================================================================
std::vector<std::wstring> VoiceAutomation::GetAvailableVoices() const {
    std::vector<std::wstring> voices;

    if (!m_pVoice) {
        return voices;
    }

    IEnumSpObjectTokens* pEnum = nullptr;
    HRESULT hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnum);
    if (FAILED(hr) || !pEnum) {
        return voices;
    }

    ISpObjectToken* pToken = nullptr;
    while (pEnum->Next(1, &pToken, nullptr) == S_OK) {
        LPWSTR pszName = nullptr;
        hr = pToken->GetStringValue(nullptr, &pszName);
        if (SUCCEEDED(hr) && pszName) {
            voices.push_back(pszName);
            CoTaskMemFree(pszName);
        }
        pToken->Release();
    }

    pEnum->Release();
    return voices;
}

bool VoiceAutomation::SetVoice(const std::wstring& voiceName) {
    if (!m_pVoice) {
        return false;
    }

    IEnumSpObjectTokens* pEnum = nullptr;
    HRESULT hr = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnum);
    if (FAILED(hr) || !pEnum) {
        return false;
    }

    bool found = false;
    ISpObjectToken* pToken = nullptr;
    while (pEnum->Next(1, &pToken, nullptr) == S_OK && !found) {
        LPWSTR pszName = nullptr;
        hr = pToken->GetStringValue(nullptr, &pszName);
        if (SUCCEEDED(hr) && pszName) {
            if (voiceName == pszName ||
                wcsstr(pszName, voiceName.c_str()) != nullptr) {
                hr = m_pVoice->SetVoice(pToken);
                if (SUCCEEDED(hr)) {
                    m_config.preferredVoice = pszName;
                    found = true;
                    VOICE_LOG_W(L"Voice set to: %s", pszName);
                }
            }
            CoTaskMemFree(pszName);
        }
        pToken->Release();
    }

    pEnum->Release();
    return found;
}

std::wstring VoiceAutomation::GetCurrentVoice() const {
    if (!m_pVoice) {
        return L"";
    }

    ISpObjectToken* pToken = nullptr;
    HRESULT hr = m_pVoice->GetVoice(&pToken);
    if (FAILED(hr) || !pToken) {
        return L"";
    }

    LPWSTR pszName = nullptr;
    hr = pToken->GetStringValue(nullptr, &pszName);
    std::wstring name;
    if (SUCCEEDED(hr) && pszName) {
        name = pszName;
        CoTaskMemFree(pszName);
    }

    pToken->Release();
    return name;
}

// ============================================================================
// Speech Recognition Control
// ============================================================================
bool VoiceAutomation::StartListening() {
    if (m_listening.load()) {
        VOICE_LOG("Already listening");
        return true;
    }

    if (!m_pGrammar || !m_pRecoContext) {
        VOICE_LOG("SR not initialized");
        return false;
    }

    // Ensure grammar is built
    if (!BuildGrammar()) {
        VOICE_LOG("Failed to build grammar");
        return false;
    }

    // Enable grammar
    HRESULT hr = m_pGrammar->SetGrammarState(SPGS_ENABLED);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to enable grammar: 0x%08X", (unsigned)hr);
        return false;
    }

    // Activate all rules
    hr = m_pGrammar->SetRuleState(nullptr, nullptr, SPRS_ACTIVE);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to activate rules: 0x%08X", (unsigned)hr);
        return false;
    }

    // Enable dictation if configured
    if (m_config.enableDictation) {
        hr = m_pGrammar->SetDictationState(SPRS_ACTIVE);
        if (FAILED(hr)) {
            VOICE_LOG("Failed to enable dictation: 0x%08X", (unsigned)hr);
            // Continue without dictation
        }
    }

    m_listening.store(true);
    m_paused.store(false);

    VOICE_LOG("Started listening for voice commands");

    // Emit event
    SpeechEvent evt;
    evt.type = SpeechEventType::Started;
    evt.text = L"Listening started";
    EmitEvent(evt);

    return true;
}

bool VoiceAutomation::StopListening() {
    if (!m_listening.load()) {
        return true;
    }

    if (!m_pGrammar) {
        return false;
    }

    // Deactivate rules
    HRESULT hr = m_pGrammar->SetRuleState(nullptr, nullptr, SPRS_INACTIVE);
    if (FAILED(hr)) {
        VOICE_LOG("Failed to deactivate rules: 0x%08X", (unsigned)hr);
    }

    // Disable dictation
    hr = m_pGrammar->SetDictationState(SPRS_INACTIVE);

    // Disable grammar
    hr = m_pGrammar->SetGrammarState(SPGS_DISABLED);

    m_listening.store(false);
    VOICE_LOG("Stopped listening");

    // Emit event
    SpeechEvent evt;
    evt.type = SpeechEventType::Ended;
    evt.text = L"Listening stopped";
    EmitEvent(evt);

    return true;
}

bool VoiceAutomation::PauseListening() {
    if (!m_listening.load() || m_paused.load()) {
        return false;
    }

    if (!m_pGrammar) {
        return false;
    }

    m_pGrammar->SetRuleState(nullptr, nullptr, SPRS_INACTIVE);
    m_paused.store(true);
    VOICE_LOG("Listening paused");
    return true;
}

bool VoiceAutomation::ResumeListening() {
    if (!m_listening.load() || !m_paused.load()) {
        return false;
    }

    if (!m_pGrammar) {
        return false;
    }

    m_pGrammar->SetRuleState(nullptr, nullptr, SPRS_ACTIVE);
    m_paused.store(false);
    VOICE_LOG("Listening resumed");
    return true;
}

// ============================================================================
// SAPI Event Processing
// ============================================================================
void VoiceAutomation::ProcessSAPINotification() {
    if (!m_pRecoContext) {
        return;
    }

    SPEVENT event;
    while (m_pRecoContext->GetEvents(1, &event, nullptr) == S_OK) {
        switch (event.eEventId) {
            case SPEI_RECOGNITION: {
                ISpRecoResult* pResult = reinterpret_cast<ISpRecoResult*>(event.lParam);
                if (pResult) {
                    ProcessRecognitionEvent(pResult);
                }
                break;
            }

            case SPEI_HYPOTHESIS: {
                ISpRecoResult* pResult = reinterpret_cast<ISpRecoResult*>(event.lParam);
                if (pResult) {
                    ProcessHypothesisEvent(pResult);
                }
                break;
            }

            case SPEI_FALSE_RECOGNITION: {
                VOICE_LOG("False recognition event");
                break;
            }

            case SPEI_INTERFERENCE: {
                VOICE_LOG("Audio interference detected");
                break;
            }

            case SPEI_SOUND_START: {
                VOICE_LOG("Sound detected");
                break;
            }

            case SPEI_SOUND_END: {
                VOICE_LOG("Silence detected");
                break;
            }

            default:
                break;
        }

        // Release any objects
        if (event.elParamType == CYCPRULEWHEN_SOUND_START) {
            if (event.lParam) {
                reinterpret_cast<IUnknown*>(event.lParam)->Release();
            }
        }
    }
}

void VoiceAutomation::ProcessRecognitionEvent(ISpRecoResult* pResult) {
    if (!pResult) {
        return;
    }

    // Get phrase info
    SPPHRASE* pPhrase = nullptr;
    HRESULT hr = pResult->GetPhrase(&pPhrase);
    if (FAILED(hr) || !pPhrase) {
        VOICE_LOG("Failed to get phrase: 0x%08X", (unsigned)hr);
        return;
    }

    // Get the recognized text
    LPWSTR pszText = nullptr;
    hr = pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pszText, nullptr);
    if (SUCCEEDED(hr) && pszText) {
        std::wstring recognizedText = pszText;

        // Check confidence
        float confidence = 0.0f;
        if (pPhrase->Rule.Confidence >= 0) {
            confidence = static_cast<float>(pPhrase->Rule.Confidence);
        }

        VOICE_LOG_W(L"Recognized: '%s' (confidence: %.2f)", pszText, confidence);

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.totalRecognitions++;
            // Running average of confidence
            double total = m_stats.avgRecognitionConfidence * (m_stats.totalRecognitions - 1);
            m_stats.avgRecognitionConfidence = (total + confidence) / m_stats.totalRecognitions;
        }

        // Emit recognition event
        SpeechEvent evt;
        evt.type = SpeechEventType::Recognition;
        evt.text = recognizedText;
        evt.errorCode = S_OK;
        EmitEvent(evt);

        // Check confidence threshold
        if (confidence >= m_config.recognitionConfidenceThreshold) {
            // Try to match and execute command
            ExecuteCommand(recognizedText, L"");
        }

        CoTaskMemFree(pszText);
    }

    CoTaskMemFree(pPhrase);
}

void VoiceAutomation::ProcessHypothesisEvent(ISpRecoResult* pResult) {
    if (!pResult) {
        return;
    }

    // Get hypothesis text
    LPWSTR pszText = nullptr;
    HRESULT hr = pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pszText, nullptr);
    if (SUCCEEDED(hr) && pszText) {
        VOICE_LOG_W(L"Hypothesis: '%s'", pszText);

        // Emit hypothesis event
        SpeechEvent evt;
        evt.type = SpeechEventType::Hypothesis;
        evt.text = pszText;
        evt.errorCode = S_OK;
        EmitEvent(evt);

        CoTaskMemFree(pszText);
    }
}

// ============================================================================
// Command Management
// ============================================================================
void VoiceAutomation::AddCommand(const std::string& phrase, VoiceCommandCallback callback) {
    AddCommand(StringToWide(phrase), callback);
}

void VoiceAutomation::AddCommand(const std::wstring& phrase, VoiceCommandCallback callback) {
    std::wstring lowerPhrase;
    lowerPhrase.reserve(phrase.size());
    for (wchar_t ch : phrase) {
        lowerPhrase.push_back(std::towlower(ch));
    }

    VoiceCommand cmd;
    cmd.phrase = lowerPhrase;
    cmd.pattern = lowerPhrase;
    cmd.hasArgument = false;
    cmd.callback = callback;

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_commands[lowerPhrase] = cmd;
    }

    VOICE_LOG_W(L"Added command: '%s'", lowerPhrase.c_str());

    // Rebuild grammar if listening
    if (m_initialized.load()) {
        RebuildGrammar();
    }
}

void VoiceAutomation::AddCommandWithArg(const std::string& pattern, VoiceCommandCallbackWithArg callback) {
    AddCommandWithArg(StringToWide(pattern), callback);
}

void VoiceAutomation::AddCommandWithArg(const std::wstring& pattern, VoiceCommandCallbackWithArg callback) {
    std::wstring lowerPattern;
    lowerPattern.reserve(pattern.size());
    for (wchar_t ch : pattern) {
        lowerPattern.push_back(std::towlower(ch));
    }

    VoiceCommand cmd;
    cmd.phrase = lowerPattern;
    cmd.pattern = lowerPattern;
    cmd.hasArgument = true;
    cmd.callbackWithArg = callback;

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_patternCommands.push_back(cmd);
    }

    VOICE_LOG_W(L"Added pattern command: '%s'", lowerPattern.c_str());

    if (m_initialized.load()) {
        RebuildGrammar();
    }
}

void VoiceAutomation::RemoveCommand(const std::string& phrase) {
    RemoveCommand(StringToWide(phrase));
}

void VoiceAutomation::RemoveCommand(const std::wstring& phrase) {
    std::wstring lowerPhrase;
    lowerPhrase.reserve(phrase.size());
    for (wchar_t ch : phrase) {
        lowerPhrase.push_back(std::towlower(ch));
    }

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_commands.erase(lowerPhrase);

        // Also check pattern commands
        m_patternCommands.erase(
            std::remove_if(m_patternCommands.begin(), m_patternCommands.end(),
                [&lowerPhrase](const VoiceCommand& cmd) {
                    return cmd.phrase == lowerPhrase;
                }),
            m_patternCommands.end());
    }

    if (m_initialized.load()) {
        RebuildGrammar();
    }
}

void VoiceAutomation::ClearCommands() {
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_commands.clear();
        m_patternCommands.clear();
    }

    if (m_initialized.load()) {
        RebuildGrammar();
    }
}

// ============================================================================
// Command Execution
// ============================================================================
void VoiceAutomation::ExecuteCommand(const std::wstring& phrase, const std::wstring& argument) {
    // Convert to lowercase for matching
    std::wstring lowerPhrase;
    lowerPhrase.reserve(phrase.size());
    for (wchar_t ch : phrase) {
        lowerPhrase.push_back(std::towlower(ch));
    }

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);

        // Try exact match first
        auto it = m_commands.find(lowerPhrase);
        if (it != m_commands.end()) {
            const VoiceCommand& cmd = it->second;
            if (cmd.callback) {
                VOICE_LOG_W(L"Executing command: '%s'", phrase.c_str());

                // Update stats
                {
                    std::lock_guard<std::mutex> statsLock(m_statsMutex);
                    m_stats.commandsExecuted++;
                }

                // Provide feedback if configured
                if (m_config.feedbackOnRecognition) {
                    std::wstring feedback = L"Executing " + cmd.phrase;
                    SpeakAsync(feedback);
                }

                // Execute callback
                cmd.callback();
                return;
            }
        }

        // Try pattern matching
        for (const auto& cmd : m_patternCommands) {
            // Extract the static prefix before '*'
            size_t starPos = cmd.pattern.find(L'*');
            std::wstring prefix;
            if (starPos != std::wstring::npos) {
                prefix = cmd.pattern.substr(0, starPos);
                while (!prefix.empty() && prefix.back() == L' ') {
                    prefix.pop_back();
                }
            } else {
                prefix = cmd.pattern;
            }

            // Check if phrase starts with prefix
            if (lowerPhrase.find(prefix) == 0) {
                // Extract argument (everything after the prefix)
                std::wstring arg;
                if (lowerPhrase.size() > prefix.size()) {
                    arg = lowerPhrase.substr(prefix.size());
                    // Trim leading/trailing whitespace
                    while (!arg.empty() && arg.front() == L' ') {
                        arg.erase(arg.begin());
                    }
                    while (!arg.empty() && arg.back() == L' ') {
                        arg.pop_back();
                    }
                }

                if (cmd.callbackWithArg) {
                    VOICE_LOG_W(L"Executing pattern command: '%s' with arg: '%s'",
                        cmd.phrase.c_str(), arg.c_str());

                    {
                        std::lock_guard<std::mutex> statsLock(m_statsMutex);
                        m_stats.commandsExecuted++;
                    }

                    if (m_config.feedbackOnRecognition) {
                        std::wstring feedback = L"Executing " + prefix + L" " + arg;
                        SpeakAsync(feedback);
                    }

                    cmd.callbackWithArg(arg);
                    return;
                }
            }
        }
    }

    VOICE_LOG_W(L"No command matched: '%s'", phrase.c_str());

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.recognitionErrors++;
    }
}

// ============================================================================
// IDE Voice Commands Registration
// ============================================================================
void VoiceAutomation::RegisterIDECommands() {
    RegisterIDECommands(m_ide);
}

void VoiceAutomation::RegisterIDECommands(Win32IDE* ide) {
    m_ide = ide;

    // File operations
    AddCommand(L"new file", [this]() {
        VOICE_LOG("Voice command: New file");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND, 
                MAKEWPARAM(ID_FILE_NEW, 0), 0);
        }
    });

    AddCommand(L"save file", [this]() {
        VOICE_LOG("Voice command: Save file");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_FILE_SAVE, 0), 0);
        }
    });

    AddCommand(L"save all", [this]() {
        VOICE_LOG("Voice command: Save all");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_FILE_SAVE, 0), 0);
        }
    });

    AddCommand(L"close file", [this]() {
        VOICE_LOG("Voice command: Close file");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_FILE_CLOSE, 0), 0);
        }
    });

    AddCommand(L"open file", [this]() {
        VOICE_LOG("Voice command: Open file");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_FILE_OPEN, 0), 0);
        }
    });

    // Open file by name
    AddCommandWithArg(L"open *", [this](const std::wstring& filename) {
        VOICE_LOG_W(L"Voice command: Open file '%s'", filename.c_str());
        // Trigger the open dialog - specific filename opening needs IDE support
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_FILE_OPEN, 0), 0);
        }
    });

    // Navigation
    AddCommandWithArg(L"go to line *", [this](const std::wstring& lineNumber) {
        VOICE_LOG_W(L"Voice command: Go to line '%s'", lineNumber.c_str());
        try {
            int line = std::stoi(lineNumber);
            // Post custom message for go-to-line
            if (m_ide) {
                PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_GOTO_LINE,
                    static_cast<WPARAM>(line), 0);
            }
        } catch (...) {
            VOICE_LOG_W(L"Invalid line number: '%s'", lineNumber.c_str());
        }
    });

    AddCommand(L"go to definition", [this]() {
        VOICE_LOG("Voice command: Go to definition");
        // F12 or LSP go to definition
    });

    AddCommand(L"go back", [this]() {
        VOICE_LOG("Voice command: Go back");
        // Navigate back in history
    });

    AddCommand(L"go forward", [this]() {
        VOICE_LOG("Voice command: Go forward");
        // Navigate forward in history
    });

    // Build commands
    AddCommand(L"build project", [this]() {
        VOICE_LOG("Voice command: Build project");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_BUILD_BUILD, 0), 0);
        }
    });

    AddCommand(L"build solution", [this]() {
        VOICE_LOG("Voice command: Build solution");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(IDM_BUILD_SOLUTION, 0), 0);
        }
    });

    AddCommand(L"rebuild project", [this]() {
        VOICE_LOG("Voice command: Rebuild project");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_BUILD_REBUILD, 0), 0);
        }
    });

    AddCommand(L"clean project", [this]() {
        VOICE_LOG("Voice command: Clean project");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_BUILD_CLEAN, 0), 0);
        }
    });

    // Run commands
    AddCommand(L"run project", [this]() {
        VOICE_LOG("Voice command: Run project");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_BUILD_RUN, 0), 0);
        }
    });

    AddCommand(L"debug project", [this]() {
        VOICE_LOG("Voice command: Debug project");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_BUILD_DEBUG, 0), 0);
        }
    });

    AddCommand(L"stop debugging", [this]() {
        VOICE_LOG("Voice command: Stop debugging");
        // Stop debug session
    });

    // Test commands
    AddCommand(L"run tests", [this]() {
        VOICE_LOG("Voice command: Run tests");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_RUN_TESTS,
                0, 0);
        }
    });

    AddCommand(L"run all tests", [this]() {
        VOICE_LOG("Voice command: Run all tests");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_RUN_TESTS,
                1, 0);  // 1 = run all
        }
    });

    // AI/Agent commands
    AddCommand(L"generate code", [this]() {
        VOICE_LOG("Voice command: Generate code");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(IDM_AGENT_START_LOOP, 0), 0);
        }
    });

    AddCommand(L"explain selection", [this]() {
        VOICE_LOG("Voice command: Explain selection");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_AI_EXPLAIN,
                0, 0);
        }
    });

    AddCommand(L"explain this", [this]() {
        VOICE_LOG("Voice command: Explain this");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_AI_EXPLAIN,
                0, 0);
        }
    });

    AddCommand(L"refactor this", [this]() {
        VOICE_LOG("Voice command: Refactor this");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_AI_REFACTOR,
                0, 0);
        }
    });

    AddCommand(L"fix errors", [this]() {
        VOICE_LOG("Voice command: Fix errors");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_AI_FIX_ERRORS,
                0, 0);
        }
    });

    AddCommand(L"optimize code", [this]() {
        VOICE_LOG("Voice command: Optimize code");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_IDE_AI_OPTIMIZE,
                0, 0);
        }
    });

    AddCommand(L"add comments", [this]() {
        VOICE_LOG("Voice command: Add comments");
        // Trigger AI comment generation
    });

    AddCommand(L"write documentation", [this]() {
        VOICE_LOG("Voice command: Write documentation");
        // Trigger AI documentation generation
    });

    // Edit commands
    AddCommand(L"undo", [this]() {
        VOICE_LOG("Voice command: Undo");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_UNDO, 0), 0);
        }
    });

    AddCommand(L"redo", [this]() {
        VOICE_LOG("Voice command: Redo");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_REDO, 0), 0);
        }
    });

    AddCommand(L"copy", [this]() {
        VOICE_LOG("Voice command: Copy");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_COPY, 0), 0);
        }
    });

    AddCommand(L"cut", [this]() {
        VOICE_LOG("Voice command: Cut");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_CUT, 0), 0);
        }
    });

    AddCommand(L"paste", [this]() {
        VOICE_LOG("Voice command: Paste");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_PASTE, 0), 0);
        }
    });

    AddCommand(L"select all", [this]() {
        VOICE_LOG("Voice command: Select all");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_SELECT_ALL, 0), 0);
        }
    });

    AddCommand(L"delete line", [this]() {
        VOICE_LOG("Voice command: Delete line");
        // Send Ctrl+Shift+K or custom command
    });

    AddCommand(L"duplicate line", [this]() {
        VOICE_LOG("Voice command: Duplicate line");
        // Send Ctrl+D or custom command
    });

    // Search commands
    AddCommand(L"find", [this]() {
        VOICE_LOG("Voice command: Find");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_FIND, 0), 0);
        }
    });

    AddCommand(L"find and replace", [this]() {
        VOICE_LOG("Voice command: Find and replace");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_EDIT_REPLACE, 0), 0);
        }
    });

    AddCommandWithArg(L"search for *", [this](const std::wstring& term) {
        VOICE_LOG_W(L"Voice command: Search for '%s'", term.c_str());
        // Would need to populate search box with term
    });

    // View commands
    AddCommand(L"toggle terminal", [this]() {
        VOICE_LOG("Voice command: Toggle terminal");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_VIEW_TERMINAL, 0), 0);
        }
    });

    AddCommand(L"show terminal", [this]() {
        VOICE_LOG("Voice command: Show terminal");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_VIEW_TERMINAL, 0), 0);
        }
    });

    AddCommand(L"hide terminal", [this]() {
        VOICE_LOG("Voice command: Hide terminal");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_VIEW_TERMINAL, 0), 0);
        }
    });

    AddCommand(L"toggle sidebar", [this]() {
        VOICE_LOG("Voice command: Toggle sidebar");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_VIEW_SIDEBAR, 0), 0);
        }
    });

    AddCommand(L"zoom in", [this]() {
        VOICE_LOG("Voice command: Zoom in");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_VIEW_ZOOM_IN, 0), 0);
        }
    });

    AddCommand(L"zoom out", [this]() {
        VOICE_LOG("Voice command: Zoom out");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(ID_VIEW_ZOOM_OUT, 0), 0);
        }
    });

    // Agent control
    AddCommand(L"start agent", [this]() {
        VOICE_LOG("Voice command: Start agent");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(IDM_AGENT_START_LOOP, 0), 0);
        }
    });

    AddCommand(L"stop agent", [this]() {
        VOICE_LOG("Voice command: Stop agent");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(IDM_AGENT_STOP, 0), 0);
        }
    });

    AddCommand(L"agent status", [this]() {
        VOICE_LOG("Voice command: Agent status");
        if (m_ide) {
            PostMessage(reinterpret_cast<HWND>(m_ide), WM_COMMAND,
                MAKEWPARAM(IDM_AGENT_VIEW_STATUS, 0), 0);
        }
    });

    // Voice control meta-commands
    AddCommand(L"stop listening", [this]() {
        VOICE_LOG("Voice command: Stop listening");
        StopListening();
    });

    AddCommand(L"pause listening", [this]() {
        VOICE_LOG("Voice command: Pause listening");
        PauseListening();
    });

    AddCommand(L"mute", [this]() {
        VOICE_LOG("Voice command: Mute");
        PauseListening();
    });

    AddCommand(L"stop speaking", [this]() {
        VOICE_LOG("Voice command: Stop speaking");
        StopSpeaking();
    });

    AddCommand(L"be quiet", [this]() {
        VOICE_LOG("Voice command: Be quiet");
        StopSpeaking();
    });

    // Help
    AddCommand(L"list commands", [this]() {
        VOICE_LOG("Voice command: List commands");
        std::wstring help = L"Available voice commands: ";
        {
            std::lock_guard<std::mutex> lock(m_commandMutex);
            int count = 0;
            for (const auto& pair : m_commands) {
                if (count++ > 0) help += L", ";
                help += pair.first;
                if (count > 10) {
                    help += L", and more";
                    break;
                }
            }
        }
        SpeakAsync(help);
    });

    AddCommand(L"voice help", [this]() {
        VOICE_LOG("Voice command: Voice help");
        SpeakAsync(L"Say a command like: new file, save file, build project, "
                   L"run tests, generate code, explain selection, or refactor this.");
    });

    VOICE_LOG("Registered %zu IDE voice commands", m_commands.size() + m_patternCommands.size());
}

void VoiceAutomation::UnregisterIDECommands() {
    ClearCommands();
    m_ide = nullptr;
}

// ============================================================================
// Events and Statistics
// ============================================================================
void VoiceAutomation::SetEventCallback(SpeechEventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    m_eventCallback = callback;
}

void VoiceAutomation::EmitEvent(const SpeechEvent& evt) {
    std::lock_guard<std::mutex> lock(m_callbackMutex);
    if (m_eventCallback) {
        m_eventCallback(evt);
    }
}

VoiceStatistics VoiceAutomation::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void VoiceAutomation::ResetStatistics() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    memset(&m_stats, 0, sizeof(m_stats));
}

// ============================================================================
// String Conversion Helpers
// ============================================================================
std::wstring VoiceAutomation::StringToWide(const std::string& str) {
    if (str.empty()) {
        return L"";
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(),
        static_cast<int>(str.size()), nullptr, 0);
    if (size <= 0) {
        return L"";
    }

    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(),
        static_cast<int>(str.size()), &result[0], size);

    return result;
}

std::string VoiceAutomation::WideToString(const std::wstring& wstr) {
    if (wstr.empty()) {
        return "";
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
        static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }

    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
        static_cast<int>(wstr.size()), &result[0], size, nullptr, nullptr);

    return result;
}
