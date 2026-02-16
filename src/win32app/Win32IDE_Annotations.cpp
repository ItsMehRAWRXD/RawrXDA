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

#ifndef IDC_ANNOTATION_OVERLAY
#define IDC_ANNOTATION_OVERLAY 19998
#endif

// ============================================================================
// CREATE ANNOTATION OVERLAY — transparent overlay window for inline annotations
// ============================================================================
void Win32IDE::createAnnotationOverlay(HWND hwndParent) {
    if (!hwndParent || m_hwndAnnotationOverlay) return;

    static bool classRegistered = false;
    if (!classRegistered) {
        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = AnnotationOverlayProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(HOLLOW_BRUSH);
        wc.lpszClassName = "RawrXDAnnotationOverlay";
        if (RegisterClassExA(&wc)) classRegistered = true;
    }

    m_hwndAnnotationOverlay = CreateWindowExA(
        WS_EX_LAYERED,
        "RawrXDAnnotationOverlay", "",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, 1, 1,
        hwndParent, (HMENU)(UINT_PTR)IDC_ANNOTATION_OVERLAY, m_hInstance, nullptr);

    if (m_hwndAnnotationOverlay) {
        SetPropA(m_hwndAnnotationOverlay, "IDE_PTR", (HANDLE)this);
        SetLayeredWindowAttributes(m_hwndAnnotationOverlay, 0, 255, LWA_ALPHA);
        LOG_INFO("Annotation overlay created");
    }
}

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
    ann.actions.clear(); // Default: no actions
    
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
// CLEAR ANNOTATIONS FOR CURRENT FILE — called on file/tab close
// ============================================================================
void Win32IDE::clearAnnotationsForCurrentFile() {
    if (m_annotations.empty() && m_currentFile.empty()) return;

    LOG_INFO("Clearing annotations for current file: " +
             (m_currentFile.empty() ? "(untitled)" : m_currentFile));
    m_annotations.clear();

    // Hardening: also purge the per-file stash so closed-file annotations
    // don't leak memory in m_annotationCache
    if (!m_currentFile.empty()) {
        m_annotationCache.erase(m_currentFile);
    }

    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// STORE ANNOTATIONS FOR TAB — stash current annotations keyed by file path
// Called before switching away from the current tab so annotations survive
// the round-trip.
// ============================================================================
void Win32IDE::storeAnnotationsForTab() {
    if (m_currentFile.empty()) return;

    if (m_annotations.empty()) {
        // Erase any stale cache entry
        m_annotationCache.erase(m_currentFile);
    } else {
        m_annotationCache[m_currentFile] = m_annotations;
        LOG_INFO("Stashed " + std::to_string(m_annotations.size()) +
                 " annotations for: " + m_currentFile);
    }

    // Annotation cache size guard: cap at 64 files to prevent unbounded growth.
    // If exceeded, evict the entry with the fewest annotations (least value).
    static const size_t MAX_ANNOTATION_CACHE_FILES = 64;
    if (m_annotationCache.size() > MAX_ANNOTATION_CACHE_FILES) {
        auto smallest = m_annotationCache.begin();
        for (auto it = m_annotationCache.begin(); it != m_annotationCache.end(); ++it) {
            if (it->second.size() < smallest->second.size()) {
                smallest = it;
            }
        }
        LOG_WARNING("Annotation cache exceeded " + std::to_string(MAX_ANNOTATION_CACHE_FILES) +
                    " files — evicting: " + smallest->first +
                    " (" + std::to_string(smallest->second.size()) + " annotations)");
        m_annotationCache.erase(smallest);
    }

    // Clear live annotations — the new tab will load its own
    m_annotations.clear();
}

// ============================================================================
// RESTORE ANNOTATIONS FOR TAB — pop stashed annotations for the incoming tab
// Called after switching to a new tab so its annotations reappear.
// ============================================================================
void Win32IDE::restoreAnnotationsForTab() {
    if (m_currentFile.empty()) {
        m_annotations.clear();
        return;
    }

    auto it = m_annotationCache.find(m_currentFile);
    if (it != m_annotationCache.end()) {
        m_annotations = it->second;
        LOG_INFO("Restored " + std::to_string(m_annotations.size()) +
                 " annotations for: " + m_currentFile);
    } else {
        m_annotations.clear();
    }

    // Trigger repaint
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
// ADD ANNOTATION WITH ACTION
// ============================================================================
void Win32IDE::addAnnotationWithAction(int line, AnnotationSeverity severity,
                                        const std::string& text,
                                        const AnnotationAction& action,
                                        const std::string& source) {
    InlineAnnotation ann;
    ann.line = line;
    ann.column = 0;
    ann.severity = severity;
    ann.text = text;
    ann.source = source;
    ann.color = getAnnotationColor(severity);
    ann.visible = m_annotationsVisible;
    ann.actions.push_back(action);

    m_annotations.push_back(ann);

    LOG_INFO("Annotation with action added: line " + std::to_string(line) +
             " [" + source + "] action=" + std::to_string((int)action.type) + " " + text);

    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// EXECUTE ANNOTATION ACTION — dispatches the click action for a single annotation
//
// If the annotation has multiple actions, executes the FIRST one.  Use
// showAnnotationActionMenu() for user-selected multi-action dispatch.
// ============================================================================
void Win32IDE::executeAnnotationAction(const InlineAnnotation& annotation) {
    // If no actions registered, show tooltip
    if (annotation.actions.empty()) {
        if (m_hwndStatusBar) {
            std::string tip = getAnnotationTooltip(annotation.line);
            SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)tip.c_str());
        }
        return;
    }

    // Execute the first action (or single action for typical annotations)
    const AnnotationAction& act = annotation.actions[0];

    switch (act.type) {
        case AnnotationActionType::None:
            // No action — just display the tooltip for visibility
            if (m_hwndStatusBar) {
                std::string tip = getAnnotationTooltip(annotation.line);
                SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)tip.c_str());
            }
            break;

        case AnnotationActionType::JumpToLine: {
            int targetLine = act.targetLine;
            if (targetLine < 1) targetLine = annotation.line;

            // If a target file is specified and differs from the current file, open it
            if (!act.targetFile.empty() &&
                act.targetFile != m_currentFile) {
                // Open the file via the tab system
                int tabIdx = findTabByPath(act.targetFile);
                if (tabIdx >= 0) {
                    setActiveTab(tabIdx);
                } else {
                    addTab(act.targetFile,
                           act.targetFile.substr(
                               act.targetFile.find_last_of("\\/") + 1));
                }
            }

            // Navigate to the target line in the editor
            if (m_hwndEditor) {
                int charIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, targetLine - 1, 0);
                if (charIdx >= 0) {
                    CHARRANGE cr = {charIdx, charIdx};
                    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
                    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
                }
            }

            LOG_INFO("Annotation action: JumpToLine " + std::to_string(targetLine));
            break;
        }

        case AnnotationActionType::ApplyFix: {
            if (act.fixContent.empty()) {
                LOG_WARNING("ApplyFix action has empty fix content");
                break;
            }

            if (!m_hwndEditor) break;

            int startLine = act.fixStartLine;
            int endLine   = act.fixEndLine;
            if (startLine < 1) startLine = annotation.line;
            if (endLine   < startLine) endLine = startLine;

            // Calculate character range for the target lines
            int startChar = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, startLine - 1, 0);
            int endLineIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX, endLine - 1, 0);
            int endLineLen = (int)SendMessage(m_hwndEditor, EM_LINELENGTH, endLineIdx, 0);
            int endChar = endLineIdx + endLineLen;

            if (startChar < 0) {
                LOG_WARNING("ApplyFix: Invalid start line " + std::to_string(startLine));
                break;
            }

            // Confirm with the user
            std::string confirmMsg = "Apply fix at line " + std::to_string(startLine);
            if (endLine > startLine) confirmMsg += "-" + std::to_string(endLine);
            confirmMsg += "?\n\nReplacement:\n" + act.fixContent.substr(0, 200);
            if (act.fixContent.size() > 200) confirmMsg += "...";

            if (MessageBoxA(m_hwndMain, confirmMsg.c_str(), "Apply Annotation Fix",
                            MB_OKCANCEL | MB_ICONQUESTION) != IDOK) {
                break;
            }

            // Replace the line range with the fix content
            CHARRANGE cr = {(LONG)startChar, (LONG)endChar};
            SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE,
                         (LPARAM)act.fixContent.c_str());

            // Remove the annotation after applying the fix
            removeAnnotations(annotation.line, annotation.source);

            // Trigger re-coloring
            onEditorContentChanged();

            LOG_INFO("Annotation action: ApplyFix at lines " +
                     std::to_string(startLine) + "-" + std::to_string(endLine));
            break;
        }

        case AnnotationActionType::AskAgent: {
            // Build a language-aware prompt with the annotation context
            std::string prompt = act.agentPrompt;
            if (prompt.empty()) {
                prompt = "Explain and help fix: " + annotation.text;
                if (annotation.line > 0) {
                    prompt += " (at line " + std::to_string(annotation.line) + ")";
                }
            }

            // Inject language context
            prompt = buildLanguageAwarePrompt(prompt);

            // Send to the agent via the Copilot Chat input and trigger execution
            if (m_hwndCopilotChatInput) {
                SetWindowTextA(m_hwndCopilotChatInput, prompt.c_str());
            }
            // Trigger agent execution
            onAgentExecuteCommand();

            LOG_INFO("Annotation action: AskAgent with prompt: " + prompt.substr(0, 80));
            break;
        }

        case AnnotationActionType::OpenFile: {
            if (act.targetFile.empty()) break;

            int tabIdx = findTabByPath(act.targetFile);
            if (tabIdx >= 0) {
                setActiveTab(tabIdx);
            } else {
                addTab(act.targetFile,
                       act.targetFile.substr(
                           act.targetFile.find_last_of("\\/") + 1));
            }

            // If a target line is specified, jump to it
            if (act.targetLine > 0 && m_hwndEditor) {
                int charIdx = (int)SendMessage(m_hwndEditor, EM_LINEINDEX,
                                               act.targetLine - 1, 0);
                if (charIdx >= 0) {
                    CHARRANGE cr = {charIdx, charIdx};
                    SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&cr);
                    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
                }
            }

            LOG_INFO("Annotation action: OpenFile " + act.targetFile);
            break;
        }

        case AnnotationActionType::RunCommand: {
            if (act.commandId.empty()) break;

            LOG_INFO("Annotation action: RunCommand " + act.commandId);
            appendToOutput("Executing command: " + act.commandId + "\n",
                           "Output", OutputSeverity::Info);
            break;
        }

        case AnnotationActionType::Suppress: {
            // Dismiss/suppress the annotation permanently — remove it
            removeAnnotations(annotation.line, annotation.source);
            LOG_INFO("Annotation action: Suppress at line " + std::to_string(annotation.line));

            if (m_hwndStatusBar) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0,
                             (LPARAM)"Annotation suppressed");
            }
            break;
        }
    }
}

// ============================================================================
// ON ANNOTATION CLICKED — resolves which annotation was clicked, dispatches
// ============================================================================
void Win32IDE::onAnnotationClicked(int line, int clickX, int clickY) {
    auto anns = getAnnotationsForLine(line);
    if (anns.empty()) return;

    // If exactly one annotation on this line, execute its action directly
    if (anns.size() == 1) {
        if (!anns[0].actions.empty() && anns[0].actions[0].type != AnnotationActionType::None) {
            executeAnnotationAction(anns[0]);
        } else {
            // No action configured — show tooltip in status bar
            std::string tip = getAnnotationTooltip(line);
            if (m_hwndStatusBar) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)tip.c_str());
            }
        }
        return;
    }

    // Multiple annotations on this line — show a context menu
    POINT pt = {clickX, clickY};
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        ClientToScreen(m_hwndAnnotationOverlay, &pt);
    }
    showAnnotationActionMenu(m_hwndMain, pt.x, pt.y, anns);
}

// ============================================================================
// SHOW ANNOTATION ACTION MENU — popup context menu for multi-annotation lines
// ============================================================================
void Win32IDE::showAnnotationActionMenu(HWND hwndParent, int x, int y,
                                         const std::vector<InlineAnnotation>& annotations) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    for (size_t i = 0; i < annotations.size(); i++) {
        const auto& ann = annotations[i];

        // Build the menu item label: "[Severity] text (action)"
        std::string label;
        switch (ann.severity) {
            case AnnotationSeverity::Error:      label = "X "; break;
            case AnnotationSeverity::Warning:    label = "! "; break;
            case AnnotationSeverity::Info:       label = "i "; break;
            case AnnotationSeverity::Hint:       label = "* "; break;
            case AnnotationSeverity::Suggestion: label = "> "; break;
        }
        label += ann.text;

        // Append action hint from the first action (if any), prefer label field
        if (!ann.actions.empty()) {
            const auto& firstAct = ann.actions[0];
            if (!firstAct.label.empty()) {
                label += "  [" + firstAct.label + "]";
            } else {
                switch (firstAct.type) {
                    case AnnotationActionType::JumpToLine: label += "  [Go to line]"; break;
                    case AnnotationActionType::ApplyFix:   label += "  [Apply fix]"; break;
                    case AnnotationActionType::AskAgent:   label += "  [Ask agent]"; break;
                    case AnnotationActionType::OpenFile:   label += "  [Open file]"; break;
                    case AnnotationActionType::RunCommand: label += "  [Run command]"; break;
                    case AnnotationActionType::Suppress:   label += "  [Suppress]"; break;
                    default: break;
                }
            }
        }

        // Truncate for menu readability
        if (label.size() > 120) label = label.substr(0, 117) + "...";

        AppendMenuA(hMenu, MF_STRING, (UINT_PTR)(1000 + i), label.c_str());
    }

    int cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, hwndParent, nullptr);
    DestroyMenu(hMenu);

    if (cmd >= 1000 && (cmd - 1000) < (int)annotations.size()) {
        const auto& chosen = annotations[cmd - 1000];
        if (!chosen.actions.empty() && chosen.actions[0].type != AnnotationActionType::None) {
            executeAnnotationAction(chosen);
        } else {
            // Default: show tooltip
            std::string tip = getAnnotationTooltip(chosen.line);
            if (m_hwndStatusBar) {
                SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)tip.c_str());
            }
        }
    }
}

// ============================================================================
// SESSION-AWARE ANNOTATIONS — save annotations per file into the session
// ============================================================================
void Win32IDE::saveSessionAnnotations(nlohmann::json& session) {
    if (m_annotations.empty()) return;

    // Group annotations by the file they belong to (source path or current file)
    // For now, all annotations belong to the current file
    nlohmann::json annArray = nlohmann::json::array();

    for (const auto& ann : m_annotations) {
        nlohmann::json j;
        j["line"] = ann.line;
        j["column"] = ann.column;
        j["severity"] = (int)ann.severity;
        j["text"] = ann.text;
        j["source"] = ann.source;
        j["visible"] = ann.visible;

        // Persist actions array
        nlohmann::json actionsArray = nlohmann::json::array();
        for (const auto& act : ann.actions) {
            nlohmann::json aj;
            aj["type"] = (int)act.type;
            aj["label"] = act.label;
            aj["targetLine"] = act.targetLine;
            aj["targetFile"] = act.targetFile;
            aj["fixContent"] = act.fixContent;
            aj["fixStartLine"] = act.fixStartLine;
            aj["fixEndLine"] = act.fixEndLine;
            aj["agentPrompt"] = act.agentPrompt;
            aj["commandId"] = act.commandId;
            actionsArray.push_back(aj);
        }
        j["actions"] = actionsArray;

        annArray.push_back(j);
    }

    // Store under the current file path key
    nlohmann::json annsByFile;
    if (!m_currentFile.empty()) {
        annsByFile[m_currentFile] = annArray;
    } else {
        annsByFile["__untitled__"] = annArray;
    }

    session["annotations"] = annsByFile;
    session["annotationsVisible"] = m_annotationsVisible;

    LOG_INFO("Session: saved " + std::to_string(m_annotations.size()) + " annotations");
}

// ============================================================================
// SESSION-AWARE ANNOTATIONS — restore annotations per file from the session
// ============================================================================
void Win32IDE::restoreSessionAnnotations(const nlohmann::json& session) {
    if (!session.contains("annotations")) return;

    // Do not restore annotations for untitled buffers or before the editor HWND exists
    if (m_currentFile.empty()) {
        LOG_INFO("Session: skipping annotation restore for untitled buffer");
        return;
    }
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) {
        LOG_INFO("Session: skipping annotation restore — editor HWND not ready");
        return;
    }

    // Restore global visibility flag
    m_annotationsVisible = session.value("annotationsVisible", true);

    const auto& annsByFile = session["annotations"];

    // Determine which key to look up
    std::string fileKey = m_currentFile;

    if (!annsByFile.contains(fileKey)) {
        LOG_INFO("Session: no annotations found for file: " + fileKey);
        return;
    }

    // Gracefully ignore annotations for files that no longer exist on disk
    DWORD fileAttrs = GetFileAttributesA(fileKey.c_str());
    if (fileAttrs == INVALID_FILE_ATTRIBUTES) {
        LOG_WARNING("Session: annotation file missing on disk, skipping: " + fileKey);
        return;
    }

    const auto& annArray = annsByFile[fileKey];
    int restoredCount = 0;

    for (size_t i = 0; i < annArray.size(); i++) {
        const auto& j = annArray[i];

        InlineAnnotation ann;
        ann.line     = j.value("line", 0);
        ann.column   = j.value("column", 0);
        ann.severity = static_cast<AnnotationSeverity>(j.value("severity", 0));
        ann.text     = j.value("text", "");
        ann.source   = j.value("source", "agent");
        ann.visible  = j.value("visible", true);
        ann.color    = getAnnotationColor(ann.severity);

        // Restore actions vector
        ann.actions.clear();
        if (j.contains("actions")) {
            const auto& actionsArr = j["actions"];
            for (size_t ai = 0; ai < actionsArr.size(); ai++) {
                const auto& aj = actionsArr[ai];
                AnnotationAction act;
                act.type         = static_cast<AnnotationActionType>(aj.value("type", 0));
                act.label        = aj.value("label", "");
                act.targetLine   = aj.value("targetLine", 0);
                act.targetFile   = aj.value("targetFile", "");
                act.fixContent   = aj.value("fixContent", "");
                act.fixStartLine = aj.value("fixStartLine", 0);
                act.fixEndLine   = aj.value("fixEndLine", 0);
                act.agentPrompt  = aj.value("agentPrompt", "");
                act.commandId    = aj.value("commandId", "");
                ann.actions.push_back(act);
            }
        } else if (j.contains("actionType")) {
            // Legacy fallback: single-action format from older sessions
            AnnotationAction act;
            act.type         = static_cast<AnnotationActionType>(j.value("actionType", 0));
            act.label        = "";
            act.targetLine   = j.value("actionTargetLine", 0);
            act.targetFile   = j.value("actionTargetFile", "");
            act.fixContent   = j.value("actionFixContent", "");
            act.fixStartLine = j.value("actionFixStartLine", 0);
            act.fixEndLine   = j.value("actionFixEndLine", 0);
            act.agentPrompt  = j.value("actionAgentPrompt", "");
            act.commandId    = j.value("actionCommandId", "");
            if (act.type != AnnotationActionType::None) {
                ann.actions.push_back(act);
            }
        }

        if (ann.line > 0 && !ann.text.empty()) {
            m_annotations.push_back(ann);
            restoredCount++;
        }
    }

    LOG_INFO("Session: restored " + std::to_string(restoredCount) + " annotations for " + fileKey);

    // Trigger repaint
    if (m_hwndLineNumbers) {
        InvalidateRect(m_hwndLineNumbers, nullptr, FALSE);
    }
    if (m_hwndAnnotationOverlay && IsWindow(m_hwndAnnotationOverlay)) {
        InvalidateRect(m_hwndAnnotationOverlay, nullptr, FALSE);
    }
}

// ============================================================================
// LANGUAGE-AWARE AGENT PROMPTS — get human-readable language name
// ============================================================================
std::string Win32IDE::getSyntaxLanguageName() const {
    switch (m_syntaxLanguage) {
        case SyntaxLanguage::Cpp:        return "C/C++";
        case SyntaxLanguage::Python:     return "Python";
        case SyntaxLanguage::JavaScript: return "JavaScript/TypeScript";
        case SyntaxLanguage::PowerShell: return "PowerShell";
        case SyntaxLanguage::JSON:       return "JSON";
        case SyntaxLanguage::Markdown:   return "Markdown";
        case SyntaxLanguage::Assembly:   return "x86/x64 Assembly (NASM/MASM)";
        case SyntaxLanguage::None:
        default:                         return "Plain Text";
    }
}

// ============================================================================
// LANGUAGE-AWARE AGENT PROMPTS — wraps a base prompt with language context
//
// Injects the detected source language, current file path, and approximate
// cursor position so the agent can tailor its response to the correct
// language and file context.
// ============================================================================
std::string Win32IDE::buildLanguageAwarePrompt(const std::string& basePrompt) const {
    std::string enriched;

    // Language context header — always inject, even for Unknown/None
    std::string langName = getSyntaxLanguageName();
    enriched += "[Language: " + langName + "] ";

    // File context — capped at 260 chars (MAX_PATH) to prevent prompt explosion
    if (!m_currentFile.empty()) {
        std::string filePath = m_currentFile;
        if (filePath.size() > 260) {
            filePath = "..." + filePath.substr(filePath.size() - 257);
        }
        enriched += "[File: " + filePath + "] ";
    }

    // Cursor position context
    if (m_hwndEditor) {
        CHARRANGE sel;
        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
        int line = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0) + 1;
        enriched += "[Line: " + std::to_string(line) + "] ";
    }

    enriched += basePrompt;

    // Hardening: cap total enriched prompt length to prevent token explosion
    // Context prefix is typically ~80-120 chars; cap generously at 4096
    const size_t MAX_PROMPT_LENGTH = 4096;
    if (enriched.size() > MAX_PROMPT_LENGTH) {
        LOG_WARNING("Agent prompt truncated from " + std::to_string(enriched.size()) +
                    " to " + std::to_string(MAX_PROMPT_LENGTH) + " chars");
        enriched.resize(MAX_PROMPT_LENGTH);
        // Ensure we don't break mid-UTF8 — trim trailing partial bytes
        while (!enriched.empty() && (enriched.back() & 0xC0) == 0x80) {
            enriched.pop_back();
        }
    }

    return enriched;
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

        case WM_NCHITTEST: {
            // Conditionally intercept clicks on annotation regions.
            // For areas with no annotation, pass through to the editor.
            if (!ide || !ide->m_hwndEditor) return HTTRANSPARENT;

            POINT ptScreen = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            POINT ptClient = ptScreen;
            ScreenToClient(hwnd, &ptClient);

            // Calculate which line the cursor is over
            HDC hdc = GetDC(hwnd);
            HFONT hFont = ide->m_editorFont ? ide->m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
            SelectObject(hdc, hFont);
            TEXTMETRICA tm;
            GetTextMetricsA(hdc, &tm);
            ReleaseDC(hwnd, hdc);

            int lineHeight = tm.tmHeight + tm.tmExternalLeading;
            if (lineHeight <= 0) lineHeight = 16;

            int firstVisible = (int)SendMessage(ide->m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
            int hoveredLine = firstVisible + (ptClient.y / lineHeight) + 1;

            auto anns = ide->getAnnotationsForLine(hoveredLine);
            if (!anns.empty()) {
                // Check if the click X is in the annotation text region (after EOL)
                int charWidth = tm.tmAveCharWidth;
                if (charWidth <= 0) charWidth = 8;
                int lineIdx = hoveredLine - 1;
                int lineStart = (int)SendMessage(ide->m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
                int lineLen = (int)SendMessage(ide->m_hwndEditor, EM_LINELENGTH, lineStart, 0);
                int eolX = (lineLen + 2) * charWidth;

                if (ptClient.x >= eolX) {
                    return HTCLIENT; // Intercept — we'll handle the click
                }
            }

            return HTTRANSPARENT; // Pass through to editor
        }

        case WM_LBUTTONDOWN: {
            // Annotation click handler
            if (!ide || !ide->m_hwndEditor) break;

            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };

            HDC hdc = GetDC(hwnd);
            HFONT hFont = ide->m_editorFont ? ide->m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
            SelectObject(hdc, hFont);
            TEXTMETRICA tm;
            GetTextMetricsA(hdc, &tm);
            ReleaseDC(hwnd, hdc);

            int lineHeight = tm.tmHeight + tm.tmExternalLeading;
            if (lineHeight <= 0) lineHeight = 16;

            int firstVisible = (int)SendMessage(ide->m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
            int clickedLine = firstVisible + (pt.y / lineHeight) + 1;

            ide->onAnnotationClicked(clickedLine, pt.x, pt.y);
            return 0;
        }
        
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

        case WM_SETCURSOR: {
            // Show hand cursor when over annotation text to indicate it's clickable
            if (!ide || !ide->m_hwndEditor) break;

            POINT ptCursor;
            GetCursorPos(&ptCursor);
            ScreenToClient(hwnd, &ptCursor);

            HDC hdc = GetDC(hwnd);
            HFONT hFont = ide->m_editorFont ? ide->m_editorFont : (HFONT)GetStockObject(ANSI_FIXED_FONT);
            SelectObject(hdc, hFont);
            TEXTMETRICA tm;
            GetTextMetricsA(hdc, &tm);
            ReleaseDC(hwnd, hdc);

            int lineHeight = tm.tmHeight + tm.tmExternalLeading;
            if (lineHeight <= 0) lineHeight = 16;
            int charWidth = tm.tmAveCharWidth;
            if (charWidth <= 0) charWidth = 8;

            int firstVisible = (int)SendMessage(ide->m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
            int hoveredLine = firstVisible + (ptCursor.y / lineHeight) + 1;

            auto anns = ide->getAnnotationsForLine(hoveredLine);
            if (!anns.empty()) {
                int lineIdx = hoveredLine - 1;
                int lineStart = (int)SendMessage(ide->m_hwndEditor, EM_LINEINDEX, lineIdx, 0);
                int lineLen = (int)SendMessage(ide->m_hwndEditor, EM_LINELENGTH, lineStart, 0);
                int eolX = (lineLen + 2) * charWidth;

                if (ptCursor.x >= eolX) {
                    // Over annotation text — show hand cursor
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                    return TRUE;
                }
            }
            break;
        }
    }
    
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}
