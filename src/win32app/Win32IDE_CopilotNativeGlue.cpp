// Win32IDE_CopilotNativeGlue.cpp — finish local GGUF minimal-agentic Copilot turns from the UI thread.

#include "../agentic/agent_controller_minimal.h"
#include "../config/IDEConfig.h"
#include "Win32IDE.h"

#include <memory>

void RawrXD_FinishCopilotMinimalAgentic(Win32IDE* ide, WPARAM /*wParam*/, LPARAM heapResponse)
{
    std::unique_ptr<rawrxd::MinimalAgenticResponse> bundle(
        reinterpret_cast<rawrxd::MinimalAgenticResponse*>(heapResponse));
    if (!ide)
        return;
    if (bundle)
    {
        METRICS.gauge("chat.minimal_agentic.ui_finish_tool_count", static_cast<double>(bundle->tool_calls_made));
    }
    ide->m_chatSendInFlight.store(false);
    ide->setCopilotInteractionBusyOnUiThread(false);
    ide->m_streamingTokenAccumulator.clear();
    if (!bundle)
        return;
    ide->applyMinimalAgenticCompletion(std::move(*bundle));
}
