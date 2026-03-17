#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace RawrXD::GUI {

/**
 * TokenStreamDisplay — Win32 control for live token streaming visualization
 * Displays tokens in real-time as they arrive from the inference engine
 */
class TokenStreamDisplay {
public:
    TokenStreamDisplay();
    ~TokenStreamDisplay();

    /**
     * Create the Win32 window for token display
     */
    bool create(HWND hParent, int x, int y, int width, int height);
    void destroy();

    /**
     * Display a token (appends to editor surface)
     */
    void appendToken(const std::string& token);

    /**
     * Clear all displayed content
     */
    void clear();

    /**
     * Get the accumulated text
     */
    std::string getText() const;

    /**
     * Set cursor to end of display
     */
    void scrollToEnd();

    HWND getHWND() const { return m_hEditor; }

private:
    HWND m_hEditor{nullptr};
    HWND m_hParent{nullptr};
    std::string m_buffer;

    static LRESULT CALLBACK editorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

/**
 * TelemetryDisplay — Shows inference metrics and telemetry
 */
class TelemetryDisplay {
public:
    struct Metrics {
        size_t totalTokens;
        int64_t totalLatencyMs;
        float avgTokensPerSecond;
        std::string engineStatus;  // "connected", "disconnected", "error"
    };

    TelemetryDisplay();
    ~TelemetryDisplay();

    bool create(HWND hParent, int x, int y, int width, int height);
    void destroy();

    void updateMetrics(const Metrics& metrics);
    void setEngineStatus(const std::string& status);

    HWND getHWND() const { return m_hStatus; }

private:
    HWND m_hStatus{nullptr};
    HWND m_hTokenCount{nullptr};
    HWND m_hLatency{nullptr};
    HWND m_hEngineStatus{nullptr};

    std::string formatMetrics(const Metrics& m);
};

}
