#pragma once

#include <windows.h>
#include <string>
#include <new>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "../hotpatch/sovereign_bridge.h"

namespace rawrxd {
namespace win32bridge {

constexpr UINT WM_RAWRXD_STREAM_BEACON = WM_APP + 640;
constexpr UINT WM_RAWRXD_SOVEREIGN_DIAGNOSTIC = RAWRXD_WM_DIAGNOSTIC_MARKER;

inline void postBeacon(HWND target, const std::string& line) {
    if (!target || line.empty()) {
        return;
    }

    std::string* payload = new (std::nothrow) std::string(line);
    if (!payload) {
        return;
    }

    if (!PostMessageA(target, WM_RAWRXD_STREAM_BEACON, 0, reinterpret_cast<LPARAM>(payload))) {
        delete payload;
    }
}

inline bool handleBeaconMessage(HWND listBox, WPARAM, LPARAM lParam) {
    std::string* line = reinterpret_cast<std::string*>(lParam);
    if (!line) {
        return false;
    }

    if (listBox) {
        SendMessageA(listBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line->c_str()));
        SendMessageA(listBox, LB_SETTOPINDEX, static_cast<WPARAM>(SendMessageA(listBox, LB_GETCOUNT, 0, 0) - 1), 0);
    }

    delete line;
    return true;
}

inline bool postSovereignDiagnostic(HWND target, float confidence) {
    return EmitSovereignMarker(target, confidence, reinterpret_cast<void*>(&::PostMessageA));
}

inline bool handleSovereignDiagnosticMessage(HWND listBox, WPARAM wParam, LPARAM lParam) {
    if (static_cast<std::uint64_t>(wParam) != RAWRXD_SOVEREIGN_MARKER) {
        return false;
    }

    float confidence = 0.0f;
    const std::uint32_t bits = static_cast<std::uint32_t>(lParam & 0xffffffffu);
    std::memcpy(&confidence, &bits, sizeof(confidence));

    if (listBox) {
        char line[128] = {};
        std::snprintf(line, sizeof(line), "SOVEREIGN 0x%llX confidence=%.4f",
                      static_cast<unsigned long long>(RAWRXD_SOVEREIGN_MARKER), confidence);
        SendMessageA(listBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line));
        SendMessageA(listBox, LB_SETTOPINDEX, static_cast<WPARAM>(SendMessageA(listBox, LB_GETCOUNT, 0, 0) - 1), 0);
    }

    return true;
}

} // namespace win32bridge
} // namespace rawrxd
