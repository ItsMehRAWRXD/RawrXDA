#include "backend/ollama_client.h"

namespace RawrXD { namespace Backend {

OllamaClient::OllamaClient(const std::string&) {}
OllamaClient::~OllamaClient() = default;
NativeInferenceResponse OllamaClient::chatSync(const OllamaChatRequest&) { return NativeInferenceResponse{}; }

}} // namespace RawrXD::Backend
