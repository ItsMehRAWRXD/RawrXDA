using System.Net.Http.Json;
using System.Text.Json;
using RawrXD.Ollama.Models;

namespace RawrXD.Ollama.Services;

public sealed class OllamaHttpTransport(HttpClient httpClient) : IOllamaTransport
{
    private readonly HttpClient _httpClient = httpClient ?? throw new ArgumentNullException(nameof(httpClient));

    public async Task SendAsync(OllamaRequestDTO request, CancellationToken cancellationToken = default)
    {
        await SendAndGetStringAsync(request, cancellationToken);
    }

    public async Task<string> SendAndGetStringAsync(OllamaRequestDTO request, CancellationToken cancellationToken = default)
    {
        if (!request.TryValidate(out var validationError))
        {
            throw new ArgumentException(validationError, nameof(request));
        }

        // Ensure stream is false for tool parsing
        var requestBody = new 
        {
            model = request.Model,
            prompt = request.Prompt,
            temperature = request.Temperature,
            stream = false,
            options = request.Options
        };

        using var response = await _httpClient.PostAsJsonAsync("/api/generate", requestBody, cancellationToken).ConfigureAwait(false);
        
        if (!response.IsSuccessStatusCode)
        {
            var body = await response.Content.ReadAsStringAsync(cancellationToken).ConfigureAwait(false);
            throw new HttpRequestException($"Ollama request failed with status {(int)response.StatusCode}: {body}");
        }

        var jsonResponse = await response.Content.ReadFromJsonAsync<JsonElement>(cancellationToken: cancellationToken);
        if (jsonResponse.TryGetProperty("response", out var responseText))
        {
            return responseText.GetString() ?? string.Empty;
        }

        return string.Empty;
    }
}
