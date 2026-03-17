#pragma once
// renderer.h - Minimal renderer interface stub

#include <windows.h>

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool initialize(HWND hwnd) = 0;
    virtual void shutdown() = 0;
    virtual void render() = 0;
    virtual void resize(int width, int height) = 0;
};

// Default stub implementation
class StubRenderer : public IRenderer {
public:
    bool initialize(HWND hwnd) override { m_hwnd = hwnd; return true; }
    void shutdown() override {}
    void render() override {}
    void resize(int width, int height) override { m_w = width; m_h = height; }
private:
    HWND m_hwnd = nullptr;
    int m_w = 0, m_h = 0;
};
