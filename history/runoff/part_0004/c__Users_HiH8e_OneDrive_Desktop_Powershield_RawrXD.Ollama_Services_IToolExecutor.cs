namespace RawrXD.Ollama.Services;

public interface IToolExecutor
{
    Task<string> ExecuteToolAsync(string toolName, string arguments);
}
