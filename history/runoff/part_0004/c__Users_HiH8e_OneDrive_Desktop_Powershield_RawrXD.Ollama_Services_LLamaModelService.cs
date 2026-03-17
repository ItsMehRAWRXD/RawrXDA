using LLama;
using LLama.Common;
using LLama.Native;
using System.Collections.Concurrent;
using Microsoft.Extensions.Logging;

namespace RawrXD.Ollama.Services;

/// <summary>
/// Direct LLaMA model inference service using LLamaSharp.
/// Runs models locally without any external services - completely offline and private.
/// </summary>
public sealed class LLamaModelService : IAsyncDisposable
{
    private LLamaWeights? _model;
    private LLamaContext? _context;
    private readonly string _modelPath;
    private readonly int _contextSize;
    private readonly int _gpuLayerCount;
    private readonly ILogger<LLamaModelService>? _logger;
    private InteractiveExecutor? _executor;
    
    // Dedicated inference thread to avoid blocking the HTTP server
    private Thread? _inferenceThread;
    private readonly BlockingCollection<InferenceRequest> _inferenceQueue = new();
    private readonly CancellationTokenSource _cancellation = new();
    private readonly SemaphoreSlim _readySignal = new(0);

    public bool IsModelLoaded => _model != null && _context != null;

    public LLamaModelService(string modelPath, int contextSize = 4096, int gpuLayerCount = 0, ILogger<LLamaModelService>? logger = null)
    {
        _modelPath = modelPath ?? throw new ArgumentNullException(nameof(modelPath));
        _contextSize = contextSize;
        _gpuLayerCount = gpuLayerCount;
        _logger = logger;
    }

    public async Task LoadModelAsync()
    {
        if (_model != null && _context != null)
        {
            _logger?.LogInformation("Model already loaded: {ModelPath}", _modelPath);
            return;
        }

        if (!File.Exists(_modelPath))
        {
            throw new FileNotFoundException($"Model file not found: {_modelPath}");
        }

        try
        {
            _logger?.LogInformation("📦 Loading model: {ModelPath}", _modelPath);
            var fileInfo = new FileInfo(_modelPath);
            _logger?.LogInformation("   Model size: {SizeGB} GB", Math.Round(fileInfo.Length / (1024.0 * 1024.0 * 1024.0), 2));
            
            var modelParams = new ModelParams(_modelPath)
            {
                ContextSize = (uint)_contextSize,
                GpuLayerCount = _gpuLayerCount
            };

            _model = LLamaWeights.LoadFromFile(modelParams);
            _context = _model.CreateContext(modelParams);
            _executor = new InteractiveExecutor(_context);
            
            // Start dedicated inference thread
            _inferenceThread = new Thread(InferenceWorkerThread)
            {
                Name = "LLamaInferenceWorker",
                IsBackground = true
            };
            _inferenceThread.Start();
            
            // Wait for inference thread to be ready
            await _readySignal.WaitAsync();
            
            _logger?.LogInformation("✅ Model loaded successfully - agentic inference ready");
        }
        catch (Exception ex)
        {
            _logger?.LogError(ex, "❌ Failed to load model: {ModelPath}", _modelPath);
            throw;
        }
    }

    public async Task<string> GenerateAsync(string prompt, int maxTokens = 512, float temperature = 0.9f, float topP = 0.9f)
    {
        if (_executor == null || _context == null || _model == null)
        {
            throw new InvalidOperationException("Model not loaded. Call LoadModelAsync first.");
        }

        try
        {
            _logger?.LogDebug("🤖 Generating: {PromptPreview}...", prompt[..Math.Min(60, prompt.Length)]);

            // Queue inference request - returns immediately, doesn't block
            // 70B model needs significant time - 10 minute timeout
            var cts = new CancellationTokenSource(TimeSpan.FromSeconds(600));
            var request = new InferenceRequest
            {
                Prompt = prompt,
                MaxTokens = maxTokens,
                Temperature = temperature,
                TopP = topP,
                ResultSource = new TaskCompletionSource<string>(),
                CancellationToken = cts.Token
            };

            _logger?.LogDebug("📤 Queuing inference request");
            _inferenceQueue.Add(request);
            
            _logger?.LogDebug("⏳ Waiting for inference result (this may take a while for large models)...");
            var response = await request.ResultSource.Task;

            _logger?.LogDebug("✅ Generation complete: {ResponseLength} characters", response.Length);
            return response;
        }
        catch (OperationCanceledException)
        {
            _logger?.LogError("⏱️ Generation timeout - model inference took longer than 10 minutes");
            throw;
        }
        catch (Exception ex)
        {
            _logger?.LogError(ex, "❌ Generation failed");
            throw;
        }
    }

    private void InferenceWorkerThread()
    {
        try
        {
            _logger?.LogInformation("🧵 Inference worker thread started");
            _readySignal.Release();

            while (!_cancellation.Token.IsCancellationRequested)
            {
                if (!_inferenceQueue.TryTake(out var request, 100, _cancellation.Token))
                    continue;

                try
                {
                    _logger?.LogInformation("🔄 Processing inference request");
                    var result = new System.Text.StringBuilder();
                    var inferParams = new InferenceParams
                    {
                        Temperature = request.Temperature,
                        TopP = request.TopP,
                        MaxTokens = request.MaxTokens
                    };

                    _logger?.LogInformation("📝 Prompting model: {Prompt}...", request.Prompt[..Math.Min(40, request.Prompt.Length)]);

                    // Run async inference synchronously on the worker thread
                    var inferTask = _executor.InferAsync(request.Prompt, inferParams);
                    var enumerator = inferTask.GetAsyncEnumerator(request.CancellationToken);
                    
                    int tokenCount = 0;
                    while (enumerator.MoveNextAsync().GetAwaiter().GetResult())
                    {
                        if (request.CancellationToken.IsCancellationRequested)
                        {
                            _logger?.LogWarning("⚠️ Inference cancelled after {TokenCount} tokens", tokenCount);
                            break;
                        }
                        result.Append(enumerator.Current);
                        tokenCount++;
                    }

                    _logger?.LogInformation("✅ Inference complete: {TokenCount} tokens generated", tokenCount);
                    request.ResultSource.SetResult(result.ToString().Trim());
                }
                catch (OperationCanceledException)
                {
                    _logger?.LogWarning("⚠️ Inference operation cancelled");
                    request.ResultSource.SetCanceled(request.CancellationToken);
                }
                catch (Exception ex)
                {
                    _logger?.LogError(ex, "💥 Inference processing error");
                    request.ResultSource.SetException(ex);
                }
            }

            _logger?.LogInformation("🧵 Inference worker thread stopped");
        }
        catch (Exception ex)
        {
            _logger?.LogError(ex, "💥 Inference worker thread crashed");
        }
    }

    public async ValueTask DisposeAsync()
    {
        _logger?.LogInformation("🛑 Unloading model...");
        _cancellation.Cancel();
        _inferenceQueue.CompleteAdding();
        
        if (_inferenceThread?.IsAlive == true)
        {
            _inferenceThread.Join(5000);
        }
        
        _context?.Dispose();
        _model?.Dispose();
        await Task.CompletedTask;
    }

    private class InferenceRequest
    {
        public required string Prompt { get; init; }
        public required int MaxTokens { get; init; }
        public required float Temperature { get; init; }
        public required float TopP { get; init; }
        public required TaskCompletionSource<string> ResultSource { get; init; }
        public required CancellationToken CancellationToken { get; init; }
    }
}

