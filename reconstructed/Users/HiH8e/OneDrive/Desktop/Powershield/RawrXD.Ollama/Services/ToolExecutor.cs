using System.Diagnostics;

namespace RawrXD.Ollama.Services;

public sealed class ToolExecutor : IToolExecutor
{
    public async Task<string> ExecuteToolAsync(string toolName, string arguments)
    {
        return toolName.ToLowerInvariant() switch
        {
            "cheetah_execute" => await ExecuteShellCommandAsync(arguments),
            "write_file" => await WriteFileAsync(arguments),
            "read_file" => await ReadFileAsync(arguments),
            _ => $"Error: Unknown tool '{toolName}'"
        };
    }

    private static async Task<string> ExecuteShellCommandAsync(string command)
    {
        try
        {
            // Clean up quotes if present
            command = command.Trim('"');
            
            var startInfo = new ProcessStartInfo
            {
                FileName = "powershell.exe",
                Arguments = $"-NoProfile -NonInteractive -Command \"{command}\"",
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            };

            using var process = new Process { StartInfo = startInfo };
            process.Start();
            
            var output = await process.StandardOutput.ReadToEndAsync();
            var error = await process.StandardError.ReadToEndAsync();
            
            await process.WaitForExitAsync();

            return string.IsNullOrWhiteSpace(error) 
                ? output.Trim() 
                : $"Error: {error.Trim()}";
        }
        catch (Exception ex)
        {
            return $"Execution failed: {ex.Message}";
        }
    }

    private static async Task<string> WriteFileAsync(string args)
    {
        try
        {
            // Simple CSV-style parsing: "filename", "content"
            var parts = args.Split(',', 2);
            if (parts.Length != 2) return "Error: Invalid arguments. Expected \"filename\", \"content\"";

            var path = parts[0].Trim().Trim('"');
            var content = parts[1].Trim().Trim('"');

            await File.WriteAllTextAsync(path, content);
            return $"Success: File written to {path}";
        }
        catch (Exception ex)
        {
            return $"Write failed: {ex.Message}";
        }
    }

    private static async Task<string> ReadFileAsync(string path)
    {
        try
        {
            path = path.Trim().Trim('"');
            if (!File.Exists(path)) return "Error: File not found";
            return await File.ReadAllTextAsync(path);
        }
        catch (Exception ex)
        {
            return $"Read failed: {ex.Message}";
        }
    }
}
