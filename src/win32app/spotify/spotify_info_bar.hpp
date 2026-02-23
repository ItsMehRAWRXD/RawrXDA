// ============================================================================
// spotify_info_bar.hpp — Moveable Spotify info bar (user + track, play controls)
// Top-level draggable window. Requires SpotifyClient and config/credentials.
// ============================================================================
#pragma once

#include "spotify_client.hpp"
#include <windows.h>
#include <string>
#include <memory>

namespace RawrXD {

class SpotifyInfoBar {
public:
    SpotifyInfoBar(HINSTANCE hInstance);
    ~SpotifyInfoBar();

    void setClient(std::shared_ptr<SpotifyClient> client) { m_client = std::move(client); }

    // Show or hide the bar. Creates window on first show.
    void show();
    void hide();
    void toggle();
    bool isVisible() const;

    // Open browser to Spotify auth; start local callback server; on code received, exchange and show bar.
    void startLoginFlow();
    void onCodeReceived(const std::string& code);

    // Call from IDE to persist tokens (e.g. save to IDEConfig)
    void setPersistTokensCallback(SpotifyClient::PersistTokensCallback cb);

private:
    void createWindow();
    void updateContent();
    void layout();
    void startPollTimer();
    void stopPollTimer();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    HWND m_hwndUser = nullptr;   // static: user name
    HWND m_hwndTrack = nullptr;  // static: track line
    HWND m_hwndPrev = nullptr;
    HWND m_hwndPlayPause = nullptr;
    HWND m_hwndNext = nullptr;
    HWND m_hwndLogin = nullptr;
    UINT_PTR m_pollTimerId = 0;
    static const int POLL_MS = 3000;
    static const int WINDOW_WIDTH = 480;
    static const int WINDOW_HEIGHT = 72;

    std::shared_ptr<SpotifyClient> m_client;
    SpotifyUser m_user;
    SpotifyTrack m_track;
    bool m_loggedIn = false;
};

} // namespace RawrXD
