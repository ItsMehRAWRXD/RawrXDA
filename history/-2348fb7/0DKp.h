#ifndef COPILOT_STREAMING_H
#define COPILOT_STREAMING_H

#include <windows.h>
#include <string>
#include <functional>

class CopilotStreaming {
public:
    CopilotStreaming(HWND outputWindow);
    ~CopilotStreaming();

    // Configuration
    void SetProxyUrl(const std::string& url);
    void SetUpstream(const std::string& upstream);
    void SetModel(const std::string& model);
    void SetProvider(const std::string& provider);

    // Streaming API
    void SendPrompt(const std::string& prompt, const std::string& context = "");
    void CancelStream();

    // Callbacks
    void OnToken(std::function<void(const std::string&)> callback);
    void OnComplete(std::function<void()> callback);
    void OnError(std::function<void(const std::string&)> callback);

private:
    HWND m_outputWindow;
    std::string m_proxyUrl;
    std::string m_upstream;
    std::string m_model;
    std::string m_provider;

    HANDLE m_streamThread;
    bool m_cancelRequested;

    std::function<void(const std::string&)> m_tokenCallback;
    std::function<void()> m_completeCallback;
    std::function<void(const std::string&)> m_errorCallback;

    static DWORD WINAPI StreamThreadProc(LPVOID param);
    void StreamRequest(const std::string& prompt, const std::string& context);
};

#endif // COPILOT_STREAMING_H
