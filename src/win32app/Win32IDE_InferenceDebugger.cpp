// Win32IDE_InferenceDebugger.cpp — open Sovereign latent-probe panel (Tools / palette hook)

#include "../hotpatch/Inference_Debugger_Bridge.hpp"
#include "Win32IDE.h"

void Win32IDE::showInferenceDebuggerPanel()
{
    rawrxd::inference_debug::showDebuggerPanel(m_hwndMain);
}
