#pragma once
// EventBus.h — Global signal bus replacing raw callbacks
// Provides decoupled cross-component communication via SignalSlot
// No Qt. No exceptions. C++20 only.

#include "RawrXD_SignalSlot.h"
#include <string>

namespace RawrXD {

struct EventBus {
    static EventBus& Get() {
        static EventBus instance;
        return instance;
    }

    // Editor events
    Signal<int, int>                EditorCursorMoved;   // line, col
    Signal<const std::string&>      FileOpened;
    Signal<const std::string&>      FileSaved;
    Signal<const std::string&>      FileClosing;

    // Build events
    Signal<const std::string&, float> BuildProgress;     // target, progress 0.0-1.0
    Signal<const std::string&, bool>  BuildFinished;     // target, success

    // Agent events
    Signal<const std::string&>      AgentMessage;
    Signal<const std::string&>      AgentToolCall;

    // Hotpatch events
    Signal<>                        HotpatchApplied;
    Signal<const std::string&>      HotpatchReverted;    // patch name

    // Security events
    Signal<bool>                    SecurityAuthRequired;
    Signal<const std::string&>      SecurityViolation;

    // Convenience connectors
    template<typename F>
    static void ConnectEditor(F&& f) { Get().EditorCursorMoved.connect(std::forward<F>(f)); }

    template<typename F>
    static void ConnectBuild(F&& f) { Get().BuildProgress.connect(std::forward<F>(f)); }

    template<typename F>
    static void ConnectAgent(F&& f) { Get().AgentMessage.connect(std::forward<F>(f)); }

private:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
};

} // namespace RawrXD
