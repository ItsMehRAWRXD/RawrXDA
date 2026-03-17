#pragma once

#include <string>

class ZeroTouch {

public:
    explicit ZeroTouch();

    void installAll();
    void installFileWatcher();
    void installGitHook();
    void installVoiceTrigger();

private:
    std::string m_lastVoiceWish;
    bool m_running;
};
