// Direct2D_HeatmapRenderer.hpp - D2D-based EXPERT heatmap for TITAN cluster.
// High-performance overlay for mo-experts (MoE) 120 FPS observability.
#pragma once

#ifndef RAWRXD_DIRECT2D_HEATMAP_RENDERER_HPP
#define RAWRXD_DIRECT2D_HEATMAP_RENDERER_HPP

#include <windows.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

namespace RawrXD::Graphics {

struct ExpertHeatmapData {
    uint32_t expertId;
    int deviceOrdinal;
    float usageIntensity; // 0.0 to 1.0 (opacity)
    float latencyMs;
    bool inUse;
    bool isPinned;
    bool isHighTraffic;
    
    // Batch 11: Integrity Metadata
    bool isModified;      // True if .text hash mismatch detected
    bool isHealing;       // True if Shadow-Page rollback in progress
    bool isTampered;      // True if Hard Tamper (Debugger) detected
};

class Direct2D_HeatmapRenderer {
public:
    static Direct2D_HeatmapRenderer& Instance();

    // Lifecycle
    bool Initialize(HWND parentHwnd);
    void Shutdown();
    void Resize(int width, int height);

    // Visibility
    void Show(bool visible);
    bool IsVisible() const { return m_visible.load(); }
    void Toggle();

    // Data Ingestion (Bridge to Backend)
    void UpdateData(const std::vector<ExpertHeatmapData>& data);
    
    // Manual trigger for refresh
    void Invalidate();

    // Direct WndProc access for the overlay window
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    Direct2D_HeatmapRenderer();
    ~Direct2D_HeatmapRenderer();

    // D2D Internals
    bool CreateDeviceResources();
    void DiscardDeviceResources();
    HRESULT Render();

    // Grid Logic
    void CalculateLayout();

    // Window Management
    HWND m_hwnd = nullptr;
    HWND m_parentHwnd = nullptr;
    std::atomic<bool> m_visible{false};
    std::atomic<bool> m_ready{true}; // Always ready after ctor

    // D2D Factory & Targets
    ID2D1Factory1* m_pD2DFactory = nullptr;
    ID2D1HwndRenderTarget* m_pRenderTarget = nullptr;
    IDWriteFactory3* m_pDWriteFactory = nullptr;

    // Brushes & Formats
    ID2D1SolidColorBrush* m_pActiveBrush = nullptr;
    ID2D1SolidColorBrush* m_pInactiveBrush = nullptr;
    ID2D1SolidColorBrush* m_pPinnedBrush = nullptr;
    ID2D1SolidColorBrush* m_pHighTrafficBrush = nullptr;
    ID2D1SolidColorBrush* m_pTextBrush = nullptr;
    ID2D1SolidColorBrush* m_pBgBrush = nullptr;
    IDWriteTextFormat* m_pTextFormat = nullptr;

    // Data Store
    std::mutex m_dataMutex;
    std::vector<ExpertHeatmapData> m_experts;
    
    // Layout
    UINT m_width = 800;
    UINT m_height = 600;
    const int m_cols = 8;
    float m_cellSize = 64.0f;
    float m_padding = 4.0f;
};

} // namespace RawrXD::Graphics

#endif // RAWRXD_DIRECT2D_HEATMAP_RENDERER_HPP

