using Microsoft.AspNetCore.Mvc;
using Microsoft.Extensions.Logging;
using RawrXD.Ollama.Models;
using RawrXD.Ollama.Services;

namespace RawrXD.Ollama.Controllers;

/// <summary>
/// LoRA adapter management and inference endpoint.
/// Allows loading, composing, and using LoRA adapters for efficient fine-tuning.
/// </summary>
[ApiController]
[Route("api/[controller]")]
public sealed class LoRAController : ControllerBase
{
    private readonly LoRAAdapterService _adapterService;
    private readonly LoRAInferenceService _inferenceService;
    private readonly ILogger<LoRAController> _logger;

    public LoRAController(
        LoRAAdapterService adapterService,
        LoRAInferenceService inferenceService,
        ILogger<LoRAController> logger)
    {
        _adapterService = adapterService;
        _inferenceService = inferenceService;
        _logger = logger;
    }

    /// <summary>
    /// Get list of available LoRA adapters.
    /// </summary>
    [HttpGet("available")]
    public async Task<IActionResult> GetAvailableAdapters()
    {
        try
        {
            var adapters = await _adapterService.GetAvailableAdaptersAsync();
            return Ok(new { adapters, count = adapters.Count });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to get available adapters");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    /// <summary>
    /// Get currently loaded adapters.
    /// </summary>
    [HttpGet("loaded")]
    public IActionResult GetLoadedAdapters()
    {
        var loaded = _adapterService.LoadedAdapters
            .Select(kvp => new
            {
                name = kvp.Key,
                path = kvp.Value.Path,
                size_mb = Math.Round(kvp.Value.FileSize / (1024.0 * 1024.0), 2),
                loaded_at = kvp.Value.LoadedAt
            })
            .ToList();

        return Ok(new { adapters = loaded, count = loaded.Count });
    }

    /// <summary>
    /// Load a LoRA adapter into memory.
    /// </summary>
    [HttpPost("load")]
    public async Task<IActionResult> LoadAdapter([FromBody] LoadAdapterRequest request)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(request.Name) || string.IsNullOrWhiteSpace(request.Path))
            {
                return BadRequest(new { error = "Name and Path are required" });
            }

            var adapter = await _adapterService.LoadAdapterAsync(request.Name, request.Path);
            return Ok(new
            {
                success = true,
                adapter = new
                {
                    name = adapter.Name,
                    path = adapter.Path,
                    size_mb = Math.Round(adapter.FileSize / (1024.0 * 1024.0), 2),
                    loaded_at = adapter.LoadedAt
                }
            });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to load adapter");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    /// <summary>
    /// Unload a LoRA adapter.
    /// </summary>
    [HttpPost("unload/{adapterName}")]
    public async Task<IActionResult> UnloadAdapter(string adapterName)
    {
        try
        {
            await _adapterService.UnloadAdapterAsync(adapterName);
            return Ok(new { success = true, message = $"Adapter '{adapterName}' unloaded" });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to unload adapter");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    /// <summary>
    /// Generate inference with a single LoRA adapter.
    /// </summary>
    [HttpPost("infer")]
    public async Task<IActionResult> GenerateWithAdapter([FromBody] LoRAInferenceRequest request, CancellationToken cancellationToken)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(request.Prompt) || string.IsNullOrWhiteSpace(request.Adapter))
            {
                return BadRequest(new { error = "Prompt and Adapter are required" });
            }

            _logger.LogInformation("LoRA inference request with adapter: {Adapter}", request.Adapter);

            var response = await _inferenceService.GenerateWithAdapterAsync(
                request.Prompt,
                request.Adapter,
                request.MaxTokens ?? 512,
                request.Temperature ?? 0.9f,
                request.TopP ?? 0.9f
            );

            var stats = _inferenceService.GetStats();
            return Ok(new
            {
                response,
                adapter = request.Adapter,
                stats
            });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "LoRA inference failed");
            return StatusCode(500, new { error = ex.Message });
        }
    }

    /// <summary>
    /// Get current inference statistics.
    /// </summary>
    [HttpGet("stats")]
    public IActionResult GetStats()
    {
        var stats = _inferenceService.GetStats();
        return Ok(stats);
    }
}

/// <summary>
/// Request to load a LoRA adapter.
/// </summary>
public class LoadAdapterRequest
{
    public string? Name { get; set; }
    public string? Path { get; set; }
}

/// <summary>
/// Request for LoRA-enhanced inference.
/// </summary>
public class LoRAInferenceRequest
{
    public string? Prompt { get; set; }
    public string? Adapter { get; set; }
    public int? MaxTokens { get; set; }
    public float? Temperature { get; set; }
    public float? TopP { get; set; }
}
