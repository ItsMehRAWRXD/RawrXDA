#pragma once

#include <windows.h>
#include <string>
#include <functional>
#include <map>
#include <memory>

// Windows Native GUI Framework
class NativeWindow {
public:
    NativeWindow();
    ~NativeWindow();
    
    bool create(const std::string& title, int width, int height);
    void show();
    void hide();
    void close();
    
    HWND getHandle() const { return m_hwnd; }
    void setOnClose(std::function<void()> callback);
    void setOnResize(std::function<void(int, int)> callback);

    friend LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
private:
    HWND m_hwnd;
    std::function<void()> m_onClose;
    std::function<void(int, int)> m_onResize;
};

class NativeWidget {
public:
    NativeWidget(NativeWidget* parent = nullptr);
    virtual ~NativeWidget();
    
    HWND getHandle() const { return m_hwnd; }
    void setVisible(bool visible);
    void setGeometry(int x, int y, int width, int height);
    void setText(const std::string& text);
    
protected:
    HWND m_hwnd;
    NativeWidget* m_parent;
};

class NativeButton : public NativeWidget {
public:
    NativeButton(NativeWidget* parent = nullptr);
    void setOnClick(std::function<void()> callback);
    
private:
    std::function<void()> m_onClick;
};

class NativeTextEditor : public NativeWidget {
public:
    NativeTextEditor(NativeWidget* parent = nullptr);
    std::string getText() const;
    void setBackgroundColor(int r, int g, int b);
    void setTextColor(int r, int g, int b);
    void setFont(const std::string& family, int size);
};

class NativeComboBox : public NativeWidget {
public:
    NativeComboBox(NativeWidget* parent = nullptr);
    void addItem(const std::string& text);
    void clear();
    int getSelectedIndex() const;
    void setOnChange(std::function<void(int)> callback);
    
private:
    std::function<void(int)> m_onChange;
};

class NativeSlider : public NativeWidget {
public:
    NativeSlider(NativeWidget* parent = nullptr);
    void setRange(int min, int max);
    void setValue(int value);
    int getValue() const;
    void setOnValueChanged(std::function<void(int)> callback);
    
private:
    std::function<void(int)> m_onValueChanged;
};

class NativeLabel : public NativeWidget {
public:
    NativeLabel(NativeWidget* parent = nullptr);
};

class NativeTabWidget : public NativeWidget {
public:
    NativeTabWidget(NativeWidget* parent = nullptr);
    ~NativeTabWidget();
    void addTab(const std::string& title, NativeWidget* widget);
    void removeTab(int index);
    int getCurrentIndex() const;
    void setOnTabChanged(std::function<void(int)> callback);
    void initializeTabHandling();
    
private:
    std::function<void(int)> m_onTabChanged;
};

// Paint Canvas for drawing
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
    
private:
    int m_width, m_height;
    HDC m_memoryDC;
    HBITMAP m_bitmap;
    struct DrawingState* m_drawingState;
};

// Layout managers
class NativeLayout {
public:
    virtual void addWidget(NativeWidget* widget, int stretch = 0) = 0;
    virtual void setMargins(int left, int top, int right, int bottom) = 0;
    virtual void setSpacing(int spacing) = 0;
};

class NativeHBoxLayout : public NativeLayout {
public:
    NativeHBoxLayout(NativeWidget* parent);
    void addWidget(NativeWidget* widget, int stretch = 0) override;
    void setMargins(int left, int top, int right, int bottom) override;
    void setSpacing(int spacing) override;
    
private:
    NativeWidget* m_parent;
};

class NativeVBoxLayout : public NativeLayout {
public:
    NativeVBoxLayout(NativeWidget* parent);
    void addWidget(NativeWidget* widget, int stretch = 0) override;
    void setMargins(int left, int top, int right, int bottom) override;
    void setSpacing(int spacing) override;
    
private:
    NativeWidget* m_parent;
};