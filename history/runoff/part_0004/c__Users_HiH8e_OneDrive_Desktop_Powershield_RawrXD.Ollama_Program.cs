using System;
using Microsoft.AspNetCore.Builder;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using RawrXD.Ollama.Agentic;
using RawrXD.Ollama.Models;
using RawrXD.Ollama.Services;

// OpenCL backend will automatically detect AMD GPU
// Environment.SetEnvironmentVariable("LLAMA_BACKEND", "vulkan");

var cliOptions = CliOptions.Parse(args);
using var shutdownCts = new CancellationTokenSource();
Console.CancelKeyPress += (_, eventArgs) =>
{
	eventArgs.Cancel = true;
	shutdownCts.Cancel();
};

if (cliOptions.TestInference)
{
	Console.WriteLine("🧪 Running direct inference test...");
	var modelPath = cliOptions.ModelPath ?? "D:\\OllamaModels\\BigDaddyG-7B-CHEETAH-V2.gguf";
	Console.WriteLine($"📂 Loading model from: {modelPath}");
	var service = new LLamaModelService(modelPath, contextSize: 4096, gpuLayerCount: cliOptions.GpuLayerCount);
	
	try
	{
		await service.LoadModelAsync();
		Console.WriteLine("✅ Model loaded. Generating text...");
		var result = await service.GenerateAsync("Hello, who are you?", maxTokens: 50);
		Console.WriteLine($"\n🤖 Response:\n{result}");
		Console.WriteLine("\n✅ Test passed!");
		return 0;
	}
	catch (Exception ex)
	{
		Console.WriteLine($"❌ Test failed: {ex.Message}");
		Console.WriteLine(ex.StackTrace);
		return 1;
	}
}

if (!string.IsNullOrWhiteSpace(cliOptions.AgentTask))
{
	return await RunAgentLoopAsync(cliOptions, shutdownCts.Token);
}

if (cliOptions.UseHttpHost)
{
	var builder = WebApplication.CreateBuilder(args);
	
	// Configure the listening URL
	builder.Configuration["Urls"] = $"http://127.0.0.1:{cliOptions.Port}";

	builder.Services.AddSingleton(cliOptions);
	builder.Services.AddSingleton<IToolExecutor, ToolExecutor>();
	
	// Register LLamaSharp model service (direct inference, no external calls)
	builder.Services.AddSingleton(sp =>
	{
		var logger = sp.GetRequiredService<ILogger<LLamaModelService>>();
		var modelPath = cliOptions.ModelPath ?? "D:\\OllamaModels\\BigDaddyG-7B-CHEETAH-V2.gguf";
		Console.WriteLine($"📂 Service loading model from: {modelPath}");
		var gpuLayers = cliOptions.GpuLayerCount;
		return new LLamaModelService(modelPath, contextSize: 4096, gpuLayerCount: gpuLayers, logger);
	});
	
	// Register LoRA adapter service for efficient fine-tuning
	builder.Services.AddSingleton(sp =>
	{
		var logger = sp.GetRequiredService<ILogger<LoRAAdapterService>>();
		var adapterDir = Path.Combine(AppContext.BaseDirectory, "adapters");
		return new LoRAAdapterService(adapterDir, logger);
	});
	
	// Register LoRA inference service
	builder.Services.AddSingleton(sp =>
	{
		var modelService = sp.GetRequiredService<LLamaModelService>();
		var adapterService = sp.GetRequiredService<LoRAAdapterService>();
		var logger = sp.GetRequiredService<ILogger<LoRAInferenceService>>();
		return new LoRAInferenceService(modelService, adapterService, logger);
	});
	
	builder.Services.AddControllers();
	builder.Services.AddLogging(logging => logging.AddConsole());

	var app = builder.Build();

	// Initialize model on startup
	var modelService = app.Services.GetRequiredService<LLamaModelService>();
	try
	{
		await modelService.LoadModelAsync();
		Console.WriteLine($"✅ Model loaded successfully from: {cliOptions.ModelPath}");
		Console.WriteLine($"✅ Ready for inference on port {cliOptions.Port}");
	}
	catch (Exception ex)
	{
		Console.WriteLine($"❌ Failed to load model: {ex.Message}");
		Console.WriteLine(ex.StackTrace);
		return 1;
	}

	app.MapControllers();
	app.MapGet("/health", () => Results.Ok(new { status = "ok", mode = "direct_inference", model_loaded = modelService.IsModelLoaded }));
	
	try
	{
		Console.WriteLine($"🚀 Starting Kestrel server on http://127.0.0.1:{cliOptions.Port}");
		await app.RunAsync(shutdownCts.Token);
	}
	catch (Exception ex)
	{
		Console.WriteLine($"❌ Server error: {ex.Message}");
		Console.WriteLine(ex.StackTrace);
		return 1;
	}
	
	return 0;
}

// CLI bridge mode not supported with direct inference
Console.WriteLine("❌ CLI bridge mode not supported. Use HTTP host mode (default).");
return 1;

static async Task<int> RunAgentLoopAsync(CliOptions cliOptions, CancellationToken cancellationToken)
{
	Console.WriteLine("🤖 Agent loop mode enabled");
	var modelPath = cliOptions.ModelPath ?? "D:\\OllamaModels\\BigDaddyG-7B-CHEETAH-V2.gguf";
	await using var modelService = new LLamaModelService(modelPath, contextSize: 4096, gpuLayerCount: cliOptions.GpuLayerCount);
	var toolExecutor = new ToolExecutor();
	var memoryStore = new AgentMemoryStore(cliOptions.AgentMemoryPath);
	var loopOptions = new AgentLoopOptions
	{
		MaxIterations = cliOptions.AgentMaxSteps,
		MemoryRecallLimit = 8,
		ScratchpadLimit = 8
	};

	try
	{
		var runner = new AgentLoopRunner(modelService, toolExecutor, memoryStore, loopOptions);
		var result = await runner.RunAsync(cliOptions.AgentTask!, cancellationToken);
		PrintAgentLoopSummary(result);
		return result.Completed ? 0 : 1;
	}
	catch (Exception ex)
	{
		Console.WriteLine($"❌ Agent loop failed: {ex.Message}");
		Console.WriteLine(ex.StackTrace);
		return 1;
	}
}

static void PrintAgentLoopSummary(AgentLoopResult result)
{
	Console.WriteLine("\n📋 Agent loop summary");
	foreach (var step in result.Steps)
	{
		Console.WriteLine($"Step {step.Index}: {step.Thought}");
		if (!string.IsNullOrWhiteSpace(step.Action))
		{
			Console.WriteLine($"  Action: {step.Action}");
		}
		if (!string.IsNullOrWhiteSpace(step.Observation))
		{
			Console.WriteLine($"  Observation: {step.Observation}");
		}
	}

	Console.WriteLine(result.Completed
		? "\n✅ Goal marked complete"
		: "\n⚠️ Goal not completed within the allotted steps");
}

internal sealed class CliOptions
{
	public bool UseHttpHost { get; set; } = true;
	public bool TestInference { get; set; }
	public bool UseStdinBridge { get; set; }
	public int Port { get; set; } = 5886;
	public string? ModelPath { get; set; } = "D:\\OllamaModels\\BigDaddyG-7B-CHEETAH-V2.gguf";
	public int GpuLayerCount { get; set; } = 100;  // Offload all layers (Codestral has < 100)
	public string? AgentTask { get; set; }
	public string? AgentMemoryPath { get; set; }
	public int AgentMaxSteps { get; set; } = 12;

	public static CliOptions Parse(string[] args)
	{
		var options = new CliOptions();

		for (var i = 0; i < args.Length; i++)
		{
			var current = args[i];

			if (string.Equals(current, "--test-inference", StringComparison.OrdinalIgnoreCase))
			{
				options.TestInference = true;
				options.UseHttpHost = false;
				continue;
			}

			if (TryReadValue(args, ref i, current, "--port", out var portValue) &&
				int.TryParse(portValue, out var parsedPort))
			{
				options.Port = parsedPort;
				continue;
			}

			if (TryReadValue(args, ref i, current, "--model", out var modelPath) &&
				!string.IsNullOrWhiteSpace(modelPath))
			{
				options.ModelPath = modelPath;
				continue;
			}

			if (TryReadValue(args, ref i, current, "--gpu-layers", out var gpuValue) &&
				int.TryParse(gpuValue, out var gpuLayers))
			{
				options.GpuLayerCount = gpuLayers;
				continue;
			}

			if (TryReadValue(args, ref i, current, "--agent-task", out var taskValue) &&
				!string.IsNullOrWhiteSpace(taskValue))
			{
				options.AgentTask = taskValue;
				options.UseHttpHost = false;
				continue;
			}

			if (TryReadValue(args, ref i, current, "--agent-memory", out var memoryPath) &&
				!string.IsNullOrWhiteSpace(memoryPath))
			{
				options.AgentMemoryPath = memoryPath;
				continue;
			}

			if (TryReadValue(args, ref i, current, "--agent-max-steps", out var stepValue) &&
				int.TryParse(stepValue, out var parsedSteps))
			{
				options.AgentMaxSteps = Math.Clamp(parsedSteps, 1, 100);
				continue;
			}
		}

		return options;
	}

	private static bool TryReadValue(string[] args, ref int index, string current, string flag, out string? value)
	{
		value = null;
		if (string.Equals(current, flag, StringComparison.OrdinalIgnoreCase))
		{
			if (index + 1 < args.Length)
			{
				value = args[++index];
				return true;
			}

			return false;
		}

		var prefix = $"{flag}=";
		if (current.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
		{
			value = current[prefix.Length..];
			return true;
		}

		return false;
	}
}
