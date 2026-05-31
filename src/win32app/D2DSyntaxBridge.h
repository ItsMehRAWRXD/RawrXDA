// ============================================================================
// D2DSyntaxBridge.h — Bridge: Win32IDE::SyntaxToken[] → TextColorRun[] → DrawColoredLine
// ============================================================================
// Declares the bridge between Win32IDE_SyntaxHighlight (RichEdit tokenizer)
// and D2DTextRenderer (Direct2D colored text).
//
// Usage:
//   1. Detect if D2D overlay is active (m_d2dEditorOverlay != nullptr)
//   2. In paint loop, call D2DSyntaxBridge::RenderViewport() instead of
//      the EM_SETCHARFORMAT path.
//   3. Falls back to RichEdit coloring if D2D not initialized.
// ============================================================================

#pragma once

#include "D2DTextRenderer.h"
#include <vector>
#include <string>

// Forward declaration
class Win32IDE;

namespace RawrXD {

class D2DSyntaxBridge {
public:
    // Render a single line with syntax coloring via D2D
    static void RenderLine(
        int lineIndex,
        const std::wstring& lineText,
        float x,
        float y,
        float fontSize,
        D2DTextRenderer* d2dRenderer
    );

    // Batch render all visible lines in viewport
    static void RenderViewport(
        HWND hwndEditor,
        D2DTextRenderer* d2dRenderer,
        int firstVisibleLine,
        int lastVisibleLine
    );

    // Mark a line as needing re-tokenization
    static void InvalidateLine(int lineIndex);

    // Check if D2D path is available for this editor
    static bool IsD2DAvailable();
};

} // namespace RawrXD
