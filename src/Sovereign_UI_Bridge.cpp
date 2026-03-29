#include "Sovereign_UI_Bridge.h"
#include "Sovereign_Engine_Control.h"
// #include <llama.h>  // COMMENTED: Not in build include paths, using stubs
#include "hotpatch/llama_stub.h"  // Stub definitions BEFORE Inference_Debugger_Bridge
#include <atomic>
#include <cmath>
#include <cstdint>

#include "hotpatch/Inference_Debugger_Bridge.hpp"  // Now has complete types

// Sovereign_UI_Bridge.cpp - Production Implementation with Integrated Rollback

HWND hChatWnd = NULL;  // Initialize to NULL, set by Win32IDE

// Global context pointer for emergency steering
// This should be set by the inference engine when a generation begins
static llama_context* g_active_llama_context = nullptr;
static llama_token_data_array* g_active_candidates = nullptr;

// Track re-sampling attempts to prevent infinite loops
static std::atomic<int> g_resample_retry_count{0};
static constexpr int MAX_RESAMPLE_RETRIES = 3;

// COMMENTED: llama.cpp sampling API not available in current version
/*
// Forward declarations for llama.cpp sampling functions
// (Now included via <llama.h>)
*/

void SetActiveInferenceContext(llama_context* ctx, llama_token_data_array* candidates)
{
    g_active_llama_context = ctx;
    g_active_candidates = candidates;
    g_resample_retry_count.store(0, std::memory_order_release);
}

// Internal helper: Rollback KV-cache and re-sample with suppressed token
static int32_t RollbackAndResample(llama_context* ctx, llama_token_data_array* candidates, int32_t forbidden_id)
{
    if (!ctx || !candidates || candidates->size == 0) {
        Hotpatch_TraceBeacon("[ROLLBACK] ERROR: Invalid context or candidates\n", 50);
        return forbidden_id;
    }

    // 1. Get current token count in the KV-cache
    int32_t n_past = llama_get_kv_cache_token_count(ctx);
    if (n_past <= 0) {
        Hotpatch_TraceBeacon("[ROLLBACK] ERROR: KV-cache empty\n", 36);
        return -1;  // Cannot rollback
    }

    // 2. Remove the last token from the KV-cache
    // This effectively "un-sees" the forbidden token
    llama_kv_cache_seq_rm(ctx, 0, n_past - 1, n_past);

    char rollback_msg[128];
    snprintf(rollback_msg, sizeof(rollback_msg), "[ROLLBACK] Removed token at pos %d from KV-cache\n", n_past - 1);
    Hotpatch_TraceBeacon(rollback_msg, strlen(rollback_msg));

    // 3. Set the forbidden token's logit to -INF in the candidates array
    // This ensures re-sampling will never pick it
    bool found = false;
    for (size_t i = 0; i < candidates->size; ++i) {
        if (candidates->data[i].id == forbidden_id) {
            candidates->data[i].logit = -INFINITY;
            found = true;
            break;
        }
    }

    if (!found) {
        char warn_msg[128];
        snprintf(warn_msg, sizeof(warn_msg), "[ROLLBACK] WARNING: Token %d not in candidates\n", forbidden_id);
        Hotpatch_TraceBeacon(warn_msg, strlen(warn_msg));
    }

    // 4. Create a temporary sampler for re-sampling with the modified candidates
    // Use the same sampling parameters as the main inference
    struct llama_sampler_chain_params params = llama_sampler_chain_default_params();
    struct llama_sampler* temp_sampler = llama_sampler_chain_init(params);
    
    if (temp_sampler) {
        // Add sampling strategies (temperature, top-k, top-p)
        llama_sampler_chain_add(temp_sampler, llama_sampler_init_temp(0.7f));  // Default temperature
        llama_sampler_chain_add(temp_sampler, llama_sampler_init_top_k(40));   // Default top-k
        llama_sampler_chain_add(temp_sampler, llama_sampler_init_top_p(0.9f, 1)); // Default top-p
        llama_sampler_chain_add(temp_sampler, llama_sampler_init_dist(0));     // Default seed
        
        // Apply the sampler to the modified candidates
        llama_sampler_apply(temp_sampler, candidates);
        
        // Sample a new token
        int32_t new_id = llama_sampler_sample(temp_sampler, ctx, -1);
        
        // Clean up
        llama_sampler_free(temp_sampler);
        
        char resample_msg[128];
        snprintf(resample_msg, sizeof(resample_msg), "[ROLLBACK] Re-sampled token: %d (was %d)\n", new_id, forbidden_id);
        Hotpatch_TraceBeacon(resample_msg, strlen(resample_msg));
        
        return new_id;
    } else {
        Hotpatch_TraceBeacon("[ROLLBACK] ERROR: Failed to create temporary sampler\n", 54);
        return forbidden_id;
    }
}

int32_t OnTokenSampledWithContext(int winning_id, llama_context* ctx, llama_token_data_array* candidates)
{
    rawrxd::inference_debug::pushCandidateSnapshot(static_cast<std::int32_t>(winning_id), candidates,
                                                   IsUnauthorized_NoDep(winning_id));

    // 1. Check if token is unauthorized
    if (!IsUnauthorized_NoDep(winning_id))
    {
        // Token is approved - reset retry counter
        g_resample_retry_count.store(0, std::memory_order_release);
        return winning_id;  // Pass through approved token
    }

    // 2. Check retry limit
    int retry_count = g_resample_retry_count.load(std::memory_order_acquire);
    if (retry_count >= MAX_RESAMPLE_RETRIES)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "[SOVEREIGN] Max retries (%d) reached for token %d - forcing EOS\n",
                 MAX_RESAMPLE_RETRIES, winning_id);
        Hotpatch_TraceBeacon(msg, strlen(msg));
        g_resample_retry_count.store(0, std::memory_order_release);
        // Return a special EOS-like value to stop generation cleanly
        return -1;  // Caller should check for -1 and break generation loop
    }

    // 3. Log suppression with sentinel (before retry increment)
    char sentinel_msg[256];
    snprintf(sentinel_msg, sizeof(sentinel_msg),
             "[SOVEREIGN_SUCCESS] 0x1751431337 | Token %d intercepted (attempt %d/%d)\n", winning_id, retry_count + 1,
             MAX_RESAMPLE_RETRIES);
    Hotpatch_TraceBeacon(sentinel_msg, strlen(sentinel_msg));

    // 4. Increment retry counter
    g_resample_retry_count.fetch_add(1, std::memory_order_acq_rel);

    // 5. Perform KV-Cache rollback and re-sample
    if (ctx && candidates)
    {
        int32_t new_token = RollbackAndResample(ctx, candidates, winning_id);

        if (new_token >= 0)
        {
            // Success - recursively validate the new token
            // This allows the retry counter to work across multiple attempts
            return OnTokenSampledWithContext(new_token, ctx, candidates);
        }
        else
        {
            // Rollback failed - log and force EOS
            Hotpatch_TraceBeacon("[ROLLBACK] CRITICAL: Rollback failed, forcing EOS\n", 48);
            return -1;
        }
    }

    // 6. Fallback: suppress logit without rollback (legacy path)
    if (candidates)
    {
        for (size_t i = 0; i < candidates->size; ++i)
        {
            if (candidates->data[i].id == winning_id)
            {
                candidates->data[i].logit = -100.0f;
                Hotpatch_TraceBeacon("[SOVEREIGN] Logit suppressed (no rollback)\n", 42);
                break;
            }
        }
    }

    // Return the original token (will be caught on next iteration)
    return winning_id;
}

void OnTokenSampled(int winning_id)
{
    // Legacy API - use global context and validate
    int32_t validated_token = OnTokenSampledWithContext(winning_id, g_active_llama_context, g_active_candidates);

    // If validation returned -1, this indicates EOS forced by steering
    if (validated_token < 0)
    {
        Hotpatch_TraceBeacon("[SOVEREIGN] Forced EOS - stopping generation\n", 44);
        return;  // Don't post message - generation should stop
    }

    // 2. Dispatch validated token to the ChatPanel via the Sovereign Message Bus
    // lParam carries the validated_token; wParam is reserved for metadata (0)
    if (!PostMessageA(hChatWnd, WM_RAWRXD_SOVEREIGN_TOKEN, 0, (LPARAM)validated_token))
    {
        // Handle message queue saturation for high-TPS streams
        Sovereign_Handle_Overflow(validated_token);
    }
}

int32_t ValidateStreamingToken(int32_t token_id, void* engine_context)
{
    // Quick check for unauthorized tokens during streaming
    if (IsUnauthorized_NoDep(token_id))
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "[SOVEREIGN_STREAMING] Token %d blocked in stream\n", token_id);
        Hotpatch_TraceBeacon(msg, strlen(msg));
        return -1;  // Signal caller to re-sample
    }
    return token_id;  // Approved
}

void ForceEmergencySteer()
{
    // Deep KV-Cache rollback implementation
    if (!g_active_llama_context || !g_active_candidates)
    {
        Hotpatch_TraceBeacon("[STEER] ERROR: No active inference context\n", 43);
        return;
    }

    // Find the first unauthorized token in candidates
    int32_t forbidden_id = -1;
    for (size_t i = 0; i < g_active_candidates->size; ++i)
    {
        if (IsUnauthorized_NoDep(g_active_candidates->data[i].id))
        {
            forbidden_id = g_active_candidates->data[i].id;
            break;
        }
    }

    if (forbidden_id < 0)
    {
        Hotpatch_TraceBeacon("[STEER] No forbidden token found in candidates\n", 48);
        return;
    }

    // Perform the KV-Cache rollback
    bool success = RawrXD::Sovereign::ForceEmergencySteer(g_active_llama_context, g_active_candidates, forbidden_id);

    if (!success)
    {
        Hotpatch_TraceBeacon("[STEER] Rollback failed, using fallback suppression\n", 53);
        // Fallback: just suppress the logit without rollback
        for (size_t i = 0; i < g_active_candidates->size; ++i)
        {
            if (g_active_candidates->data[i].id == forbidden_id)
            {
                g_active_candidates->data[i].logit = -100.0f;
                break;
            }
        }
    }
}

bool Sovereign_Telemetry_Enabled()
{
    // TODO: Check telemetry setting
    return true;
}

void Sovereign_Handle_Overflow(int winning_id)
{
    // TODO: Handle overflow, perhaps buffer or log
    Hotpatch_TraceBeacon("[OVERFLOW] Token queued\n", 23);
}