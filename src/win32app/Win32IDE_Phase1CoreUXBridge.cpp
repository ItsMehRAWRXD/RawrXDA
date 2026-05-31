#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

uint64_t ClampCopy(const char* src, char* dst, uint32_t cap) {
    if (!dst || cap == 0) {
        return 0;
    }
    if (!src) {
        dst[0] = '\0';
        return 0;
    }

    const size_t srcLen = std::strlen(src);
    const size_t copyLen = std::min(srcLen, static_cast<size_t>(cap - 1));
    std::memcpy(dst, src, copyLen);
    dst[copyLen] = '\0';
    return static_cast<uint64_t>(copyLen);
}

}  // namespace

extern "C" void Editor_GetCursorPixelPos(HWND editor,
                                           uint64_t /*line*/,
                                           uint64_t /*col*/,
                                           DWORD* outX,
                                           DWORD* outY) {
    if (outX) {
        *outX = 0;
    }
    if (outY) {
        *outY = 0;
    }

    if (!editor || !outX || !outY) {
        return;
    }

    POINT pt{};
    if (GetCaretPos(&pt)) {
        *outX = static_cast<DWORD>(pt.x);
        *outY = static_cast<DWORD>(pt.y);
    }
}

extern "C" uint64_t Editor_GetLinePrefixA(HWND editor,
                                            uint64_t /*line*/,
                                            char* outBuf,
                                            uint32_t outCap) {
    if (!editor || !outBuf || outCap == 0) {
        return 0;
    }

    const LRESULT textLenRaw = SendMessageA(editor, WM_GETTEXTLENGTH, 0, 0);
    if (textLenRaw <= 0) {
        outBuf[0] = '\0';
        return 0;
    }

    const size_t textLen = static_cast<size_t>(textLenRaw);
    std::vector<char> tmp(textLen + 1, '\0');
    SendMessageA(editor, WM_GETTEXT, static_cast<WPARAM>(tmp.size()), reinterpret_cast<LPARAM>(tmp.data()));

    const char* firstNewline = std::strpbrk(tmp.data(), "\r\n");
    if (!firstNewline) {
        return ClampCopy(tmp.data(), outBuf, outCap);
    }

    const size_t prefixLen = static_cast<size_t>(firstNewline - tmp.data());
    const size_t copyLen = std::min(prefixLen, static_cast<size_t>(outCap - 1));
    std::memcpy(outBuf, tmp.data(), copyLen);
    outBuf[copyLen] = '\0';
    return static_cast<uint64_t>(copyLen);
}

extern "C" void Editor_InsertTextA(HWND editor, const char* text, uint32_t len) {
    if (!editor || !text || len == 0) {
        return;
    }

    std::string payload(text, text + len);
    SendMessageA(editor, EM_REPLACESEL, static_cast<WPARAM>(TRUE), reinterpret_cast<LPARAM>(payload.c_str()));
}
