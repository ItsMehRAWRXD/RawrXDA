// Sovereign_Native_Integration.h
// Integration guide for Sovereign KV-Cache Rollback with custom model loading

#pragma once

#include \"Sovereign_UI_Bridge.h\"
#include \"Sovereign_Engine_Control.h\"
#include \"RawrXD_Inference_Wrapper.h\"

namespace RawrXD {
namespace Sovereign {

// ============================================================================
// INTEGRATION PATTERN: Native Streaming Inference
// ============================================================================
//
// This demonstrates how to integrate Sovereign steering with your custom
// model loading and streaming infrastructure.
//
// Key Integration Points:
// 1. Before generation: Call SetActiveInferenceContext()
// 2. After each token: Call ValidateStreamingToken()
// 3. On unauthorized token: Re-sample or fallback
// 4. After generation: Clear context
//
// ============================================================================

/// Example: Integration with Win32IDE::generateNativeResponse
///
/// ```cpp
/// void Win32IDE::generateNativeResponse(const std::string& prompt) {
///     // ... setup code ...
///     
///     // 1. Register inference context for Sovereign steering
///     SetActiveInferenceContext(llama_ctx, &candidates);
///     
///     // 2. Generation loop
///     for (int i = 0; i < max_tokens; ++i) {
///         // Get next token from model
///         int32_t token_id = SampleToken(llama_ctx, &candidates);
///         
///         // 3. Validate token before streaming to UI
///         int32_t validated_id = ValidateStreamingToken(token_id, llama_ctx);
///         
///         if (validated_id < 0) {
///             // Token was suppressed - re-sample
///             // The candidates array has been modified with -INF for the bad token
///             token_id = SampleToken(llama_ctx, &candidates);
///             validated_id = ValidateStreamingToken(token_id, llama_ctx);
///             
///             // After max retries, this will pass through
///         }
///         
///         // 4. Stream approved token to UI
///         std::string token_text = Detokenize(validated_id);
///         postSovereignStreamToken(token_text.c_str(), token_text.size());
///         
///         // 5. Append to KV cache for next iteration
///         AppendTokenToKVCache(llama_ctx, validated_id);
///     }
///     
///     // 6. Clear context after generation
///     SetActiveInferenceContext(nullptr, nullptr);
/// }
/// ```

/// Example: Integration with CPUInferenceEngine streaming
///
/// ```cpp
/// void CPUInferenceEngine::GenerateStreaming(
///     const std::vector<int32_t>& input_tokens,
///     int max_tokens,
///     TokenCallback token_callback,
///     CompleteCallback complete_callback,
///     TokenIdCallback token_id_callback
/// ) {
///     // Build candidates array
///     llama_token_data_array candidates;
///     // ... fill candidates from logits ...
///     
///     // Register for Sovereign steering
///     SetActiveInferenceContext(m_llamaContext, &candidates);
///     
///     for (int i = 0; i < max_tokens; ++i) {
///         // Forward pass
///         std::vector<float> logits = Forward(current_tokens);
///         
///         // Fill candidates from logits
///         FillCandidates(&candidates, logits);
///         
///         // Apply Heretic MASM hotpatch (pre-emptive suppression)
///         Heretic_Main_Entry(&candidates);
///         
///         // Sample token
///         int32_t token_id = SampleNextToken(logits, m_temperature);
///         
///         // Post-sampling validation
///         if (OnTokenSampledWithContext(token_id, m_llamaContext, &candidates)) {
///             // Token was suppressed - re-sample from modified candidates
///             token_id = SampleFromCandidates(&candidates);
///         }
///         
///         // Stream to callback
///         if (token_id_callback) {
///             token_id_callback(token_id);
///         }
///         
///         std::string token_text = Detokenize({token_id});
///         if (token_callback) {
///             token_callback(token_text);
///         }
///         
///         current_tokens.push_back(token_id);
///     }
///     
///     // Clear context
///     SetActiveInferenceContext(nullptr, nullptr);
///     
///     if (complete_callback) {
///         complete_callback();
///     }
/// }
/// ```

/// Example: Integration with Ollama/Cloud streaming
///
/// ```cpp
/// void Win32IDE::generateCloudResponse(const std::string& prompt) {
///     // For cloud models, we validate token IDs as they arrive
///     auto on_token = [](const std::string& token_text) {
///         // Tokenize the received string to get ID
///         int32_t token_id = TokenizeCloudFragment(token_text);
///         
///         // Validate (no rollback possible for cloud, but we can censor)
///         int32_t validated = ValidateStreamingToken(token_id, nullptr);
///         
///         if (validated < 0) {
///             // Suppress this token - don't stream to UI
///             LogSovereignEvent(\"[CLOUD_CENSOR] Token %d suppressed\", token_id);
///             return;
///         }
///         
///         // Stream approved token
///         postSovereignStreamToken(token_text.c_str(), token_text.size());
///     };
///     
///     // Make streaming API call
///     OllamaClient::StreamGenerate(prompt, on_token);
/// }
/// ```

// ============================================================================
// Helper Functions for Integration
// ============================================================================

/// Check if Sovereign steering is enabled
inline bool IsSovereignSteeringEnabled() {
    // TODO: Check feature flag or environment variable
    return true;
}

/// Log a Sovereign event with the diagnostic sentinel
inline void LogSovereignEvent(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    Hotpatch_TraceBeacon(buffer, strlen(buffer));
}

/// Convert token ID to string for logging (requires vocab access)
/// Implementation depends on your tokenizer
std::string TokenIdToDebugString(int32_t token_id);

} // namespace Sovereign
} // namespace RawrXD
