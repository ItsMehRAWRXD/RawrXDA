// ============================================================================
// spotify_client.hpp — Spotify Web API client (OAuth + playback control)
// Uses WinHttp for HTTPS. Tokens stored via callback for persistence.
// ============================================================================
#pragma once

#include <string>
#include <functional>
#include <mutex>
#include <chrono>

namespace RawrXD {

struct SpotifyUser {
    std::string id;
    std::string displayName;
    std::string email;
    std::string imageUrl;
};

struct SpotifyTrack {
    std::string id;
    std::string name;
    std::string artist;
    std::string album;
    int durationMs = 0;
    int progressMs = 0;
    bool isPlaying = false;
};

class SpotifyClient {
public:
    SpotifyClient() = default;

    // Credentials (from config or settings)
    void setClientId(const std::string& clientId) { m_clientId = clientId; }
    void setClientSecret(const std::string& clientSecret) { m_clientSecret = clientSecret; }
    void setRedirectUri(const std::string& uri) { m_redirectUri = uri; }
    void setRefreshToken(const std::string& token) { m_refreshToken = token; }
    void setAccessToken(const std::string& token, int expiresInSeconds = 3600);

    std::string getClientId() const { return m_clientId; }
    std::string getRedirectUri() const { return m_redirectUri; }
    std::string getRefreshToken() const { return m_refreshToken; }
    bool hasValidToken() const;

    // Build URL for user to open in browser (authorization code flow)
    std::string getAuthorizationUrl() const;

    // Exchange authorization code for tokens. Returns true on success.
    // On success, call persistTokens() to save refresh_token for next run.
    bool exchangeCodeForTokens(const std::string& code);

    // Refresh access token using refresh_token. Returns true on success.
    bool refreshAccessToken();

    // Persist tokens: callback so IDE can save to config (e.g. spotify.refreshToken)
    using PersistTokensCallback = std::function<void(const std::string& refreshToken, const std::string& accessToken, int expiresIn)>;
    void setPersistTokensCallback(PersistTokensCallback cb) { m_persistTokens = std::move(cb); }

    // API calls (require valid access token)
    bool getCurrentUser(SpotifyUser& out);
    bool getCurrentPlayback(SpotifyTrack& out);
    bool pause();
    bool play();
    bool skipNext();
    bool skipPrevious();
    bool setVolume(int percent);  // 0..100

private:
    std::string apiRequest(const std::string& method, const std::string& path,
                           const std::string& body = "",
                           const std::string& contentType = "application/json");
    std::string tokenRequest(const std::string& body);

    std::string m_clientId;
    std::string m_clientSecret;
    std::string m_redirectUri;
    std::string m_refreshToken;
    std::string m_accessToken;
    std::chrono::steady_clock::time_point m_tokenExpiry;
    mutable std::mutex m_mutex;
    PersistTokensCallback m_persistTokens;
};

} // namespace RawrXD
