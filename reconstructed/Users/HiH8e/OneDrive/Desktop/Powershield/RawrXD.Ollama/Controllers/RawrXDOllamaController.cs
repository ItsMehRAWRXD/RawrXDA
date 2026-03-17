using System.Text.RegularExpressions;
using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using RawrXD.Ollama.Models;
using RawrXD.Ollama.Services;

namespace RawrXD.Ollama.Controllers;

[ApiController]
[Route("api/[controller]")]
public sealed class RawrXDOllamaController(LLamaModelService modelService, IToolExecutor toolExecutor, ILogger<RawrXDOllamaController> logger) : ControllerBase
{
    private readonly LLamaModelService _modelService = modelService ?? throw new ArgumentNullException(nameof(modelService));
    private readonly IToolExecutor _toolExecutor = toolExecutor ?? throw new ArgumentNullException(nameof(toolExecutor));
    private readonly ILogger<RawrXDOllamaController> _logger = logger ?? throw new ArgumentNullException(nameof(logger));

    [HttpPost]
    public async Task<IActionResult> GenerateAsync([FromBody] OllamaRequestDTO request, CancellationToken cancellationToken)
    {
        if (!_modelService.IsModelLoaded)
        {
            return BadRequest(new { error = "Model not loaded" });
        }

        try
        {
            // 1. Generate response from local model (no Ollama, stays on machine)
            _logger.LogInformation("Generating response for prompt: {PromptPreview}", request.Prompt[..Math.Min(50, request.Prompt.Length)]);
            
            var responseText = await _modelService.GenerateAsync(
                request.Prompt,
                maxTokens: 1024,
                temperature: (float)request.Temperature,
                topP: 0.9f
            ).ConfigureAwait(false);

            // 2. Parse for tool calls
            var toolMatches = Regex.Matches(responseText, @"(CHEETAH_execute|write_file|read_file)\((.*?)\)");
            
            var executionResults = new List<string>();

            foreach (Match match in toolMatches)
            {
                var toolName = match.Groups[1].Value;
                var args = match.Groups[2].Value;
                
                _logger.LogInformation("Executing tool: {ToolName}", toolName);
                var result = await _toolExecutor.ExecuteToolAsync(toolName, args);
                executionResults.Add($"[TOOL: {toolName}] {result}");
            }

            _logger.LogInformation("✅ Generation complete with {ToolCount} tool executions", executionResults.Count);

            return Ok(new { 
                response = responseText, 
                tool_executions = executionResults 
            });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "❌ Generation failed");
            return StatusCode(500, new { error = ex.Message });
        }
    }
}
