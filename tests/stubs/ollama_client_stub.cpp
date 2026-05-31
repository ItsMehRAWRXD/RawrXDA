#include "backend/ollama_client.h"

namespace RawrXD { namespace Backend {

OllamaClient::OllamaClient(const std::string&) {}
OllamaClient::~OllamaClient() = default;
OllamaResponse OllamaClient::chatSync(const OllamaChatRequest&) { return OllamaResponse{}; }

}} // namespace RawrXD::Backend
