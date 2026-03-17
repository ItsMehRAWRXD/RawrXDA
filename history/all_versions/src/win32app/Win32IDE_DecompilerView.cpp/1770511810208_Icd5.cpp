// Win32IDE_DecompilerView.cpp — Direct2D Decompiler View with Syntax Coloring,
// Synchronized Disassembly Selection, and SSA Variable Renaming
//
// Phase 18B: Moves the Reverse Engineering subsystem from FACADE → REAL by
// replacing plain-text EDIT output with a Direct2D-rendered split view:
//   1. Decompiler pane (left)  — syntax-colored pseudocode / C output
//   2. Disassembly pane (right) — address-tagged assembly listing
//   3. Bidirectional selection synchronization between panes
//   4. Right-click variable rename → propagates through SSA graph
//
// Reuses existing infrastructure:
//   - Win32IDE::tokenizeLine()   for C/C++ tokenization
//   - Win32IDE::getTokenColor()  for theme-aware color lookup
//   - IDETheme / m_currentTheme  for full color palette
//   - RawrReverseEngine.hpp      for SSA / DataFlowInfo structures

#include "Win32IDE.h"
#include "../reverse_engineering/RawrReverseEngine.hpp"
#include "../reverse_engineering/RawrCodex.hpp"

#include <d2d1.h>
#include <dwrite.h>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cassert>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

// ============================================================================
// DECOMPILER VIEW CONSTANTS
// ============================================================================
static const float DECOMP_LINE_HEIGHT    = 18.0f;
static const float DECOMP_GUTTER_WIDTH   = 60.0f;  // Line number / address gutter
static const float DECOMP_LEFT_PADDING   = 6.0f;
static const float DECOMP_FONT_SIZE      = 13.0f;
static const wchar_t* DECOMP_FONT_FACE   = L"Cascadia Code";
static const wchar_t* DECOMP_FONT_FALLBACK = L"Consolas";
static const int DECOMP_SPLITTER_WIDTH   = 4;
static const int DECOMP_MIN_PANE_WIDTH   = 200;

// Window class names
static const char* DECOMP_VIEW_CLASS  = "RawrXDDecompView";
static const char* DISASM_VIEW_CLASS  = "RawrXDDisasmView";
static const char* DECOMP_SPLIT_CLASS = "RawrXDDecompSplit";

// Custom messages
#define WM_DECOMP_SYNC_SELECT   (WM_APP + 200)
#define WM_DECOMP_RENAME_VAR    (WM_APP + 201)
#define WM_DECOMP_REFRESH       (WM_APP + 202)

// Context menu IDs
#define IDM_DECOMP_RENAME_VAR   8001
#define IDM_DECOMP_GOTO_DEF     8002
#define IDM_DECOMP_FIND_REFS    8003
#define IDM_DECOMP_COPY_LINE    8004
#define IDM_DECOMP_COPY_ALL     8005
#define IDM_DECOMP_GOTO_ADDR    8006

// ============================================================================
// HELPER: Convert COLORREF (GDI) → D2D1_COLOR_F
// ============================================================================
static D2D1_COLOR_F ColorRefToD2D(COLORREF cr, float alpha = 1.0f) {
    return D2D1::ColorF(
        GetRValue(cr) / 255.0f,
        GetGValue(cr) / 255.0f,
        GetBValue(cr) / 255.0f,
        alpha
    );
}

// ============================================================================
// DECOMPILER LINE — a single line of decompiled pseudocode with metadata
// ============================================================================
struct DecompLine {
    std::string text;               // The C-like source line
    uint64_t    address;            // Corresponding binary address (0 = none)
    int         asmStartIndex;      // Index into the disassembly lines array (-1 = none)
    int         asmEndIndex;        // Inclusive end index (-1 = none)
    int         indentLevel;        // For rendering indentation
    bool        isComment;          // Line is a comment / header
    bool        isLabel;            // Function label / block label

    // SSA variable references on this line
    struct VarRef {
        int         charStart;      // Character offset within `text`
        int         charLen;        // Length of the variable name
        std::string varName;        // e.g., "var_rax_1"
        int         ssaId;          // SSA variable ID (-1 if not SSA-tracked)
    };
    std::vector<VarRef> varRefs;
};

// ============================================================================
// DISASM LINE — a single disassembly instruction with metadata
// ============================================================================
struct DisasmLine {
    uint64_t    address;            // Instruction address
    std::string hexBytes;           // Raw byte hex string
    std::string mnemonic;           // Instruction mnemonic
    std::string operands;           // Operand string
    std::string comment;            // Optional comment
    int         decompLineIndex;    // Corresponding decompiler line (-1 = none)
    bool        isLabel;            // Is a label line (e.g., "loc_401000:")
    bool        isBlockBoundary;    // Start of a basic block
};

// ============================================================================
// DECOMPILER VIEW STATE — per-instance state for the split view
// ============================================================================
struct DecompViewState {
    // Direct2D resources
    ID2D1Factory*           pD2DFactory;
    ID2D1HwndRenderTarget*  pDecompRT;
    ID2D1HwndRenderTarget*  pDisasmRT;
    IDWriteFactory*         pDWriteFactory;
    IDWriteTextFormat*      pCodeFont;
    IDWriteTextFormat*      pGutterFont;

    // Window handles
    HWND hwndContainer;     // The split-view container
    HWND hwndDecompPane;    // Left pane (decompiler)
    HWND hwndDisasmPane;    // Right pane (disassembly)
    HWND hwndSplitter;      // Vertical splitter bar

    // Content
    std::vector<DecompLine> decompLines;
    std::vector<DisasmLine> disasmLines;

    // Address ↔ line bidirectional mapping
    std::map<uint64_t, int> addrToDecompLine;   // address → decompLine index
    std::map<uint64_t, int> addrToDisasmLine;   // address → disasmLine index
    std::map<int, int>      decompToDisasm;     // decompLine → first disasmLine
    std::map<int, int>      disasmToDecomp;     // disasmLine → decompLine

    // SSA variable rename mapping: original name → user-given name
    std::map<std::string, std::string> varRenameMap;
    // SSA ID → all DecompLine indices containing that variable
    std::map<int, std::vector<int>> ssaIdToDecompLines;

    // Selection state
    int selectedDecompLine;     // Currently selected decompiler line (-1 = none)
    int selectedDisasmLine;     // Currently selected disassembly line (-1 = none)
    int highlightDecompStart;   // Range highlight start (for sync)
    int highlightDecompEnd;
    int highlightDisasmStart;
    int highlightDisasmEnd;

    // Scroll state
    int decompScrollY;
    int disasmScrollY;
    int decompMaxScroll;
    int disasmMaxScroll;

    // Splitter state
    int splitterPos;            // X position of splitter
    bool splitterDragging;

    // Hover state (for variable highlighting)
    int hoverDecompLine;
    int hoverVarRefIndex;       // Index into DecompLine::varRefs

    // Parent Win32IDE pointer (for theme/tokenizer access)
    Win32IDE* pIDE;

    // Binary path for the loaded analysis
    std::string binaryPath;

    // Initialization flag
    bool initialized;
};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
static LRESULT CALLBACK DecompSplitProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK DecompPaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK DisasmPaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void DecompView_InitD2D(DecompViewState* state);
static void DecompView_ReleaseD2D(DecompViewState* state);
void DecompView_PaintDecomp(DecompViewState* state);
void DecompView_PaintDisasm(DecompViewState* state);
static void DecompView_SyncFromDecomp(DecompViewState* state, int decompLineIdx);
static void DecompView_SyncFromDisasm(DecompViewState* state, int disasmLineIdx);
static void DecompView_BuildAddressMap(DecompViewState* state);
static void DecompView_ShowRenameDialog(DecompViewState* state, int lineIdx, int varRefIdx);
static void DecompView_PropagateRename(DecompViewState* state, const std::string& oldName,
                                        const std::string& newName, int ssaId);
static std::string DecompView_ApplyRenames(DecompViewState* state, const std::string& text,
                                            const std::vector<DecompLine::VarRef>& refs);
static int DecompView_HitTestLine(int mouseY, int scrollY);
static int DecompView_HitTestVarRef(DecompViewState* state, int lineIdx, int mouseX);

// Global decompiler view state (one instance — modal analysis window)
static DecompViewState* g_decompState = nullptr;

// ============================================================================
// REGISTER WINDOW CLASSES
// ============================================================================
static bool s_decompClassesRegistered = false;

static void RegisterDecompClasses(HINSTANCE hInst) {
    if (s_decompClassesRegistered) return;

    WNDCLASSEXA wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // Split container class
    wc.lpfnWndProc = DecompSplitProc;
    wc.lpszClassName = DECOMP_SPLIT_CLASS;
    RegisterClassExA(&wc);

    // Decompiler pane class
    wc.lpfnWndProc = DecompPaneProc;
    wc.lpszClassName = DECOMP_VIEW_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    RegisterClassExA(&wc);

    // Disassembly pane class
    wc.lpfnWndProc = DisasmPaneProc;
    wc.lpszClassName = DISASM_VIEW_CLASS;
    RegisterClassExA(&wc);

    s_decompClassesRegistered = true;
}

// ============================================================================
// DIRECT2D INITIALIZATION & TEARDOWN
// ============================================================================
static void DecompView_InitD2D(DecompViewState* state) {
    if (!state) return;

    // Create D2D Factory
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &state->pD2DFactory);
    if (FAILED(hr)) {
        LOG_ERROR("DecompView: D2D1CreateFactory failed: 0x" + std::to_string(hr));
        return;
    }

    // Create DWrite Factory
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                              __uuidof(IDWriteFactory),
                              reinterpret_cast<IUnknown**>(&state->pDWriteFactory));
    if (FAILED(hr)) {
        LOG_ERROR("DecompView: DWriteCreateFactory failed");
        return;
    }

    // Create code text format
    hr = state->pDWriteFactory->CreateTextFormat(
        DECOMP_FONT_FACE,
        nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        DECOMP_FONT_SIZE,
        L"en-us",
        &state->pCodeFont
    );
    if (FAILED(hr)) {
        // Try fallback font
        state->pDWriteFactory->CreateTextFormat(
            DECOMP_FONT_FALLBACK,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            DECOMP_FONT_SIZE,
            L"en-us",
            &state->pCodeFont
        );
    }
    if (state->pCodeFont) {
        state->pCodeFont->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        state->pCodeFont->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }

    // Create gutter text format (slightly smaller)
    state->pDWriteFactory->CreateTextFormat(
        DECOMP_FONT_FACE,
        nullptr,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        DECOMP_FONT_SIZE - 1.0f,
        L"en-us",
        &state->pGutterFont
    );
    if (!state->pGutterFont) {
        state->pDWriteFactory->CreateTextFormat(
            DECOMP_FONT_FALLBACK,
            nullptr,
            DWRITE_FONT_WEIGHT_REGULAR,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            DECOMP_FONT_SIZE - 1.0f,
            L"en-us",
            &state->pGutterFont
        );
    }
    if (state->pGutterFont) {
        state->pGutterFont->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
        state->pGutterFont->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    }

    // Create render targets for each pane
    if (state->hwndDecompPane) {
        RECT rc;
        GetClientRect(state->hwndDecompPane, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
            state->hwndDecompPane, size);
        state->pD2DFactory->CreateHwndRenderTarget(rtProps, hwndProps, &state->pDecompRT);
    }

    if (state->hwndDisasmPane) {
        RECT rc;
        GetClientRect(state->hwndDisasmPane, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
            state->hwndDisasmPane, size);
        state->pD2DFactory->CreateHwndRenderTarget(rtProps, hwndProps, &state->pDisasmRT);
    }

    state->initialized = true;
}

static void DecompView_ReleaseD2D(DecompViewState* state) {
    if (!state) return;

    if (state->pDecompRT)     { state->pDecompRT->Release();     state->pDecompRT = nullptr; }
    if (state->pDisasmRT)     { state->pDisasmRT->Release();     state->pDisasmRT = nullptr; }
    if (state->pCodeFont)     { state->pCodeFont->Release();     state->pCodeFont = nullptr; }
    if (state->pGutterFont)   { state->pGutterFont->Release();   state->pGutterFont = nullptr; }
    if (state->pDWriteFactory){ state->pDWriteFactory->Release(); state->pDWriteFactory = nullptr; }
    if (state->pD2DFactory)   { state->pD2DFactory->Release();   state->pD2DFactory = nullptr; }

    state->initialized = false;
}

// ============================================================================
// RECREATE RENDER TARGETS (on resize)
// ============================================================================
static void DecompView_RecreateTargets(DecompViewState* state) {
    if (!state || !state->pD2DFactory) return;

    if (state->pDecompRT) {
        state->pDecompRT->Release();
        state->pDecompRT = nullptr;
    }
    if (state->pDisasmRT) {
        state->pDisasmRT->Release();
        state->pDisasmRT = nullptr;
    }

    if (state->hwndDecompPane) {
        RECT rc;
        GetClientRect(state->hwndDecompPane, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        state->pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(state->hwndDecompPane, size),
            &state->pDecompRT);
    }

    if (state->hwndDisasmPane) {
        RECT rc;
        GetClientRect(state->hwndDisasmPane, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        state->pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(state->hwndDisasmPane, size),
            &state->pDisasmRT);
    }
}

// ============================================================================
// BUILD ADDRESS ↔ LINE BIDIRECTIONAL MAP
// ============================================================================
static void DecompView_BuildAddressMap(DecompViewState* state) {
    if (!state) return;

    state->addrToDecompLine.clear();
    state->addrToDisasmLine.clear();
    state->decompToDisasm.clear();
    state->disasmToDecomp.clear();
    state->ssaIdToDecompLines.clear();

    // Map decompiler lines by address
    for (int i = 0; i < (int)state->decompLines.size(); ++i) {
        const auto& dl = state->decompLines[i];
        if (dl.address != 0) {
            state->addrToDecompLine[dl.address] = i;
        }
        // Map decomp line → disasm range
        if (dl.asmStartIndex >= 0) {
            state->decompToDisasm[i] = dl.asmStartIndex;
        }
        // Build SSA ID → decompiler lines index
        for (const auto& vr : dl.varRefs) {
            if (vr.ssaId >= 0) {
                state->ssaIdToDecompLines[vr.ssaId].push_back(i);
            }
        }
    }

    // Map disassembly lines by address
    for (int i = 0; i < (int)state->disasmLines.size(); ++i) {
        const auto& al = state->disasmLines[i];
        if (al.address != 0) {
            state->addrToDisasmLine[al.address] = i;
        }
        if (al.decompLineIndex >= 0) {
            state->disasmToDecomp[i] = al.decompLineIndex;
        }
    }
}

// ============================================================================
// HIT TESTING
// ============================================================================
static int DecompView_HitTestLine(int mouseY, int scrollY) {
    float y = (float)(mouseY + scrollY);
    int line = (int)(y / DECOMP_LINE_HEIGHT);
    return line;
}

static int DecompView_HitTestVarRef(DecompViewState* state, int lineIdx, int mouseX) {
    if (!state || lineIdx < 0 || lineIdx >= (int)state->decompLines.size()) return -1;

    const auto& dl = state->decompLines[lineIdx];
    // Approximate character width for monospace font (Direct2D exact measurement would be better)
    float charWidth = DECOMP_FONT_SIZE * 0.6f;
    float textStartX = DECOMP_GUTTER_WIDTH + DECOMP_LEFT_PADDING;
    float mx = (float)mouseX - textStartX;

    for (int v = 0; v < (int)dl.varRefs.size(); ++v) {
        float varStart = dl.varRefs[v].charStart * charWidth;
        float varEnd   = varStart + dl.varRefs[v].charLen * charWidth;
        if (mx >= varStart && mx <= varEnd) {
            return v;
        }
    }
    return -1;
}

// ============================================================================
// APPLY VARIABLE RENAMES to a line of text
// ============================================================================
static std::string DecompView_ApplyRenames(DecompViewState* state, const std::string& text,
                                            const std::vector<DecompLine::VarRef>& refs) {
    if (!state || refs.empty()) return text;

    // Build a list of replacements sorted by position (reverse to preserve offsets)
    struct Replacement {
        int start;
        int len;
        std::string newName;
    };
    std::vector<Replacement> replacements;

    for (const auto& vr : refs) {
        auto it = state->varRenameMap.find(vr.varName);
        if (it != state->varRenameMap.end()) {
            replacements.push_back({vr.charStart, vr.charLen, it->second});
        }
    }

    if (replacements.empty()) return text;

    // Sort by position descending so earlier replacements don't shift offsets
    std::sort(replacements.begin(), replacements.end(),
              [](const Replacement& a, const Replacement& b) { return a.start > b.start; });

    std::string result = text;
    for (const auto& rep : replacements) {
        if (rep.start >= 0 && rep.start + rep.len <= (int)result.size()) {
            result.replace(rep.start, rep.len, rep.newName);
        }
    }
    return result;
}

// ============================================================================
// SYNCHRONIZED SELECTION — Decompiler → Disassembly
// ============================================================================
static void DecompView_SyncFromDecomp(DecompViewState* state, int decompLineIdx) {
    if (!state || decompLineIdx < 0 || decompLineIdx >= (int)state->decompLines.size()) return;

    state->selectedDecompLine = decompLineIdx;
    const auto& dl = state->decompLines[decompLineIdx];

    // Find corresponding disassembly range
    state->highlightDisasmStart = -1;
    state->highlightDisasmEnd = -1;

    if (dl.asmStartIndex >= 0 && dl.asmEndIndex >= 0) {
        state->highlightDisasmStart = dl.asmStartIndex;
        state->highlightDisasmEnd = dl.asmEndIndex;
        state->selectedDisasmLine = dl.asmStartIndex;
    } else if (dl.address != 0) {
        // Try address-based lookup
        auto it = state->addrToDisasmLine.find(dl.address);
        if (it != state->addrToDisasmLine.end()) {
            state->highlightDisasmStart = it->second;
            state->highlightDisasmEnd = it->second;
            state->selectedDisasmLine = it->second;
        }
    }

    // Auto-scroll disasm pane to show the highlighted range
    if (state->highlightDisasmStart >= 0 && state->hwndDisasmPane) {
        RECT rc;
        GetClientRect(state->hwndDisasmPane, &rc);
        int visibleLines = (int)((rc.bottom - rc.top) / DECOMP_LINE_HEIGHT);
        int targetY = (int)(state->highlightDisasmStart * DECOMP_LINE_HEIGHT);

        if (targetY < state->disasmScrollY || targetY > state->disasmScrollY + (visibleLines - 2) * (int)DECOMP_LINE_HEIGHT) {
            state->disasmScrollY = std::max(0, targetY - (int)(2 * DECOMP_LINE_HEIGHT));
            state->disasmScrollY = std::min(state->disasmScrollY, state->disasmMaxScroll);
        }
        InvalidateRect(state->hwndDisasmPane, NULL, FALSE);
    }
}

// ============================================================================
// SYNCHRONIZED SELECTION — Disassembly → Decompiler
// ============================================================================
static void DecompView_SyncFromDisasm(DecompViewState* state, int disasmLineIdx) {
    if (!state || disasmLineIdx < 0 || disasmLineIdx >= (int)state->disasmLines.size()) return;

    state->selectedDisasmLine = disasmLineIdx;
    const auto& al = state->disasmLines[disasmLineIdx];

    // Find corresponding decompiler line
    state->highlightDecompStart = -1;
    state->highlightDecompEnd = -1;

    if (al.decompLineIndex >= 0) {
        state->highlightDecompStart = al.decompLineIndex;
        state->highlightDecompEnd = al.decompLineIndex;
        state->selectedDecompLine = al.decompLineIndex;
    } else if (al.address != 0) {
        // Try address-based lookup
        auto it = state->addrToDecompLine.find(al.address);
        if (it != state->addrToDecompLine.end()) {
            state->highlightDecompStart = it->second;
            state->highlightDecompEnd = it->second;
            state->selectedDecompLine = it->second;
        }
    }

    // Auto-scroll decompiler pane
    if (state->highlightDecompStart >= 0 && state->hwndDecompPane) {
        RECT rc;
        GetClientRect(state->hwndDecompPane, &rc);
        int visibleLines = (int)((rc.bottom - rc.top) / DECOMP_LINE_HEIGHT);
        int targetY = (int)(state->highlightDecompStart * DECOMP_LINE_HEIGHT);

        if (targetY < state->decompScrollY || targetY > state->decompScrollY + (visibleLines - 2) * (int)DECOMP_LINE_HEIGHT) {
            state->decompScrollY = std::max(0, targetY - (int)(2 * DECOMP_LINE_HEIGHT));
            state->decompScrollY = std::min(state->decompScrollY, state->decompMaxScroll);
        }
        InvalidateRect(state->hwndDecompPane, NULL, FALSE);
    }
}

// ============================================================================
// PAINT — DECOMPILER PANE (Direct2D with syntax coloring)
// ============================================================================
void DecompView_PaintDecomp(DecompViewState* state) {
    if (!state || !state->pDecompRT || !state->pCodeFont || !state->pIDE) return;

    ID2D1HwndRenderTarget* rt = state->pDecompRT;
    rt->BeginDraw();

    // Get theme colors from the IDE
    const IDETheme& theme = state->pIDE->m_currentTheme;
    D2D1_COLOR_F bgColor = ColorRefToD2D(theme.backgroundColor ? theme.backgroundColor : RGB(30, 30, 30));
    D2D1_COLOR_F gutterBg = ColorRefToD2D(theme.lineNumberBg ? theme.lineNumberBg : RGB(37, 37, 38));
    D2D1_COLOR_F gutterFg = ColorRefToD2D(theme.lineNumberColor ? theme.lineNumberColor : RGB(133, 133, 133));
    D2D1_COLOR_F selectionBg = ColorRefToD2D(theme.selectionColor ? theme.selectionColor : RGB(38, 79, 120));
    D2D1_COLOR_F currentLineBg = ColorRefToD2D(theme.currentLineBg ? theme.currentLineBg : RGB(40, 44, 52));
    D2D1_COLOR_F highlightBg = ColorRefToD2D(theme.accentColor ? theme.accentColor : RGB(255, 215, 0), 0.15f);

    rt->Clear(bgColor);

    D2D1_SIZE_F rtSize = rt->GetSize();

    // Create brushes
    ID2D1SolidColorBrush* pBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &pBrush);
    if (!pBrush) { rt->EndDraw(); return; }

    // Draw gutter background
    pBrush->SetColor(gutterBg);
    rt->FillRectangle(D2D1::RectF(0, 0, DECOMP_GUTTER_WIDTH, rtSize.height), pBrush);

    // Calculate visible line range
    int firstVisible = (int)(state->decompScrollY / DECOMP_LINE_HEIGHT);
    int visibleCount = (int)(rtSize.height / DECOMP_LINE_HEIGHT) + 2;
    int totalLines = (int)state->decompLines.size();

    // Update max scroll
    state->decompMaxScroll = std::max(0, (int)(totalLines * DECOMP_LINE_HEIGHT - rtSize.height));

    for (int i = firstVisible; i < std::min(firstVisible + visibleCount, totalLines); ++i) {
        float y = i * DECOMP_LINE_HEIGHT - (float)state->decompScrollY;
        const auto& dl = state->decompLines[i];

        // Determine if this line is highlighted (selected or sync-highlighted)
        bool isSelected = (i == state->selectedDecompLine);
        bool isHighlighted = (i >= state->highlightDecompStart && i <= state->highlightDecompEnd &&
                              state->highlightDecompStart >= 0);

        // Draw line background
        if (isSelected) {
            pBrush->SetColor(currentLineBg);
            rt->FillRectangle(D2D1::RectF(DECOMP_GUTTER_WIDTH, y, rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
        } else if (isHighlighted) {
            pBrush->SetColor(highlightBg);
            rt->FillRectangle(D2D1::RectF(DECOMP_GUTTER_WIDTH, y, rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
        }

        // Draw line number in gutter
        {
            wchar_t lineNum[16];
            swprintf_s(lineNum, L"%d", i + 1);
            pBrush->SetColor(gutterFg);
            rt->DrawText(lineNum, (UINT32)wcslen(lineNum), state->pGutterFont,
                         D2D1::RectF(0, y, DECOMP_GUTTER_WIDTH - 4, y + DECOMP_LINE_HEIGHT), pBrush);
        }

        // Apply variable renames to the display text
        std::string displayText = DecompView_ApplyRenames(state, dl.text, dl.varRefs);

        // Tokenize the line using the IDE's syntax coloring engine (C++ language)
        auto tokens = state->pIDE->tokenizeLine(displayText, 0, Win32IDE::SyntaxLanguage::Cpp);

        if (tokens.empty()) {
            // No tokens — render as default text
            std::wstring wtext(displayText.begin(), displayText.end());
            COLORREF defaultColor = state->pIDE->getTokenColor(Win32IDE::TokenType::Default);
            pBrush->SetColor(ColorRefToD2D(defaultColor));
            rt->DrawText(wtext.c_str(), (UINT32)wtext.size(), state->pCodeFont,
                         D2D1::RectF(DECOMP_GUTTER_WIDTH + DECOMP_LEFT_PADDING, y,
                                     rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
        } else {
            // Render each token with its syntax color
            // Approximate character width for positioning (monospace font)
            float charWidth = DECOMP_FONT_SIZE * 0.6f;

            for (const auto& tok : tokens) {
                int tokStart = tok.start;   // Offset from 0 (we passed lineStartOffset=0)
                int tokLen = tok.length;
                if (tokStart < 0 || tokLen <= 0 || tokStart >= (int)displayText.size()) continue;
                int endClamp = std::min(tokStart + tokLen, (int)displayText.size());

                std::string tokText = displayText.substr(tokStart, endClamp - tokStart);
                std::wstring wTokText(tokText.begin(), tokText.end());

                COLORREF tokColor = state->pIDE->getTokenColor(tok.type);
                pBrush->SetColor(ColorRefToD2D(tokColor));

                float xPos = DECOMP_GUTTER_WIDTH + DECOMP_LEFT_PADDING + tokStart * charWidth;
                rt->DrawText(wTokText.c_str(), (UINT32)wTokText.size(), state->pCodeFont,
                             D2D1::RectF(xPos, y, xPos + tokLen * charWidth + 20, y + DECOMP_LINE_HEIGHT),
                             pBrush);
            }
        }

        // Draw underline for hovered variable reference
        if (i == state->hoverDecompLine && state->hoverVarRefIndex >= 0 &&
            state->hoverVarRefIndex < (int)dl.varRefs.size()) {
            const auto& vr = dl.varRefs[state->hoverVarRefIndex];
            float charWidth = DECOMP_FONT_SIZE * 0.6f;
            float xStart = DECOMP_GUTTER_WIDTH + DECOMP_LEFT_PADDING + vr.charStart * charWidth;
            float xEnd = xStart + vr.charLen * charWidth;

            pBrush->SetColor(ColorRefToD2D(theme.accentColor ? theme.accentColor : RGB(255, 215, 0)));
            rt->DrawLine(D2D1::Point2F(xStart, y + DECOMP_LINE_HEIGHT - 2),
                         D2D1::Point2F(xEnd, y + DECOMP_LINE_HEIGHT - 2), pBrush, 1.0f);
        }
    }

    pBrush->Release();
    rt->EndDraw();
}

// ============================================================================
// PAINT — DISASSEMBLY PANE (Direct2D with ASM coloring)
// ============================================================================
void DecompView_PaintDisasm(DecompViewState* state) {
    if (!state || !state->pDisasmRT || !state->pCodeFont || !state->pIDE) return;

    ID2D1HwndRenderTarget* rt = state->pDisasmRT;
    rt->BeginDraw();

    const IDETheme& theme = state->pIDE->m_currentTheme;
    D2D1_COLOR_F bgColor = ColorRefToD2D(theme.backgroundColor ? theme.backgroundColor : RGB(30, 30, 30));
    D2D1_COLOR_F gutterBg = ColorRefToD2D(theme.lineNumberBg ? theme.lineNumberBg : RGB(37, 37, 38));
    D2D1_COLOR_F gutterFg = ColorRefToD2D(theme.lineNumberColor ? theme.lineNumberColor : RGB(133, 133, 133));
    D2D1_COLOR_F selectionBg = ColorRefToD2D(theme.selectionColor ? theme.selectionColor : RGB(38, 79, 120));
    D2D1_COLOR_F currentLineBg = ColorRefToD2D(theme.currentLineBg ? theme.currentLineBg : RGB(40, 44, 52));
    D2D1_COLOR_F highlightBg = ColorRefToD2D(theme.accentColor ? theme.accentColor : RGB(255, 215, 0), 0.15f);

    // ASM-specific colors from theme
    D2D1_COLOR_F addrColor = ColorRefToD2D(theme.numberColor ? theme.numberColor : RGB(181, 206, 168));
    D2D1_COLOR_F mnemonicColor = ColorRefToD2D(theme.keywordColor ? theme.keywordColor : RGB(86, 156, 214));
    D2D1_COLOR_F operandColor = ColorRefToD2D(theme.textColor ? theme.textColor : RGB(212, 212, 212));
    D2D1_COLOR_F commentColor = ColorRefToD2D(theme.commentColor ? theme.commentColor : RGB(106, 153, 85));
    D2D1_COLOR_F hexByteColor = ColorRefToD2D(theme.stringColor ? theme.stringColor : RGB(206, 145, 120));
    D2D1_COLOR_F labelColor = ColorRefToD2D(theme.functionColor ? theme.functionColor : RGB(220, 220, 170));
    D2D1_COLOR_F blockColor = ColorRefToD2D(theme.preprocessorColor ? theme.preprocessorColor : RGB(155, 89, 182));

    rt->Clear(bgColor);

    D2D1_SIZE_F rtSize = rt->GetSize();

    ID2D1SolidColorBrush* pBrush = nullptr;
    rt->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1), &pBrush);
    if (!pBrush) { rt->EndDraw(); return; }

    // Draw gutter background
    pBrush->SetColor(gutterBg);
    rt->FillRectangle(D2D1::RectF(0, 0, DECOMP_GUTTER_WIDTH, rtSize.height), pBrush);

    int firstVisible = (int)(state->disasmScrollY / DECOMP_LINE_HEIGHT);
    int visibleCount = (int)(rtSize.height / DECOMP_LINE_HEIGHT) + 2;
    int totalLines = (int)state->disasmLines.size();

    state->disasmMaxScroll = std::max(0, (int)(totalLines * DECOMP_LINE_HEIGHT - rtSize.height));

    for (int i = firstVisible; i < std::min(firstVisible + visibleCount, totalLines); ++i) {
        float y = i * DECOMP_LINE_HEIGHT - (float)state->disasmScrollY;
        const auto& al = state->disasmLines[i];

        bool isSelected = (i == state->selectedDisasmLine);
        bool isHighlighted = (i >= state->highlightDisasmStart && i <= state->highlightDisasmEnd &&
                              state->highlightDisasmStart >= 0);

        // Line background
        if (isSelected) {
            pBrush->SetColor(currentLineBg);
            rt->FillRectangle(D2D1::RectF(DECOMP_GUTTER_WIDTH, y, rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
        } else if (isHighlighted) {
            pBrush->SetColor(highlightBg);
            rt->FillRectangle(D2D1::RectF(DECOMP_GUTTER_WIDTH, y, rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
        }

        // Block boundary marker
        if (al.isBlockBoundary && !al.isLabel) {
            pBrush->SetColor(blockColor);
            rt->DrawLine(D2D1::Point2F(DECOMP_GUTTER_WIDTH, y),
                         D2D1::Point2F(rtSize.width, y), pBrush, 0.5f);
        }

        float charWidth = DECOMP_FONT_SIZE * 0.6f;
        float x = DECOMP_GUTTER_WIDTH + DECOMP_LEFT_PADDING;

        if (al.isLabel) {
            // Label line: render as function/block name
            std::wstring wlabel(al.mnemonic.begin(), al.mnemonic.end());
            if (!al.operands.empty()) {
                std::wstring wop(al.operands.begin(), al.operands.end());
                wlabel += L" " + wop;
            }
            pBrush->SetColor(labelColor);
            rt->DrawText(wlabel.c_str(), (UINT32)wlabel.size(), state->pCodeFont,
                         D2D1::RectF(x, y, rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
        } else {
            // Address in gutter
            if (al.address != 0) {
                wchar_t addrBuf[32];
                swprintf_s(addrBuf, L"%08llX", (unsigned long long)al.address);
                pBrush->SetColor(addrColor);
                rt->DrawText(addrBuf, (UINT32)wcslen(addrBuf), state->pGutterFont,
                             D2D1::RectF(0, y, DECOMP_GUTTER_WIDTH - 4, y + DECOMP_LINE_HEIGHT), pBrush);
            }

            // Hex bytes
            if (!al.hexBytes.empty()) {
                std::wstring whex(al.hexBytes.begin(), al.hexBytes.end());
                pBrush->SetColor(hexByteColor);
                rt->DrawText(whex.c_str(), (UINT32)whex.size(), state->pCodeFont,
                             D2D1::RectF(x, y, x + 20 * charWidth, y + DECOMP_LINE_HEIGHT), pBrush);
                x += 20 * charWidth;
            } else {
                x += 2 * charWidth; // Small indent when no hex bytes
            }

            // Mnemonic
            if (!al.mnemonic.empty()) {
                std::wstring wmnem(al.mnemonic.begin(), al.mnemonic.end());
                pBrush->SetColor(mnemonicColor);
                rt->DrawText(wmnem.c_str(), (UINT32)wmnem.size(), state->pCodeFont,
                             D2D1::RectF(x, y, x + 10 * charWidth, y + DECOMP_LINE_HEIGHT), pBrush);
                x += 10 * charWidth;
            }

            // Operands
            if (!al.operands.empty()) {
                std::wstring wops(al.operands.begin(), al.operands.end());
                pBrush->SetColor(operandColor);
                rt->DrawText(wops.c_str(), (UINT32)wops.size(), state->pCodeFont,
                             D2D1::RectF(x, y, rtSize.width - 20, y + DECOMP_LINE_HEIGHT), pBrush);
            }

            // Comment
            if (!al.comment.empty()) {
                std::wstring wcomment(al.comment.begin(), al.comment.end());
                std::wstring full = L"; " + wcomment;
                pBrush->SetColor(commentColor);
                float commentX = rtSize.width - (float)(full.size() + 2) * charWidth;
                commentX = std::max(commentX, x + 30 * charWidth);
                rt->DrawText(full.c_str(), (UINT32)full.size(), state->pCodeFont,
                             D2D1::RectF(commentX, y, rtSize.width, y + DECOMP_LINE_HEIGHT), pBrush);
            }
        }
    }

    pBrush->Release();
    rt->EndDraw();
}

// ============================================================================
// VARIABLE RENAME DIALOG
// ============================================================================
struct RenameDialogData {
    std::string oldName;
    std::string newName;
    int ssaId;
    bool confirmed;
};

static INT_PTR CALLBACK RenameDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static RenameDialogData* pData = nullptr;

    switch (msg) {
    case WM_INITDIALOG: {
        pData = reinterpret_cast<RenameDialogData*>(lParam);
        SetWindowTextA(hwnd, "Rename Variable");

        // Center the dialog
        RECT rcOwner, rcDlg;
        GetWindowRect(GetParent(hwnd), &rcOwner);
        GetWindowRect(hwnd, &rcDlg);
        int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        // Set initial text
        HWND hwndEdit = GetDlgItem(hwnd, 101);
        if (hwndEdit && pData) {
            SetWindowTextA(hwndEdit, pData->oldName.c_str());
            SendMessageA(hwndEdit, EM_SETSEL, 0, -1); // Select all
        }

        HWND hwndLabel = GetDlgItem(hwnd, 100);
        if (hwndLabel && pData) {
            std::string label = "Rename '" + pData->oldName + "' to:";
            if (pData->ssaId >= 0) {
                label += " (SSA v" + std::to_string(pData->ssaId) + ")";
            }
            SetWindowTextA(hwndLabel, label.c_str());
        }
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            HWND hwndEdit = GetDlgItem(hwnd, 101);
            char buf[256] = {};
            GetWindowTextA(hwndEdit, buf, sizeof(buf));
            if (pData) {
                pData->newName = buf;
                pData->confirmed = true;
            }
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            if (pData) pData->confirmed = false;
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    case WM_CLOSE:
        if (pData) pData->confirmed = false;
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

// Create the rename dialog template in memory (no .rc file needed)
static INT_PTR ShowRenameDialogModal(HWND hwndParent, RenameDialogData* pData) {
    // Allocate a dialog template in memory
    // Layout: Label (ID=100), Edit (ID=101), OK (IDOK), Cancel (IDCANCEL)
    struct {
        DLGTEMPLATE dlg;
        WORD menu; WORD wndClass; WORD title;
    } dlgTemplate = {};

    dlgTemplate.dlg.style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
    dlgTemplate.dlg.dwExtendedStyle = 0;
    dlgTemplate.dlg.cdit = 0; // We'll create controls manually
    dlgTemplate.dlg.x = 0; dlgTemplate.dlg.y = 0;
    dlgTemplate.dlg.cx = 250; dlgTemplate.dlg.cy = 80;

    HWND hwndDlg = CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
                                    "#32770", "Rename Variable",
                                    WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 380, 160,
                                    hwndParent, NULL, GetModuleHandle(NULL), NULL);
    if (!hwndDlg) return IDCANCEL;

    // Center
    RECT rcOwner, rcDlg;
    GetWindowRect(hwndParent, &rcOwner);
    GetWindowRect(hwndDlg, &rcDlg);
    int x = rcOwner.left + ((rcOwner.right - rcOwner.left) - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcOwner.top + ((rcOwner.bottom - rcOwner.top) - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hwndDlg, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE);

    // Create label
    std::string labelText = "Rename '" + pData->oldName + "' to:";
    if (pData->ssaId >= 0) {
        labelText += "  (SSA v" + std::to_string(pData->ssaId) + ")";
    }
    HWND hwndLabel = CreateWindowExA(0, "STATIC", labelText.c_str(),
                                      WS_CHILD | WS_VISIBLE,
                                      10, 10, 350, 20, hwndDlg, (HMENU)100, GetModuleHandle(NULL), NULL);

    // Create edit box
    HWND hwndEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", pData->oldName.c_str(),
                                     WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                                     10, 40, 350, 24, hwndDlg, (HMENU)101, GetModuleHandle(NULL), NULL);
    SendMessageA(hwndEdit, EM_SETSEL, 0, -1);
    SetFocus(hwndEdit);

    // Create OK button
    HWND hwndOK = CreateWindowExA(0, "BUTTON", "Rename",
                                   WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
                                   190, 80, 80, 28, hwndDlg, (HMENU)IDOK, GetModuleHandle(NULL), NULL);

    // Create Cancel button
    HWND hwndCancel = CreateWindowExA(0, "BUTTON", "Cancel",
                                       WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                                       280, 80, 80, 28, hwndDlg, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);

    // Custom font
    HFONT hFont = CreateFontA(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
    if (hFont) {
        SendMessage(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hwndEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hwndOK, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hwndCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    // Modal message loop
    pData->confirmed = false;
    EnableWindow(hwndParent, FALSE);

    MSG msg;
    bool running = true;
    while (running && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.hwnd == hwndDlg || IsChild(hwndDlg, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
                char buf[256] = {};
                GetWindowTextA(hwndEdit, buf, sizeof(buf));
                pData->newName = buf;
                pData->confirmed = true;
                running = false;
                break;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                running = false;
                break;
            }
            if (msg.message == WM_COMMAND) {
                if (LOWORD(msg.wParam) == IDOK) {
                    char buf[256] = {};
                    GetWindowTextA(hwndEdit, buf, sizeof(buf));
                    pData->newName = buf;
                    pData->confirmed = true;
                    running = false;
                    break;
                }
                if (LOWORD(msg.wParam) == IDCANCEL) {
                    running = false;
                    break;
                }
            }
        }
        if (!IsDialogMessage(hwndDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
    if (hFont) DeleteObject(hFont);
    DestroyWindow(hwndDlg);

    return pData->confirmed ? IDOK : IDCANCEL;
}

static void DecompView_ShowRenameDialog(DecompViewState* state, int lineIdx, int varRefIdx) {
    if (!state || lineIdx < 0 || lineIdx >= (int)state->decompLines.size()) return;
    const auto& dl = state->decompLines[lineIdx];
    if (varRefIdx < 0 || varRefIdx >= (int)dl.varRefs.size()) return;

    const auto& vr = dl.varRefs[varRefIdx];

    // Check if already renamed
    std::string displayName = vr.varName;
    auto it = state->varRenameMap.find(vr.varName);
    if (it != state->varRenameMap.end()) {
        displayName = it->second;
    }

    RenameDialogData data;
    data.oldName = displayName;
    data.ssaId = vr.ssaId;
    data.confirmed = false;

    INT_PTR result = ShowRenameDialogModal(state->hwndContainer, &data);

    if (result == IDOK && data.confirmed && !data.newName.empty() && data.newName != data.oldName) {
        DecompView_PropagateRename(state, vr.varName, data.newName, vr.ssaId);
    }
}

// ============================================================================
// PROPAGATE RENAME THROUGH SSA GRAPH
// ============================================================================
static void DecompView_PropagateRename(DecompViewState* state, const std::string& oldName,
                                        const std::string& newName, int ssaId) {
    if (!state) return;

    // Store the rename mapping (original SSA name → user name)
    state->varRenameMap[oldName] = newName;

    // If we have an SSA ID, also rename all other occurrences of the same SSA variable
    // This is the key feature: renaming one instance propagates through the entire SSA graph
    if (ssaId >= 0) {
        auto it = state->ssaIdToDecompLines.find(ssaId);
        if (it != state->ssaIdToDecompLines.end()) {
            for (int lineIdx : it->second) {
                if (lineIdx >= 0 && lineIdx < (int)state->decompLines.size()) {
                    for (auto& vr : state->decompLines[lineIdx].varRefs) {
                        if (vr.ssaId == ssaId && vr.varName != oldName) {
                            // Also map any alias of this SSA variable
                            state->varRenameMap[vr.varName] = newName;
                        }
                    }
                }
            }
        }
    }

    // Also propagate by name: find all lines containing the same variable name
    for (auto& dl : state->decompLines) {
        for (auto& vr : dl.varRefs) {
            if (vr.varName == oldName) {
                // Already mapped via varRenameMap, no structural change needed
            }
        }
    }

    // Redraw both panes to reflect the rename
    if (state->hwndDecompPane) InvalidateRect(state->hwndDecompPane, NULL, FALSE);
    if (state->hwndDisasmPane) InvalidateRect(state->hwndDisasmPane, NULL, FALSE);

    LOG_INFO("DecompView: Renamed '" + oldName + "' → '" + newName + "' (SSA v" + std::to_string(ssaId) + ")");
}

// ============================================================================
// DECOMPILER PANE WINDOW PROC
// ============================================================================
static LRESULT CALLBACK DecompPaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DecompViewState* state = g_decompState;

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (state) DecompView_PaintDecomp(state);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        if (state) DecompView_RecreateTargets(state);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!state) break;
        int mouseY = HIWORD(lParam);
        int lineIdx = DecompView_HitTestLine(mouseY, state->decompScrollY);
        if (lineIdx >= 0 && lineIdx < (int)state->decompLines.size()) {
            state->selectedDecompLine = lineIdx;
            DecompView_SyncFromDecomp(state, lineIdx);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        SetFocus(hwnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!state) break;
        int mouseY = HIWORD(lParam);
        int mouseX = LOWORD(lParam);
        int lineIdx = DecompView_HitTestLine(mouseY, state->decompScrollY);
        int varIdx = DecompView_HitTestVarRef(state, lineIdx, mouseX);

        bool needsRedraw = false;
        if (lineIdx != state->hoverDecompLine || varIdx != state->hoverVarRefIndex) {
            state->hoverDecompLine = lineIdx;
            state->hoverVarRefIndex = varIdx;
            needsRedraw = true;
        }

        // Change cursor to hand when hovering over a variable
        if (varIdx >= 0) {
            SetCursor(LoadCursor(NULL, IDC_HAND));
        } else {
            SetCursor(LoadCursor(NULL, IDC_IBEAM));
        }

        if (needsRedraw) InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_RBUTTONDOWN: {
        if (!state) break;
        int mouseY = HIWORD(lParam);
        int mouseX = LOWORD(lParam);
        int lineIdx = DecompView_HitTestLine(mouseY, state->decompScrollY);
        int varIdx = DecompView_HitTestVarRef(state, lineIdx, mouseX);

        // Select the line first
        if (lineIdx >= 0 && lineIdx < (int)state->decompLines.size()) {
            state->selectedDecompLine = lineIdx;
            DecompView_SyncFromDecomp(state, lineIdx);
            InvalidateRect(hwnd, NULL, FALSE);
        }

        // Show context menu
        HMENU hMenu = CreatePopupMenu();
        if (varIdx >= 0) {
            const auto& vr = state->decompLines[lineIdx].varRefs[varIdx];
            std::string renameLabel = "Rename '" + vr.varName + "'...";
            auto mapIt = state->varRenameMap.find(vr.varName);
            if (mapIt != state->varRenameMap.end()) {
                renameLabel = "Rename '" + mapIt->second + "'...";
            }
            AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_RENAME_VAR, renameLabel.c_str());
            AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_GOTO_DEF, "Go to Definition");
            AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_FIND_REFS, "Find All References");
            AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
        }
        AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_COPY_LINE, "Copy Line");
        AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_COPY_ALL, "Copy All");

        if (lineIdx >= 0 && lineIdx < (int)state->decompLines.size() &&
            state->decompLines[lineIdx].address != 0) {
            AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
            char addrLabel[64];
            snprintf(addrLabel, sizeof(addrLabel), "Go to Address 0x%llX",
                     (unsigned long long)state->decompLines[lineIdx].address);
            AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_GOTO_ADDR, addrLabel);
        }

        POINT pt;
        GetCursorPos(&pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        switch (cmd) {
        case IDM_DECOMP_RENAME_VAR:
            if (varIdx >= 0) {
                DecompView_ShowRenameDialog(state, lineIdx, varIdx);
            }
            break;
        case IDM_DECOMP_COPY_LINE:
            if (lineIdx >= 0 && lineIdx < (int)state->decompLines.size()) {
                std::string text = state->decompLines[lineIdx].text;
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
                    if (hGlobal) {
                        char* pDst = (char*)GlobalLock(hGlobal);
                        memcpy(pDst, text.c_str(), text.size() + 1);
                        GlobalUnlock(hGlobal);
                        SetClipboardData(CF_TEXT, hGlobal);
                    }
                    CloseClipboard();
                }
            }
            break;
        case IDM_DECOMP_COPY_ALL: {
            std::string all;
            for (const auto& dl : state->decompLines) {
                all += dl.text + "\r\n";
            }
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, all.size() + 1);
                if (hGlobal) {
                    char* pDst = (char*)GlobalLock(hGlobal);
                    memcpy(pDst, all.c_str(), all.size() + 1);
                    GlobalUnlock(hGlobal);
                    SetClipboardData(CF_TEXT, hGlobal);
                }
                CloseClipboard();
            }
            break;
        }
        case IDM_DECOMP_GOTO_ADDR:
            // Sync to disassembly at this address
            if (lineIdx >= 0 && lineIdx < (int)state->decompLines.size()) {
                uint64_t addr = state->decompLines[lineIdx].address;
                auto it = state->addrToDisasmLine.find(addr);
                if (it != state->addrToDisasmLine.end()) {
                    DecompView_SyncFromDisasm(state, it->second);
                }
            }
            break;
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (!state) break;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        state->decompScrollY -= (int)(delta / 120.0f * DECOMP_LINE_HEIGHT * 3);
        state->decompScrollY = std::max(0, std::min(state->decompScrollY, state->decompMaxScroll));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_KEYDOWN: {
        if (!state) break;
        switch (wParam) {
        case VK_UP:
            if (state->selectedDecompLine > 0) {
                state->selectedDecompLine--;
                DecompView_SyncFromDecomp(state, state->selectedDecompLine);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        case VK_DOWN:
            if (state->selectedDecompLine < (int)state->decompLines.size() - 1) {
                state->selectedDecompLine++;
                DecompView_SyncFromDecomp(state, state->selectedDecompLine);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        case VK_F2: {
            // F2 = Rename variable under cursor (if any)
            if (state->hoverDecompLine >= 0 && state->hoverVarRefIndex >= 0) {
                DecompView_ShowRenameDialog(state, state->hoverDecompLine, state->hoverVarRefIndex);
            }
            return 0;
        }
        }
        break;
    }

    case WM_ERASEBKGND:
        return 1; // Prevent flicker — D2D handles background
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// DISASSEMBLY PANE WINDOW PROC
// ============================================================================
static LRESULT CALLBACK DisasmPaneProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DecompViewState* state = g_decompState;

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        if (state) DecompView_PaintDisasm(state);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        if (state) DecompView_RecreateTargets(state);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        if (!state) break;
        int mouseY = HIWORD(lParam);
        int lineIdx = DecompView_HitTestLine(mouseY, state->disasmScrollY);
        if (lineIdx >= 0 && lineIdx < (int)state->disasmLines.size()) {
            state->selectedDisasmLine = lineIdx;
            DecompView_SyncFromDisasm(state, lineIdx);
            InvalidateRect(hwnd, NULL, FALSE);
            if (state->hwndDecompPane) InvalidateRect(state->hwndDecompPane, NULL, FALSE);
        }
        SetFocus(hwnd);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (!state) break;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        state->disasmScrollY -= (int)(delta / 120.0f * DECOMP_LINE_HEIGHT * 3);
        state->disasmScrollY = std::max(0, std::min(state->disasmScrollY, state->disasmMaxScroll));
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_KEYDOWN: {
        if (!state) break;
        switch (wParam) {
        case VK_UP:
            if (state->selectedDisasmLine > 0) {
                state->selectedDisasmLine--;
                DecompView_SyncFromDisasm(state, state->selectedDisasmLine);
                InvalidateRect(hwnd, NULL, FALSE);
                if (state->hwndDecompPane) InvalidateRect(state->hwndDecompPane, NULL, FALSE);
            }
            return 0;
        case VK_DOWN:
            if (state->selectedDisasmLine < (int)state->disasmLines.size() - 1) {
                state->selectedDisasmLine++;
                DecompView_SyncFromDisasm(state, state->selectedDisasmLine);
                InvalidateRect(hwnd, NULL, FALSE);
                if (state->hwndDecompPane) InvalidateRect(state->hwndDecompPane, NULL, FALSE);
            }
            return 0;
        }
        break;
    }

    case WM_RBUTTONDOWN: {
        if (!state) break;
        HMENU hMenu = CreatePopupMenu();
        AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_COPY_LINE, "Copy Instruction");
        AppendMenuA(hMenu, MF_STRING, IDM_DECOMP_COPY_ALL, "Copy All Disassembly");

        POINT pt;
        GetCursorPos(&pt);
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        DestroyMenu(hMenu);

        if (cmd == IDM_DECOMP_COPY_LINE) {
            int lineIdx = DecompView_HitTestLine(HIWORD(lParam), state->disasmScrollY);
            if (lineIdx >= 0 && lineIdx < (int)state->disasmLines.size()) {
                const auto& al = state->disasmLines[lineIdx];
                std::string text = al.mnemonic + " " + al.operands;
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
                    if (hGlobal) {
                        char* pDst = (char*)GlobalLock(hGlobal);
                        memcpy(pDst, text.c_str(), text.size() + 1);
                        GlobalUnlock(hGlobal);
                        SetClipboardData(CF_TEXT, hGlobal);
                    }
                    CloseClipboard();
                }
            }
        } else if (cmd == IDM_DECOMP_COPY_ALL) {
            std::string all;
            for (const auto& al : state->disasmLines) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%08llX  ", (unsigned long long)al.address);
                all += buf;
                all += al.mnemonic + " " + al.operands + "\r\n";
            }
            if (OpenClipboard(hwnd)) {
                EmptyClipboard();
                HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, all.size() + 1);
                if (hGlobal) {
                    char* pDst = (char*)GlobalLock(hGlobal);
                    memcpy(pDst, all.c_str(), all.size() + 1);
                    GlobalUnlock(hGlobal);
                    SetClipboardData(CF_TEXT, hGlobal);
                }
                CloseClipboard();
            }
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// SPLIT CONTAINER WINDOW PROC (manages layout of decomp + splitter + disasm)
// ============================================================================
static LRESULT CALLBACK DecompSplitProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DecompViewState* state = g_decompState;

    switch (msg) {
    case WM_CREATE: {
        return 0;
    }

    case WM_SIZE: {
        if (!state) break;
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);

        // Ensure splitter is within bounds
        if (state->splitterPos < DECOMP_MIN_PANE_WIDTH)
            state->splitterPos = DECOMP_MIN_PANE_WIDTH;
        if (state->splitterPos > width - DECOMP_MIN_PANE_WIDTH - DECOMP_SPLITTER_WIDTH)
            state->splitterPos = width - DECOMP_MIN_PANE_WIDTH - DECOMP_SPLITTER_WIDTH;

        // Layout: [DecompPane][Splitter][DisasmPane]
        int decompW = state->splitterPos;
        int disasmX = state->splitterPos + DECOMP_SPLITTER_WIDTH;
        int disasmW = width - disasmX;

        if (state->hwndDecompPane)
            MoveWindow(state->hwndDecompPane, 0, 0, decompW, height, TRUE);
        if (state->hwndSplitter)
            MoveWindow(state->hwndSplitter, state->splitterPos, 0, DECOMP_SPLITTER_WIDTH, height, TRUE);
        if (state->hwndDisasmPane)
            MoveWindow(state->hwndDisasmPane, disasmX, 0, disasmW, height, TRUE);

        return 0;
    }

    case WM_DESTROY: {
        if (state) {
            DecompView_ReleaseD2D(state);
            delete state;
            g_decompState = nullptr;
        }
        return 0;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// SPLITTER BAR — CUSTOM SUBCLASS for drag-to-resize
// ============================================================================
LRESULT CALLBACK SplitterBarProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DecompViewState* state = g_decompState;

    switch (msg) {
    case WM_LBUTTONDOWN:
        if (state) {
            state->splitterDragging = true;
            SetCapture(hwnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (state && state->splitterDragging) {
            state->splitterDragging = false;
            ReleaseCapture();
        }
        return 0;

    case WM_MOUSEMOVE:
        SetCursor(LoadCursor(NULL, IDC_SIZEWE));
        if (state && state->splitterDragging) {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(GetParent(hwnd), &pt);

            RECT rcParent;
            GetClientRect(GetParent(hwnd), &rcParent);

            int newPos = pt.x;
            newPos = std::max(newPos, DECOMP_MIN_PANE_WIDTH);
            newPos = std::min(newPos, (int)(rcParent.right - DECOMP_MIN_PANE_WIDTH - DECOMP_SPLITTER_WIDTH));
            state->splitterPos = newPos;

            // Trigger relayout
            RECT rc;
            GetClientRect(GetParent(hwnd), &rc);
            SendMessage(GetParent(hwnd), WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom));

            DecompView_RecreateTargets(state);
            if (state->hwndDecompPane) InvalidateRect(state->hwndDecompPane, NULL, FALSE);
            if (state->hwndDisasmPane) InvalidateRect(state->hwndDisasmPane, NULL, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        COLORREF splitterColor = RGB(60, 60, 60);
        if (state && state->pIDE) {
            splitterColor = state->pIDE->m_currentTheme.panelBorder ?
                            state->pIDE->m_currentTheme.panelBorder : RGB(60, 60, 60);
        }
        HBRUSH hBrush = CreateSolidBrush(splitterColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ============================================================================
// PARSE DECOMPILATION OUTPUT INTO DecompLine / DisasmLine STRUCTURES
// ============================================================================
static void ParseDecompOutput(DecompViewState* state,
                               const std::string& decompCode,
                               const std::string& disasmText) {
    if (!state) return;

    state->decompLines.clear();
    state->disasmLines.clear();

    // ---- Parse decompiled C code ----
    std::istringstream decompStream(decompCode);
    std::string line;
    int lineNum = 0;
    int currentDisasmIdx = 0;

    while (std::getline(decompStream, line)) {
        DecompLine dl;
        dl.text = line;
        dl.address = 0;
        dl.asmStartIndex = -1;
        dl.asmEndIndex = -1;
        dl.indentLevel = 0;
        dl.isComment = false;
        dl.isLabel = false;

        // Calculate indent level
        for (char c : line) {
            if (c == ' ') dl.indentLevel++;
            else if (c == '\t') dl.indentLevel += 4;
            else break;
        }
        dl.indentLevel /= 4;

        // Detect comment lines
        std::string trimmed = line;
        while (!trimmed.empty() && (trimmed[0] == ' ' || trimmed[0] == '\t'))
            trimmed.erase(0, 1);
        if (trimmed.substr(0, 2) == "//" || trimmed.substr(0, 2) == "/*")
            dl.isComment = true;

        // Detect labels (function declarations, block labels)
        if (!trimmed.empty() && trimmed.back() == ':')
            dl.isLabel = true;
        if (trimmed.find("(") != std::string::npos && trimmed.find("{") != std::string::npos)
            dl.isLabel = true;

        // Extract address from comment markers like "// @0x401000"
        size_t atPos = line.find("// @0x");
        if (atPos == std::string::npos) atPos = line.find("/* @0x");
        if (atPos != std::string::npos) {
            std::string addrStr = line.substr(atPos + 5); // after "// @0"
            if (addrStr[0] == 'x' || addrStr[0] == 'X') addrStr = addrStr.substr(1);
            // Find end of hex number
            size_t end = 0;
            while (end < addrStr.size() && std::isxdigit((unsigned char)addrStr[end])) end++;
            if (end > 0) {
                try {
                    dl.address = std::stoull(addrStr.substr(0, end), nullptr, 16);
                } catch (...) {}
            }
        }

        // Scan for SSA variable references: var_rax_N, var_rsp_N, arg_N, etc.
        // Pattern: var_{register}_{number} or arg_{number} or local_{number}
        size_t pos = 0;
        while (pos < line.size()) {
            // Look for variable patterns
            bool foundVar = false;
            static const char* varPrefixes[] = {"var_", "arg_", "local_", "tmp_", "ret_"};
            for (const char* prefix : varPrefixes) {
                size_t prefLen = strlen(prefix);
                if (pos + prefLen < line.size() &&
                    line.substr(pos, prefLen) == prefix &&
                    (pos == 0 || !std::isalnum((unsigned char)line[pos-1]))) {

                    size_t end = pos + prefLen;
                    while (end < line.size() && (std::isalnum((unsigned char)line[end]) || line[end] == '_'))
                        end++;
                    if (end > pos + prefLen) {
                        DecompLine::VarRef vr;
                        vr.charStart = (int)pos;
                        vr.charLen = (int)(end - pos);
                        vr.varName = line.substr(pos, end - pos);
                        vr.ssaId = -1;

                        // Try to extract SSA ID from the variable name suffix
                        // e.g., "var_rax_1" → SSA ID 1
                        size_t lastUnderscore = vr.varName.rfind('_');
                        if (lastUnderscore != std::string::npos && lastUnderscore + 1 < vr.varName.size()) {
                            std::string suffix = vr.varName.substr(lastUnderscore + 1);
                            bool allDigits = !suffix.empty();
                            for (char c : suffix) {
                                if (!std::isdigit((unsigned char)c)) { allDigits = false; break; }
                            }
                            if (allDigits) {
                                try { vr.ssaId = std::stoi(suffix); } catch (...) {}
                            }
                        }

                        dl.varRefs.push_back(vr);
                        pos = end;
                        foundVar = true;
                        break;
                    }
                }
            }
            if (!foundVar) pos++;
        }

        state->decompLines.push_back(dl);
        lineNum++;
    }

    // ---- Parse disassembly output ----
    std::istringstream disasmStream(disasmText);
    int disasmIdx = 0;

    while (std::getline(disasmStream, line)) {
        DisasmLine al;
        al.address = 0;
        al.decompLineIndex = -1;
        al.isLabel = false;
        al.isBlockBoundary = false;

        // Skip header lines
        if (line.empty()) {
            al.mnemonic = "";
            state->disasmLines.push_back(al);
            disasmIdx++;
            continue;
        }
        if (line.find("===") != std::string::npos || line.find("Instructions") != std::string::npos) {
            al.mnemonic = line;
            al.isLabel = true;
            state->disasmLines.push_back(al);
            disasmIdx++;
            continue;
        }

        // Parse "  0xAAAAAAAAAAAA: BB CC DD  mnemonic operands  ; comment"
        std::string trimmed = line;
        while (!trimmed.empty() && trimmed[0] == ' ') trimmed.erase(0, 1);

        // Check for label lines (e.g., "loc_401000:" or "function_name:")
        if (!trimmed.empty() && trimmed.back() == ':' && trimmed.find(' ') == std::string::npos) {
            al.mnemonic = trimmed;
            al.isLabel = true;
            state->disasmLines.push_back(al);
            disasmIdx++;
            continue;
        }

        // Try to parse address
        if (trimmed.substr(0, 2) == "0x" || trimmed.substr(0, 2) == "0X") {
            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos) {
                std::string addrStr = trimmed.substr(2, colonPos - 2);
                try {
                    al.address = std::stoull(addrStr, nullptr, 16);
                } catch (...) {}
                trimmed = trimmed.substr(colonPos + 1);
                while (!trimmed.empty() && trimmed[0] == ' ') trimmed.erase(0, 1);
            }
        }

        // Try to extract hex bytes (sequences of 2-digit hex values before the mnemonic)
        std::string remaining = trimmed;
        std::string hexPart;
        while (remaining.size() >= 2) {
            bool isHex = std::isxdigit((unsigned char)remaining[0]) &&
                         std::isxdigit((unsigned char)remaining[1]) &&
                         (remaining.size() == 2 || remaining[2] == ' ');
            if (isHex) {
                hexPart += remaining.substr(0, 2) + " ";
                remaining = remaining.substr(std::min<size_t>(3, remaining.size()));
                while (!remaining.empty() && remaining[0] == ' ') remaining.erase(0, 1);
            } else {
                break;
            }
        }
        al.hexBytes = hexPart;

        // Split remaining into mnemonic + operands
        while (!remaining.empty() && remaining[0] == ' ') remaining.erase(0, 1);
        size_t spacePos = remaining.find(' ');
        if (spacePos != std::string::npos) {
            al.mnemonic = remaining.substr(0, spacePos);
            std::string rest = remaining.substr(spacePos + 1);

            // Check for comment
            size_t commentPos = rest.find(';');
            if (commentPos != std::string::npos) {
                al.operands = rest.substr(0, commentPos);
                al.comment = rest.substr(commentPos + 1);
                // Trim
                while (!al.operands.empty() && al.operands.back() == ' ') al.operands.pop_back();
                while (!al.comment.empty() && al.comment[0] == ' ') al.comment.erase(0, 1);
            } else {
                al.operands = rest;
            }
        } else {
            al.mnemonic = remaining;
        }

        // Detect basic block boundaries (jmp/jcc/ret)
        if (al.mnemonic == "ret" || al.mnemonic == "jmp" || al.mnemonic == "retn" ||
            al.mnemonic.substr(0, 1) == "j") {
            al.isBlockBoundary = true;
        }

        // Try to map to decompiler line by address
        if (al.address != 0) {
            // Find the nearest decompiler line with a matching or nearby address
            for (int d = 0; d < (int)state->decompLines.size(); ++d) {
                if (state->decompLines[d].address == al.address) {
                    al.decompLineIndex = d;
                    // Update decomp line's disasm range
                    if (state->decompLines[d].asmStartIndex < 0)
                        state->decompLines[d].asmStartIndex = disasmIdx;
                    state->decompLines[d].asmEndIndex = disasmIdx;
                    break;
                }
            }
        }

        state->disasmLines.push_back(al);
        disasmIdx++;
    }

    // Build the bidirectional mapping
    DecompView_BuildAddressMap(state);
}

// ============================================================================
// PUBLIC API: Win32IDE::initDecompilerView()
// ============================================================================
void Win32IDE::initDecompilerView() {
    LOG_FUNCTION();
    RegisterDecompClasses(m_hInstance);
    m_decompViewActive = false;
    LOG_INFO("DecompilerView: Window classes registered, ready");
}

// ============================================================================
// PUBLIC API: Win32IDE::showDecompilerView()
// Opens the split decompiler/disassembly view for the current RE analysis
// ============================================================================
void Win32IDE::showDecompilerView(const std::string& decompCode,
                                   const std::string& disasmText,
                                   const std::string& binaryName) {
    LOG_FUNCTION();

    // Destroy previous view if any
    destroyDecompilerView();

    // Create state
    g_decompState = new DecompViewState();
    memset(g_decompState, 0, sizeof(DecompViewState));
    g_decompState->pIDE = this;
    g_decompState->binaryPath = binaryName;
    g_decompState->selectedDecompLine = -1;
    g_decompState->selectedDisasmLine = -1;
    g_decompState->highlightDecompStart = -1;
    g_decompState->highlightDecompEnd = -1;
    g_decompState->highlightDisasmStart = -1;
    g_decompState->highlightDisasmEnd = -1;
    g_decompState->hoverDecompLine = -1;
    g_decompState->hoverVarRefIndex = -1;
    g_decompState->splitterDragging = false;
    g_decompState->initialized = false;

    // Create the container window
    RECT rcMain;
    GetClientRect(m_hwndMain, &rcMain);
    int width = rcMain.right - rcMain.left;
    int height = rcMain.bottom - rcMain.top;

    // Position: take up the editor area (replace main editor temporarily)
    std::string title = "Decompiler — " + binaryName;
    g_decompState->hwndContainer = CreateWindowExA(
        0, DECOMP_SPLIT_CLASS, title.c_str(),
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 30, width, height - 60,  // Leave room for tab bar and status bar
        m_hwndMain, NULL, m_hInstance, NULL);

    if (!g_decompState->hwndContainer) {
        LOG_ERROR("DecompilerView: Failed to create container window");
        delete g_decompState;
        g_decompState = nullptr;
        return;
    }

    // Default splitter position at 50%
    RECT rcContainer;
    GetClientRect(g_decompState->hwndContainer, &rcContainer);
    g_decompState->splitterPos = (rcContainer.right - rcContainer.left) / 2;

    // Create decompiler pane
    int decompW = g_decompState->splitterPos;
    g_decompState->hwndDecompPane = CreateWindowExA(
        0, DECOMP_VIEW_CLASS, "Decompiler",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        0, 0, decompW, rcContainer.bottom,
        g_decompState->hwndContainer, NULL, m_hInstance, NULL);

    // Create splitter bar
    g_decompState->hwndSplitter = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE,
        decompW, 0, DECOMP_SPLITTER_WIDTH, rcContainer.bottom,
        g_decompState->hwndContainer, NULL, m_hInstance, NULL);

    // Subclass the splitter for drag behavior
    if (g_decompState->hwndSplitter) {
        SetWindowLongPtrA(g_decompState->hwndSplitter, GWLP_WNDPROC, (LONG_PTR)SplitterBarProc);
    }

    // Create disassembly pane
    int disasmX = decompW + DECOMP_SPLITTER_WIDTH;
    int disasmW = rcContainer.right - disasmX;
    g_decompState->hwndDisasmPane = CreateWindowExA(
        0, DISASM_VIEW_CLASS, "Disassembly",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
        disasmX, 0, disasmW, rcContainer.bottom,
        g_decompState->hwndContainer, NULL, m_hInstance, NULL);

    // Initialize Direct2D
    DecompView_InitD2D(g_decompState);

    // Parse the content
    ParseDecompOutput(g_decompState, decompCode, disasmText);

    // Store HWND references
    m_hwndDecompView = g_decompState->hwndContainer;
    m_hwndDecompPane = g_decompState->hwndDecompPane;
    m_hwndDisasmPane = g_decompState->hwndDisasmPane;
    m_decompViewActive = true;

    // Force initial paint
    InvalidateRect(g_decompState->hwndDecompPane, NULL, FALSE);
    InvalidateRect(g_decompState->hwndDisasmPane, NULL, FALSE);

    LOG_INFO("DecompilerView: Opened for '" + binaryName + "' with " +
             std::to_string(g_decompState->decompLines.size()) + " decomp lines and " +
             std::to_string(g_decompState->disasmLines.size()) + " disasm lines");
}

// ============================================================================
// PUBLIC API: Win32IDE::destroyDecompilerView()
// ============================================================================
void Win32IDE::destroyDecompilerView() {
    if (g_decompState) {
        if (g_decompState->hwndContainer) {
            DestroyWindow(g_decompState->hwndContainer);
            // The WM_DESTROY handler will clean up the state
        } else {
            DecompView_ReleaseD2D(g_decompState);
            delete g_decompState;
            g_decompState = nullptr;
        }
    }
    m_hwndDecompView = nullptr;
    m_hwndDecompPane = nullptr;
    m_hwndDisasmPane = nullptr;
    m_decompViewActive = false;
}

// ============================================================================
// PUBLIC API: Win32IDE::decompViewRenameVariable()
// Programmatic rename (e.g., from command palette)
// ============================================================================
void Win32IDE::decompViewRenameVariable(const std::string& oldName, const std::string& newName) {
    if (!g_decompState) return;

    // Find the SSA ID for this variable
    int ssaId = -1;
    for (const auto& dl : g_decompState->decompLines) {
        for (const auto& vr : dl.varRefs) {
            if (vr.varName == oldName) {
                ssaId = vr.ssaId;
                break;
            }
        }
        if (ssaId >= 0) break;
    }

    DecompView_PropagateRename(g_decompState, oldName, newName, ssaId);
}

// ============================================================================
// PUBLIC API: Win32IDE::decompViewGetRenameMap()
// Returns the current rename mapping for serialization/export
// ============================================================================
std::map<std::string, std::string> Win32IDE::decompViewGetRenameMap() const {
    if (!g_decompState) return {};
    return g_decompState->varRenameMap;
}

// ============================================================================
// PUBLIC API: Win32IDE::decompViewSyncToAddress()
// Programmatic sync: jump both panes to a specific address
// ============================================================================
void Win32IDE::decompViewSyncToAddress(uint64_t address) {
    if (!g_decompState) return;

    auto decompIt = g_decompState->addrToDecompLine.find(address);
    if (decompIt != g_decompState->addrToDecompLine.end()) {
        DecompView_SyncFromDecomp(g_decompState, decompIt->second);
        if (g_decompState->hwndDecompPane) InvalidateRect(g_decompState->hwndDecompPane, NULL, FALSE);
        if (g_decompState->hwndDisasmPane) InvalidateRect(g_decompState->hwndDisasmPane, NULL, FALSE);
        return;
    }

    auto disasmIt = g_decompState->addrToDisasmLine.find(address);
    if (disasmIt != g_decompState->addrToDisasmLine.end()) {
        DecompView_SyncFromDisasm(g_decompState, disasmIt->second);
        if (g_decompState->hwndDecompPane) InvalidateRect(g_decompState->hwndDecompPane, NULL, FALSE);
        if (g_decompState->hwndDisasmPane) InvalidateRect(g_decompState->hwndDisasmPane, NULL, FALSE);
    }
}

// ============================================================================
// PUBLIC API: Win32IDE::isDecompilerViewActive()
// ============================================================================
bool Win32IDE::isDecompilerViewActive() const {
    return m_decompViewActive && g_decompState != nullptr && g_decompState->initialized;
}
