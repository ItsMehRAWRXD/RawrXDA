#pragma once

// Sovereign_UI_Bridge.h - Win32 UI bridge for Sovereign token sampling

#include <windows.h>
#include "RawrXD_Inference_Wrapper.h"

// Forward declarations
struct llama_context;
struct llama_token_data_array;

// Custom message for Sovereign tokens
#define WM_RAWRXD_SOVEREIGN_TOKEN (WM_USER + 0x1751)  // 0x1751 from sentinel 0x1751431337

// Global chat window handle (set by Win32IDE)
extern HWND hChatWnd;

// ============================================================================
// Sovereign Token Steering API
// ============================================================================

// Legacy single-token check (used by post-sampling validation)
void OnTokenSampled(int winning_id);

// Modern streaming token check with full context (recommended)
// Returns: validated token ID (>= 0) or -1 if forced EOS due to max retries
int32_t OnTokenSampledWithContext(
    int winning_id,
    llama_context* ctx,
    llama_token_data_array* candidates
);

// Emergency steering (internal - called by OnTokenSampled variants)
void ForceEmergencySteer();

bool Sovereign_Telemetry_Enabled();
void Sovereign_Handle_Overflow(int winning_id);

// Set the active inference context for emergency steering
// Must be called before each generation pass
void SetActiveInferenceContext(llama_context* ctx, llama_token_data_array* candidates);

// Streaming token validation for native pipeline integration
// Returns: token_id if approved, -1 if suppressed (caller should re-sample)
int32_t ValidateStreamingToken(int32_t token_id, void* engine_context);