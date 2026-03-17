#pragma once
// ============================================================================
// Win32Handle.h — RAII wrappers for Win32 GDI/HWND/HANDLE resources
// Prevents leaks by ensuring automatic cleanup. Finding #5 fix.
// ============================================================================

#include <windows.h>
#include <utility>

namespace RawrXD {

// ── Generic RAII wrapper for Win32 HANDLE objects ────────────────────────────
// Wraps kernel objects (HANDLE) with auto-close semantics.
class ScopedHandle {
public:
    ScopedHandle() noexcept : m_handle(INVALID_HANDLE_VALUE) {}
    explicit ScopedHandle(HANDLE h) noexcept : m_handle(h) {}
    ~ScopedHandle() { reset(); }

    // Move-only
    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.release()) {}
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) { reset(); m_handle = other.release(); }
        return *this;
    }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    HANDLE get() const noexcept { return m_handle; }
    explicit operator bool() const noexcept {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }

    HANDLE release() noexcept {
        HANDLE h = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return h;
    }

    void reset(HANDLE h = INVALID_HANDLE_VALUE) noexcept {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr) {
            ::CloseHandle(m_handle);
        }
        m_handle = h;
    }

private:
    HANDLE m_handle;
};

// ── RAII wrapper for GDI objects (HBRUSH, HFONT, HPEN, HBITMAP, etc.) ───────
template<typename T>
class ScopedGdi {
public:
    ScopedGdi() noexcept : m_obj(nullptr) {}
    explicit ScopedGdi(T obj) noexcept : m_obj(obj) {}
    ~ScopedGdi() { reset(); }

    ScopedGdi(ScopedGdi&& other) noexcept : m_obj(other.release()) {}
    ScopedGdi& operator=(ScopedGdi&& other) noexcept {
        if (this != &other) { reset(); m_obj = other.release(); }
        return *this;
    }
    ScopedGdi(const ScopedGdi&) = delete;
    ScopedGdi& operator=(const ScopedGdi&) = delete;

    T get() const noexcept { return m_obj; }
    explicit operator bool() const noexcept { return m_obj != nullptr; }

    T release() noexcept {
        T obj = m_obj;
        m_obj = nullptr;
        return obj;
    }

    void reset(T obj = nullptr) noexcept {
        if (m_obj) ::DeleteObject(m_obj);
        m_obj = obj;
    }

private:
    T m_obj;
};

// Convenient type aliases
using ScopedBrush   = ScopedGdi<HBRUSH>;
using ScopedFont    = ScopedGdi<HFONT>;
using ScopedPen     = ScopedGdi<HPEN>;
using ScopedBitmap  = ScopedGdi<HBITMAP>;
using ScopedRegion  = ScopedGdi<HRGN>;

// ── RAII wrapper for HMENU ──────────────────────────────────────────────────
class ScopedMenu {
public:
    ScopedMenu() noexcept : m_menu(nullptr) {}
    explicit ScopedMenu(HMENU menu) noexcept : m_menu(menu) {}
    ~ScopedMenu() { reset(); }

    ScopedMenu(ScopedMenu&& other) noexcept : m_menu(other.release()) {}
    ScopedMenu& operator=(ScopedMenu&& other) noexcept {
        if (this != &other) { reset(); m_menu = other.release(); }
        return *this;
    }
    ScopedMenu(const ScopedMenu&) = delete;
    ScopedMenu& operator=(const ScopedMenu&) = delete;

    HMENU get() const noexcept { return m_menu; }
    explicit operator bool() const noexcept { return m_menu != nullptr; }

    HMENU release() noexcept {
        HMENU m = m_menu;
        m_menu = nullptr;
        return m;
    }

    void reset(HMENU menu = nullptr) noexcept {
        if (m_menu) ::DestroyMenu(m_menu);
        m_menu = menu;
    }

private:
    HMENU m_menu;
};

// ── RAII wrapper for HIMAGELIST ─────────────────────────────────────────────
class ScopedImageList {
public:
    ScopedImageList() noexcept : m_list(nullptr) {}
    explicit ScopedImageList(HIMAGELIST list) noexcept : m_list(list) {}
    ~ScopedImageList() { reset(); }

    ScopedImageList(ScopedImageList&& other) noexcept : m_list(other.release()) {}
    ScopedImageList& operator=(ScopedImageList&& other) noexcept {
        if (this != &other) { reset(); m_list = other.release(); }
        return *this;
    }
    ScopedImageList(const ScopedImageList&) = delete;
    ScopedImageList& operator=(const ScopedImageList&) = delete;

    HIMAGELIST get() const noexcept { return m_list; }
    explicit operator bool() const noexcept { return m_list != nullptr; }

    HIMAGELIST release() noexcept {
        HIMAGELIST l = m_list;
        m_list = nullptr;
        return l;
    }

    void reset(HIMAGELIST list = nullptr) noexcept {
        if (m_list) ImageList_Destroy(m_list);
        m_list = list;
    }

private:
    HIMAGELIST m_list;
};

// ── RAII wrapper for HDC (Device Context) ───────────────────────────────────
// For GetDC/ReleaseDC pattern (not CreateDC)
class ScopedDC {
public:
    ScopedDC(HWND hwnd) noexcept : m_hwnd(hwnd), m_hdc(::GetDC(hwnd)) {}
    ~ScopedDC() { if (m_hdc) ::ReleaseDC(m_hwnd, m_hdc); }

    ScopedDC(const ScopedDC&) = delete;
    ScopedDC& operator=(const ScopedDC&) = delete;

    HDC get() const noexcept { return m_hdc; }
    explicit operator bool() const noexcept { return m_hdc != nullptr; }

private:
    HWND m_hwnd;
    HDC m_hdc;
};

// ── Select-into-DC RAII guard ───────────────────────────────────────────────
// Selects a GDI object into an HDC on construction, restores on destruction
class ScopedSelectObject {
public:
    ScopedSelectObject(HDC hdc, HGDIOBJ obj) noexcept
        : m_hdc(hdc), m_old(::SelectObject(hdc, obj)) {}
    ~ScopedSelectObject() { if (m_old) ::SelectObject(m_hdc, m_old); }

    ScopedSelectObject(const ScopedSelectObject&) = delete;
    ScopedSelectObject& operator=(const ScopedSelectObject&) = delete;

private:
    HDC m_hdc;
    HGDIOBJ m_old;
};

} // namespace RawrXD
