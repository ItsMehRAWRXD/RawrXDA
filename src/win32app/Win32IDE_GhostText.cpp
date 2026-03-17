// ============================================================================
// Win32IDE_GhostText.cpp — Ghost Text / Inline Completion Renderer
// ============================================================================
// Provides GitHub Copilot-style inline code completions:
//   - Grayed-out "ghost text" rendered after the cursor position
//   - Tab to accept, Esc to dismiss
//   - Debounced trigger on typing pause (500ms)
//   - Integration with CompletionServer (local GGUF) or Ollama
//   - Multi-line ghost text support
//   - Cursor movement auto-dismisses
//
// Architecture:
//   - EN_CHANGE fires debounce timer (GHOST_TEXT_TIMER_ID)
//   - Timer fires → background thread requests completion
//   - WM_APP+400 delivers completion to UI thread
//   - Custom paint via editor subclass intercepts WM_PAINT
//   - Ghost text rendered in italic gray after the cursor position
// ============================================================================

#include "Win32IDE.h"
#include "../../include/ghost_text_renderer.h"
#include "IDELogger.h"
#include <richedit.h>
#include <algorithm>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <array>
#include <cctype>

#include "../agentic/OllamaProvider.h"
#include "../agentic/OrchestratorBridge.h"

// ============================================================================
// CONSTANTS
// ============================================================================
static const UINT_PTR GHOST_TEXT_TIMER_ID   = 8888;
static const UINT      GHOST_TEXT_DELAY_MS  = 120;   // Debounce: 120ms after last keystroke
static const int       GHOST_TEXT_MAX_CHARS = 512;    // Max ghost text length
static const int       GHOST_TEXT_MAX_LINES = 8;      // Max multi-line completions
static const uint64_t  GHOST_TEXT_CACHE_TTL_MS = 5000;  // 5s for 70B inference latency
static const size_t    GHOST_TEXT_CACHE_MAX_ITEMS = 256;

namespace {
uint64_t nowMs() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string buildGhostCacheKey(const std::string& filePath,
                               const std::string& language,
                               const std::string& prefix,
                               const std::string& suffix,
                               int line, int column) {
    const size_t prefixWindow = (prefix.size() > 512) ? 512 : prefix.size();
    const size_t suffixWindow = (suffix.size() > 256) ? 256 : suffix.size();
    const std::string prefixTail = prefix.substr(prefix.size() - prefixWindow, prefixWindow);
    const std::string suffixHead = suffix.substr(0, suffixWindow);
    const std::string seed = filePath + "|" + language + "|" + std::to_string(line) + "|" +
                             std::to_string(column) + "|" + prefixTail + "|" + suffixHead;
    return std::to_string(std::hash<std::string>{}(seed));
}

std::string trimLeftCopy(const std::string& in) {
    size_t i = 0;
    while (i < in.size() && std::isspace(static_cast<unsigned char>(in[i])) != 0) ++i;
    return in.substr(i);
}

std::string lowerCopy(const std::string& in) {
    std::string out = in;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string getLinePrefix(const std::string& context) {
    const size_t pos = context.find_last_of('\n');
    if (pos == std::string::npos) return context;
    if (pos + 1 >= context.size()) return "";
    return context.substr(pos + 1);
}

std::string buildSnippetCompletion(const std::string& context, const std::string& language) {
    const std::string line = trimLeftCopy(getLinePrefix(context));
    const std::string lowered = lowerCopy(line);
    const std::string lang = lowerCopy(language);

    if (lowered == "if") {
        return " () {\n    \n}";
    }
    if (lowered == "for") {
        return " (int i = 0; i < ; ++i) {\n    \n}";
    }
    if (lowered == "while") {
        return " () {\n    \n}";
    }
    if ((lang == "c++" || lang == "cpp" || lang == "c") && lowered == "switch") {
        return " () {\ncase :\n    break;\ndefault:\n    break;\n}";
    }
    return "";
}
} // namespace

// ============================================================================
// INITIALIZATION
// ============================================================================

void Win32IDE::initGhostText() {
    m_ghostTextEnabled        = true;
    m_ghostTextVisible        = false;
    m_ghostTextAccepted       = false;
    m_ghostTextPending        = false;
    m_ghostTextContent.clear();
    m_ghostTextLine           = -1;
    m_ghostTextColumn         = -1;
    m_ghostTextFont           = nullptr;

    // Wire ghost text overlay to editor so overlay can render/draw when used
    if (m_ghostTextRendererOverlay && m_hwndEditor) {
        m_ghostTextRendererOverlay->setEditorHwnd(m_hwndEditor);
        m_ghostTextRendererOverlay->initialize();
    }

    // Create ghost text font — italic version of editor font (DPI-scaled)
    LOGFONTA lf = {};
    lf.lfHeight         = -dpiScale(14);    // ~10.5pt at 96 DPI
    lf.lfWeight         = FW_NORMAL;
    lf.lfItalic         = TRUE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(lf.lfFaceName, m_currentTheme.fontName.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    m_ghostTextFont = CreateFontIndirectA(&lf);

    LOG_INFO("Ghost text renderer initialized (debounce=" +
             std::to_string(GHOST_TEXT_DELAY_MS) + "ms)");
}

void Win32IDE::shutdownGhostText() {
    dismissGhostText();
    if (m_ghostTextFont) {
        DeleteObject(m_ghostTextFont);
        m_ghostTextFont = nullptr;
    }
}

// ============================================================================
// DEBOUNCE TRIGGER — called from onEditorContentChanged()
// ============================================================================

void Win32IDE::triggerGhostTextCompletion() {
    if (!m_ghostTextEnabled || !m_hwndEditor) return;

    // Kill any existing ghost text timer and dismiss current ghost text
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);
    ++m_ghostTextRequestSeq;
    dismissGhostText();

    // Start a new debounce timer
    SetTimer(m_hwndMain, GHOST_TEXT_TIMER_ID, GHOST_TEXT_DELAY_MS, nullptr);
}

// ============================================================================
// TIMER CALLBACK — fires after debounce period, requests completion
// ============================================================================

void Win32IDE::onGhostTextTimer() {
    KillTimer(m_hwndMain, GHOST_TEXT_TIMER_ID);

    if (!m_ghostTextEnabled || !m_hwndEditor) return;
    if (m_ghostTextPending) return;  // Already requesting

    // Gather context: text before cursor
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);
    int cursorPos = sel.cpMin;

    if (cursorPos <= 0) return;

    // Get up to 4KB of text before cursor for context
    int contextStart = (cursorPos > 4096) ? cursorPos - 4096 : 0;
    int contextLen = cursorPos - contextStart;

    TEXTRANGEA tr;
    tr.chrg.cpMin = contextStart;
    tr.chrg.cpMax = cursorPos;

    std::vector<char> buf(contextLen + 1, 0);
    tr.lpstrText = buf.data();
    SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);

    std::string context(buf.data());
    if (context.empty()) return;

    // Get current line/column for positioning
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, cursorPos, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int column = cursorPos - lineStart;

    // Store cursor position for ghost text rendering
    m_ghostTextLine   = lineIndex;
    m_ghostTextColumn = column;
    m_ghostTextPending = true;
    const uint64_t requestSeq = m_ghostTextRequestSeq.load();

    {
        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
        m_ghostTextMetrics.requests++;
    }

    // Detect language for context
    std::string language = getSyntaxLanguageName();

    // Gather suffix context (text after cursor, up to 2KB) for FIM
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    int suffixEnd = (cursorPos + 2048 < textLen) ? cursorPos + 2048 : textLen;
    int suffixLen = suffixEnd - cursorPos;
    std::string suffix;
    if (suffixLen > 0) {
        TEXTRANGEA trSuffix;
        trSuffix.chrg.cpMin = cursorPos;
        trSuffix.chrg.cpMax = suffixEnd;
        std::vector<char> suffBuf(suffixLen + 1, 0);
        trSuffix.lpstrText = suffBuf.data();
        SendMessageA(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&trSuffix);
        suffix.assign(suffBuf.data());
    }

    // Fire background thread for completion
    std::string contextCopy = context;
    std::string suffixCopy = suffix;
    std::string langCopy = language;
    std::string fileCopy = m_currentFile;
    int cursorCopy = cursorPos;
    int lineCopy = lineIndex;
    int colCopy = column;
    std::string cacheKey = buildGhostCacheKey(fileCopy, langCopy, contextCopy, suffixCopy, lineCopy, colCopy);

    {
        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
        auto it = m_ghostTextCache.find(cacheKey);
        if (it != m_ghostTextCache.end() && (nowMs() - it->second.createdAtMs) <= GHOST_TEXT_CACHE_TTL_MS) {
            m_ghostTextMetrics.cacheHits++;
            m_ghostTextPending = false;
            onGhostTextReady(cursorCopy, it->second.completion.c_str());
            return;
        }
    }

    std::thread([this, contextCopy, suffixCopy, langCopy, fileCopy, cursorCopy, lineCopy, colCopy, requestSeq]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        const uint64_t startedAt = nowMs();
        std::string completion = requestGhostTextCompletion(
            contextCopy, langCopy, suffixCopy, fileCopy, lineCopy, colCopy, requestSeq);
        const uint64_t elapsedMs = nowMs() - startedAt;

        if (requestSeq != m_ghostTextRequestSeq.load()) {
            std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
            m_ghostTextMetrics.staleDrops++;
            if (!isShuttingDown()) {
                PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)nullptr);
            }
            return;
        }

        if (!completion.empty()) {
            std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
            if (m_ghostTextCache.size() >= GHOST_TEXT_CACHE_MAX_ITEMS) {
                m_ghostTextCache.erase(m_ghostTextCache.begin());
            }
            const std::string key = buildGhostCacheKey(fileCopy, langCopy, contextCopy, suffixCopy, lineCopy, colCopy);
            m_ghostTextCache[key] = GhostTextCacheEntry{completion, nowMs()};
            m_ghostTextMetrics.lastLatencyMs = (double)elapsedMs;
            const double reqCount = (double)std::max<uint64_t>(1, m_ghostTextMetrics.requests - m_ghostTextMetrics.cacheHits);
            m_ghostTextMetrics.avgLatencyMs =
                ((m_ghostTextMetrics.avgLatencyMs * (reqCount - 1.0)) + (double)elapsedMs) / reqCount;
        }

        // Post result to UI thread
        if (isShuttingDown()) return;
        if (!completion.empty()) {
            char* heapText = _strdup(completion.c_str());
            PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)heapText);
        } else {
            PostMessageA(m_hwndMain, WM_GHOST_TEXT_READY, (WPARAM)cursorCopy, (LPARAM)nullptr);
        }
    }).detach();
}

// ============================================================================
// COMPLETION REQUEST — runs on background thread
// ============================================================================

std::string Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                   const std::string& language) {
    // Legacy 2-arg overload — forward to FIM-aware version with empty suffix
    return requestGhostTextCompletion(context, language, "", "", 0, 0);
}

std::string Win32IDE::requestGhostTextCompletion(const std::string& context,
                                                   const std::string& language,
                                                   const std::string& suffix,
                                                   const std::string& filePath,
                                                   int cursorLine, int cursorCol,
                                                   uint64_t expectedSeq) {
    using namespace RawrXD::Prediction;

    const auto isStale = [this, expectedSeq]() -> bool {
        return expectedSeq != 0 && expectedSeq != m_ghostTextRequestSeq.load();
    };

    enum class GhostProviderKind {
        Local,
        Snippet,
        Lsp
    };
    const std::array<GhostProviderKind, 3> precedence = {
        GhostProviderKind::Local,
        GhostProviderKind::Snippet,
        GhostProviderKind::Lsp
    };

    for (GhostProviderKind provider : precedence) {
        if (isStale()) return "";

        if (provider == GhostProviderKind::Local) {
            // ---- Primary: OrchestratorBridge FIM (uses AgentOllamaClient + FIMPromptBuilder) ----
            {
                auto& orchBridge = RawrXD::Agent::OrchestratorBridge::Instance();
                if (orchBridge.IsInitialized()) {
                    RawrXD::Prediction::PredictionContext bCtx;
                    bCtx.prefix       = context;
                    bCtx.suffix       = suffix;
                    bCtx.language     = language;
                    bCtx.filePath     = filePath;
                    bCtx.cursorLine   = cursorLine;
                    bCtx.cursorColumn = cursorCol;

                    auto bResult = orchBridge.RequestGhostText(bCtx);
                    if (bResult.success && !bResult.completion.empty()) {
                        std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                        m_ghostTextMetrics.localWins++;
                        return trimGhostText(bResult.completion);
                    }
                }
            }

            if (isStale()) return "";

            // ---- Fallback: prediction backend, then native model, then local Ollama prompt ----
            if (!m_predictionProvider) {
                std::string baseUrl = m_ollamaBaseUrl.empty() ? "http://localhost:11434" : m_ollamaBaseUrl;
                m_predictionProvider = std::make_unique<OllamaProvider>(baseUrl);

                PredictionConfig cfg;
                cfg.model       = getResolvedOllamaModel().empty() ? "qwen2.5-coder:14b" : getResolvedOllamaModel();
                cfg.temperature = 0.2f;
                cfg.maxTokens   = 256;
                cfg.maxLines    = GHOST_TEXT_MAX_LINES;
                cfg.useFIM      = true;
                cfg.stopSequences = "<|endoftext|>,<|fim_pad|>,\n\n\n";
                m_predictionProvider->Configure(cfg);
            }

            if (m_predictionProvider->IsAvailable()) {
                PredictionContext ctx;
                ctx.prefix       = context;
                ctx.suffix       = suffix;
                ctx.language     = language;
                ctx.filePath     = filePath;
                ctx.cursorLine   = cursorLine;
                ctx.cursorColumn = cursorCol;

                PredictionResult result = m_predictionProvider->Predict(ctx);
                if (result.success && !result.completion.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return trimGhostText(result.completion);
                }
            }

            if (isStale()) return "";

            if (m_nativeEngine && m_nativeEngine->IsModelLoaded()) {
                auto tokens = m_nativeEngine->Tokenize(
                    "Complete the following " + language + " code. Output ONLY the completion, "
                    "no explanation, no markdown:\n\n" + context);

                auto generated = m_nativeEngine->Generate(tokens, 64);
                std::string result = trimGhostText(m_nativeEngine->Detokenize(generated));
                if (!result.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return result;
                }
            }

            if (isStale()) return "";

            if (!m_ollamaBaseUrl.empty()) {
                std::string response;
                std::string prompt = "Complete the following " + language + " code. "
                                     "Output ONLY the completion, no explanation, no markdown. "
                                     "Maximum 3 lines:\n\n" + context;
                if (trySendToOllama(prompt, response)) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.localWins++;
                    return trimGhostText(response);
                }
            }
        }

        if (provider == GhostProviderKind::Snippet) {
            const std::string snippet = trimGhostText(buildSnippetCompletion(context, language));
            if (!snippet.empty()) {
                std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                m_ghostTextMetrics.snippetWins++;
                return snippet;
            }
        }

        if (provider == GhostProviderKind::Lsp) {
            if (filePath.empty()) continue;
            const int lspLine = cursorLine > 0 ? cursorLine - 1 : 0;

            // Fast path: direct LSP completion for current cursor.
            std::string uri = filePathToUri(filePath);
            auto lspItems = lspCompletion(uri, lspLine, cursorCol);
            for (const auto& item : lspItems) {
                if (item.insertText.empty()) continue;
                std::string lspText = trimGhostText(item.insertText);
                if (!lspText.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.lspWins++;
                    return lspText;
                }
            }

            // Fallback: hybrid merge if direct LSP did not yield usable insert text.
            auto items = requestHybridCompletion(filePath, lspLine, cursorCol);
            for (const auto& item : items) {
                if (item.insertText.empty()) continue;
                if (item.source != "lsp" && item.source != "merged") continue;
                std::string lspText = trimGhostText(item.insertText);
                if (!lspText.empty()) {
                    std::lock_guard<std::mutex> lock(m_ghostTextCacheMutex);
                    m_ghostTextMetrics.lspWins++;
                    return lspText;
                }
            }
        }
    }

    return "";
}

// ============================================================================
// GHOST TEXT DELIVERY — WM_GHOST_TEXT_READY handler (UI thread)
// ============================================================================

void Win32IDE::onGhostTextReady(int requestedCursorPos, const char* completionText) {
    m_ghostTextPending = false;

    if (!completionText || !m_hwndEditor) return;

    // Verify cursor hasn't moved since request
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    if (sel.cpMin != requestedCursorPos) {
        // Cursor moved — discard stale completion
        return;
    }

    // ARCHITECTURE ALIGNMENT: 
    // Directly update state and invalidate for RichEdit overlay rendering.
    // Ensure we don't just store, but activate the 'visible' state for WM_PAINT.
    m_ghostTextContent = completionText;
    m_ghostTextVisible = true;
    m_ghostTextAccepted = false;

    // Align with MASM Sovereign logic: trigger immediate repaint
    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
        UpdateWindow(m_hwndEditor); // Force immediate paint to minimize flicker
    }

    // Record event
    recordEvent(AgentEventType::GhostTextRequested, "",
                m_ghostTextContent.substr(0, 128), "", 0, true);
}

// ============================================================================
// DISMISS — clears ghost text
// ============================================================================

void Win32IDE::dismissGhostText() {
    if (!m_ghostTextVisible) return;

    m_ghostTextVisible  = false;
    m_ghostTextContent.clear();
    m_ghostTextLine     = -1;
    m_ghostTextColumn   = -1;
    m_ghostTextAccepted = false;

    if (m_hwndEditor) {
        InvalidateRect(m_hwndEditor, nullptr, FALSE);
    }
}

// ============================================================================
// ACCEPT — Tab key inserts the ghost text into the editor
// ============================================================================

void Win32IDE::acceptGhostText() {
    if (!m_ghostTextVisible || m_ghostTextContent.empty() || !m_hwndEditor) return;

    std::string textToInsert = m_ghostTextContent;
    m_ghostTextAccepted = true;

    // Dismiss first to avoid re-rendering ghost during insert
    dismissGhostText();

    // Insert text at current cursor position
    SendMessageA(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)textToInsert.c_str());

    // Record acceptance event
    recordEvent(AgentEventType::GhostTextAccepted, "",
                textToInsert.substr(0, 128), "", 0, true);

    LOG_INFO("Ghost text accepted (" + std::to_string(textToInsert.size()) + " chars)");
}

// ============================================================================
// RENDER — paints ghost text onto the editor surface
// ============================================================================

void Win32IDE::renderGhostText(HDC hdc) {
    if (!m_ghostTextVisible || m_ghostTextContent.empty() || !m_hwndEditor) return;

    // Get current cursor position to know where to draw
    CHARRANGE sel;
    SendMessageA(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&sel);

    // Get the pixel position of the cursor
    POINTL pt;
    SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&pt, sel.cpMin);

    if (pt.x < 0 || pt.y < 0) return;  // Cursor not visible

    // Get the text after cursor on the same line to know rendering offset
    int lineIndex = (int)SendMessageA(m_hwndEditor, EM_LINEFROMCHAR, sel.cpMin, 0);
    int lineStart = (int)SendMessageA(m_hwndEditor, EM_LINEINDEX, lineIndex, 0);
    int lineLen   = (int)SendMessageA(m_hwndEditor, EM_LINELENGTH, sel.cpMin, 0);
    int lineEnd   = lineStart + lineLen;

    // If cursor is not at end of line, render after end of line text
    // (ghost text appears after existing text)
    POINTL endPt;
    if (lineEnd > sel.cpMin) {
        SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&endPt, lineEnd);
    } else {
        endPt = pt;
    }

    // Setup ghost text rendering
    HFONT oldFont = (HFONT)SelectObject(hdc, m_ghostTextFont ? m_ghostTextFont : GetStockObject(SYSTEM_FONT));
    
    // Ghost text color: muted/grayed version of text color
    COLORREF ghostColor = RGB(
        (GetRValue(m_currentTheme.textColor) + GetRValue(m_currentTheme.backgroundColor)) / 2,
        (GetGValue(m_currentTheme.textColor) + GetGValue(m_currentTheme.backgroundColor)) / 2,
        (GetBValue(m_currentTheme.textColor) + GetBValue(m_currentTheme.backgroundColor)) / 2
    );
    SetTextColor(hdc, ghostColor);
    SetBkMode(hdc, TRANSPARENT);

    // Split ghost text into lines
    std::vector<std::string> lines;
    std::istringstream stream(m_ghostTextContent);
    std::string line;
    int lineCount = 0;
    while (std::getline(stream, line) && lineCount < GHOST_TEXT_MAX_LINES) {
        lines.push_back(line);
        lineCount++;
    }

    if (lines.empty()) {
        SelectObject(hdc, oldFont);
        return;
    }

    // Get line height
    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;

    // Draw first line at cursor/end-of-line position
    int drawX = endPt.x + 2;  // Small gap after existing text
    int drawY = endPt.y;

    // Get editor client rect for clipping
    RECT editorRC;
    GetClientRect(m_hwndEditor, &editorRC);

    for (size_t i = 0; i < lines.size(); i++) {
        if (drawY + lineHeight > editorRC.bottom) break;  // Don't draw below editor

        if (i == 0) {
            // First line: render after cursor
            TextOutA(hdc, drawX, drawY, lines[i].c_str(), (int)lines[i].size());
        } else {
            // Subsequent lines: render at left margin (indented to match cursor column)
            // Get x position of the start of the line (respect indentation)
            POINTL lineStartPt;
            SendMessageA(m_hwndEditor, EM_POSFROMCHAR, (WPARAM)&lineStartPt, lineStart);
            int indentX = lineStartPt.x;

            drawY += lineHeight;
            TextOutA(hdc, indentX, drawY, lines[i].c_str(), (int)lines[i].size());
        }
    }

    SelectObject(hdc, oldFont);
}

// ============================================================================
// KEY HANDLER — intercepts Tab/Esc in editor subclass
// ============================================================================

bool Win32IDE::handleGhostTextKey(UINT vk) {
    if (!m_ghostTextVisible) return false;

    if (vk == VK_TAB) {
        acceptGhostText();
        return true;  // Consumed
    }

    if (vk == VK_ESCAPE) {
        dismissGhostText();
        return true;  // Consumed
    }

    // Any other key dismisses ghost text (typing, arrow keys, etc.)
    // Exception: don't dismiss on Shift/Ctrl/Alt alone
    if (vk != VK_SHIFT && vk != VK_CONTROL && vk != VK_MENU) {
        dismissGhostText();
    }

    return false;  // Not consumed — pass to editor
}

// ============================================================================
// TOGGLE — enable/disable ghost text
// ============================================================================

void Win32IDE::toggleGhostText() {
    m_ghostTextEnabled = !m_ghostTextEnabled;
    if (!m_ghostTextEnabled) {
        dismissGhostText();
    }
    appendToOutput(std::string("[Ghost Text] ") +
                   (m_ghostTextEnabled ? "Enabled" : "Disabled"),
                   "General", OutputSeverity::Info);
}

// ============================================================================
// TRIM HELPER — truncates completion to reasonable size
// ============================================================================

std::string Win32IDE::trimGhostText(const std::string& raw) {
    std::string result;
    int lineCount = 0;
    int charCount = 0;

    for (char c : raw) {
        if (charCount >= GHOST_TEXT_MAX_CHARS) break;
        if (c == '\n') {
            lineCount++;
            if (lineCount >= GHOST_TEXT_MAX_LINES) break;
        }
        result += c;
        charCount++;
    }

    // Strip trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}
