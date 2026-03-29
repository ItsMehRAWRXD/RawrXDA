#pragma once

// C++20 / Win32. Chat workspace; no Qt. Callback for command.

#include <string>
#include <functional>

class ChatWorkspace
{
public:
    using CommandIssuedFn = std::function<void(const std::string& command)>;

    ChatWorkspace() = default;
    void initialize();
    void setOnCommandIssued(CommandIssuedFn f) { m_onCommandIssued = std::move(f); }
    void* getWidgetHandle() const { return m_handle; }

private:
    void* m_handle = nullptr;
    CommandIssuedFn m_onCommandIssued;
};
