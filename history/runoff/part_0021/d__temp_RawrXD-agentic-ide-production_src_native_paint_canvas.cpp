#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "windows_gui_framework.h"
#include <windows.h>
#include <iostream>
#include <cmath>

// Stub implementation - GDI+ removed to avoid ATL dependency

class GdiPlusInitializer {
public:
    GdiPlusInitializer() {}
    ~GdiPlusInitializer() {}
};

static GdiPlusInitializer g_gdiplusInit;

// Helper function to get encoder CLSID - STUB
static int GetEncoderClsid(const WCHAR* format, void* pClsid) {
    return -1;  // Stub implementation
}

// Drawing state management
struct DrawingState {
    int currentTool = 0;           // 0=brush, 1=eraser, 2=line, 3=rect, 4=circle
    int brushSize = 3;
    int foregroundColor = RGB(0, 0, 0);
    int backgroundColor = RGB(255, 255, 255);
    bool isDrawing = false;
    int lastX = 0, lastY = 0;
};

NativePaintCanvas::NativePaintCanvas(int width, int height, NativeWidget* parent)
    : NativeWidget(parent), m_width(width), m_height(height),
      m_memoryDC(nullptr), m_bitmap(nullptr), m_drawingState(new DrawingState)
{
    if (parent && parent->getHandle()) {
        m_hwnd = CreateWindow(
            "STATIC", "",
            WS_VISIBLE | WS_CHILD | SS_BITMAP,
            0, 0, width, height,
            parent->getHandle(), nullptr, GetModuleHandle(nullptr), nullptr
        );
        
        // Create memory DC for drawing
        HDC screenDC = GetDC(m_hwnd);
        m_memoryDC = CreateCompatibleDC(screenDC);
        m_bitmap = CreateCompatibleBitmap(screenDC, width, height);
        SelectObject(m_memoryDC, m_bitmap);
        ReleaseDC(m_hwnd, screenDC);
        
        // Clear canvas to white
        clearCanvas();
    }
}

NativePaintCanvas::~NativePaintCanvas() {
    if (m_memoryDC) {
        DeleteDC(m_memoryDC);
    }
    if (m_bitmap) {
        DeleteObject(m_bitmap);
    }
    delete m_drawingState;
}

void NativePaintCanvas::clearCanvas() {
    if (m_memoryDC) {
        RECT rect = {0, 0, m_width, m_height};
        HBRUSH bgBrush = CreateSolidBrush(m_drawingState->backgroundColor);
        FillRect(m_memoryDC, &rect, bgBrush);
        DeleteObject(bgBrush);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void NativePaintCanvas::setTool(int tool) {
    m_drawingState->currentTool = tool;
    std::cout << "PaintCanvas: Set tool to " << tool << std::endl;
}

void NativePaintCanvas::setForegroundColor(int r, int g, int b) {
    m_drawingState->foregroundColor = RGB(r, g, b);
    std::cout << "PaintCanvas: Set foreground color to RGB(" << r << "," << g << "," << b << ")" << std::endl;
}

void NativePaintCanvas::setBackgroundColor(int r, int g, int b) {
    m_drawingState->backgroundColor = RGB(r, g, b);
    std::cout << "PaintCanvas: Set background color to RGB(" << r << "," << g << "," << b << ")" << std::endl;
}

void NativePaintCanvas::setBrushSize(int size) {
    m_drawingState->brushSize = std::max(1, std::min(50, size));
    std::cout << "PaintCanvas: Brush size set to " << m_drawingState->brushSize << std::endl;
}

void NativePaintCanvas::drawLine(int x1, int y1, int x2, int y2) {
    if (!m_memoryDC) return;
    
    HPEN pen = CreatePen(PS_SOLID, m_drawingState->brushSize, m_drawingState->foregroundColor);
    HPEN oldPen = (HPEN)SelectObject(m_memoryDC, pen);
    
    MoveToEx(m_memoryDC, x1, y1, nullptr);
    LineTo(m_memoryDC, x2, y2);
    
    SelectObject(m_memoryDC, oldPen);
    DeleteObject(pen);
    
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativePaintCanvas::drawCircle(int cx, int cy, int radius, bool filled) {
    if (!m_memoryDC) return;
    
    HBRUSH brush = nullptr;
    HBRUSH oldBrush = nullptr;
    
    if (filled) {
        brush = CreateSolidBrush(m_drawingState->foregroundColor);
        oldBrush = (HBRUSH)SelectObject(m_memoryDC, brush);
    }
    
    HPEN pen = CreatePen(PS_SOLID, m_drawingState->brushSize, m_drawingState->foregroundColor);
    HPEN oldPen = (HPEN)SelectObject(m_memoryDC, pen);
    
    Ellipse(m_memoryDC, cx - radius, cy - radius, cx + radius, cy + radius);
    
    SelectObject(m_memoryDC, oldPen);
    DeleteObject(pen);
    
    if (filled && oldBrush) {
        SelectObject(m_memoryDC, oldBrush);
        DeleteObject(brush);
    }
    
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativePaintCanvas::drawRectangle(int x, int y, int width, int height, bool filled) {
    if (!m_memoryDC) return;
    
    HBRUSH brush = nullptr;
    HBRUSH oldBrush = nullptr;
    
    if (filled) {
        brush = CreateSolidBrush(m_drawingState->foregroundColor);
        oldBrush = (HBRUSH)SelectObject(m_memoryDC, brush);
    }
    
    HPEN pen = CreatePen(PS_SOLID, m_drawingState->brushSize, m_drawingState->foregroundColor);
    HPEN oldPen = (HPEN)SelectObject(m_memoryDC, pen);
    
    Rectangle(m_memoryDC, x, y, x + width, y + height);
    
    SelectObject(m_memoryDC, oldPen);
    DeleteObject(pen);
    
    if (filled && oldBrush) {
        SelectObject(m_memoryDC, oldBrush);
        DeleteObject(brush);
    }
    
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativePaintCanvas::drawPixel(int x, int y) {
    if (!m_memoryDC) return;
    
    SetPixel(m_memoryDC, x, y, m_drawingState->foregroundColor);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativePaintCanvas::fillRect(int x, int y, int width, int height) {
    if (!m_memoryDC) return;
    
    RECT rect = {x, y, x + width, y + height};
    HBRUSH brush = CreateSolidBrush(m_drawingState->foregroundColor);
    FillRect(m_memoryDC, &rect, brush);
    DeleteObject(brush);
    
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativePaintCanvas::drawBrushStroke(int x, int y) {
    if (!m_memoryDC) return;
    
    int brushSize = m_drawingState->brushSize;
    
    // Draw filled circle at cursor position for brush effect
    drawCircle(x, y, brushSize / 2, true);
}

void NativePaintCanvas::eraseArea(int x, int y) {
    if (!m_memoryDC) return;
    
    RECT rect = {
        x - m_drawingState->brushSize / 2,
        y - m_drawingState->brushSize / 2,
        x + m_drawingState->brushSize / 2,
        y + m_drawingState->brushSize / 2
    };
    
    HBRUSH bgBrush = CreateSolidBrush(m_drawingState->backgroundColor);
    FillRect(m_memoryDC, &rect, bgBrush);
    DeleteObject(bgBrush);
    
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void NativePaintCanvas::startStroke(int x, int y) {
    m_drawingState->isDrawing = true;
    m_drawingState->lastX = x;
    m_drawingState->lastY = y;
}

void NativePaintCanvas::continueStroke(int x, int y) {
    if (!m_drawingState->isDrawing) return;
    
    switch (m_drawingState->currentTool) {
        case 0: // Brush
            drawLine(m_drawingState->lastX, m_drawingState->lastY, x, y);
            break;
        case 1: // Eraser
            eraseArea(x, y);
            break;
    }
    
    m_drawingState->lastX = x;
    m_drawingState->lastY = y;
}

void NativePaintCanvas::endStroke() {
    m_drawingState->isDrawing = false;
}

int NativePaintCanvas::getWidth() const {
    return m_width;
}

int NativePaintCanvas::getHeight() const {
    return m_height;
}

void NativePaintCanvas::saveToPNG(const std::string& filename) {
    // GDI+ is not used in this build; provide a stub
    if (!m_memoryDC || !m_bitmap) {
        std::cerr << "PaintCanvas: Cannot save - no canvas initialized" << std::endl;
        return;
    }
    std::cerr << "PaintCanvas: PNG save not available in this build" << std::endl;
}

void NativePaintCanvas::loadFromPNG(const std::string& filename) {
    // GDI+ is not used in this build; provide a stub
    std::cerr << "PaintCanvas: PNG load not available in this build" << std::endl;
}