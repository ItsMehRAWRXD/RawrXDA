// ============================================================================
// D2DSyntaxBridge.cpp — Bridge: Win32IDE::SyntaxToken[] → TextColorRun[] → DrawColoredLine
// ============================================================================
// Connects Win32IDE_SyntaxHighlight.cpp (RichEdit tokenization) to
// D2DTextRenderer::DrawColoredLine() (Direct2D colored text rendering).
//
// Build: Already linked via RawrXD-Win32IDE target (D2DTextRenderer.cpp.obj
//        symbols confirmed in build2/RawrXD-Win32IDE.map).
//
// Integration point: Replace EM_SETCHARFORMAT calls in applySyntaxColoring()
// with D2D path when m_d2dEditorOverlay is initialized.
// ============================================================================

#include "D2DSyntaxBridge.h"
#include "Win32IDE.h"
#include <vector>
#include <string>
#include <algorithm>

namespace RawrXD {

static std::wstring GetLineText(HWND hwnd, int lineIndex);

// ============================================================================
// D2DSyntaxBridge::RenderLine — main entry point
//
// Called from Win32IDE paint loop when D2D overlay is active.
// Replaces the EM_SETCHARFORMAT path for the visible viewport.
//
// Parameters:
//   lineIndex    — 0-based line number in document
//   lineText     — UTF-16 text of the line
//   x, y         — D2D pixel coordinates
//   fontSize     — font size in DIPs (default 14.0f)
//   d2dRenderer  — pointer to D2DTextRenderer instance
// ============================================================================
void D2DSyntaxBridge::RenderLine(
    int lineIndex,
    const std::wstring& lineText,
    float x,
    float y,
    float fontSize,
    D2DTextRenderer* d2dRenderer
) {
    (void)fontSize; // Unused for now; D2DTextRenderer manages its own text format
    if (!d2dRenderer || !d2dRenderer->IsReady()) return;
    if (lineText.empty()) return;

    // Detect language from active file
    Win32IDE::SyntaxLanguage lang = Win32IDE::SyntaxLanguage::None;
    extern Win32IDE* g_pMainIDE;
    if (g_pMainIDE) {
        lang = g_pMainIDE->getCurrentSyntaxLanguage();
    }

    // Tokenize (reuses existing Win32IDE_SyntaxHighlight.cpp tokenizer)
    std::vector<Win32IDE::SyntaxToken> tokens;
    if (g_pMainIDE) {
        std::string lineUtf8(lineText.begin(), lineText.end());
        tokens = g_pMainIDE->tokenizeLine(lineUtf8, 0, lang);
    }

    auto tokenTypeToD2DColor = [](Win32IDE::TokenType type) {
        switch (type) {
            case Win32IDE::TokenType::Keyword:      return D2D1::ColorF(0x569CD6);
            case Win32IDE::TokenType::BuiltinType:  return D2D1::ColorF(0x4EC9B0);
            case Win32IDE::TokenType::String:       return D2D1::ColorF(0xCE9178);
            case Win32IDE::TokenType::Comment:      return D2D1::ColorF(0x6A9955);
            case Win32IDE::TokenType::Number:       return D2D1::ColorF(0xB5CEA8);
            case Win32IDE::TokenType::Operator:     return D2D1::ColorF(0xD4D4D4);
            case Win32IDE::TokenType::Preprocessor: return D2D1::ColorF(0xC586C0);
            case Win32IDE::TokenType::Function:     return D2D1::ColorF(0xDCDCAA);
            case Win32IDE::TokenType::Bracket:      return D2D1::ColorF(0xFFD700);
            case Win32IDE::TokenType::Default:
            default:                                 return D2D1::ColorF(0xD4D4D4);
        }
    };

    std::vector<TextColorRun> runs;
    runs.clear();
    if (!lineText.empty()) {
        const size_t lineLen = lineText.length();
        size_t pos = 0;
        for (const auto& tok : tokens) {
            if (tok.start >= static_cast<int>(lineLen))
                continue;
            const size_t tokEnd = std::min(static_cast<size_t>(tok.start + tok.length), lineLen);
            if (tokEnd <= static_cast<size_t>(tok.start))
                continue;

            D2D1_COLOR_F color = tokenTypeToD2DColor(tok.type);
            if (!runs.empty() &&
                runs.back().start + runs.back().length == static_cast<UINT32>(tok.start) &&
                runs.back().color.r == color.r && runs.back().color.g == color.g &&
                runs.back().color.b == color.b && runs.back().color.a == color.a) {
                runs.back().length += static_cast<UINT32>(tokEnd - tok.start);
            } else {
                if (pos < static_cast<size_t>(tok.start)) {
                    runs.push_back({
                        static_cast<UINT32>(pos),
                        static_cast<UINT32>(tok.start - pos),
                        tokenTypeToD2DColor(Win32IDE::TokenType::Default)
                    });
                }
                runs.push_back({
                    static_cast<UINT32>(tok.start),
                    static_cast<UINT32>(tokEnd - tok.start),
                    color
                });
            }
            pos = tokEnd;
        }
        if (pos < lineLen) {
            runs.push_back({
                static_cast<UINT32>(pos),
                static_cast<UINT32>(lineLen - pos),
                tokenTypeToD2DColor(Win32IDE::TokenType::Default)
            });
        }
    }

    // Draw via D2D
    d2dRenderer->DrawColoredLine(
        lineText.c_str(),
        static_cast<int>(lineText.length()),
        x,
        y,
        runs.data(),
        static_cast<int>(runs.size())
    );
}

// ============================================================================
// D2DSyntaxBridge::RenderViewport — batch render visible lines
//
// Called during WM_PAINT when D2D overlay is enabled.
// Iterates visible lines, tokenizes each, builds runs, draws.
// ============================================================================
void D2DSyntaxBridge::RenderViewport(
    HWND hwndEditor,
    D2DTextRenderer* d2dRenderer,
    int firstVisibleLine,
    int lastVisibleLine
) {
    if (!d2dRenderer || !d2dRenderer->IsReady()) return;

    // Get editor metrics
    RECT rcClient;
    GetClientRect(hwndEditor, &rcClient);

    // Line height from D2D metrics
    float lineHeight = d2dRenderer->GetLineHeight();
    float fontSize = 14.0f; // Matches Consolas 14pt

    // Begin D2D frame
    if (!d2dRenderer->BeginDraw()) return;

    // Clear background (optional — if overlay is transparent, skip)
    // d2dRenderer->Clear();

    // Render each visible line
    for (int line = firstVisibleLine; line <= lastVisibleLine; ++line) {
        // Get line text from RichEdit
        std::wstring lineText = GetLineText(hwndEditor, line);
        if (lineText.empty()) continue;

        float y = static_cast<float>((line - firstVisibleLine) * lineHeight);
        float x = 0.0f; // Left margin

        RenderLine(line, lineText, x, y, fontSize, d2dRenderer);
    }

    d2dRenderer->EndDraw();
}

// ============================================================================
// Helper: GetLineText — extract UTF-16 text from RichEdit for a given line
// ============================================================================
static std::wstring GetLineText(HWND hwnd, int lineIndex) {
    if (!hwnd || !IsWindow(hwnd)) return L"";

    // EM_GETLINE requires pre-allocated buffer
    int lineLen = (int)SendMessage(hwnd, EM_LINELENGTH,
        SendMessage(hwnd, EM_LINEINDEX, lineIndex, 0), 0);
    if (lineLen <= 0) return L"";

    std::wstring text;
    text.resize(lineLen + 1);

    // EM_GETLINE: first word of buffer = max length
    *(WORD*)text.data() = static_cast<WORD>(lineLen);
    int copied = (int)SendMessage(hwnd, EM_GETLINE, lineIndex, (LPARAM)text.data());
    if (copied > 0) {
        text.resize(copied);
    } else {
        text.clear();
    }
    return text;
}

// ============================================================================
// D2DSyntaxBridge::InvalidateLine — mark one line for re-tokenization
// Called from onEditorContentChanged() when a single line is modified.
// ============================================================================
void D2DSyntaxBridge::InvalidateLine(int lineIndex) {
    // In a full implementation, this would mark the line dirty in a cache.
    // For now, tokenization is done on-demand during RenderViewport.
    (void)lineIndex;
}

} // namespace RawrXD
