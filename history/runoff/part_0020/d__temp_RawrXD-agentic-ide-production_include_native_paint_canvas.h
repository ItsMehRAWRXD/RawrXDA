#pragma once

#include <windows.h>
#include <string>
#include "native_widgets.h"

struct DrawingState;

class NativePaintCanvas : public NativeWidget {
public:
    NativePaintCanvas(int width, int height, NativeWidget* parent = nullptr);
    ~NativePaintCanvas();
    
    // Canvas operations
    void clearCanvas();
    void setTool(int tool);
    void setForegroundColor(int r, int g, int b);
    void setBackgroundColor(int r, int g, int b);
    void setBrushSize(int size);
    
    // Drawing primitives
    void drawLine(int x1, int y1, int x2, int y2);
    void drawCircle(int cx, int cy, int radius, bool filled = false);
    void drawRectangle(int x, int y, int width, int height, bool filled = false);
    void drawPixel(int x, int y);
    void fillRect(int x, int y, int width, int height);
    void drawBrushStroke(int x, int y);
    void eraseArea(int x, int y);
    
    // Stroke management
    void startStroke(int x, int y);
    void continueStroke(int x, int y);
    void endStroke();
    
    // Dimensions
    int getWidth() const;
    int getHeight() const;
    
    // File operations
    void saveToPNG(const std::string& filename);
    void loadFromPNG(const std::string& filename);
    
    // Event handlers (to be implemented)
    virtual void onMousePress(int x, int y, int button);
    virtual void onMouseRelease(int x, int y, int button);
    virtual void onMouseMove(int x, int y, int buttons);
    virtual void onWheel(int delta);
    
private:
    int m_width, m_height;
    HDC m_memoryDC;
    HBITMAP m_bitmap;
    DrawingState* m_drawingState;
};