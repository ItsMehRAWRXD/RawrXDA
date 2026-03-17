using System;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace CompilerStudio.Services;

public class LocalLlmService
{
    // Path to your local llama.cpp executable and model
    private readonly string _llamaExePath = @"C:\\Models\\llama.cpp\\build\\bin\\Release\\main.exe";
    private readonly string _modelPath = @"C:\\Models\\phi-2.Q4_K_M.gguf";

    public async Task<string> AskAsync(string prompt)
    {
        if (!File.Exists(_llamaExePath))
            throw new FileNotFoundException("llama.cpp executable not found.", _llamaExePath);

        if (!File.Exists(_modelPath))
            throw new FileNotFoundException("LLM model file not found.", _modelPath);

        var process = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = _llamaExePath,
                Arguments = $"-m \"{_modelPath}\" -p \"{prompt}\" -n 256 --temp 0.7 --repeat_penalty 1.1",
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true
            }
        };

        process.Start();

        var output = new StringBuilder();
        var error = new StringBuilder();

        // Read output asynchronously
        var outputTask = process.StandardOutput.ReadToEndAsync();
        var errorTask = process.StandardError.ReadToEndAsync();

        await process.WaitForExitAsync();

        output.Append(await outputTask);
        error.Append(await errorTask);

        if (process.ExitCode != 0)
            throw new InvalidOperationException($"LLM process failed: {error}");

        return output.ToString().Trim();
    }
}
