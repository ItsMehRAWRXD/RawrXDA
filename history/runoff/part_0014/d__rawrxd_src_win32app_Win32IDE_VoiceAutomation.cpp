// ============================================================================
// Win32IDE_VoiceAutomation.cpp — Voice Automation UI Panel (Phase 44)
// Integrates VoiceAutomation engine into the Win32 IDE with:
//   - Toggle button to enable/disable voice responses
//   - Voice/Provider selection dropdowns
//   - Rate, Volume, Pitch sliders
//   - Speech status indicator
//   - Menu items under Tools > Voice Automation
//   - Keyboard shortcut: Ctrl+Shift+A to toggle
//   - Integration with ChatSystem response pipeline
// ============================================================================

#include "Win32IDE.h"
#include "../core/voice_automation.hpp"
#include "../chat_interface_real.hpp"
#include <commctrl.h>
#include <vector>
#include <cstdio>
#include <string>

// ============================================================================
// Constants
// ============================================================================

// Menu command IDs (Phase 44 range: 10200-10220 — avoids Plugin System collision)
#define IDM_VOICE_AUTO_TOGGLE      10200
#define IDM_VOICE_AUTO_SETTINGS    10201
#define IDM_VOICE_AUTO_NEXT_VOICE  10202
#define IDM_VOICE_AUTO_PREV_VOICE  10203
#define IDM_VOICE_AUTO_RATE_UP     10204
#define IDM_VOICE_AUTO_RATE_DOWN   10205
#define IDM_VOICE_AUTO_STOP        10206

// Child control IDs
#define IDC_VA_TOGGLE_BTN      10210
#define IDC_VA_PROVIDER_COMBO  10211
#define IDC_VA_VOICE_COMBO     10212
#define IDC_VA_RATE_SLIDER     10213
#define IDC_VA_VOLUME_SLIDER   10214
#define IDC_VA_PITCH_SLIDER    10215
#define IDC_VA_STATUS_LABEL    10216
#define IDC_VA_RATE_LABEL      10217
#define IDC_VA_VOLUME_LABEL    10218
#define IDC_VA_PITCH_LABEL     10219
#define IDC_VA_STOP_BTN        10220

// Timer for status updates
static const UINT_PTR VA_TIMER_ID = 0x7C10;
static const int VA_TIMER_INTERVAL_MS = 250;

// ============================================================================
// Static UI handles
// ============================================================================
static HWND g_hwndVAPanel         = nullptr;
static HWND g_hwndVAToggleBtn     = nullptr;
static HWND g_hwndVAProviderCombo = nullptr;
static HWND g_hwndVAVoiceCombo    = nullptr;
static HWND g_hwndVARateSlider    = nullptr;
static HWND g_hwndVAVolumeSlider  = nullptr;
static HWND g_hwndVAPitchSlider   = nullptr;
static HWND g_hwndVAStatusLabel   = nullptr;
static HWND g_hwndVARateLabel     = nullptr;
static HWND g_hwndVAVolumeLabel   = nullptr;
static HWND g_hwndVAPitchLabel    = nullptr;
static HWND g_hwndVAStopBtn       = nullptr;

static bool g_vaInitialized = false;

// ============================================================================
// Callbacks — route events to Win32 UI
// ============================================================================
static void vaToggleCallback(bool enabled, void* /*userData*/) {
    if (g_hwndVAToggleBtn) {
        SetWindowTextA(g_hwndVAToggleBtn, enabled ? "Voice: ON" : "Voice: OFF");
    }
    if (g_hwndVAStatusLabel) {
        SetWindowTextA(g_hwndVAStatusLabel, enabled ? "Voice automation enabled" : "Voice automation disabled");
    }
}

static void vaSpeechCallback(const char* text, const char* voiceId, void* /*userData*/) {
    if (g_hwndVAStatusLabel) {
        char buf[256];
        int textLen = text ? static_cast<int>(strlen(text)) : 0;
        snprintf(buf, sizeof(buf), "Speaking %d chars [%s]", textLen, voiceId ? voiceId : "?");
        SetWindowTextA(g_hwndVAStatusLabel, buf);
    }
}

static void vaErrorCallback(const char* error, int code, void* /*userData*/) {
    if (g_hwndVAStatusLabel) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Error %d: %s", code, error ? error : "unknown");
        SetWindowTextA(g_hwndVAStatusLabel, buf);
    }
}

// ============================================================================
// Refresh provider and voice dropdowns
// ============================================================================
static void vaRefreshProviderList() {
    if (!g_hwndVAProviderCombo) return;
    SendMessage(g_hwndVAProviderCombo, CB_RESETCONTENT, 0, 0);

    auto& va = getVoiceAutomation();
    auto providers = va.listProviders();
    std::string active = va.getActiveProviderName();

    int selIdx = 0;
    for (size_t i = 0; i < providers.size(); ++i) {
        SendMessageA(g_hwndVAProviderCombo, CB_ADDSTRING, 0, (LPARAM)providers[i].c_str());
        if (providers[i] == active) selIdx = static_cast<int>(i);
    }
    SendMessage(g_hwndVAProviderCombo, CB_SETCURSEL, selIdx, 0);
}

static void vaRefreshVoiceList() {
    if (!g_hwndVAVoiceCombo) return;
    SendMessage(g_hwndVAVoiceCombo, CB_RESETCONTENT, 0, 0);

    auto& va = getVoiceAutomation();
    auto voices = va.listVoices();
    std::string activeVoice = va.getActiveVoiceId();

    int selIdx = 0;
    for (size_t i = 0; i < voices.size(); ++i) {
        char display[256];
        snprintf(display, sizeof(display), "%s (%s) [%s]",
                 voices[i].name, voices[i].language, voices[i].provider);
        SendMessageA(g_hwndVAVoiceCombo, CB_ADDSTRING, 0, (LPARAM)display);
        // Store voice ID as item data
        SendMessageA(g_hwndVAVoiceCombo, CB_SETITEMDATA, i, (LPARAM)i);
        if (std::string(voices[i].id) == activeVoice) selIdx = static_cast<int>(i);
    }
    SendMessage(g_hwndVAVoiceCombo, CB_SETCURSEL, selIdx, 0);
}

// ============================================================================
// Create the Voice Automation panel
// ============================================================================
void Win32IDE_CreateVoiceAutomationPanel(HWND hwndParent, int x, int y, int w, int h) {
    if (g_vaInitialized) return;

    // Initialize VoiceAutomation engine
    VoiceAutoConfig config;
    config.enabled = false;
    config.rate = 1.0f;
    config.volume = 0.8f;
    config.pitch = 1.0f;
    config.speakOnStream = true;
    config.interruptOnNew = true;
    config.pluginSearchPaths = "plugins;voice_plugins;extensions/voice";

    auto& va = getVoiceAutomation();
    va.initialize(config);
    va.setToggleCallback(vaToggleCallback, nullptr);
    va.setSpeechCallback(vaSpeechCallback, nullptr);
    va.setErrorCallback(vaErrorCallback, nullptr);

    // Panel background
    g_hwndVAPanel = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                     x, y, w, h, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);

    int cx = 8, cy = 4;

    // Toggle button
    g_hwndVAToggleBtn = CreateWindowExA(0, "BUTTON", "Voice: OFF",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx, cy, 100, 28, hwndParent, (HMENU)(INT_PTR)IDC_VA_TOGGLE_BTN,
        GetModuleHandle(nullptr), nullptr);
    cx += 108;

    // Stop button
    g_hwndVAStopBtn = CreateWindowExA(0, "BUTTON", "Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        cx, cy, 60, 28, hwndParent, (HMENU)(INT_PTR)IDC_VA_STOP_BTN,
        GetModuleHandle(nullptr), nullptr);
    cx += 68;

    // Provider dropdown
    CreateWindowExA(0, "STATIC", "Provider:", WS_CHILD | WS_VISIBLE,
                     cx, cy + 6, 56, 20, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    cx += 58;
    g_hwndVAProviderCombo = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        cx, cy, 140, 200, hwndParent, (HMENU)(INT_PTR)IDC_VA_PROVIDER_COMBO,
        GetModuleHandle(nullptr), nullptr);
    cx += 148;

    // Voice dropdown
    CreateWindowExA(0, "STATIC", "Voice:", WS_CHILD | WS_VISIBLE,
                     cx, cy + 6, 40, 20, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    cx += 42;
    g_hwndVAVoiceCombo = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        cx, cy, 180, 200, hwndParent, (HMENU)(INT_PTR)IDC_VA_VOICE_COMBO,
        GetModuleHandle(nullptr), nullptr);

    // Second row: sliders
    cy += 34;
    cx = 8;

    // Rate slider
    CreateWindowExA(0, "STATIC", "Rate:", WS_CHILD | WS_VISIBLE,
                     cx, cy + 2, 36, 20, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    cx += 38;
    g_hwndVARateSlider = CreateWindowExA(0, TRACKBAR_CLASSA, "",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        cx, cy, 120, 24, hwndParent, (HMENU)(INT_PTR)IDC_VA_RATE_SLIDER,
        GetModuleHandle(nullptr), nullptr);
    SendMessage(g_hwndVARateSlider, TBM_SETRANGE, TRUE, MAKELONG(1, 100)); // 0.1x to 10.0x
    SendMessage(g_hwndVARateSlider, TBM_SETPOS, TRUE, 10);  // 1.0x default
    cx += 124;
    g_hwndVARateLabel = CreateWindowExA(0, "STATIC", "1.0x", WS_CHILD | WS_VISIBLE,
                                         cx, cy + 2, 36, 20, hwndParent,
                                         (HMENU)(INT_PTR)IDC_VA_RATE_LABEL,
                                         GetModuleHandle(nullptr), nullptr);
    cx += 44;

    // Volume slider
    CreateWindowExA(0, "STATIC", "Vol:", WS_CHILD | WS_VISIBLE,
                     cx, cy + 2, 28, 20, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    cx += 30;
    g_hwndVAVolumeSlider = CreateWindowExA(0, TRACKBAR_CLASSA, "",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        cx, cy, 100, 24, hwndParent, (HMENU)(INT_PTR)IDC_VA_VOLUME_SLIDER,
        GetModuleHandle(nullptr), nullptr);
    SendMessage(g_hwndVAVolumeSlider, TBM_SETRANGE, TRUE, MAKELONG(0, 100)); // 0% to 100%
    SendMessage(g_hwndVAVolumeSlider, TBM_SETPOS, TRUE, 80);  // 80% default
    cx += 104;
    g_hwndVAVolumeLabel = CreateWindowExA(0, "STATIC", "80%", WS_CHILD | WS_VISIBLE,
                                           cx, cy + 2, 32, 20, hwndParent,
                                           (HMENU)(INT_PTR)IDC_VA_VOLUME_LABEL,
                                           GetModuleHandle(nullptr), nullptr);
    cx += 40;

    // Pitch slider
    CreateWindowExA(0, "STATIC", "Pitch:", WS_CHILD | WS_VISIBLE,
                     cx, cy + 2, 38, 20, hwndParent, nullptr, GetModuleHandle(nullptr), nullptr);
    cx += 40;
    g_hwndVAPitchSlider = CreateWindowExA(0, TRACKBAR_CLASSA, "",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
        cx, cy, 100, 24, hwndParent, (HMENU)(INT_PTR)IDC_VA_PITCH_SLIDER,
        GetModuleHandle(nullptr), nullptr);
    SendMessage(g_hwndVAPitchSlider, TBM_SETRANGE, TRUE, MAKELONG(5, 20)); // 0.5x to 2.0x
    SendMessage(g_hwndVAPitchSlider, TBM_SETPOS, TRUE, 10);  // 1.0x default
    cx += 104;
    g_hwndVAPitchLabel = CreateWindowExA(0, "STATIC", "1.0x", WS_CHILD | WS_VISIBLE,
                                          cx, cy + 2, 36, 20, hwndParent,
                                          (HMENU)(INT_PTR)IDC_VA_PITCH_LABEL,
                                          GetModuleHandle(nullptr), nullptr);

    // Status bar
    cy += 28;
    g_hwndVAStatusLabel = CreateWindowExA(0, "STATIC", "Voice automation ready (Ctrl+Shift+A to toggle)",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        8, cy, w - 16, 20, hwndParent, (HMENU)(INT_PTR)IDC_VA_STATUS_LABEL,
        GetModuleHandle(nullptr), nullptr);

    // Populate dropdowns
    vaRefreshProviderList();
    vaRefreshVoiceList();

    // Start status update timer
    SetTimer(hwndParent, VA_TIMER_ID, VA_TIMER_INTERVAL_MS, nullptr);

    g_vaInitialized = true;
}

// ============================================================================
// Menu integration — add Voice Automation items to Tools menu
// ============================================================================
void Win32IDE_AddVoiceAutomationMenu(HMENU hToolsMenu) {
    AppendMenuA(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hToolsMenu, MF_STRING, IDM_VOICE_AUTO_TOGGLE, "Toggle Voice Automation\tCtrl+Shift+A");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_VOICE_AUTO_STOP, "Stop Speaking\tEscape");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_VOICE_AUTO_NEXT_VOICE, "Next Voice\tCtrl+Shift+]");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_VOICE_AUTO_PREV_VOICE, "Previous Voice\tCtrl+Shift+[");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_VOICE_AUTO_RATE_UP, "Increase Speech Rate\tCtrl+Shift+=");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_VOICE_AUTO_RATE_DOWN, "Decrease Speech Rate\tCtrl+Shift+-");
}

// ============================================================================
// Handle WM_COMMAND for voice automation controls
// ============================================================================
bool Win32IDE_HandleVoiceAutomationCommand(HWND hwnd, WPARAM wParam) {
    auto& va = getVoiceAutomation();
    int cmd = LOWORD(wParam);
    int notif = HIWORD(wParam);

    switch (cmd) {
    case IDM_VOICE_AUTO_TOGGLE:
    case IDC_VA_TOGGLE_BTN:
        va.toggle();
        return true;

    case IDM_VOICE_AUTO_STOP:
    case IDC_VA_STOP_BTN:
        va.cancelAll();
        if (g_hwndVAStatusLabel) SetWindowTextA(g_hwndVAStatusLabel, "Speech stopped");
        return true;

    case IDM_VOICE_AUTO_NEXT_VOICE:
    case IDM_VOICE_AUTO_PREV_VOICE: {
        auto voices = va.listVoices();
        if (voices.empty()) return true;
        std::string current = va.getActiveVoiceId();
        int idx = 0;
        for (size_t i = 0; i < voices.size(); ++i) {
            if (std::string(voices[i].id) == current) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (cmd == IDM_VOICE_AUTO_NEXT_VOICE) {
            idx = (idx + 1) % static_cast<int>(voices.size());
        } else {
            idx = (idx - 1 + static_cast<int>(voices.size())) % static_cast<int>(voices.size());
        }
        va.setVoice(voices[idx].id);
        vaRefreshVoiceList();
        char buf[256];
        snprintf(buf, sizeof(buf), "Voice: %s (%s)", voices[idx].name, voices[idx].provider);
        if (g_hwndVAStatusLabel) SetWindowTextA(g_hwndVAStatusLabel, buf);
        return true;
    }

    case IDM_VOICE_AUTO_RATE_UP: {
        auto cfg = va.getConfig();
        va.setRate(cfg.rate + 0.2f);
        return true;
    }

    case IDM_VOICE_AUTO_RATE_DOWN: {
        auto cfg = va.getConfig();
        va.setRate(cfg.rate - 0.2f);
        return true;
    }

    case IDC_VA_PROVIDER_COMBO:
        if (notif == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_hwndVAProviderCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                char buf[128];
                SendMessageA(g_hwndVAProviderCombo, CB_GETLBTEXT, sel, (LPARAM)buf);
                va.setActiveProvider(buf);
                vaRefreshVoiceList();
            }
        }
        return true;

    case IDC_VA_VOICE_COMBO:
        if (notif == CBN_SELCHANGE) {
            int sel = (int)SendMessage(g_hwndVAVoiceCombo, CB_GETCURSEL, 0, 0);
            if (sel >= 0) {
                auto voices = va.listVoices();
                if (sel < static_cast<int>(voices.size())) {
                    va.setVoice(voices[sel].id);
                }
            }
        }
        return true;
    }

    return false;
}

// ============================================================================
// Handle WM_HSCROLL for sliders
// ============================================================================
bool Win32IDE_HandleVoiceAutomationScroll(HWND hwnd, LPARAM lParam) {
    HWND hTrack = (HWND)lParam;
    auto& va = getVoiceAutomation();

    if (hTrack == g_hwndVARateSlider) {
        int pos = (int)SendMessage(g_hwndVARateSlider, TBM_GETPOS, 0, 0);
        float rate = static_cast<float>(pos) / 10.0f;
        va.setRate(rate);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fx", rate);
        if (g_hwndVARateLabel) SetWindowTextA(g_hwndVARateLabel, buf);
        return true;
    }

    if (hTrack == g_hwndVAVolumeSlider) {
        int pos = (int)SendMessage(g_hwndVAVolumeSlider, TBM_GETPOS, 0, 0);
        float volume = static_cast<float>(pos) / 100.0f;
        va.setVolume(volume);
        char buf[32];
        snprintf(buf, sizeof(buf), "%d%%", pos);
        if (g_hwndVAVolumeLabel) SetWindowTextA(g_hwndVAVolumeLabel, buf);
        return true;
    }

    if (hTrack == g_hwndVAPitchSlider) {
        int pos = (int)SendMessage(g_hwndVAPitchSlider, TBM_GETPOS, 0, 0);
        float pitch = static_cast<float>(pos) / 10.0f;
        va.setPitch(pitch);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1fx", pitch);
        if (g_hwndVAPitchLabel) SetWindowTextA(g_hwndVAPitchLabel, buf);
        return true;
    }

    return false;
}

// ============================================================================
// Timer handler — update status display
// ============================================================================
void Win32IDE_VoiceAutomationTimerTick() {
    if (!g_vaInitialized) return;

    auto& va = getVoiceAutomation();

    // Update toggle button text
    if (g_hwndVAToggleBtn) {
        bool enabled = va.isEnabled();
        SetWindowTextA(g_hwndVAToggleBtn, enabled ? "Voice: ON" : "Voice: OFF");
    }

    // Update status if speaking
    if (va.isSpeaking() && g_hwndVAStatusLabel) {
        auto metrics = va.getMetrics();
        char buf[256];
        snprintf(buf, sizeof(buf), "Speaking... (%lld total, %.0fms avg)",
                 (long long)metrics.totalSpeechRequests, metrics.avgSpeechLatencyMs);
        SetWindowTextA(g_hwndVAStatusLabel, buf);
    }
}

// ============================================================================
// Cleanup
// ============================================================================
void Win32IDE_DestroyVoiceAutomationPanel() {
    if (!g_vaInitialized) return;

    auto& va = getVoiceAutomation();
    va.shutdown();

    g_vaInitialized = false;
    g_hwndVAPanel = nullptr;
    g_hwndVAToggleBtn = nullptr;
    g_hwndVAProviderCombo = nullptr;
    g_hwndVAVoiceCombo = nullptr;
    g_hwndVARateSlider = nullptr;
    g_hwndVAVolumeSlider = nullptr;
    g_hwndVAPitchSlider = nullptr;
    g_hwndVAStatusLabel = nullptr;
    g_hwndVAStopBtn = nullptr;
}

// ============================================================================
// Chat Pipeline Integration — Hook into response flow
// ============================================================================

/**
 * Call this from the chat response handler after a response is generated.
 * If voice automation is enabled, the response will be spoken aloud.
 */
void Win32IDE_VoiceAutomation_OnResponse(const std::string& responseText) {
    if (!g_vaInitialized) return;
    auto& va = getVoiceAutomation();
    if (va.isEnabled()) {
        va.speakResponse(responseText);
    }
}

/**
 * Call this from streaming response chunk handler.
 * Accumulates text and speaks sentence-by-sentence for natural pacing.
 */
static std::string g_streamAccumulator;
static const char* g_sentenceEnders = ".!?\n";

void Win32IDE_VoiceAutomation_OnStreamChunk(const std::string& chunk) {
    if (!g_vaInitialized) return;
    auto& va = getVoiceAutomation();
    if (!va.isEnabled()) return;

    g_streamAccumulator += chunk;

    // Check if we have a complete sentence to speak
    size_t lastSentenceEnd = std::string::npos;
    for (const char* p = g_sentenceEnders; *p; ++p) {
        size_t pos = g_streamAccumulator.rfind(*p);
        if (pos != std::string::npos) {
            if (lastSentenceEnd == std::string::npos || pos > lastSentenceEnd) {
                lastSentenceEnd = pos;
            }
        }
    }

    if (lastSentenceEnd != std::string::npos && lastSentenceEnd > 0) {
        std::string toSpeak = g_streamAccumulator.substr(0, lastSentenceEnd + 1);
        g_streamAccumulator = g_streamAccumulator.substr(lastSentenceEnd + 1);
        va.speakStreamChunk(toSpeak);
    }
}

/**
 * Call this when streaming is complete to flush remaining text.
 */
void Win32IDE_VoiceAutomation_OnStreamComplete() {
    if (!g_vaInitialized) return;
    auto& va = getVoiceAutomation();
    if (!va.isEnabled()) return;

    if (!g_streamAccumulator.empty()) {
        va.speakStreamChunk(g_streamAccumulator);
        g_streamAccumulator.clear();
    }
}
