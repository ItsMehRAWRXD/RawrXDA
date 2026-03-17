// ============================================================================
// Win32IDE_StreamingUX.cpp — Streaming / Model UX
// ============================================================================
// Provides user-facing progress, cancellation, and status feedback for
// model loading, downloading, and inference operations.
//
// Components:
//   - Progress bar overlay (top of editor area)
//   - Cancel button
//   - Status text label
//   - Timer-driven UI updates from background threads
//   - Integration with model download callbacks
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>

// ============================================================================
// SHOW PROGRESS BAR — creates the progress overlay UI
// ============================================================================
void Win32IDE::showModelProgressBar(const std::string& operation) {
    LOG_INFO("Model progress: starting — " + operation);
    
    m_modelOperationActive.store(true);
    m_modelOperationCancelled.store(false);
    m_modelProgressPercent.store(0.0f);
    
    {
        std::lock_guard<std::mutex> lock(m_modelProgressMutex);
        m_modelProgressStatus = operation;
    }
    
    if (!m_hwndMain) return;
    
    // Get the editor area rect to position the progress bar
    RECT mainRC;
    GetClientRect(m_hwndMain, &mainRC);
    
    int barHeight = 32;
    int barY = 0;
    
    // If editor exists, position just above it
    if (m_hwndEditor) {
        RECT editorRC;
        GetWindowRect(m_hwndEditor, &editorRC);
        POINT pt = {editorRC.left, editorRC.top};
        ScreenToClient(m_hwndMain, &pt);
        barY = pt.y;
    }
    
    // Create container panel
    if (!m_hwndModelProgressContainer) {
        m_hwndModelProgressContainer = CreateWindowExA(
            0, "STATIC", "",
            WS_CHILD | SS_OWNERDRAW,
            0, barY, mainRC.right, barHeight,
            m_hwndMain, nullptr, m_hInstance, nullptr);
        
        SetPropA(m_hwndModelProgressContainer, "IDE_PTR", (HANDLE)this);
    }
    
    // Position and resize container
    SetWindowPos(m_hwndModelProgressContainer, HWND_TOP,
                 0, barY, mainRC.right, barHeight,
                 SWP_SHOWWINDOW);
    
    // Create progress bar control (use Win32 PROGRESS_CLASS)
    if (!m_hwndModelProgressBar) {
        m_hwndModelProgressBar = CreateWindowExA(
            0, PROGRESS_CLASSA, "",
            WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
            8, 4, mainRC.right - 180, 24,
            m_hwndModelProgressContainer, nullptr, m_hInstance, nullptr);
        
        // VS Code dark theme colors
        SendMessage(m_hwndModelProgressBar, PBM_SETBARCOLOR, 0, (LPARAM)RGB(0, 122, 204));
        SendMessage(m_hwndModelProgressBar, PBM_SETBKCOLOR, 0, (LPARAM)RGB(37, 37, 38));
        SendMessage(m_hwndModelProgressBar, PBM_SETRANGE32, 0, 1000);
    } else {
        SetWindowPos(m_hwndModelProgressBar, nullptr,
                     8, 4, mainRC.right - 180, 24, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    SendMessage(m_hwndModelProgressBar, PBM_SETPOS, 0, 0);
    
    // Create status label
    if (!m_hwndModelProgressLabel) {
        m_hwndModelProgressLabel = CreateWindowExA(
            0, "STATIC", operation.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            8, 4, mainRC.right - 180, 24,
            m_hwndModelProgressContainer, nullptr, m_hInstance, nullptr);
        
        if (m_hFontUI) {
            SendMessage(m_hwndModelProgressLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        }
    }
    SetWindowTextA(m_hwndModelProgressLabel, operation.c_str());
    // Position label on top of progress bar (overlaying text)
    SetWindowPos(m_hwndModelProgressLabel, HWND_TOP,
                 12, 4, mainRC.right - 184, 24, SWP_SHOWWINDOW);
    
    // Create cancel button
    if (!m_hwndModelCancelBtn) {
        m_hwndModelCancelBtn = CreateWindowExA(
            0, "BUTTON", "Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            mainRC.right - 160, 4, 80, 24,
            m_hwndModelProgressContainer, (HMENU)9903, m_hInstance, nullptr);
        
        if (m_hFontUI) {
            SendMessage(m_hwndModelCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        }
    } else {
        SetWindowPos(m_hwndModelCancelBtn, nullptr,
                     mainRC.right - 160, 4, 80, 24, SWP_NOZORDER | SWP_SHOWWINDOW);
    }
    EnableWindow(m_hwndModelCancelBtn, TRUE);
    
    // Paint the container with dark background
    ShowWindow(m_hwndModelProgressContainer, SW_SHOW);
    
    // Paint container background
    HDC hdc = GetDC(m_hwndModelProgressContainer);
    RECT containerRC;
    GetClientRect(m_hwndModelProgressContainer, &containerRC);
    HBRUSH bgBrush = CreateSolidBrush(RGB(37, 37, 38));
    FillRect(hdc, &containerRC, bgBrush);
    DeleteObject(bgBrush);
    ReleaseDC(m_hwndModelProgressContainer, hdc);
    
    // Start timer for polling progress updates from background threads
    SetTimer(m_hwndMain, MODEL_PROGRESS_TIMER_ID, 100, nullptr);
    
    LOG_INFO("Model progress bar displayed.");
}

// ============================================================================
// UPDATE PROGRESS — thread-safe, called from any thread
// ============================================================================
void Win32IDE::updateModelProgress(float percent, const std::string& statusText) {
    m_modelProgressPercent.store(percent);
    
    {
        std::lock_guard<std::mutex> lock(m_modelProgressMutex);
        m_modelProgressStatus = statusText;
    }
    
    // Signal the main thread to update UI
    if (m_hwndMain) {
        PostMessage(m_hwndMain, WM_MODEL_PROGRESS_UPDATE, (WPARAM)(int)(percent * 10.0f), 0);
    }
}

// ============================================================================
// HIDE PROGRESS BAR
// ============================================================================
void Win32IDE::hideModelProgressBar() {
    m_modelOperationActive.store(false);
    
    KillTimer(m_hwndMain, MODEL_PROGRESS_TIMER_ID);
    
    if (m_hwndModelProgressContainer) {
        ShowWindow(m_hwndModelProgressContainer, SW_HIDE);
    }
    
    // Trigger layout update to reclaim editor space
    if (m_hwndMain) {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        SendMessage(m_hwndMain, WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.right, rc.bottom));
    }
    
    LOG_INFO("Model progress bar hidden.");
}

// ============================================================================
// CANCEL MODEL OPERATION
// ============================================================================
void Win32IDE::cancelModelOperation() {
    if (!m_modelOperationActive.load()) return;
    
    LOG_INFO("Model operation cancellation requested.");
    m_modelOperationCancelled.store(true);
    
    // Also set the inference stop flag if inference is running
    m_inferenceStopRequested = true;
    
    // Update the UI
    if (m_hwndModelProgressLabel) {
        SetWindowTextA(m_hwndModelProgressLabel, "Cancelling...");
    }
    if (m_hwndModelCancelBtn) {
        EnableWindow(m_hwndModelCancelBtn, FALSE);
    }
    
    // Update status bar
    showModelStatus("Model operation cancelled.", 3000);
}

// ============================================================================
// IS MODEL OPERATION IN PROGRESS
// ============================================================================
bool Win32IDE::isModelOperationInProgress() const {
    return m_modelOperationActive.load();
}

// ============================================================================
// SHOW STATUS MESSAGE — timed status bar message
// ============================================================================
void Win32IDE::showModelStatus(const std::string& text, int durationMs) {
    if (m_hwndStatusBar) {
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)text.c_str());
    }
    
    // Flash the status text in the output panel too
    appendToOutput(text + "\n", "Output", OutputSeverity::Info);
    
    // Set a timer to clear the status
    if (durationMs > 0 && m_hwndMain) {
        // Use timer ID 42 (IDT_STATUS_FLASH) to auto-clear
        SetTimer(m_hwndMain, 42, durationMs, nullptr);
    }
}

// ============================================================================
// PROGRESS WINDOW PROC — handles painting for the progress container
// ============================================================================
LRESULT CALLBACK Win32IDE::ModelProgressProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Dark background
            HBRUSH bgBrush = CreateSolidBrush(RGB(37, 37, 38));
            FillRect(hdc, &rc, bgBrush);
            DeleteObject(bgBrush);
            
            // Top border accent
            HPEN accentPen = CreatePen(PS_SOLID, 1, RGB(0, 122, 204));
            HPEN oldPen = (HPEN)SelectObject(hdc, accentPen);
            MoveToEx(hdc, rc.left, rc.top, nullptr);
            LineTo(hdc, rc.right, rc.top);
            SelectObject(hdc, oldPen);
            DeleteObject(accentPen);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_COMMAND: {
            if (LOWORD(wParam) == 9903 && ide) { // Cancel button ID
                ide->cancelModelOperation();
            }
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1;
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
