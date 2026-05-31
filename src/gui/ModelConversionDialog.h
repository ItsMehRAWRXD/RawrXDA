// ============================================================================
// ModelConversionDialog.h — Pure Win32 Native Model Conversion Dialog
// ============================================================================
// Dialog for model quantization conversion. Detects unsupported quantization
// types and orchestrates conversion via external script. Shows real-time
// progress, terminal output monitoring, ETA, and auto-verification.
//
// Pattern: C-style extern "C" API + OOP internal
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>

// ============================================================================
// Result codes
// ============================================================================
enum class ConversionResult {
    Cancelled,
    ConversionSucceeded,
    ConversionFailed
};

// ============================================================================
// ConversionConfig — passed into the dialog
// ============================================================================
struct ConversionConfig {
    const wchar_t* unsupportedTypes[16];   // Null-terminated array of type names
    int             unsupportedCount;
    wchar_t         recommendedType[64];
    wchar_t         modelPath[MAX_PATH];
};

// ============================================================================
// Class: ModelConversionDialog
// ============================================================================
class ModelConversionDialog {
public:
    explicit ModelConversionDialog(HWND parent, const ConversionConfig& cfg);
    ~ModelConversionDialog();

    // Show modal — returns ConversionResult
    ConversionResult showModal();

    // Public getters
    const wchar_t* convertedPath() const { return m_convertedPath; }

private:
    // Window management
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void registerClass(HINSTANCE hInst);
    void createControls(HWND hwnd);

    // Paint
    void paint(HDC hdc, const RECT& rc);
    void paintHeader(HDC hdc, int x, int y, int w);
    void paintProgressBar(HDC hdc, int x, int y, int w, int h);
    void paintOutputLog(HDC hdc, int x, int y, int w, int h);

    // Conversion logic
    void startConversion();
    void cancelConversion();
    void pollProcess();
    void parseOutputLine(const char* line);
    void updateProgressFromChunks(int current, int total);
    bool verifyConvertedModel();
    void logConversionHistory(bool success, DWORD durationMs);

    // Append to output log
    void appendOutput(const wchar_t* text, COLORREF color = 0);

    // Timer callback
    static void CALLBACK timerProc(HWND hwnd, UINT, UINT_PTR, DWORD);

    // Instance data
    HWND        m_hwnd            = nullptr;
    HWND        m_parent          = nullptr;
    HINSTANCE   m_hInst           = nullptr;
    HFONT       m_fontTitle       = nullptr;
    HFONT       m_fontBody        = nullptr;
    HFONT       m_fontMono        = nullptr;
    HDC         m_backDC          = nullptr;
    HBITMAP     m_backBuf         = nullptr;
    int         m_bufW = 0, m_bufH = 0;
    UINT_PTR    m_timerId         = 0;

    // Buttons
    HWND m_btnConvert       = nullptr;
    HWND m_btnCancel        = nullptr;
    HWND m_btnCancelConvert = nullptr;
    HWND m_btnMoreInfo      = nullptr;

    // Process
    HANDLE m_hProcess    = nullptr;
    HANDLE m_hThread     = nullptr;
    HANDLE m_hReadPipe   = nullptr;
    HANDLE m_hWritePipe  = nullptr;

    // State
    ConversionConfig  m_config     = {};
    ConversionResult  m_result     = ConversionResult::Cancelled;
    bool              m_converting = false;
    bool              m_infoShown  = false;
    int               m_progress   = 0;         // 0..100
    int               m_stage      = 0;
    int               m_chunksProcessed = 0;
    int               m_totalChunks     = 0;
    DWORD             m_startTick       = 0;
    wchar_t           m_convertedPath[MAX_PATH] = {};
    wchar_t           m_statusText[256]         = L"Ready";

    // Output log ring buffer
    struct LogEntry {
        wchar_t  text[512];
        COLORREF color;
    };
    static constexpr int MAX_LOG_LINES = 200;
    LogEntry m_log[MAX_LOG_LINES] = {};
    int      m_logCount = 0;
    int      m_logScroll = 0;

    // Class registration
    static bool s_classRegistered;
};

// ============================================================================
// C API for Win32IDE integration
// ============================================================================
extern "C" {
    int  ModelConversionDialog_ShowModal(HWND parent, const ConversionConfig* cfg);
    const wchar_t* ModelConversionDialog_GetConvertedPath();
}
