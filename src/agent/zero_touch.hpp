#pragma once
// zero_touch.hpp – Qt-free ZeroTouch (C++20 / Win32)
#include <string>

class ZeroTouch {
public:
    ZeroTouch();
    ~ZeroTouch() = default;

    void installAll();
    void installFileWatcher();
    void installGitHook();
    void installVoiceTrigger();

private:
    std::string m_lastVoiceWish;
};
