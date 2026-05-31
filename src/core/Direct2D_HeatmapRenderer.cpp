#include "Direct2D_HeatmapRenderer.hpp"
#include "sentinel_watchdog.hpp"
#include <d2d1_1.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "d2d1.lib")

// Batch 11: Color Palette for Integrity Visualization
namespace RawrXD::Graphics::Colors {
    const D2D1_COLOR_F HEALTHY = D2D1::ColorF(D2D1::ColorF::Lime, 1.0f);
    const D2D1_COLOR_F HEALING = D2D1::ColorF(D2D1::ColorF::Gold, 1.0f);
    const D2D1_COLOR_F TAMPERED = D2D1::ColorF(D2D1::ColorF::Crimson, 1.0f);
    const D2D1_COLOR_F INACTIVE = D2D1::ColorF(D2D1::ColorF::SlateGray, 0.3f);
}

void Direct2D_HeatmapRenderer::Initialize(HWND hwndParent) {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pDirect2dFactory);
    if (SUCCEEDED(hr)) {
        RECT rect;
        GetClientRect(hwndParent, &rect);
        Resize(rect.right, rect.bottom);
        CreateResources(hwndParent);
    }
}

Direct2D_HeatmapRenderer::~Direct2D_HeatmapRenderer() {
    DiscardResources();
    if (m_pDirect2dFactory) m_pDirect2dFactory->Release();
}

void Direct2D_HeatmapRenderer::DiscardResources() {
    if (m_pRenderTarget) { m_pRenderTarget->Release(); m_pRenderTarget = nullptr; }
    if (m_pGridBrush) { m_pGridBrush->Release(); m_pGridBrush = nullptr; }
    if (m_pActiveBrush) { m_pActiveBrush->Release(); m_pActiveBrush = nullptr; }
    if (m_pInactiveBrush) { m_pInactiveBrush->Release(); m_pInactiveBrush = nullptr; }
}

void Direct2D_HeatmapRenderer::CreateResources(HWND hwnd) {
    if (!m_pRenderTarget) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);
        
        m_pDirect2dFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(hwnd, size),
            &m_pRenderTarget
        );

        if (m_pRenderTarget) {
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray, 0.5f), &m_pGridBrush);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Cyan, 1.0f), &m_pActiveBrush);
            m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::SlateGray, 0.3f), &m_pInactiveBrush);
        }
    }
}

void Direct2D_HeatmapRenderer::UpdateDensityMap(const std::vector<ExpertHeatmapData>& data) {
    std::lock_guard<std::mutex> lock(m_dataMutex);
    m_expertData = data;
}

void Direct2D_HeatmapRenderer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }
}

void Direct2D_HeatmapRenderer::Render(HWND hwnd) {
    CreateResources(hwnd);
    if (!m_pRenderTarget) return;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));

    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_expertData.empty()) {
        const int cols = 8;
        const int rows = static_cast<int>(std::ceil(m_expertData.size() / 8.0f));
        const float cellW = static_cast<float>(m_width) / cols;
        const float cellH = static_cast<float>(m_height) / rows;

        for (size_t i = 0; i < m_expertData.size(); ++i) {
            const auto& expert = m_expertData[i];
            int r = i / cols;
            int c = i % cols;
            
            D2D1_RECT_F rect = D2D1::RectF(
                c * cellW + 2, r * cellH + 2,
                (c + 1) * cellW - 2, (r + 1) * cellH - 2
            );

            // ---- BATCH 11: INTEGRITY HEATMAP SHADING ----
            // Color mapping based on Sentinel status for real-time observability
            ID2D1SolidColorBrush* brush = m_pInactiveBrush;
            
            // ---- BATCH 11: PULSE SHADER ANIMATION ----
            // Use a sine-based pulse for blocks currently in "Healing" state
            float pulseOpacity = 1.0f;
            if (expert.isHealing) {
                static float s_pulseTime = 0.0f;
                s_pulseTime += 0.01f; // Simulated time step
                pulseOpacity = 0.5f + 0.5f * sinf(s_pulseTime * 5.0f);
            }

            if (expert.isTampered) {
                m_pActiveBrush->SetColor(Colors::TAMPERED);
                brush = m_pActiveBrush;
                m_pActiveBrush->SetOpacity(1.0f);
            } else if (expert.isHealing) {
                m_pActiveBrush->SetColor(Colors::HEALING);
                brush = m_pActiveBrush;
                m_pActiveBrush->SetOpacity(pulseOpacity);
            } else if (expert.isModified || expert.inUse) {
                m_pActiveBrush->SetColor(Colors::HEALTHY);
                brush = m_pActiveBrush;
                m_pActiveBrush->SetOpacity(0.3f + 0.7f * expert.usageIntensity);
            } else {
                brush = m_pInactiveBrush;
            }

            m_pRenderTarget->FillRectangle(&rect, brush);
            m_pRenderTarget->DrawRectangle(&rect, m_pGridBrush);
        }
    }

    HRESULT hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        DiscardResources();
    }
}
