#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <windows.h>
#include <objbase.h>

namespace rawrxd::bridge {

// Direct bridge between WebView2 and Native C++/MASM
class LogicBridge {
public:
    static LogicBridge& instance() {
        static LogicBridge instance;
        return instance;
    }

    // Register a native command to be callable from JS
    using CommandHandler = std::function<void(const std::string& args)>;
    void registerCommand(const std::string& name, CommandHandler handler);

    // Incoming from JS: dispatch to native components
    void dispatchToNative(const std::string& json_payload);

    // Outgoing to JS: push updates (diagnostics, diffs, terminal output)
    void postMessageToUI(const std::string& type, const std::string& data);

    // Fast event-based notification for high-frequency updates (cursor, scroll)
    void notifyUIEvent(const std::string& event_name, uint32_t x, uint32_t y);

    // MASM-optimized logic for processing UI-heavy JSON blobs
    void processUIPayloadInline(const std::string& raw_data);

private:
    std::map<std::string, CommandHandler> handlers;
    
    // Low-level Windows message dispatch helper
    HWND target_hwnd = nullptr;
    void setupHwndBridge(HWND main_window);
};

} // namespace rawrxd::bridge
