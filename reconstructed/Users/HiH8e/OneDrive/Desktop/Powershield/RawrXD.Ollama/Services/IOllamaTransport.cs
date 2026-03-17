using RawrXD.Ollama.Models;

namespace RawrXD.Ollama.Services;

public interface IOllamaTransport
{
    Task SendAsync(OllamaRequestDTO request, CancellationToken cancellationToken = default);
    Task<string> SendAndGetStringAsync(OllamaRequestDTO request, CancellationToken cancellationToken = default);
}
