#pragma once
#include <windows.h>
#include <string>

namespace RawrXD::UI {

enum class GhostType {
    Insert,
    Replace
};

struct GhostSuggestion {
    GhostType type = GhostType::Insert;
    int line = 0;              // 0-based line index
    int column = 0;            // character index
    std::wstring text;       // suggested text
    std::wstring replace;    // original text (for replace)
    bool active = false;
};

class GhostOverlay {
public:
    GhostOverlay();
    ~GhostOverlay();

    // Attach to an editor window (subclassing)
    bool Attach(HWND hEditor);
    void Detach();

    // Set/clear suggestion
    void SetSuggestion(const GhostSuggestion& s);
    void ClearSuggestion();
    bool HasSuggestion() const;

    // Apply/reject current suggestion
    bool ApplySuggestion();
    void RejectSuggestion();

    // Static accessor
    static GhostOverlay* GetInstance();

private:
    static LRESULT CALLBACK SubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void DrawGhost(HDC hdc);
    POINT GetCaretPixelPos();
    int GetLineHeight(HDC hdc);

    HWND m_editor = nullptr;
    WNDPROC m_origProc = nullptr;
    GhostSuggestion m_suggestion;

    static GhostOverlay* g_instance;
};

} // namespace RawrXD::UI
