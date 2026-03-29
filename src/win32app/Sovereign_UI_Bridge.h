#pragma once

#include <cstdint>
#include <windows.h>
#include <memory>

namespace RawrXD {
namespace Sovereign {

// Sovereign UI Bridge - Maps diagnostic telemetry to Win32IDE chat panel and status bar
class SovereignUIBridge {
public:
    SovereignUIBridge();
    ~SovereignUIBridge();

    // Initialize bridge with target window
    bool Initialize(HWND chatPanel, HWND statusBar);
    
    // Handle sovereign diagnostic messages from streaming thread
    void HandleSovereignStart();
    void HandleSovereignToken(const char* token);
    void HandleSovereignSuccess(uint32_t confidence, uint32_t tokenCount);
    
    // Update UI with telemetry
    void UpdateChatPanel(const std::string& text);
    void UpdateStatusBar(const std::string& status);
    
private:
    HWND m_chatPanel = nullptr;
    HWND m_statusBar = nullptr;
    uint32_t m_diagnosticCount = 0;
    uint32_t m_lastConfidence = 0;
};

// Global bridge provider
SovereignUIBridge& GetSovereignBridge();

} // namespace Sovereign
} // namespace RawrXD
