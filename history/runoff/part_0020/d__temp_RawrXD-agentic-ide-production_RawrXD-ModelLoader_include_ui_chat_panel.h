#pragma once

#include <string>
#include <windows.h>

namespace RawrXD {
namespace UI {

class ChatPanel {
public:
    ChatPanel() = default;

    // Create Win32 child controls that compose the chat panel UI.
    bool create(HWND parent, int idBase);

    // Resize child controls when the parent layout changes.
    void resize(int x, int y, int w, int h);

    // Append a message to the transcript with a speaker label.
    void appendMessage(const std::string& who, const std::string& text);

    // Retrieve current input text from the user field.
    std::string getInput() const;

    // Clear the user input field.
    void clearInput();

    // Return the container HWND for layout and message routing.
    HWND hwnd() const { return m_container; }

private:
    HWND m_container{nullptr};
    HWND m_transcript{nullptr};
    HWND m_input{nullptr};
    HWND m_send{nullptr};
};

} // namespace UI
} // namespace RawrXD
