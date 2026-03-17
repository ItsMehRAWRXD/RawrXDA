#pragma once

#include <string>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class CICDSettings {
public:
    using ShowCallback = void(*)(void* ctx);

    explicit CICDSettings(void* parent = nullptr)
        : m_parent(parent) {}

    ~CICDSettings() = default;

    void setShowCallback(ShowCallback cb, void* ctx) {
        m_showCb = cb;
        m_showCtx = ctx;
    }

    void show() {
        if (m_showCb) {
            m_showCb(m_showCtx ? m_showCtx : m_parent);
            return;
        }
#if defined(_WIN32)
        MessageBoxA(static_cast<HWND>(m_parent),
                    "CI/CD settings dialog callback is not wired yet.",
                    "RawrXD CI/CD Settings",
                    MB_OK | MB_ICONINFORMATION);
#endif
    }

private:
    void* m_parent = nullptr;
    ShowCallback m_showCb = nullptr;
    void* m_showCtx = nullptr;
};
