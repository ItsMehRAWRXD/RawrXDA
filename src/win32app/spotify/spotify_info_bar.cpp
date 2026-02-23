// ============================================================================
// spotify_info_bar.cpp — Moveable Spotify bar implementation
// ============================================================================

#include "spotify_info_bar.hpp"
#include <commctrl.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include <thread>
#include <mutex>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "ws2_32.lib")

namespace RawrXD {

namespace {

const wchar_t kClassName[] = L"RawrXD_SpotifyInfoBar";
#define WM_SPOTIFY_LOGIN_DONE (WM_APP + 1)

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &out[0], n);
    return out;
}

// Minimal callback server: one connection, parse code from GET /callback?code=...
void runCallbackServer(std::string* outCode, std::mutex* mutex, HANDLE readyEvent,
                       const std::string& redirectUri) {
    // Parse port from redirectUri (e.g. http://localhost:8765/callback -> 8765)
    int port = 8765;
    size_t p = redirectUri.find("://");
    if (p != std::string::npos) {
        size_t start = p + 3;
        size_t colon = redirectUri.find(':', start);
        size_t slash = redirectUri.find('/', start);
        if (colon != std::string::npos && (slash == std::string::npos || colon < slash))
            port = std::atoi(redirectUri.substr(colon + 1, slash - colon - 1).c_str());
    }
    WSADATA wsa = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return;
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) { WSACleanup(); return; }
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((u_short)port);
    if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)) != 0) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }
    if (listen(listenSocket, 1) != 0) {
        closesocket(listenSocket);
        WSACleanup();
        return;
    }
    SetEvent(readyEvent);
    SOCKET client = accept(listenSocket, nullptr, nullptr);
    closesocket(listenSocket);
    if (client == INVALID_SOCKET) { WSACleanup(); return; }
    char buf[2048] = {};
    int n = recv(client, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::string line(buf);
        size_t q = line.find('?');
        size_t codeStart = line.find("code=", q != std::string::npos ? q : 0);
        if (codeStart != std::string::npos) {
            codeStart += 5;
            size_t codeEnd = line.find('&', codeStart);
            if (codeEnd == std::string::npos) codeEnd = line.find(' ', codeStart);
            if (codeEnd == std::string::npos) codeEnd = line.size();
            std::string code = line.substr(codeStart, codeEnd - codeStart);
            std::lock_guard<std::mutex> lock(*mutex);
            *outCode = code;
        }
    }
    const char* response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
        "<html><body><p>Logged in. You can close this window.</p></body></html>";
    send(client, response, (int)strlen(response), 0);
    closesocket(client);
    WSACleanup();
}

} // anonymous

SpotifyInfoBar::SpotifyInfoBar(HINSTANCE hInstance) : m_hInstance(hInstance) {}

SpotifyInfoBar::~SpotifyInfoBar() {
    stopPollTimer();
    if (m_hwnd) DestroyWindow(m_hwnd);
    m_hwnd = nullptr;
}

void SpotifyInfoBar::setPersistTokensCallback(SpotifyClient::PersistTokensCallback cb) {
    if (m_client) m_client->setPersistTokensCallback(std::move(cb));
}

void SpotifyInfoBar::show() {
    if (!m_hwnd) createWindow();
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        updateContent();
        startPollTimer();
    }
}

void SpotifyInfoBar::hide() {
    stopPollTimer();
    if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
}

void SpotifyInfoBar::toggle() {
    if (!m_hwnd) createWindow();
    if (m_hwnd) {
        BOOL vis = IsWindowVisible(m_hwnd);
        if (vis) hide();
        else show();
    }
}

bool SpotifyInfoBar::isVisible() const {
    return m_hwnd && IsWindowVisible(m_hwnd);
}

void SpotifyInfoBar::startLoginFlow() {
    if (!m_client || m_client->getClientId().empty()) return;
    std::string url = m_client->getAuthorizationUrl();
    if (url.empty()) return;
    std::wstring wUrl = utf8ToWide(url);
    ShellExecuteW(nullptr, L"open", wUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    static std::string s_loginCode;
    static std::mutex s_loginMutex;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    std::thread t([this, ready]() {
        runCallbackServer(&s_loginCode, &s_loginMutex, ready, m_client->getRedirectUri());
        if (m_hwnd) {
            std::string code;
            { std::lock_guard<std::mutex> lock(s_loginMutex); code = s_loginCode; s_loginCode.clear(); }
            if (!code.empty())
                PostMessageW(m_hwnd, WM_SPOTIFY_LOGIN_DONE, 0, (LPARAM)new std::string(code));
        }
    });
    t.detach();
    WaitForSingleObject(ready, 5000);
    CloseHandle(ready);
}

void SpotifyInfoBar::onCodeReceived(const std::string& code) {
    if (m_client && m_client->exchangeCodeForTokens(code)) {
        m_loggedIn = true;
        updateContent();
        startPollTimer();
    }
}

void SpotifyInfoBar::createWindow() {
    if (m_hwnd) return;
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;

    int x = 100, y = 100;
    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        kClassName,
        L"Spotify",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        x, y, WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr, m_hInstance, this
    );
    if (!m_hwnd) return;
    SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, (LONG_PTR)this);

    DWORD st = WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX;
    m_hwndUser = CreateWindowExW(0, L"STATIC", L"User", st, 8, 8, 120, 18, m_hwnd, nullptr, m_hInstance, nullptr);
    m_hwndTrack = CreateWindowExW(0, L"STATIC", L"Track — Artist", st, 8, 28, 280, 32, m_hwnd, nullptr, m_hInstance, nullptr);
    m_hwndPrev = CreateWindowExW(0, L"BUTTON", L"Prev", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 300, 12, 44, 36, m_hwnd, (HMENU)1, m_hInstance, nullptr);
    m_hwndPlayPause = CreateWindowExW(0, L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 348, 12, 44, 36, m_hwnd, (HMENU)2, m_hInstance, nullptr);
    m_hwndNext = CreateWindowExW(0, L"BUTTON", L"Next", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 396, 12, 44, 36, m_hwnd, (HMENU)3, m_hInstance, nullptr);
    m_hwndLogin = CreateWindowExW(0, L"BUTTON", L"Log in with Spotify", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 8, 20, 160, 32, m_hwnd, (HMENU)4, m_hInstance, nullptr);

    SendMessageW(m_hwndUser, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    SendMessageW(m_hwndTrack, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    layout();
}

void SpotifyInfoBar::layout() {
    if (!m_hwnd) return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (m_loggedIn) {
        ShowWindow(m_hwndLogin, SW_HIDE);
        ShowWindow(m_hwndUser, SW_SHOW);
        ShowWindow(m_hwndTrack, SW_SHOW);
        ShowWindow(m_hwndPrev, SW_SHOW);
        ShowWindow(m_hwndPlayPause, SW_SHOW);
        ShowWindow(m_hwndNext, SW_SHOW);
        MoveWindow(m_hwndUser, 8, 8, 120, 18, TRUE);
        MoveWindow(m_hwndTrack, 8, 28, w - 160, 32, TRUE);
        int cx = w - 140;
        MoveWindow(m_hwndPrev, cx, 12, 44, 36, TRUE);
        MoveWindow(m_hwndPlayPause, cx + 48, 12, 44, 36, TRUE);
        MoveWindow(m_hwndNext, cx + 96, 12, 44, 36, TRUE);
    } else {
        ShowWindow(m_hwndUser, SW_HIDE);
        ShowWindow(m_hwndTrack, SW_HIDE);
        ShowWindow(m_hwndPrev, SW_HIDE);
        ShowWindow(m_hwndPlayPause, SW_HIDE);
        ShowWindow(m_hwndNext, SW_HIDE);
        ShowWindow(m_hwndLogin, SW_SHOW);
        MoveWindow(m_hwndLogin, 8, (h - 32) / 2, 160, 32, TRUE);
    }
}

void SpotifyInfoBar::updateContent() {
    if (!m_client) return;
    if (!m_client->hasValidToken()) {
        if (m_client->getRefreshToken().empty()) {
            m_loggedIn = false;
            if (m_hwndUser) SetWindowTextW(m_hwndUser, L"Not logged in");
            if (m_hwndTrack) SetWindowTextW(m_hwndTrack, L"");
            layout();
            return;
        }
        m_client->refreshAccessToken();
    }
    m_loggedIn = true;
    if (m_client->getCurrentUser(m_user)) {
        SetWindowTextW(m_hwndUser, utf8ToWide(m_user.displayName.empty() ? m_user.id : m_user.displayName).c_str());
    }
    if (m_client->getCurrentPlayback(m_track)) {
        std::string line = m_track.name;
        if (!m_track.artist.empty()) line += " — " + m_track.artist;
        SetWindowTextW(m_hwndTrack, utf8ToWide(line).c_str());
        SetWindowTextW(m_hwndPlayPause, m_track.isPlaying ? L"Pause" : L"Play");
    } else {
        SetWindowTextW(m_hwndTrack, L"No playback");
        SetWindowTextW(m_hwndPlayPause, L"Play");
    }
    layout();
}

void SpotifyInfoBar::startPollTimer() {
    stopPollTimer();
    if (m_hwnd) m_pollTimerId = SetTimer(m_hwnd, 1, POLL_MS, nullptr);
}

void SpotifyInfoBar::stopPollTimer() {
    if (m_hwnd && m_pollTimerId) {
        KillTimer(m_hwnd, m_pollTimerId);
        m_pollTimerId = 0;
    }
}

LRESULT CALLBACK SpotifyInfoBar::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SpotifyInfoBar* self = (SpotifyInfoBar*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (uMsg == WM_CREATE) {
        self = (SpotifyInfoBar*)((CREATESTRUCT*)lParam)->lpCreateParams;
        if (self) SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)self);
        return 0;
    }
    if (!self) return DefWindowProcW(hwnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_SPOTIFY_LOGIN_DONE: {
        std::string* code = (std::string*)lParam;
        if (code) {
            self->onCodeReceived(*code);
            delete code;
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == 1) self->updateContent();
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == 1) { if (self->m_client) self->m_client->skipPrevious(); }
        else if (id == 2) {
            if (self->m_client) {
                if (self->m_track.isPlaying) self->m_client->pause();
                else self->m_client->play();
            }
        }
        else if (id == 3) { if (self->m_client) self->m_client->skipNext(); }
        else if (id == 4) self->startLoginFlow();
        self->updateContent();
        return 0;
    }
    case WM_DESTROY:
        self->stopPollTimer();
        self->m_hwnd = nullptr;
        return 0;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

} // namespace RawrXD
