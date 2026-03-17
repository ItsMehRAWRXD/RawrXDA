// ============================================================================
// Win32IDE_VoiceChat.cpp — Voice Chat UI Panel for Win32 IDE (Phase 33)
// Integrates VoiceChat engine into the Win32 IDE with:
//   - Voice Chat menu items under Tools
//   - Voice panel in bottom panel area
//   - PTT hotkey support (Ctrl+Shift+V default)
//   - VU meter / level indicator
//   - Device selection dialog
//   - Transcription display
//   - Room management UI
// ============================================================================

#include "Win32IDE.h"
#include "../core/voice_chat.hpp"
#include <commctrl.h>
#include <cstdio>
#include <string>

// ============================================================================
// Static VoiceChat engine instance (one per IDE)
// ============================================================================
static std::unique_ptr<VoiceChat> g_voiceChat;
static bool g_voiceChatInitialized = false;
static HWND g_hwndVoicePanel = nullptr;
static HWND g_hwndVoiceStatus = nullptr;
static HWND g_hwndVoiceLevel = nullptr;      // progress bar as VU meter
static HWND g_hwndVoiceTranscript = nullptr;  // transcript edit box
static HWND g_hwndVoicePTTBtn = nullptr;
static HWND g_hwndVoiceRecordBtn = nullptr;
static HWND g_hwndVoiceModeCombo = nullptr;
static HWND g_hwndVoiceDeviceCombo = nullptr;
static HWND g_hwndVoiceRoomEdit = nullptr;
static HWND g_hwndVoiceJoinBtn = nullptr;
static HWND g_hwndVoiceSpeakBtn = nullptr;

// Timer ID for VU meter updates
static const UINT_PTR VOICE_TIMER_ID = 0x7C01;
static const int VOICE_TIMER_INTERVAL_MS = 50;

// ============================================================================
// Voice Chat Event Callback — routes events to Win32 UI
// ============================================================================
static void voiceChatEventCallback(const char* event, const char* detail, void* userData)
{
    (void)userData;
    // Post event to UI thread for safe updates
    if (g_hwndVoiceStatus) {
        std::string msg = std::string(event);
        if (detail && detail[0]) {
            msg += " — ";
            msg += detail;
        }
        SetWindowTextA(g_hwndVoiceStatus, msg.c_str());
    }
}

static void voiceChatVADCallback(VADState state, float rmsLevel, void* userData)
{
    (void)userData;
    // Update VU meter
    if (g_hwndVoiceLevel) {
        int level = static_cast<int>(rmsLevel * 1000.0f);
        if (level > 1000) level = 1000;
        SendMessage(g_hwndVoiceLevel, PBM_SETPOS, level, 0);
    }
}

static void voiceChatTranscriptionCallback(const char* text, void* userData)
{
    (void)userData;
    if (g_hwndVoiceTranscript && text) {
        // Append transcription to edit box
        int len = GetWindowTextLengthA(g_hwndVoiceTranscript);
        SendMessageA(g_hwndVoiceTranscript, EM_SETSEL, len, len);
        std::string line = std::string("[STT] ") + text + "\r\n";
        SendMessageA(g_hwndVoiceTranscript, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    }
}

// ============================================================================
// Initialize Voice Chat Subsystem
// ============================================================================
void Win32IDE::initVoiceChat()
{
    if (g_voiceChatInitialized) return;

    g_voiceChat = std::make_unique<VoiceChat>();

    VoiceChatConfig cfg;
    cfg.sampleRate    = 16000;
    cfg.channels      = 1;
    cfg.bitsPerSample = 16;
    cfg.enableVAD     = true;
    cfg.vadThreshold  = 0.02f;
    cfg.vadSilenceMs  = 1500;
    cfg.enableMetrics = true;
    cfg.userName      = "IDE User";

    g_voiceChat->configure(cfg);
    g_voiceChat->setEventCallback(voiceChatEventCallback, nullptr);
    g_voiceChat->setVADCallback(voiceChatVADCallback, nullptr);
    g_voiceChat->setTranscriptionCallback(voiceChatTranscriptionCallback, nullptr);

    g_voiceChatInitialized = true;
    logStructured("INFO", "Voice Chat subsystem initialized", "{}");
}

// ============================================================================
// Shutdown Voice Chat
// ============================================================================
void Win32IDE::shutdownVoiceChat()
{
    if (!g_voiceChatInitialized) return;

    if (g_voiceChat) {
        g_voiceChat->shutdown();
        g_voiceChat.reset();
    }

    g_voiceChatInitialized = false;
    logStructured("INFO", "Voice Chat subsystem shut down", "{}");
}

// ============================================================================
// Create Voice Chat Panel UI
// ============================================================================
void Win32IDE::createVoiceChatPanel(HWND hwndParent)
{
    if (!g_voiceChatInitialized) {
        initVoiceChat();
    }

    // Create voice panel window
    g_hwndVoicePanel = CreateWindowExA(
        0, "STATIC", nullptr,
        WS_CHILD | WS_CLIPCHILDREN,
        0, 0, 600, 200,
        hwndParent, nullptr, m_hInstance, nullptr);

    int y = 4;
    int x = 8;

    // Title label
    CreateWindowExA(0, "STATIC", "🎙️ Voice Chat",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, 120, 20, g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    // Mode selector
    CreateWindowExA(0, "STATIC", "Mode:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x + 130, y, 40, 20, g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    g_hwndVoiceModeCombo = CreateWindowExA(
        0, "COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x + 175, y - 2, 130, 120,
        g_hwndVoicePanel, (HMENU)10201, m_hInstance, nullptr);
    SendMessageA(g_hwndVoiceModeCombo, CB_ADDSTRING, 0, (LPARAM)"Push-to-Talk");
    SendMessageA(g_hwndVoiceModeCombo, CB_ADDSTRING, 0, (LPARAM)"Continuous (VAD)");
    SendMessageA(g_hwndVoiceModeCombo, CB_ADDSTRING, 0, (LPARAM)"Disabled");
    SendMessageA(g_hwndVoiceModeCombo, CB_SETCURSEL, 0, 0);

    // Input device
    CreateWindowExA(0, "STATIC", "Input:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x + 320, y, 40, 20, g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    g_hwndVoiceDeviceCombo = CreateWindowExA(
        0, "COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x + 365, y - 2, 200, 120,
        g_hwndVoicePanel, (HMENU)10202, m_hInstance, nullptr);

    // Populate input devices
    auto devices = VoiceChat::enumerateInputDevices();
    for (const auto& dev : devices) {
        SendMessageA(g_hwndVoiceDeviceCombo, CB_ADDSTRING, 0, (LPARAM)dev.name.c_str());
    }
    if (!devices.empty()) {
        SendMessageA(g_hwndVoiceDeviceCombo, CB_SETCURSEL, 0, 0);
    }

    y += 28;

    // PTT Button
    g_hwndVoicePTTBtn = CreateWindowExA(
        0, "BUTTON", "🎤 Hold to Talk (Ctrl+Shift+V)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, 230, 30, g_hwndVoicePanel, (HMENU)IDM_VOICE_PTT, m_hInstance, nullptr);

    // Record / Stop button
    g_hwndVoiceRecordBtn = CreateWindowExA(
        0, "BUTTON", "⏺ Record",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 240, y, 100, 30, g_hwndVoicePanel, (HMENU)IDM_VOICE_RECORD, m_hInstance, nullptr);

    // Speak (TTS) button
    g_hwndVoiceSpeakBtn = CreateWindowExA(
        0, "BUTTON", "🔊 Speak",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 350, y, 80, 30, g_hwndVoicePanel, (HMENU)IDM_VOICE_SPEAK, m_hInstance, nullptr);

    y += 36;

    // VU Meter (progress bar)
    CreateWindowExA(0, "STATIC", "Level:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y + 2, 40, 16, g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    g_hwndVoiceLevel = CreateWindowExA(
        0, PROGRESS_CLASSA, nullptr,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        x + 48, y, 250, 18,
        g_hwndVoicePanel, nullptr, m_hInstance, nullptr);
    SendMessage(g_hwndVoiceLevel, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
    SendMessage(g_hwndVoiceLevel, PBM_SETPOS, 0, 0);
    SendMessage(g_hwndVoiceLevel, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 200, 80));

    // Room controls
    CreateWindowExA(0, "STATIC", "Room:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x + 310, y + 2, 36, 16, g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    g_hwndVoiceRoomEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "general",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x + 350, y, 120, 20,
        g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    g_hwndVoiceJoinBtn = CreateWindowExA(
        0, "BUTTON", "Join",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + 478, y - 1, 60, 22,
        g_hwndVoicePanel, (HMENU)IDM_VOICE_JOIN_ROOM, m_hInstance, nullptr);

    y += 26;

    // Status label
    g_hwndVoiceStatus = CreateWindowExA(
        0, "STATIC", "Ready — Voice chat idle",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, 550, 18, g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    y += 22;

    // Transcript area
    g_hwndVoiceTranscript = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        x, y, 555, 60,
        g_hwndVoicePanel, nullptr, m_hInstance, nullptr);

    // Start VU meter timer
    SetTimer(hwndParent, VOICE_TIMER_ID, VOICE_TIMER_INTERVAL_MS, nullptr);

    logStructured("INFO", "Voice Chat panel created", "{}");
}

// ============================================================================
// Layout Voice Chat Panel (called during onSize)
// ============================================================================
void Win32IDE::layoutVoiceChatPanel(int panelWidth, int panelHeight)
{
    if (g_hwndVoicePanel) {
        MoveWindow(g_hwndVoicePanel, 0, 0, panelWidth, panelHeight, TRUE);
        ShowWindow(g_hwndVoicePanel, SW_SHOW);
    }
}

// ============================================================================
// Voice Chat Timer Handler (VU meter update)
// ============================================================================
void Win32IDE::onVoiceChatTimer()
{
    if (!g_voiceChatInitialized || !g_voiceChat) return;

    float rms = g_voiceChat->getCurrentRMS();
    if (g_hwndVoiceLevel) {
        int level = static_cast<int>(rms * 1000.0f);
        if (level > 1000) level = 1000;
        SendMessage(g_hwndVoiceLevel, PBM_SETPOS, level, 0);

        // Color based on level
        COLORREF color;
        if (level > 500) color = RGB(255, 60, 60);      // red (loud)
        else if (level > 200) color = RGB(255, 200, 0);  // yellow
        else color = RGB(0, 200, 80);                     // green
        SendMessage(g_hwndVoiceLevel, PBM_SETBARCOLOR, 0, (LPARAM)color);
    }
}

// ============================================================================
// Command Handlers
// ============================================================================
void Win32IDE::cmdVoiceRecord()
{
    if (!g_voiceChatInitialized) initVoiceChat();

    if (g_voiceChat->isRecording()) {
        g_voiceChat->stopRecording();
        if (g_hwndVoiceRecordBtn) {
            SetWindowTextA(g_hwndVoiceRecordBtn, "⏺ Record");
        }
        logInfo("Voice: Recording stopped");

        // Auto-transcribe if API is configured
        std::string transcript;
        VoiceChatResult r = g_voiceChat->transcribeLastRecording(transcript);
        if (r.success && !transcript.empty()) {
            logInfo("Voice STT: " + transcript);
        }
    } else {
        g_voiceChat->startRecording();
        if (g_hwndVoiceRecordBtn) {
            SetWindowTextA(g_hwndVoiceRecordBtn, "⏹ Stop");
        }
        logInfo("Voice: Recording started");
    }
}

void Win32IDE::cmdVoicePTT()
{
    if (!g_voiceChatInitialized) initVoiceChat();

    if (g_voiceChat->isPTTActive()) {
        g_voiceChat->pttEnd();
        logInfo("Voice PTT: Released");
    } else {
        g_voiceChat->pttBegin();
        logInfo("Voice PTT: Holding");
    }
}

void Win32IDE::cmdVoiceSpeak()
{
    if (!g_voiceChatInitialized) initVoiceChat();

    // Get selected text from editor or last transcript
    char buf[4096] = {};
    if (m_hwndEditor) {
        DWORD start = 0, end = 0;
        SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        if (end > start && (end - start) < sizeof(buf)) {
            // Get selected text
            TEXTRANGEA tr;
            tr.chrg.cpMin = start;
            tr.chrg.cpMax = end;
            tr.lpstrText = buf;
            SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        }
    }

    if (buf[0]) {
        g_voiceChat->speak(std::string(buf));
        logInfo("Voice TTS: Speaking selected text");
    } else {
        logInfo("Voice TTS: No text selected");
    }
}

void Win32IDE::cmdVoiceJoinRoom()
{
    if (!g_voiceChatInitialized) initVoiceChat();

    char roomName[128] = {};
    if (g_hwndVoiceRoomEdit) {
        GetWindowTextA(g_hwndVoiceRoomEdit, roomName, sizeof(roomName));
    }

    if (g_voiceChat->isInRoom()) {
        g_voiceChat->leaveRoom();
        if (g_hwndVoiceJoinBtn) SetWindowTextA(g_hwndVoiceJoinBtn, "Join");
        logInfo("Voice: Left room");
    } else {
        VoiceChatResult r = g_voiceChat->joinRoom(roomName[0] ? roomName : "general");
        if (r.success) {
            if (g_hwndVoiceJoinBtn) SetWindowTextA(g_hwndVoiceJoinBtn, "Leave");
            logInfo(std::string("Voice: Joined room '") + roomName + "'");
        }
    }
}

void Win32IDE::cmdVoiceModeChanged()
{
    if (!g_voiceChatInitialized || !g_hwndVoiceModeCombo) return;

    int sel = (int)SendMessageA(g_hwndVoiceModeCombo, CB_GETCURSEL, 0, 0);
    VoiceChatMode mode = static_cast<VoiceChatMode>(sel);
    g_voiceChat->setMode(mode);

    // Enable/disable PTT button based on mode
    if (g_hwndVoicePTTBtn) {
        EnableWindow(g_hwndVoicePTTBtn, mode == VoiceChatMode::PushToTalk);
    }
}

void Win32IDE::cmdVoiceDeviceChanged()
{
    if (!g_voiceChatInitialized || !g_hwndVoiceDeviceCombo) return;

    int sel = (int)SendMessageA(g_hwndVoiceDeviceCombo, CB_GETCURSEL, 0, 0);
    if (sel >= 0) {
        g_voiceChat->selectInputDevice(sel);
        logInfo("Voice: Input device changed");
    }
}

void Win32IDE::cmdVoiceShowDevices()
{
    auto inputs = VoiceChat::enumerateInputDevices();
    auto outputs = VoiceChat::enumerateOutputDevices();

    std::string msg = "=== Input Devices ===\n";
    for (const auto& dev : inputs) {
        msg += "  [" + std::to_string(dev.id) + "] " + dev.name;
        if (dev.isDefault) msg += " (default)";
        msg += "\n";
    }
    msg += "\n=== Output Devices ===\n";
    for (const auto& dev : outputs) {
        msg += "  [" + std::to_string(dev.id) + "] " + dev.name;
        if (dev.isDefault) msg += " (default)";
        msg += "\n";
    }

    MessageBoxA(m_hwndMain, msg.c_str(), "Voice Chat — Audio Devices", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdVoiceShowMetrics()
{
    if (!g_voiceChatInitialized || !g_voiceChat) {
        MessageBoxA(m_hwndMain, "Voice Chat not initialized.", "Voice Metrics", MB_OK);
        return;
    }

    auto m = g_voiceChat->getMetrics();
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "Voice Chat Metrics\n"
        "══════════════════\n"
        "Recordings:       %lld\n"
        "Playbacks:        %lld\n"
        "Transcriptions:   %lld\n"
        "TTS Calls:        %lld\n"
        "Errors:           %lld\n"
        "Bytes Recorded:   %lld\n"
        "Bytes Played:     %lld\n"
        "VAD Speech Events:%lld\n"
        "Relay Sent:       %lld\n"
        "Relay Received:   %lld\n"
        "\n"
        "Avg Record Latency:    %.2f ms\n"
        "Avg Transcribe Latency:%.2f ms\n"
        "Avg TTS Latency:       %.2f ms\n"
        "Avg Playback Latency:  %.2f ms",
        (long long)m.recordingCount.load(), (long long)m.playbackCount.load(),
        (long long)m.transcriptionCount.load(), (long long)m.ttsCount.load(),
        (long long)m.errorCount.load(), (long long)m.bytesRecorded.load(),
        (long long)m.bytesPlayed.load(), (long long)m.vadSpeechEvents.load(),
        (long long)m.relayMessagesSent.load(), (long long)m.relayMessagesRecv.load(),
        m.avgRecordingLatencyMs, m.avgTranscriptionLatencyMs,
        m.avgTtsLatencyMs, m.avgPlaybackLatencyMs);

    MessageBoxA(m_hwndMain, buf, "Voice Chat Metrics", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// Voice Chat Command Router
// ============================================================================
bool Win32IDE::handleVoiceChatCommand(int commandId)
{
    switch (commandId) {
        case IDM_VOICE_RECORD:
            cmdVoiceRecord();
            return true;
        case IDM_VOICE_PTT:
            cmdVoicePTT();
            return true;
        case IDM_VOICE_SPEAK:
            cmdVoiceSpeak();
            return true;
        case IDM_VOICE_JOIN_ROOM:
            cmdVoiceJoinRoom();
            return true;
        case IDM_VOICE_SHOW_DEVICES:
            cmdVoiceShowDevices();
            return true;
        case IDM_VOICE_METRICS:
            cmdVoiceShowMetrics();
            return true;
        case IDM_VOICE_TOGGLE_PANEL:
            cmdVoiceTogglePanel();
            return true;
        default:
            return false;
    }
}

void Win32IDE::cmdVoiceTogglePanel()
{
    m_voicePanelVisible = !m_voicePanelVisible;
    if (g_hwndVoicePanel) {
        ShowWindow(g_hwndVoicePanel, m_voicePanelVisible ? SW_SHOW : SW_HIDE);
    }
    logInfo(m_voicePanelVisible ? "Voice panel shown" : "Voice panel hidden");
}

// ============================================================================
// Access to the global VoiceChat instance (for CLI bridge)
// ============================================================================
VoiceChat* Win32IDE::getVoiceChatEngine()
{
    if (!g_voiceChatInitialized) initVoiceChat();
    return g_voiceChat.get();
}
