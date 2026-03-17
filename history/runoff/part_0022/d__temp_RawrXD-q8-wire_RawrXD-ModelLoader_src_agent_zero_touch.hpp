#pragma once

#include <string>

namespace RawrXD {

class ZeroTouch {
public:
    explicit ZeroTouch();
    ~ZeroTouch() = default;

    void installAll();
    void installFileWatcher();
    void installGitHook();
    void installVoiceTrigger();

private:
    std::string m_lastVoiceWish;
};

} // namespace RawrXD
