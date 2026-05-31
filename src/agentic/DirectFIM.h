#pragma once

/**
 * DirectFIM.h — Single-function API for real FIM completions
 * 
 * No abstractions. No routers. No bridges.
 * Just call DirectFIM_Complete() and get real tokens.
 */

#include <string>
#include <functional>

/**
 * Get a real FIM completion using existing working backends.
 * 
 * Priority:
 *   1. RawrXD_Titan.dll (native, fastest)
 *   2. BackendOrchestrator (queue-based)
 *   3. Ollama HTTP (fallback)
 * 
 * @param prefix     Code before cursor
 * @param suffix     Code after cursor
 * @param modelPath  Path to GGUF for Titan (empty = skip Titan)
 * @param maxTokens  Max tokens to generate
 * @return           Completion string (empty if all backends failed)
 */
std::string DirectFIM_Complete(const std::string& prefix, const std::string& suffix,
                                  const std::string& modelPath, int maxTokens);

/**
 * Streaming version for real-time ghost text.
 * 
 * @param prefix     Code before cursor
 * @param suffix     Code after cursor
 * @param modelPath  Path to GGUF for Titan (empty = use Ollama streaming)
 * @param maxTokens  Max tokens to generate
 * @param onToken    Callback for each token
 * @return           true if streaming succeeded
 */
bool DirectFIM_CompleteWithStream(const std::string& prefix, const std::string& suffix,
                                     const std::string& modelPath, int maxTokens,
                                     std::function<void(const std::string&)> onToken);

/**
 * Quick configuration (call before first use if defaults wrong)
 */
void DirectFIM_SetOllamaHost(const char* host, int port);
void DirectFIM_SetOllamaModel(const char* modelName);
void DirectFIM_SetTitanPath(const char* dllPath);
