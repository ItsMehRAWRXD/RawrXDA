// Inference_Debugger_Bridge.hpp — Latent probe / SSAPYB-facing capture ring (Sovereign 0x1751431337)
//
// Capture: invoked from Sovereign_UI_Bridge on each candidate snapshot (pre-steering).
// Expose: lock-free-ish latest frame + small ring for Win32 panel / telemetry.

#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>

// Forward declaration - only if not already defined by llama_stub.h
#ifndef LLAMA_STUB_H_INCLUDED
struct llama_token_data_array;
#endif

namespace rawrxd::inference_debug
{

constexpr int kTopK = 5;
constexpr std::size_t kRingCap = 256;

#pragma pack(push, 1)
struct DebugFrame
{
    std::uint64_t qpcTick = 0;
    std::int32_t chosenId = -1;
    std::int32_t topId[kTopK]{};
    float topLogit[kTopK]{};
    std::uint32_t flags = 0;  // bit0 chosen matched sovereign suppression list
};
#pragma pack(pop)

// Called from inference path when a token is about to be validated (full candidate array).
void pushCandidateSnapshot(std::int32_t chosenId, const llama_token_data_array* candidates, bool chosenUnauthorized);

// Copy latest frame (returns false if none yet).
bool copyLatestFrame(DebugFrame& out);

// Win32 panel (top-k logits, sovereign rows in red via NM_CUSTOMDRAW).
// Palette: command IDM_INFERENCE_DEBUGGER_PANEL (5322) → Win32IDE::showInferenceDebuggerPanel().
void showDebuggerPanel(HWND ownerHwnd);
void closeDebuggerPanel();

}  // namespace rawrxd::inference_debug
