// ============================================================================
// interpretability_panel.cpp — Full Win32 Native Implementation
// ============================================================================
// Production-ready interpretability visualization panel for model inference.
// Displays attention heatmaps, layer activations, token attributions,
// and inference statistics. Pure Win32 GDI rendering, no Qt dependency.
//
// Pattern: PatchResult-style structured results, no exceptions
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <mutex>
#include <atomic>

// ============================================================================
// Data Structures
// ============================================================================

enum class VisualizationType {
    AttentionHeatmap,
    LayerActivations,
    TokenAttribution,
    EmbeddingProjection,
    LogitDistribution,
    GradientFlow
};

struct AttentionData {
    std::vector<std::vector<float>> weights;  // [heads][seq_len * seq_len]
    int numHeads = 0;
    int seqLen = 0;
};

struct LayerActivationData {
    std::vector<float> activations;  // flattened [layers][embed_dim]
    int numLayers = 0;
    int embedDim = 0;
    float minVal = 0.0f;
    float maxVal = 0.0f;
};

struct TokenAttributionData {
    std::vector<std::string> tokens;
    std::vector<float> scores;
    float minScore = 0.0f;
    float maxScore = 1.0f;
};

struct LogitDistData {
    std::vector<std::pair<std::string, float>> topTokens;
    float temperature = 1.0f;
};

struct InferenceStats {
    float tokensPerSec = 0.0f;
    float latencyMs = 0.0f;
    int totalTokens = 0;
    int promptTokens = 0;
    int generatedTokens = 0;
    float memoryUsageMB = 0.0f;
    int activeLayer = -1;
    float avgAttentionEntropy = 0.0f;
};

struct VisualizationState {
    VisualizationType currentType = VisualizationType::AttentionHeatmap;
    int selectedHead = 0;
    int minLayer = 0;
    int maxLayer = 31;
    int selectedLayer = 0;
    float zoomLevel = 1.0f;
    bool showGrid = true;
    bool showLabels = true;
    bool autoRefresh = true;
};

// ============================================================================
// Color Helpers
// ============================================================================

static COLORREF HeatmapColor(float value) {
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    int r, g, b;
    if (value < 0.25f) {
        float t = value / 0.25f;
        r = 0; g = (int)(t * 255); b = 255;
    } else if (value < 0.5f) {
        float t = (value - 0.25f) / 0.25f;
        r = 0; g = 255; b = (int)((1.0f - t) * 255);
    } else if (value < 0.75f) {
        float t = (value - 0.5f) / 0.25f;
        r = (int)(t * 255); g = 255; b = 0;
    } else {
        float t = (value - 0.75f) / 0.25f;
        r = 255; g = (int)((1.0f - t) * 255); b = 0;
    }
    return RGB(r, g, b);
}

static COLORREF AttributionColor(float score, float minS, float maxS) {
    if (maxS <= minS) return RGB(80, 80, 80);
    float norm = (score - minS) / (maxS - minS);
    norm = (std::max)(0.0f, (std::min)(1.0f, norm));
    if (score >= 0) {
        return RGB(30, (int)(100 + norm * 155), 30);
    }
    return RGB((int)(100 + norm * 155), 30, 30);
}

static COLORREF BlendColor(COLORREF c1, COLORREF c2, float t) {
    t = (std::max)(0.0f, (std::min)(1.0f, t));
    return RGB(
        (int)(GetRValue(c1) * (1 - t) + GetRValue(c2) * t),
        (int)(GetGValue(c1) * (1 - t) + GetGValue(c2) * t),
        (int)(GetBValue(c1) * (1 - t) + GetBValue(c2) * t));
}

// ============================================================================
// Constants
// ============================================================================

static constexpr COLORREF BG_COLOR       = RGB(30, 30, 30);
static constexpr COLORREF PANEL_BG       = RGB(37, 37, 38);
static constexpr COLORREF BORDER_COLOR   = RGB(60, 60, 60);
static constexpr COLORREF TEXT_COLOR      = RGB(212, 212, 212);
static constexpr COLORREF ACCENT_COLOR   = RGB(86, 156, 214);
static constexpr COLORREF HEADER_BG      = RGB(45, 45, 48);
static constexpr COLORREF GRID_COLOR     = RGB(50, 50, 55);
static constexpr COLORREF STAT_LABEL_C   = RGB(136, 136, 136);
static constexpr COLORREF STAT_VALUE_C   = RGB(78, 201, 176);

// ============================================================================
// Panel State (module-internal singleton)
// ============================================================================

static struct InterpPanel {
    HWND hwnd = nullptr;
    HWND parentWnd = nullptr;

    VisualizationState state;
    InferenceStats stats;
    std::mutex dataMutex;

    AttentionData attentionData;
    LayerActivationData layerData;
    TokenAttributionData attrData;
    LogitDistData logitData;

    std::vector<std::string> headNames;

    HBITMAP backBuffer = nullptr;
    HDC backDC = nullptr;
    int bufW = 0, bufH = 0;

    HFONT fontTitle = nullptr;
    HFONT fontBody = nullptr;
    HFONT fontSmall = nullptr;
    HFONT fontMono = nullptr;
} g_interp;

// ============================================================================
// Forward declarations
// ============================================================================

static void InterpPaint(HDC hdc, const RECT& rc);
static void InterpPaintHeader(HDC hdc, const RECT& area);
static void InterpPaintLayerSelector(HDC hdc, const RECT& area);
static void InterpPaintAttention(HDC hdc, const RECT& area);
static void InterpPaintActivations(HDC hdc, const RECT& area);
static void InterpPaintAttribution(HDC hdc, const RECT& area);
static void InterpPaintLogits(HDC hdc, const RECT& area);
static void InterpPaintStats(HDC hdc, const RECT& area);

// ============================================================================
// WndProc
// ============================================================================

static LRESULT CALLBACK InterpWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (w != g_interp.bufW || h != g_interp.bufH) {
            if (g_interp.backBuffer) DeleteObject(g_interp.backBuffer);
            if (g_interp.backDC) DeleteDC(g_interp.backDC);
            g_interp.backDC = CreateCompatibleDC(hdc);
            g_interp.backBuffer = CreateCompatibleBitmap(hdc, w, h);
            SelectObject(g_interp.backDC, g_interp.backBuffer);
            g_interp.bufW = w;
            g_interp.bufH = h;
        }

        InterpPaint(g_interp.backDC, rc);
        BitBlt(hdc, 0, 0, w, h, g_interp.backDC, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        RECT rc;
        GetClientRect(hwnd, &rc);
        int headerH = 40;
        int selectorH = 30;
        if (y >= headerH && y < headerH + selectorH) {
            int numLayers = g_interp.state.maxLayer - g_interp.state.minLayer + 1;
            if (numLayers > 0) {
                int layerW = (rc.right - rc.left) / numLayers;
                if (layerW > 0) {
                    int layer = g_interp.state.minLayer + x / layerW;
                    layer = (std::min)(layer, g_interp.state.maxLayer);
                    g_interp.state.selectedLayer = layer;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_interp.state.zoomLevel += (delta > 0) ? 0.1f : -0.1f;
        g_interp.state.zoomLevel = (std::max)(0.5f, (std::min)(4.0f, g_interp.state.zoomLevel));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ============================================================================
// Paint — Master
// ============================================================================

static void InterpPaint(HDC hdc, const RECT& rc) {
    HBRUSH bgBrush = CreateSolidBrush(BG_COLOR);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w < 50 || h < 50) return;

    RECT headerRect = { rc.left, rc.top, rc.right, rc.top + 40 };
    InterpPaintHeader(hdc, headerRect);

    RECT selectorRect = { rc.left, rc.top + 40, rc.right, rc.top + 70 };
    InterpPaintLayerSelector(hdc, selectorRect);

    RECT vizRect = { rc.left + 5, rc.top + 75, rc.right - 5, rc.bottom - 100 };

    std::lock_guard<std::mutex> lock(g_interp.dataMutex);

    switch (g_interp.state.currentType) {
    case VisualizationType::AttentionHeatmap:  InterpPaintAttention(hdc, vizRect); break;
    case VisualizationType::LayerActivations:  InterpPaintActivations(hdc, vizRect); break;
    case VisualizationType::TokenAttribution:  InterpPaintAttribution(hdc, vizRect); break;
    case VisualizationType::LogitDistribution: InterpPaintLogits(hdc, vizRect); break;
    default: InterpPaintAttention(hdc, vizRect); break;
    }

    RECT statsRect = { rc.left, rc.bottom - 95, rc.right, rc.bottom };
    InterpPaintStats(hdc, statsRect);
}

// ============================================================================
// Paint — Header
// ============================================================================

static void InterpPaintHeader(HDC hdc, const RECT& area) {
    HBRUSH hdrBrush = CreateSolidBrush(HEADER_BG);
    FillRect(hdc, &area, hdrBrush);
    DeleteObject(hdrBrush);

    SelectObject(hdc, g_interp.fontTitle);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, ACCENT_COLOR);

    const wchar_t* typeNames[] = {
        L"Attention Heatmap", L"Layer Activations",
        L"Token Attribution", L"Embedding Projection",
        L"Logit Distribution", L"Gradient Flow"
    };
    int idx = static_cast<int>(g_interp.state.currentType);
    if (idx < 0 || idx > 5) idx = 0;

    RECT textRect = area;
    textRect.left += 10;
    textRect.top += 10;
    DrawTextW(hdc, typeNames[idx], -1, &textRect, DT_LEFT | DT_SINGLELINE);

    if (g_interp.state.currentType == VisualizationType::AttentionHeatmap && !g_interp.headNames.empty()) {
        SetTextColor(hdc, TEXT_COLOR);
        SelectObject(hdc, g_interp.fontSmall);
        wchar_t headBuf[64];
        swprintf_s(headBuf, L"Head %d/%d", g_interp.state.selectedHead + 1, (int)g_interp.headNames.size());
        RECT headRect = area;
        headRect.right -= 10;
        headRect.top += 12;
        DrawTextW(hdc, headBuf, -1, &headRect, DT_RIGHT | DT_SINGLELINE);
    }

    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_COLOR);
    SelectObject(hdc, pen);
    MoveToEx(hdc, area.left, area.bottom - 1, nullptr);
    LineTo(hdc, area.right, area.bottom - 1);
    DeleteObject(pen);
}

// ============================================================================
// Paint — Layer Selector
// ============================================================================

static void InterpPaintLayerSelector(HDC hdc, const RECT& area) {
    HBRUSH stripBg = CreateSolidBrush(RGB(35, 35, 38));
    FillRect(hdc, &area, stripBg);
    DeleteObject(stripBg);

    int numLayers = g_interp.state.maxLayer - g_interp.state.minLayer + 1;
    if (numLayers <= 0) return;

    int areaW = area.right - area.left;
    float cellW = static_cast<float>(areaW) / numLayers;
    int cellH = area.bottom - area.top - 6;

    SelectObject(hdc, g_interp.fontSmall);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < numLayers; ++i) {
        int layerIdx = g_interp.state.minLayer + i;
        int x = area.left + static_cast<int>(i * cellW);
        int w = (std::max)(2, static_cast<int>(cellW) - 1);

        RECT cellRect = { x, area.top + 3, x + w, area.top + 3 + cellH };

        COLORREF cellColor = (layerIdx == g_interp.state.selectedLayer) ? ACCENT_COLOR :
                             (layerIdx == g_interp.stats.activeLayer) ? RGB(78, 201, 176) :
                             RGB(55, 55, 60);

        HBRUSH cellBrush = CreateSolidBrush(cellColor);
        FillRect(hdc, &cellRect, cellBrush);
        DeleteObject(cellBrush);

        if (w >= 20) {
            SetTextColor(hdc, (layerIdx == g_interp.state.selectedLayer) ? RGB(0, 0, 0) : TEXT_COLOR);
            wchar_t layerBuf[8];
            swprintf_s(layerBuf, L"%d", layerIdx);
            DrawTextW(hdc, layerBuf, -1, &cellRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

// ============================================================================
// Paint — Attention Heatmap
// ============================================================================

static void InterpPaintAttention(HDC hdc, const RECT& area) {
    if (g_interp.attentionData.numHeads == 0 || g_interp.attentionData.seqLen == 0) {
        SelectObject(hdc, g_interp.fontBody);
        SetTextColor(hdc, STAT_LABEL_C);
        SetBkMode(hdc, TRANSPARENT);
        RECT textRect = area;
        DrawTextW(hdc, L"No attention data available.\nRun inference to see attention patterns.", -1,
                  &textRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }

    int headIdx = (std::min)(g_interp.state.selectedHead, g_interp.attentionData.numHeads - 1);
    if (headIdx < 0) headIdx = 0;

    const auto& weights = g_interp.attentionData.weights[headIdx];
    int seqLen = g_interp.attentionData.seqLen;
    int vizW = area.right - area.left;
    int vizH = area.bottom - area.top;
    float cellW = static_cast<float>(vizW) / seqLen * g_interp.state.zoomLevel;
    float cellH = static_cast<float>(vizH) / seqLen * g_interp.state.zoomLevel;

    for (int row = 0; row < seqLen; ++row) {
        for (int col = 0; col < seqLen; ++col) {
            int px = area.left + static_cast<int>(col * cellW);
            int py = area.top + static_cast<int>(row * cellH);
            int pw = (std::max)(1, static_cast<int>(cellW));
            int ph = (std::max)(1, static_cast<int>(cellH));
            if (px >= area.right || py >= area.bottom) continue;

            float val = weights[row * seqLen + col];
            COLORREF color = HeatmapColor(val);
            RECT cr = { px, py, px + pw, py + ph };
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(hdc, &cr, brush);
            DeleteObject(brush);
        }
    }

    if (g_interp.state.showGrid && cellW >= 4.0f) {
        HPEN gridPen = CreatePen(PS_SOLID, 1, GRID_COLOR);
        SelectObject(hdc, gridPen);
        for (int i = 0; i <= seqLen; ++i) {
            int x = area.left + static_cast<int>(i * cellW);
            int y = area.top + static_cast<int>(i * cellH);
            if (x <= area.right) { MoveToEx(hdc, x, area.top, nullptr); LineTo(hdc, x, area.bottom); }
            if (y <= area.bottom) { MoveToEx(hdc, area.left, y, nullptr); LineTo(hdc, area.right, y); }
        }
        DeleteObject(gridPen);
    }
}

// ============================================================================
// Paint — Layer Activations
// ============================================================================

static void InterpPaintActivations(HDC hdc, const RECT& area) {
    if (g_interp.layerData.numLayers == 0) {
        SelectObject(hdc, g_interp.fontBody);
        SetTextColor(hdc, STAT_LABEL_C);
        SetBkMode(hdc, TRANSPARENT);
        RECT tr = area;
        DrawTextW(hdc, L"No layer activation data.\nRun forward pass to see activations.", -1, &tr, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }

    int vizW = area.right - area.left;
    int vizH = area.bottom - area.top;
    float barWidth = static_cast<float>(vizW) / g_interp.layerData.numLayers;
    float range = g_interp.layerData.maxVal - g_interp.layerData.minVal;
    if (range < 1e-6f) range = 1.0f;

    for (int layer = 0; layer < g_interp.layerData.numLayers; ++layer) {
        float sum = 0.0f;
        int offset = layer * g_interp.layerData.embedDim;
        for (int d = 0; d < g_interp.layerData.embedDim; ++d) {
            sum += std::abs(g_interp.layerData.activations[offset + d]);
        }
        float mean = sum / g_interp.layerData.embedDim;
        float norm = (mean - g_interp.layerData.minVal) / range;
        norm = (std::max)(0.0f, (std::min)(1.0f, norm));

        int barH = static_cast<int>(norm * vizH);
        int x = area.left + static_cast<int>(layer * barWidth);
        int w = (std::max)(2, static_cast<int>(barWidth) - 1);

        RECT barRect = { x, area.bottom - barH, x + w, area.bottom };
        COLORREF barColor = (layer == g_interp.state.selectedLayer) ? ACCENT_COLOR : HeatmapColor(norm);
        HBRUSH brush = CreateSolidBrush(barColor);
        FillRect(hdc, &barRect, brush);
        DeleteObject(brush);

        if (g_interp.state.showLabels && barWidth >= 15.0f && (layer % 4 == 0)) {
            SelectObject(hdc, g_interp.fontSmall);
            SetTextColor(hdc, TEXT_COLOR);
            SetBkMode(hdc, TRANSPARENT);
            wchar_t buf[8];
            swprintf_s(buf, L"%d", layer);
            RECT labelRect = { x, area.bottom - 14, x + w, area.bottom };
            DrawTextW(hdc, buf, -1, &labelRect, DT_CENTER | DT_SINGLELINE);
        }
    }
}

// ============================================================================
// Paint — Token Attribution
// ============================================================================

static void InterpPaintAttribution(HDC hdc, const RECT& area) {
    if (g_interp.attrData.tokens.empty()) {
        SelectObject(hdc, g_interp.fontBody);
        SetTextColor(hdc, STAT_LABEL_C);
        SetBkMode(hdc, TRANSPARENT);
        RECT tr = area;
        DrawTextW(hdc, L"No token attribution data.\nGenerate output to see token contributions.", -1, &tr, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }

    int vizW = area.right - area.left - 10;
    int tokCount = static_cast<int>(g_interp.attrData.tokens.size());
    int colsPerRow = (std::max)(1, vizW / 80);
    int cellW = vizW / colsPerRow;
    int cellH = 28;

    SelectObject(hdc, g_interp.fontMono);
    SetBkMode(hdc, TRANSPARENT);

    for (int i = 0; i < tokCount; ++i) {
        int col = i % colsPerRow;
        int row = i / colsPerRow;
        int x = area.left + 5 + col * cellW;
        int y = area.top + row * cellH;
        if (y + cellH > area.bottom) break;

        float score = (i < (int)g_interp.attrData.scores.size()) ? g_interp.attrData.scores[i] : 0.0f;
        COLORREF bgColor = AttributionColor(score, g_interp.attrData.minScore, g_interp.attrData.maxScore);

        RECT cr = { x, y, x + cellW - 2, y + cellH - 2 };
        HBRUSH brush = CreateSolidBrush(bgColor);
        FillRect(hdc, &cr, brush);
        DeleteObject(brush);

        SetTextColor(hdc, RGB(255, 255, 255));
        std::wstring wtoken;
        for (char c : g_interp.attrData.tokens[i]) wtoken += static_cast<wchar_t>(c);
        DrawTextW(hdc, wtoken.c_str(), -1, &cr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

// ============================================================================
// Paint — Logit Distribution
// ============================================================================

static void InterpPaintLogits(HDC hdc, const RECT& area) {
    if (g_interp.logitData.topTokens.empty()) {
        SelectObject(hdc, g_interp.fontBody);
        SetTextColor(hdc, STAT_LABEL_C);
        SetBkMode(hdc, TRANSPARENT);
        RECT tr = area;
        DrawTextW(hdc, L"No logit data.\nGenerate tokens to see probability distribution.", -1, &tr, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
        return;
    }

    int vizW = area.right - area.left - 120;
    int vizH = area.bottom - area.top;
    int numTokens = (std::min)(static_cast<int>(g_interp.logitData.topTokens.size()), 20);
    int barH = (std::max)(16, vizH / numTokens - 2);

    float maxLogit = -1e9f;
    for (const auto& [tok, logit] : g_interp.logitData.topTokens) {
        if (logit > maxLogit) maxLogit = logit;
    }
    if (maxLogit <= 0) maxLogit = 1.0f;

    for (int i = 0; i < numTokens; ++i) {
        const auto& [token, logit] = g_interp.logitData.topTokens[i];
        float norm = logit / maxLogit;
        norm = (std::max)(0.0f, (std::min)(1.0f, norm));
        int y = area.top + i * (barH + 2);
        if (y + barH > area.bottom) break;

        SelectObject(hdc, g_interp.fontMono);
        SetTextColor(hdc, TEXT_COLOR);
        SetBkMode(hdc, TRANSPARENT);
        RECT labelRect = { area.left, y, area.left + 110, y + barH };
        std::wstring wtoken;
        for (char c : token) wtoken += static_cast<wchar_t>(c);
        DrawTextW(hdc, wtoken.c_str(), -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        int barW = static_cast<int>(norm * vizW);
        RECT barRect = { area.left + 115, y + 2, area.left + 115 + barW, y + barH - 2 };
        COLORREF barColor = BlendColor(RGB(40, 100, 180), RGB(78, 201, 176), norm);
        HBRUSH brush = CreateSolidBrush(barColor);
        FillRect(hdc, &barRect, brush);
        DeleteObject(brush);

        wchar_t valBuf[32];
        swprintf_s(valBuf, L"%.2f", logit);
        RECT valRect = { area.left + 120 + barW, y, area.right, y + barH };
        SetTextColor(hdc, STAT_VALUE_C);
        DrawTextW(hdc, valBuf, -1, &valRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

// ============================================================================
// Paint — Stats Panel
// ============================================================================

static void InterpPaintStats(HDC hdc, const RECT& area) {
    HBRUSH panelBg = CreateSolidBrush(PANEL_BG);
    FillRect(hdc, &area, panelBg);
    DeleteObject(panelBg);

    HPEN pen = CreatePen(PS_SOLID, 1, BORDER_COLOR);
    SelectObject(hdc, pen);
    MoveToEx(hdc, area.left, area.top, nullptr);
    LineTo(hdc, area.right, area.top);
    DeleteObject(pen);

    SelectObject(hdc, g_interp.fontSmall);
    SetBkMode(hdc, TRANSPARENT);

    int colW = (area.right - area.left) / 3;
    int y = area.top + 8;
    int lineH = 18;

    auto drawStat = [&](int col, int row, const wchar_t* label, const wchar_t* value) {
        RECT lr = { area.left + col * colW + 8, y + row * lineH,
                     area.left + col * colW + colW, y + row * lineH + lineH };
        SetTextColor(hdc, STAT_LABEL_C);
        DrawTextW(hdc, label, -1, &lr, DT_LEFT | DT_SINGLELINE);
        RECT vr = lr;
        vr.left += 90;
        SetTextColor(hdc, STAT_VALUE_C);
        DrawTextW(hdc, value, -1, &vr, DT_LEFT | DT_SINGLELINE);
    };

    wchar_t buf[64];
    swprintf_s(buf, L"%.1f t/s", g_interp.stats.tokensPerSec); drawStat(0, 0, L"Speed:", buf);
    swprintf_s(buf, L"%.0f ms", g_interp.stats.latencyMs); drawStat(0, 1, L"Latency:", buf);
    swprintf_s(buf, L"%d", g_interp.stats.totalTokens); drawStat(0, 2, L"Tokens:", buf);
    swprintf_s(buf, L"%d / %d", g_interp.stats.promptTokens, g_interp.stats.generatedTokens); drawStat(1, 0, L"Prompt/Gen:", buf);
    swprintf_s(buf, L"%.1f MB", g_interp.stats.memoryUsageMB); drawStat(1, 1, L"Memory:", buf);
    swprintf_s(buf, L"%.3f", g_interp.stats.avgAttentionEntropy); drawStat(1, 2, L"Attn Entropy:", buf);
    swprintf_s(buf, L"%.1fx", g_interp.state.zoomLevel); drawStat(2, 0, L"Zoom:", buf);
    swprintf_s(buf, L"%d-%d", g_interp.state.minLayer, g_interp.state.maxLayer); drawStat(2, 1, L"Layers:", buf);
    drawStat(2, 2, L"Grid:", g_interp.state.showGrid ? L"ON" : L"OFF");
}

// ============================================================================
// Public C-style API (callable from Win32IDE)
// ============================================================================

static const wchar_t* INTERP_CLASS = L"RawrXD_InterpretabilityPanel";
static bool s_classRegistered = false;

extern "C" {

HWND InterpretabilityPanel_Create(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);

    if (!s_classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = InterpWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CreateSolidBrush(BG_COLOR);
        wc.lpszClassName = INTERP_CLASS;
        RegisterClassExW(&wc);
        s_classRegistered = true;
    }

    g_interp.fontTitle = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_interp.fontBody = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_interp.fontSmall = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    g_interp.fontMono = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");

    g_interp.parentWnd = parent;
    g_interp.hwnd = CreateWindowExW(0, INTERP_CLASS, L"Interpretability",
        WS_CHILD | WS_CLIPCHILDREN, 0, 0, 400, 600, parent, nullptr, hInst, nullptr);

    return g_interp.hwnd;
}

void InterpretabilityPanel_SetVisualizationType(int type) {
    g_interp.state.currentType = static_cast<VisualizationType>(type);
    if (g_interp.hwnd) InvalidateRect(g_interp.hwnd, nullptr, FALSE);
}

void InterpretabilityPanel_SetLayerRange(int minLayer, int maxLayer) {
    g_interp.state.minLayer = (std::max)(0, minLayer);
    g_interp.state.maxLayer = (std::max)(g_interp.state.minLayer, maxLayer);
    g_interp.state.selectedLayer = (std::max)(g_interp.state.minLayer, (std::min)(g_interp.state.selectedLayer, g_interp.state.maxLayer));
    if (g_interp.hwnd) InvalidateRect(g_interp.hwnd, nullptr, FALSE);
}

void InterpretabilityPanel_UpdateStats(float tps, float latency, int total, int prompt, int gen, float memMB, int activeLayer, float entropy) {
    g_interp.stats.tokensPerSec = tps;
    g_interp.stats.latencyMs = latency;
    g_interp.stats.totalTokens = total;
    g_interp.stats.promptTokens = prompt;
    g_interp.stats.generatedTokens = gen;
    g_interp.stats.memoryUsageMB = memMB;
    g_interp.stats.activeLayer = activeLayer;
    g_interp.stats.avgAttentionEntropy = entropy;
    if (g_interp.hwnd) InvalidateRect(g_interp.hwnd, nullptr, FALSE);
}

void InterpretabilityPanel_Clear() {
    std::lock_guard<std::mutex> lock(g_interp.dataMutex);
    g_interp.attentionData = {};
    g_interp.layerData = {};
    g_interp.attrData = {};
    g_interp.logitData = {};
    g_interp.stats = {};
    if (g_interp.hwnd) InvalidateRect(g_interp.hwnd, nullptr, FALSE);
}

void InterpretabilityPanel_Show(BOOL show) {
    if (g_interp.hwnd) ShowWindow(g_interp.hwnd, show ? SW_SHOW : SW_HIDE);
}

void InterpretabilityPanel_Resize(int x, int y, int w, int h) {
    if (g_interp.hwnd) MoveWindow(g_interp.hwnd, x, y, w, h, TRUE);
}

} // extern "C"
