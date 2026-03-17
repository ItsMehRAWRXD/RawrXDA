#pragma once

#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
using HWND = void*;
#endif

namespace RawrXD {

struct GhostTextDecoration {
    int line = 0;
    int column = 0;
    std::string text;
    std::string type = "completion";
    bool multiline = false;
    std::vector<std::string> lines;
};

struct DiffDecoration {
    int startLine = 0;
    int endLine = 0;
    std::string oldText;
    std::string newText;
    std::string type = "modify";
};

class GhostTextRenderer {
public:
    GhostTextRenderer() = default;
    explicit GhostTextRenderer(HWND editorHwnd) : m_editorHwnd(editorHwnd) {}
    ~GhostTextRenderer() = default;

    void setEditorHwnd(HWND hwnd) { m_editorHwnd = hwnd; }
    HWND editorHwnd() const { return m_editorHwnd; }

    void initialize() {}

    void showGhostText(const std::string& text, const std::string& type = "completion") {
        m_currentGhostText = text;
        m_ghostDecoration.text = text;
        m_ghostDecoration.type = type;
        m_visible = !text.empty();
    }

    void showMultilineGhost(const std::vector<std::string>& lines) {
        m_ghostDecoration.multiline = true;
        m_ghostDecoration.lines = lines;
        m_currentGhostText.clear();
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i > 0) m_currentGhostText += '\n';
            m_currentGhostText += lines[i];
        }
        m_visible = !m_currentGhostText.empty();
    }

    void updateGhostText(const std::string& additionalText) {
        m_currentGhostText += additionalText;
        m_ghostDecoration.text = m_currentGhostText;
        m_visible = !m_currentGhostText.empty();
    }

    void showDiffPreview(int startLine, int endLine, const std::string& oldText, const std::string& newText) {
        DiffDecoration diff;
        diff.startLine = startLine;
        diff.endLine = endLine;
        diff.oldText = oldText;
        diff.newText = newText;
        if (oldText.empty()) diff.type = "add";
        else if (newText.empty()) diff.type = "remove";
        m_diffDecorations.push_back(std::move(diff));
    }

    void clearGhostText() {
        m_currentGhostText.clear();
        m_ghostDecoration = GhostTextDecoration{};
        m_visible = false;
    }

    void clearDiffPreview() {
        m_diffDecorations.clear();
    }

    void acceptGhostText() {
        m_accepted = !m_currentGhostText.empty();
        clearGhostText();
    }

    std::string getCurrentGhostText() const { return m_currentGhostText; }
    bool hasGhostText() const { return !m_currentGhostText.empty(); }
    bool isVisible() const { return m_visible; }
    bool wasAccepted() const { return m_accepted; }

private:
    HWND m_editorHwnd = nullptr;
    bool m_visible = false;
    bool m_accepted = false;
    std::string m_currentGhostText;
    GhostTextDecoration m_ghostDecoration;
    std::vector<DiffDecoration> m_diffDecorations;
};

} // namespace RawrXD
