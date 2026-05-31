// Win32IDE_HexMag.cpp — HexMag FastAPI service UI hooks (menu, status bar, copilot routing)
#include "../agent/hexmag_client.hpp"
#include "Win32IDE.h"
#include "Win32IDE_Commands.h"
#include <atomic>
#include <memory>
#include <thread>

#ifndef MSFTEDIT_CLASS
#define MSFTEDIT_CLASS L"RichEdit20W"
#endif


#ifndef WM_HEXMAG_ASK_DONE
#define WM_HEXMAG_ASK_DONE (WM_APP + 109)
#endif
#ifndef WM_HEXMAG_TELEMETRY_CHUNK
#define WM_HEXMAG_TELEMETRY_CHUNK (WM_APP + 110)
#endif
#ifndef WM_HEXMAG_TELEMETRY_DONE
#define WM_HEXMAG_TELEMETRY_DONE (WM_APP + 111)
#endif

namespace
{

std::wstring utf8ToWideLocal(const std::string& text)
{
    if (text.empty())
        return {};
    const int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (wideLength <= 0)
        return {};
    std::wstring result(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), wideLength);
    return result;
}

struct HexMagAskDonePayload
{
    unsigned long long traceId = 0;
    bool success = false;
    std::string answer;
    std::string error;
    bool requestGgufFallback = false;
    std::string fallbackQuestion;
};

void postHexMagTelemetryChunk(HWND hwndMain, std::string* chunk)
{
    if (!chunk)
        return;
    if (hwndMain && IsWindow(hwndMain))
        PostMessageW(hwndMain, WM_HEXMAG_TELEMETRY_CHUNK, 0, reinterpret_cast<LPARAM>(chunk));
    else
        delete chunk;
}

void postHexMagTelemetryDone(HWND hwndMain, bool success)
{
    if (hwndMain && IsWindow(hwndMain))
        PostMessageW(hwndMain, WM_HEXMAG_TELEMETRY_DONE, success ? 1 : 0, 0);
}

void postHexMagAskDone(HWND hwndMain, HexMagAskDonePayload* payload)
{
    if (!payload)
        return;
    if (hwndMain && IsWindow(hwndMain))
    {
        // PostMessage only — never SendMessage from worker threads (blocks worker on UI dispatch).
        PostMessageW(hwndMain, WM_HEXMAG_ASK_DONE, payload->success ? 1 : 0, reinterpret_cast<LPARAM>(payload));
    }
    else
    {
        delete payload;
    }
}

}  // namespace

void Win32IDE::refreshHexMagAgentMenuChecks()
{
    HMENU hexMenu = nullptr;
    if (m_hwndMain)
    {
        HMENU hMenu = GetMenu(m_hwndMain);
        if (hMenu)
        {
            HMENU hAgentMenu = GetSubMenu(hMenu, 6);
            if (hAgentMenu)
            {
                for (int i = 0; i < GetMenuItemCount(hAgentMenu); ++i)
                {
                    HMENU sub = GetSubMenu(hAgentMenu, i);
                    if (sub && GetMenuItemID(sub, 0) == IDM_AGENT_HEXMAG_START)
                    {
                        hexMenu = sub;
                        break;
                    }
                }
            }
        }
    }
    if (!hexMenu)
        return;

    CheckMenuItem(hexMenu, IDM_AGENT_HEXMAG_TOGGLE_FALLBACK,
                  m_settings.hexmagGgufFallbackEnabled ? MF_CHECKED : MF_UNCHECKED);
    CheckMenuItem(hexMenu, IDM_AGENT_HEXMAG_ROUTE_COPILOT,
                  m_settings.hexmagRouteCopilotPanel ? MF_CHECKED : MF_UNCHECKED);
}

void RawrXD_FinishHexMagAsk(Win32IDE* ide, WPARAM wParam, LPARAM lParam)
{
    std::unique_ptr<HexMagAskDonePayload> payload(reinterpret_cast<HexMagAskDonePayload*>(lParam));
    if (!ide || !payload)
        return;

    const bool ok = (wParam != 0);
    ide->m_chatSendInFlight.store(false);
    ide->setCopilotInteractionBusyOnUiThread(false);

    if (ok)
    {
        ide->m_lastCopilotAssistantResponse = payload->answer;
        ide->appendCopilotChatTextOnUiThread("\n[HexMag Copilot]\n" + payload->answer + "\n");
        ide->appendToOutput("[HexMag Copilot] response delivered (" + std::to_string(payload->answer.size()) +
                                " chars)\n",
                            "Agent", Win32IDE::OutputSeverity::Info);
        ide->setHexMagStatusBarHint(L"HexMag: OK");
        return;
    }

    if (payload->requestGgufFallback && ide->m_agent && !payload->fallbackQuestion.empty())
    {
        ide->appendCopilotChatTextOnUiThread("\n[HexMag] " + payload->error + " — local model fallback engaged.\n");
        ide->setHexMagStatusBarHint(L"HexMag: fallback");
        ide->m_agent->Ask(payload->fallbackQuestion);
        return;
    }

    ide->appendCopilotChatTextOnUiThread("\n[HexMag] " + payload->error + "\n");
    ide->appendToOutput("[HexMag] " + payload->error + "\n", "Errors", Win32IDE::OutputSeverity::Error);
    ide->setHexMagStatusBarHint(L"HexMag: offline");
}

void Win32IDE::setHexMagStatusBarHint(const std::wstring& text)
{
    if (m_hwndStatusBar && IsWindow(m_hwndStatusBar))
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(text.c_str()));
}

void Win32IDE::onHexMagStartService()
{
    setHexMagStatusBarHint(L"HexMag: spawning engine...");
    appendToOutput("HexMag: starting background service...\n", "Agent", Win32IDE::OutputSeverity::Info);

    const bool ok = RawrXD::HexMag::tryLaunchService();
    if (ok)
    {
        setHexMagStatusBarHint(L"HexMag: started");
        showAgentActivityStatus("HexMag service started");
        appendToOutput("HexMag service started.\n", "Agent", Win32IDE::OutputSeverity::Info);
    }
    else
    {
        setHexMagStatusBarHint(L"HexMag: start failed");
        appendToOutput("Failed to start HexMag service (check Python + services/requirements.txt).\n", "Errors",
                       Win32IDE::OutputSeverity::Error);
    }
}

void Win32IDE::onHexMagHealthCheck()
{
    const bool ok = RawrXD::HexMag::healthCheck();
    const std::wstring base = utf8ToWideLocal(RawrXD::HexMag::resolveBaseUrl());
    if (ok)
    {
        setHexMagStatusBarHint(L"HexMag: connected " + base);
        showAgentActivityStatus("HexMag healthy at " + RawrXD::HexMag::resolveBaseUrl());
        appendToOutput("HexMag service: healthy (" + RawrXD::HexMag::resolveBaseUrl() + ")\n", "Agent",
                       Win32IDE::OutputSeverity::Info);
    }
    else
    {
        setHexMagStatusBarHint(L"HexMag: offline");
        appendToOutput("HexMag service: not reachable at " + RawrXD::HexMag::resolveBaseUrl() + "\n", "Agent",
                       OutputSeverity::Warning);
    }
}

void Win32IDE::onHexMagToggleGgufFallback()
{
    m_settings.hexmagGgufFallbackEnabled = !m_settings.hexmagGgufFallbackEnabled;
    saveSettings();
    refreshHexMagAgentMenuChecks();
    const std::string state = m_settings.hexmagGgufFallbackEnabled ? "enabled" : "disabled";
    showAgentActivityStatus("HexMag GGUF fallback " + state);
    appendToOutput("HexMag GGUF fallback: " + state + "\n", "Agent", Win32IDE::OutputSeverity::Info);
}

void Win32IDE::onHexMagToggleRouteCopilotPanel()
{
    m_settings.hexmagRouteCopilotPanel = !m_settings.hexmagRouteCopilotPanel;
    saveSettings();
    refreshHexMagAgentMenuChecks();
    const std::string state = m_settings.hexmagRouteCopilotPanel ? "enabled" : "disabled";
    showAgentActivityStatus("HexMag copilot routing " + state);
    appendToOutput("HexMag copilot panel routing: " + state + "\n", "Agent", Win32IDE::OutputSeverity::Info);
}

void Win32IDE::dispatchHexMagAskFromUi(const std::string& question, bool toCopilotPanel)
{
    if (question.empty())
        return;

    const std::string codeContext = m_currentFile.empty() ? std::string{} : getEditorText();
    const auto hex = RawrXD::HexMag::askWithAutoStart(question, codeContext);

    if (hex.success)
    {
        if (toCopilotPanel)
        {
            m_lastCopilotAssistantResponse = hex.answer;
            appendCopilotChatTextOnUiThread("\n[HexMag Copilot]\n" + hex.answer + "\n");
        }
        else
        {
            appendToOutput("[HexMag Copilot]\n" + hex.answer + "\n", "Agent", Win32IDE::OutputSeverity::Info);
        }
        setHexMagStatusBarHint(L"HexMag: OK");
        return;
    }

    if (m_settings.hexmagGgufFallbackEnabled && m_agent)
    {
        const std::string warn = "[HexMag] " + hex.error + " — using local model.\n";
        if (toCopilotPanel)
            appendCopilotChatTextOnUiThread("\n" + warn);
        else
            appendToOutput(warn, "Agent", OutputSeverity::Warning);
        m_agent->Ask(question);
        setHexMagStatusBarHint(L"HexMag: GGUF fallback");
        return;
    }

    const std::string err = "[HexMag] " + hex.error + "\n";
    if (toCopilotPanel)
        appendCopilotChatTextOnUiThread("\n" + err);
    else
        appendToOutput(err, "Errors", Win32IDE::OutputSeverity::Error);
    setHexMagStatusBarHint(L"HexMag: offline");
}

bool Win32IDE::tryDispatchCopilotThroughHexMag(const std::string& userMessage, unsigned long long traceId)
{
    if (!m_settings.hexmagRouteCopilotPanel)
        return false;
    if (userMessage.empty() || userMessage.front() == '/')
        return false;

    bool expectedIdle = false;
    if (!m_chatSendInFlight.compare_exchange_strong(expectedIdle, true))
        return false;

    setCopilotInteractionBusyOnUiThread(true);
    appendCopilotChatTextOnUiThread("\n[HexMag] routing...\n");
    setHexMagStatusBarHint(L"HexMag: working...");

    const std::string codeContext = m_currentFile.empty() ? std::string{} : getEditorText();
    HWND hwndMain = m_hwndMain;
    const bool allowFallback = m_settings.hexmagGgufFallbackEnabled;
    const bool agentAvailable = (m_agent != nullptr);

    std::thread(
        [hwndMain, userMessage, codeContext, traceId, allowFallback, agentAvailable]()
        {
            auto* payload = new HexMagAskDonePayload();
            payload->traceId = traceId;

            if (hwndMain && IsWindow(hwndMain))
            {
                const auto hex = RawrXD::HexMag::askWithAutoStart(userMessage, codeContext);
                payload->success = hex.success;
                payload->answer = hex.answer;
                payload->error = hex.error.empty() ? "HexMag request failed" : hex.error;

                if (!hex.success && allowFallback && agentAvailable)
                {
                    payload->requestGgufFallback = true;
                    payload->fallbackQuestion = userMessage;
                }
            }
            else
            {
                payload->success = false;
                payload->error = "IDE window closed";
            }

            postHexMagAskDone(hwndMain, payload);
        })
        .detach();

    return true;
}

void Win32IDE::appendHexMagTelemetryText(const std::wstring& text)
{
    if (!m_hwndHexMagTelemetry || !IsWindow(m_hwndHexMagTelemetry) || text.empty())
        return;
    SendMessageW(m_hwndHexMagTelemetry, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(m_hwndHexMagTelemetry, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(m_hwndHexMagTelemetry, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::ensureHexMagTelemetryTab()
{
    if (!m_hwndOutputTabs || !IsWindow(m_hwndOutputTabs))
        createOutputTabs();

    if (m_hwndHexMagTelemetry && IsWindow(m_hwndHexMagTelemetry))
        return;

    RECT client{};
    GetClientRect(m_hwndMain, &client);
    const int tabBarHeight = 24;

    m_hwndHexMagTelemetry = CreateWindowExW(
        WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"",
        WS_CHILD | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_READONLY, 0,
        tabBarHeight, client.right, m_outputTabHeight - tabBarHeight, m_hwndMain,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_OUTPUT_EDIT_AGENT_TELEMETRY)), m_hInstance, nullptr);

    if (!m_hwndHexMagTelemetry)
        return;

    SendMessageW(m_hwndHexMagTelemetry, EM_SETTARGETDEVICE, 0, 1);
    m_outputWindows["Agent Telemetry"] = m_hwndHexMagTelemetry;

    const int tabCount = TabCtrl_GetItemCount(m_hwndOutputTabs);
    bool hasTab = false;
    for (int i = 0; i < tabCount; ++i)
    {
        wchar_t label[64] = {};
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = label;
        item.cchTextMax = static_cast<int>(std::size(label));
        if (TabCtrl_GetItem(m_hwndOutputTabs, i, &item) && wcscmp(label, L"Agent Telemetry") == 0)
        {
            hasTab = true;
            break;
        }
    }
    if (!hasTab)
    {
        TCITEMW tie{};
        tie.mask = TCIF_TEXT;
        tie.pszText = const_cast<wchar_t*>(L"Agent Telemetry");
        SendMessageW(m_hwndOutputTabs, TCM_INSERTITEMW, static_cast<WPARAM>(tabCount),
                     reinterpret_cast<LPARAM>(&tie));
    }
}

void Win32IDE::showHexMagTelemetryPanel()
{
    ensureHexMagTelemetryTab();
    if (!m_outputPanelVisible)
        toggleOutputPanel();

    m_activeOutputTab = "Agent Telemetry";
    if (m_hwndOutputTabs)
    {
        const int tabCount = TabCtrl_GetItemCount(m_hwndOutputTabs);
        for (int i = 0; i < tabCount; ++i)
        {
            wchar_t label[64] = {};
            TCITEMW item{};
            item.mask = TCIF_TEXT;
            item.pszText = label;
            item.cchTextMax = static_cast<int>(std::size(label));
            if (TabCtrl_GetItem(m_hwndOutputTabs, i, &item) && wcscmp(label, L"Agent Telemetry") == 0)
            {
                TabCtrl_SetCurSel(m_hwndOutputTabs, i);
                m_selectedOutputTab = i;
                break;
            }
        }
    }

    for (auto& kv : m_outputWindows)
    {
        const bool show = (kv.first == m_activeOutputTab && m_outputPanelVisible);
        ShowWindow(kv.second, show ? SW_SHOW : SW_HIDE);
    }
    if (m_hwndHexMagTelemetry)
        ShowWindow(m_hwndHexMagTelemetry,
                   (m_activeOutputTab == "Agent Telemetry" && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
}

void Win32IDE::clearHexMagTelemetryPanel()
{
    if (m_hwndHexMagTelemetry && IsWindow(m_hwndHexMagTelemetry))
        SetWindowTextW(m_hwndHexMagTelemetry, L"");
}

void Win32IDE::onHexMagShowTelemetryPanel()
{
    showHexMagTelemetryPanel();
    setHexMagStatusBarHint(L"HexMag: telemetry panel");
}

void Win32IDE::onHexMagStartAgentTelemetryStream()
{
    if (m_hexmagTelemetryStreaming.exchange(true))
    {
        appendToOutput("HexMag agent telemetry stream already running.\n", "Agent", OutputSeverity::Warning);
        return;
    }

    showHexMagTelemetryPanel();
    clearHexMagTelemetryPanel();
    appendHexMagTelemetryText(L"=== HexMag /agent SSE ===\r\n");

    const std::string goal = m_lastCopilotUserPrompt.empty() ? "Inspect workspace and report agent status"
                                                             : m_lastCopilotUserPrompt;
    HWND hwndMain = m_hwndMain;

    std::thread(
        [hwndMain, goal]()
        {
            bool streamOk = false;
            const auto onChunk = [hwndMain](const std::string& line)
            {
                auto* heap = new std::string(line);
                postHexMagTelemetryChunk(hwndMain, heap);
            };

            const auto result = RawrXD::HexMag::streamAgentWithAutoStart(goal, onChunk, 30.0f);
            streamOk = result.success;

            if (!streamOk)
            {
                auto* errLine = new std::string("[telemetry] error: " + result.error + "\n");
                postHexMagTelemetryChunk(hwndMain, errLine);
            }
            else if (result.goalSatisfied)
            {
                auto* doneLine = new std::string("[telemetry] goal.satisfied\n");
                postHexMagTelemetryChunk(hwndMain, doneLine);
            }

            postHexMagTelemetryDone(hwndMain, streamOk);
        })
        .detach();
}

void RawrXD_FinishHexMagTelemetryChunk(Win32IDE* ide, LPARAM lParam)
{
    std::unique_ptr<std::string> chunk(reinterpret_cast<std::string*>(lParam));
    if (!ide || !chunk || chunk->empty())
        return;
    ide->appendHexMagTelemetryText(utf8ToWideLocal(*chunk));
}

void RawrXD_FinishHexMagTelemetryDone(Win32IDE* ide, WPARAM wParam)
{
    if (!ide)
        return;
    ide->m_hexmagTelemetryStreaming.store(false);
    const bool ok = (wParam != 0);
    ide->setHexMagStatusBarHint(ok ? L"HexMag: stream done" : L"HexMag: stream failed");
    ide->appendHexMagTelemetryText(ok ? L"\r\n[telemetry] stream complete\r\n" : L"\r\n[telemetry] stream failed\r\n");
}
