// ═════════════════════════════════════════════════════════════════════════════
// Win32_HexConsole.hpp - Pure Win32 Replacement for Qt HexMagConsole
// Zero Qt Dependencies - Uses Win32 RichEdit Control
// ═════════════════════════════════════════════════════════════════════════════

#pragma once

#ifndef RAWRXD_WIN32_HEX_CONSOLE_HPP
#define RAWRXD_WIN32_HEX_CONSOLE_HPP

#include "agent_kernel_main.hpp"
#include <windows.h>
#include <richedit.h>
#include <functional>
#include <deque>
#include <memory>

#pragma comment(lib, "riched20.lib")

namespace RawrXD {
namespace Win32 {

// ═════════════════════════════════════════════════════════════════════════════
// Hex Console - Win32 RichEdit-Based Text Display
// ═════════════════════════════════════════════════════════════════════════════

class HexConsole {
public:
    explicit HexConsole(HWND parentWindow = nullptr) 
        : hwndParent_(parentWindow), hwndRichEdit_(nullptr), maxLines_(10000), currentLines_(0) {}
    
    virtual ~HexConsole() = default;
    
    // ─────────────────────────────────────────────────────────────────────────
    // Window Management
    // ─────────────────────────────────────────────────────────────────────────
    
    HWND Create(int x, int y, int width, int height) {
        if (!hwndParent_) {
            hwndParent_ = GetForegroundWindow();
        }
        
        hwndRichEdit_ = CreateWindowExW(
            0,
            RICHEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER | 
            WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
            x, y, width, height,
            hwndParent_,
            (HMENU)9001,  // Child control ID
            GetModuleHandle(nullptr),
            nullptr
        );
        
        if (hwndRichEdit_) {
            // Configure RichEdit
            SendMessageW(hwndRichEdit_, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
            
            // Set font
            CHARFORMATW cf{};
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
            cf.yHeight = 200;  // 10pt
            cf.crTextColor = RGB(0, 255, 0);  // Bright green for text
            wcscpy_s(cf.szFaceName, L"Courier New");
            SendMessageW(hwndRichEdit_, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
            
            // Limit text
            SendMessageW(hwndRichEdit_, EM_SETLIMITTEXT, 1000000, 0);
        }
        
        return hwndRichEdit_;
    }
    
    HWND GetHandle() const { return hwndRichEdit_; }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Text Output
    // ─────────────────────────────────────────────────────────────────────────
    
    void AppendLog(const String& message) {
        if (!hwndRichEdit_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Keep history limited
        logHistory_.push_back(message);
        if (logHistory_.size() > maxLines_) {
            logHistory_.pop_front();
        }
        
        // Append to RichEdit
        SendMessageW(hwndRichEdit_, EM_SETSEL, -1, -1);  // Select end
        
        String output = message + L"\r\n";
        SendMessageW(hwndRichEdit_, EM_REPLACESEL, FALSE, (LPARAM)output.c_str());
        
        // Auto-scroll to bottom
        SendMessageW(hwndRichEdit_, WM_VSCROLL, SB_BOTTOM, 0);
        
        currentLines_++;
    }
    
    void AppendLogFormatted(const String& prefix, const String& message, COLORREF color = RGB(0, 255, 0)) {
        if (!hwndRichEdit_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Move to end
        SendMessageW(hwndRichEdit_, EM_SETSEL, -1, -1);
        
        // Set color for prefix
        CHARFORMATW cf{};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = color;
        SendMessageW(hwndRichEdit_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        
        // Insert prefix
        String prefixText = prefix + L": ";
        SendMessageW(hwndRichEdit_, EM_REPLACESEL, FALSE, (LPARAM)prefixText.c_str());
        
        // Set color back to default for message
        cf.crTextColor = RGB(0, 255, 0);
        SendMessageW(hwndRichEdit_, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
        
        // Insert message
        String fullMessage = message + L"\r\n";
        SendMessageW(hwndRichEdit_, EM_REPLACESEL, FALSE, (LPARAM)fullMessage.c_str());
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Hex Display
    // ─────────────────────────────────────────────────────────────────────────
    
    void DisplayHex(const Vector<uint8_t>& data) {
        if (!hwndRichEdit_ || data.empty()) return;
        
        String hexOutput;
        const size_t bytesPerLine = 16;
        
        for (size_t i = 0; i < data.size(); i += bytesPerLine) {
            // Address
            wchar_t buffer[256];
            swprintf_s(buffer, sizeof(buffer)/sizeof(wchar_t), L"%08X: ", (DWORD)i);
            hexOutput += buffer;
            
            // Hex bytes
            size_t lineBytes = (std::min)(bytesPerLine, data.size() - i);
            for (size_t j = 0; j < lineBytes; ++j) {
                swprintf_s(buffer, sizeof(buffer)/sizeof(wchar_t), L"%02X ", data[i + j]);
                hexOutput += buffer;
            }
            
            // Padding
            for (size_t j = lineBytes; j < bytesPerLine; ++j) {
                hexOutput += L"   ";
            }
            
            hexOutput += L"| ";
            
            // ASCII representation
            for (size_t j = 0; j < lineBytes; ++j) {
                uint8_t byte = data[i + j];
                if (byte >= 32 && byte <= 126) {
                    hexOutput += static_cast<wchar_t>(byte);
                } else {
                    hexOutput += L'.';
                }
            }
            
            hexOutput += L"\r\n";
        }
        
        AppendLog(hexOutput);
    }
    
    // ─────────────────────────────────────────────────────────────────────────
    // Management
    // ─────────────────────────────────────────────────────────────────────────
    
    void Clear() {
        if (!hwndRichEdit_) return;
        
        std::lock_guard<std::mutex> lock(mutex_);
        SendMessageW(hwndRichEdit_, WM_SETTEXT, 0, (LPARAM)L"");
        logHistory_.clear();
        currentLines_ = 0;
    }
    
    void SetMaxLines(size_t maxLines) {
        std::lock_guard<std::mutex> lock(mutex_);
        maxLines_ = maxLines;
        
        // Trim if necessary
        while (logHistory_.size() > maxLines_) {
            logHistory_.pop_front();
        }
    }
    
    size_t GetLineCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentLines_;
    }
    
    Vector<String> GetHistory() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return Vector<String>(logHistory_.begin(), logHistory_.end());
    }
    
    void SaveToFile(const String& filepath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::wofstream file(filepath);
        if (file.is_open()) {
            for (const auto& line : logHistory_) {
                file << line << L"\n";
            }
            file.close();
        }
    }
    
private:
    HWND hwndParent_;
    HWND hwndRichEdit_;
    mutable std::mutex mutex_;
    std::deque<String> logHistory_;
    size_t maxLines_;
    size_t currentLines_;
};

} // namespace Win32
} // namespace RawrXD

#endif // RAWRXD_WIN32_HEX_CONSOLE_HPP
