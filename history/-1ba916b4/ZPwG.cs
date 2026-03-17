namespace RawrXD.Ollama.Models;

public sealed class OllamaRequestDTO
{
    public required string Model { get; init; }
    public string? Prompt { get; init; }
    public double Temperature { get; init; } = 0.7;
    public IReadOnlyDictionary<string, object>? Options { get; init; }

    public bool TryValidate(out string errorMessage)
    {
        if (string.IsNullOrWhiteSpace(Model))
        {
            errorMessage = "Model must be provided.";
            return false;
        }

        if (Temperature is < 0 or > 2)
        {
            errorMessage = "Temperature must be between 0 and 2.";
            return false;
        }

        errorMessage = string.Empty;
        return true;
    }
}
