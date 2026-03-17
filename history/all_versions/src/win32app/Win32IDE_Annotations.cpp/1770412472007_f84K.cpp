// ============================================================================
// Win32IDE_Annotations.cpp — Agent Inline Annotation System
// ============================================================================
// Provides an overlay annotation layer for the editor:
//   - Gutter icons (severity-colored circles/squares)
//   - Inline text annotations (rendered after end-of-line)
//   - Tooltip on hover
//   - Source-based filtering (agent, linter, diagnostics)
//
// Annotations are stored as metadata, not injected into the RichEdit buffer.
// They are rendered as an overlay, preserving the editor content exactly.
// ============================================================================

#include "Win32IDE.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>

// ============================================================================
// ANNOTATION SEVERITY → COLOR
// ============================================================================
COLORREF Win32IDE::getAnnotationColor(AnnotationSeverity severity) const {
    switch (severity) {
        case AnnotationSeverity::Hint:       return RGB(180, 180, 180);  // Grey
        case AnnotationSeverity::Info:       return RGB(75, 172, 198);   // Cyan
        case AnnotationSeverity::Warning:    return RGB(227, 186, 33);   // Yellow
        case AnnotationSeverity::Error:      return RGB(228, 86, 73);    // Red
        case AnnotationSeverity::Suggestion: return RGB(86, 156, 214);   // Blue
        default:                             return RGB(180, 180, 180);
    }
}

// ============================================================================
// GUTTER ICON WIDTH
// ============================================================================
int Win32IDE::getAnnotationGutterIconWidth() const {
    return 16; // 16px for severity icon in the gutter
}

// ============================================================================
// ADD ANNOTATION
// ============================================================================
void Win32IDE::addAnnotation(int line, AnnotationSeverity severity, 
                              const std::string& text, const std::string& source) {
    InlineAnnotation ann;
    ann.line = line;
    ann.column = 0;
    ann.severity = severity;
    ann.text = text;
    ann.source = source;
    ann.color = getAnnotationColor(severity);
    ann.visible = m_annotationsVisible;
    ann.action = {}; // Default: no action
    ann.action.type = AnnotationActionType::None;
    ann.action.targetLine = 0;
    ann.action.fixStartLine = 0;
    ann.action.fixEndLine = 0;
    
    m_annotations.push_back(ann);
    
    LOG_INFO("Annotation added: line " + std::to_string(line) + " [" + source + "] " + text);
    
    // Trigger repaint of gutter and editor overlay
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// REMOVE ANNOTATIONS
// ============================================================================
void Win32IDE::removeAnnotations(int line, const std::string& source) {
    m_annotations.erase(
        std::remove_if(m_annotations.begin(), m_annotations.end(),
            [line, &source](const InlineAnnotation& ann) {
                if (ann.line != line) return false;
                if (!source.empty() && ann.source != source) return false;
                return true;
            }),
        m_annotations.end());
    
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// CLEAR ALL ANNOTATIONS
// ============================================================================
void Win32IDE::clearAllAnnotations(const std::string& source) {
    if (source.empty()) {
        m_annotations.clear();
    } else {
        m_annotations.erase(
            std::remove_if(m_annotations.begin(), m_annotations.end(),
                [&source](const InlineAnnotation& ann) {
                    return ann.source == source;
                }),
            m_annotations.end());
    }
    
    LOG_INFO("Annotations cleared" + (source.empty() ? "" : " for source: " + source));
    
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// GET ANNOTATIONS FOR LINE
// ============================================================================
std::vector<Win32IDE::InlineAnnotation> Win32IDE::getAnnotationsForLine(int line) const {
    std::vector<InlineAnnotation> result;
    for (const auto& ann : m_annotations) {
        if (ann.line == line && ann.visible) {
            result.push_back(ann);
        }
    }
    return result;
}

// ============================================================================
// GET TOOLTIP TEXT FOR LINE
// ============================================================================
std::string Win32IDE::getAnnotationTooltip(int line) const {
    auto anns = getAnnotationsForLine(line);
    if (anns.empty()) return "";
    
    std::string tooltip;
    for (const auto& ann : anns) {
        if (!tooltip.empty()) tooltip += "\n";
        
        // Severity prefix
        switch (ann.severity) {
            case AnnotationSeverity::Error:      tooltip += "[Error] "; break;
            case AnnotationSeverity::Warning:    tooltip += "[Warning] "; break;
            case AnnotationSeverity::Info:       tooltip += "[Info] "; break;
            case AnnotationSeverity::Hint:       tooltip += "[Hint] "; break;
            case AnnotationSeverity::Suggestion: tooltip += "[Suggestion] "; break;
        }
        tooltip += ann.text;
        
        if (!ann.source.empty()) {
            tooltip += " (" + ann.source + ")";
        }
    }
    return tooltip;
}

// ============================================================================
// TOGGLE VISIBILITY BY SOURCE
// ============================================================================
void Win32IDE::toggleAnnotationVisibility(const std::string& source) {
    for (auto& ann : m_annotations) {
        if (source.empty() || ann.source == source) {
            ann.visible = !ann.visible;
        }
    }
    
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// UPDATE ANNOTATION POSITIONS — called when lines are inserted/deleted
// ============================================================================
void Win32IDE::updateAnnotationPositions(int fromLine, int lineDelta) {
    for (auto& ann : m_annotations) {
        if (ann.line >= fromLine) {
            ann.line += lineDelta;
            if (ann.line < 1) ann.line = 1; // Clamp
        }
    }
}

// ============================================================================
// PAINT GUTTER ICONS — called from paintLineNumbers or a separate gutter pass
// ============================================================================
void Win32IDE::paintAnnotationGutterIcons(HDC hdc, RECT& gutterRC) {
    if (!m_annotationsVisible || m_annotations.empty()) return;
    if (!m_hwndEditor) return;
    
    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    
    // Get line height from editor font metrics
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;
    SelectObject(hdc, hOldFont);
    
    int visibleLines = (gutterRC.bottom - gutterRC.top) / lineHeight + 1;
    int iconSize = 10;
    int iconMargin = (getAnnotationGutterIconWidth() - iconSize) / 2;
    
    for (int i = 0; i < visibleLines; i++) {
        int editorLine = firstVisibleLine + i + 1; // 1-based
        
        // Find highest-severity annotation for this line
        AnnotationSeverity highestSeverity = AnnotationSeverity::Hint;
        bool hasAnnotation = false;
        
        for (const auto& ann : m_annotations) {
            if (ann.line == editorLine && ann.visible) {
                hasAnnotation = true;
                if ((int)ann.severity > (int)highestSeverity) {
                    highestSeverity = ann.severity;
                }
            }
        }
        
        if (!hasAnnotation) continue;
        
        // Draw a colored circle/diamond in the gutter
        COLORREF iconColor = getAnnotationColor(highestSeverity);
        HBRUSH hBrush = CreateSolidBrush(iconColor);
        HPEN hPen = CreatePen(PS_SOLID, 1, iconColor);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        
        int cy = i * lineHeight + (lineHeight - iconSize) / 2;
        int cx = gutterRC.left + iconMargin;
        
        // Error/Warning = filled circle, Info/Hint = filled diamond, Suggestion = filled square
        if (highestSeverity == AnnotationSeverity::Error || 
            highestSeverity == AnnotationSeverity::Warning) {
            // Filled circle
            Ellipse(hdc, cx, cy, cx + iconSize, cy + iconSize);
        } else if (highestSeverity == AnnotationSeverity::Suggestion) {
            // Filled square
            Rectangle(hdc, cx, cy, cx + iconSize, cy + iconSize);
        } else {
            // Diamond (rotated square)
            POINT diamond[4];
            int half = iconSize / 2;
            diamond[0] = {cx + half, cy};
            diamond[1] = {cx + iconSize, cy + half};
            diamond[2] = {cx + half, cy + iconSize};
            diamond[3] = {cx, cy + half};
            Polygon(hdc, diamond, 4);
        }
        
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
        DeleteObject(hBrush);
    }
}

// ============================================================================
// PAINT INLINE ANNOTATIONS — rendered after end-of-line in the editor overlay
// ============================================================================
void Win32IDE::paintAnnotations(HDC hdc, RECT& rc) {
    if (!m_annotationsVisible || m_annotations.empty()) return;
    if (!m_hwndEditor) return;
    
    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    
    // Get editor font metrics
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;
    int charWidth = tm.tmAveCharWidth;
    if (charWidth <= 0) charWidth = 8;
    
    int visibleLines = (rc.bottom - rc.top) / lineHeight + 1;
    
    // Use a smaller italic font for annotations
    HFONT annFont = m_annotationFont;
    if (!annFont) {
        LOGFONTA lf = {};
        lf.lfHeight = tm.tmHeight - 2; // Slightly smaller
        lf.lfItalic = TRUE;
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = ANSI_CHARSET;
        strcpy(lf.lfFaceName, "Consolas");
        annFont = CreateFontIndirectA(&lf);
        // Store for reuse (const_cast since this is a cache-like operation)
        const_cast<Win32IDE*>(this)->m_annotationFont = annFont;
    }
    
    // Set transparent background for annotations
    int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    
    for (int i = 0; i < visibleLines; i++) {
        int editorLine = firstVisibleLine + i + 1; // 1-based
        
        auto anns = getAnnotationsForLine(editorLine);
        if (anns.empty()) continue;
        
        // Get the end-of-line position for this line
        int lineIdx = editorLine - 1; // 0-based for EM_ messages
        int lineStart = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
        int lineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, lineStart, 0);
        
        // Calculate pixel position of end of line
        int eolX = (lineLen + 2) * charWidth; // Add 2 chars of padding
        int lineY = i * lineHeight;
        
        // Compose annotation text: "⚡ annotation1  ⚡ annotation2"
        std::string annText;
        for (const auto& ann : anns) {
            if (!annText.empty()) annText += "  ";
            
            // Severity icon (ASCII approximation)
            switch (ann.severity) {
                case AnnotationSeverity::Error:      annText += "X "; break;
                case AnnotationSeverity::Warning:    annText += "! "; break;
                case AnnotationSeverity::Info:       annText += "i "; break;
                case AnnotationSeverity::Hint:       annText += "* "; break;
                case AnnotationSeverity::Suggestion: annText += "> "; break;
            }
            annText += ann.text;
        }
        
        if (annText.empty()) continue;
        
        // Draw with annotation color (use highest severity color)
        COLORREF annColor = anns[0].color;
        for (const auto& ann : anns) {
            if ((int)ann.severity > (int)anns[0].severity) {
                annColor = ann.color;
            }
        }
        
        // Draw semi-transparent background behind annotation text
        HFONT prevFont = (HFONT)SelectObject(hdc, annFont);
        
        SIZE textSize;
        GetTextExtentPoint32A(hdc, annText.c_str(), (int)annText.size(), &textSize);
        
        // Background rect
        RECT annBg;
        annBg.left = rc.left + eolX;
        annBg.top = lineY;
        annBg.right = annBg.left + textSize.cx + 8;
        annBg.bottom = lineY + lineHeight;
        
        // Semi-dark background for annotation
        HBRUSH bgBrush = CreateSolidBrush(RGB(40, 40, 40));
        FillRect(hdc, &annBg, bgBrush);
        DeleteObject(bgBrush);
        
        // Draw annotation text
        SetTextColor(hdc, annColor);
        RECT textRect = annBg;
        textRect.left += 4;
        DrawTextA(hdc, annText.c_str(), (int)annText.size(), &textRect, 
                  DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        
        SelectObject(hdc, prevFont);
    }
    
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, hOldFont);
}

// ============================================================================
// ANNOTATION OVERLAY WINDOW PROC
// ============================================================================
LRESULT CALLBACK Win32IDE::AnnotationOverlayProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropA(hwnd, "IDE_PTR");
    
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (ide) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                ide->paintAnnotations(hdc, rc);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_ERASEBKGND:
            return 1; // We handle painting — transparent overlay
        
        case WM_NCHITTEST:
            return HTTRANSPARENT; // Pass all mouse events through to the editor
        
        case WM_MOUSEMOVE: {
            // Tooltip handling for annotation hover
            if (ide && ide->m_hwndEditor) {
                // Get cursor position relative to editor
                POINT pt = {LOWORD(lParam), HIWORD(lParam)};
                
                // Calculate which line the cursor is over
                TEXTMETRICA tm;
                HDC hdc = GetDC(hwnd);
                HFONT hFont = ide->m_editorFont ? ide->m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
                SelectObject(hdc, hFont);
                GetTextMetricsA(hdc, &tm);
                ReleaseDC(hwnd, hdc);
                
                int lineHeight = tm.tmHeight + tm.tmExternalLeading;
                if (lineHeight > 0) {
                    int firstVisible = (int)SendMessage(ide->m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
                    int hoveredLine = firstVisible + (pt.y / lineHeight) + 1;
                    
                    std::string tooltip = ide->getAnnotationTooltip(hoveredLine);
                    // Tooltip would be shown via TOOLTIPS control — for now just set status bar
                    if (!tooltip.empty() && ide->m_hwndStatusBar) {
                        SendMessage(ide->m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)tooltip.c_str());
                    }
                }
            }
            break;
        }
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
