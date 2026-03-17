#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// Forward declarations
namespace RawrXD { namespace UI {
    struct ToolActionStatus;
    struct WorkingBubble;
}}

namespace RawrXD {
namespace UI {

class ChatPanel {
public:
    bool create(HWND parent, int idBase);
    void resize(int x, int y, int w, int h);
    void appendMessage(const std::string& who, const std::string& text);

    // Append a tool action status line (plain text)
    void appendToolAction(const ToolActionStatus& action);

    // Append a working bubble summary (plain text)
    void appendWorkingBubble(const WorkingBubble& bubble);

    std::string getInput() const;
    void clearInput();
    HWND hwnd() const { return m_container; }

private:
    HWND m_container{nullptr};
    HWND m_transcript{nullptr};
    HWND m_input{nullptr};
    HWND m_send{nullptr};
};

} // namespace UI
} // namespace RawrXD
