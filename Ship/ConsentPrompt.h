#pragma once
// ConsentPrompt.h - Stub for Win32 IDE

#include <windows.h>
#include <string>
#include <functional>

class ConsentPrompt {
public:
    ConsentPrompt() = default;
    ~ConsentPrompt() = default;
    
    bool show(HWND parent, const std::string& title, const std::string& message) {
        int result = MessageBoxA(parent, message.c_str(), title.c_str(), MB_YESNO | MB_ICONQUESTION);
        return result == IDYES;
    }
    
    void setCallback(std::function<void(bool)> cb) { m_callback = cb; }
    
private:
    std::function<void(bool)> m_callback;
};
