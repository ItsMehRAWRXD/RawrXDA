#pragma once
// renderer.h - Minimal renderer interface stub

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual bool initialize(void* hwnd) = 0;
    virtual void shutdown() = 0;
    virtual void render() = 0;
    virtual void resize(int width, int height) = 0;
};
