using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using RawrXD.Ollama.Services;

namespace RawrXD.Ollama.Agentic;

public sealed class AgentLoopRunner
{
    private readonly LLamaModelService _modelService;
    private readonly IToolExecutor _toolExecutor;
    private readonly AgentMemoryStore _memoryStore;
    private readonly AgentLoopOptions _options;

    public AgentLoopRunner(
        LLamaModelService modelService,
        IToolExecutor toolExecutor,
        AgentMemoryStore memoryStore,
        AgentLoopOptions options)
    {
        _modelService = modelService;
        _toolExecutor = toolExecutor;
        _memoryStore = memoryStore;
        _options = options;
    }

    public async Task<AgentLoopResult> RunAsync(string goal, CancellationToken cancellationToken)
    {
        if (! _modelService.IsModelLoaded)
        {
            await _modelService.LoadModelAsync();
        }

        var steps = new List<AgentStepRecord>();
        for (var iteration = 1; iteration <= _options.MaxIterations; iteration++)
        {
            cancellationToken.ThrowIfCancellationRequested();
            var prompt = BuildPrompt(goal, steps);
            var rawResponse = await _modelService.GenerateAsync(prompt, _options.MaxTokensPerStep, _options.Temperature, _options.TopP);
            var parsed = ParseResponse(rawResponse);

            string observation;
            if (parsed.Action != null && string.Equals(parsed.Action.Type, "tool", StringComparison.OrdinalIgnoreCase))
            {
                observation = await _toolExecutor.ExecuteToolAsync(
                    parsed.Action.Name ?? string.Empty,
                    parsed.Action.Input ?? string.Empty);
            }
            else
            {
                observation = parsed.Action?.Input ?? rawResponse;
            }

            var record = new AgentStepRecord
            {
                Index = iteration,
                Thought = parsed.Thought ?? rawResponse,
                Action = parsed.Action?.Name,
                Observation = observation,
                RawResponse = rawResponse,
                Completed = parsed.Completed
            };
            steps.Add(record);

            if (parsed.MemoryNotes?.Length > 0)
            {
                foreach (var note in parsed.MemoryNotes)
                {
                    _memoryStore.AddEntry(goal, note, parsed.Action?.Name);
                }
            }

            if (parsed.Completed)
            {
                break;
            }
        }

        return new AgentLoopResult(goal, steps);
    }

    private string BuildPrompt(string goal, IReadOnlyList<AgentStepRecord> steps)
    {
        var sb = new StringBuilder();
        sb.AppendLine("You are RawrXD's IDE operations agent.");
        sb.AppendLine("Stay focused on the stated goal, reason before acting, and prefer precise tool invocations.");
        sb.AppendLine();
        sb.AppendLine($"# Goal\n{goal}");
        sb.AppendLine();
        sb.AppendLine("# Long-term memory snippets (recent)");
        foreach (var memory in _memoryStore.Entries.Take(_options.MemoryRecallLimit))
        {
            sb.AppendLine($"- [{memory.Timestamp:u}] {memory.Note}");
        }
        if (!_memoryStore.Entries.Any())
        {
            sb.AppendLine("- (none)");
        }
        sb.AppendLine();
        sb.AppendLine("# Recent steps");
        if (steps.Count == 0)
        {
            sb.AppendLine("- No actions taken yet.");
        }
        else
        {
            foreach (var step in steps.TakeLast(_options.ScratchpadLimit))
            {
                sb.AppendLine($"{step.Index}. Thought: {step.Thought}");
                if (!string.IsNullOrWhiteSpace(step.Action))
                {
                    sb.AppendLine($"   Action: {step.Action}");
                }
                if (!string.IsNullOrWhiteSpace(step.Observation))
                {
                    sb.AppendLine($"   Observation: {TrimForPrompt(step.Observation)}");
                }
            }
        }
        sb.AppendLine();
        sb.AppendLine("# Response format");
        sb.AppendLine("Reply ONLY as minified JSON matching this schema:");
        sb.AppendLine("{\"thought\":string,\"completed\":bool,\"action\":{\"type\":string,\"name\":string,\"input\":string},\"memoryNotes\":[string]}\n");
        sb.AppendLine("- action.type may be 'tool' or 'response'.");
        sb.AppendLine("- When completed=true provide the final answer inside action.input.");
        sb.AppendLine("- memoryNotes is optional but helps future runs.");
        return sb.ToString();
    }

    private static string TrimForPrompt(string value)
    {
        if (string.IsNullOrWhiteSpace(value)) return string.Empty;
        var normalized = value.Replace('\n', ' ').Replace('\r', ' ');
        return normalized.Length <= 160 ? normalized : normalized[..160] + "...";
    }

    private static AgentModelResponse ParseResponse(string raw)
    {
        var json = ExtractJson(raw);
        if (json == null)
        {
            return new AgentModelResponse
            {
                Thought = raw.Trim(),
                Completed = raw.Contains("[done]", StringComparison.OrdinalIgnoreCase),
                Action = new AgentActionResponse
                {
                    Type = "response",
                    Input = raw.Trim()
                }
            };
        }

        try
        {
            var options = new JsonSerializerOptions
            {
                PropertyNameCaseInsensitive = true,
                DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
            };
            var parsed = JsonSerializer.Deserialize<AgentModelResponse>(json, options);
            return parsed ?? new AgentModelResponse { Thought = raw.Trim() };
        }
        catch
        {
            return new AgentModelResponse
            {
                Thought = raw.Trim(),
                Action = new AgentActionResponse { Type = "response", Input = raw.Trim() }
            };
        }
    }

    private static string? ExtractJson(string raw)
    {
        var start = raw.IndexOf('{');
        var end = raw.LastIndexOf('}');
        if (start < 0 || end <= start) return null;
        return raw[start..(end + 1)];
    }
}

public sealed record AgentLoopOptions
{
    public int MaxIterations { get; init; } = 12;
    public int MaxTokensPerStep { get; init; } = 384;
    public float Temperature { get; init; } = 0.8f;
    public float TopP { get; init; } = 0.9f;
    public int MemoryRecallLimit { get; init; } = 5;
    public int ScratchpadLimit { get; init; } = 6;
}

public sealed record AgentLoopResult(string Goal, IReadOnlyList<AgentStepRecord> Steps)
{
    public bool Completed => Steps.LastOrDefault()?.Completed ?? false;
}

public sealed class AgentStepRecord
{
    public int Index { get; init; }
    public string Thought { get; init; } = string.Empty;
    public string? Action { get; init; }
    public string Observation { get; init; } = string.Empty;
    public string RawResponse { get; init; } = string.Empty;
    public bool Completed { get; init; }
}

public sealed class AgentModelResponse
{
    [JsonPropertyName("thought")]
    public string? Thought { get; set; }

    [JsonPropertyName("completed")]
    public bool Completed { get; set; }

    [JsonPropertyName("action")]
    public AgentActionResponse? Action { get; set; }

    [JsonPropertyName("memoryNotes")]
    public string[]? MemoryNotes { get; set; }
}

public sealed class AgentActionResponse
{
    [JsonPropertyName("type")]
    public string? Type { get; set; }

    [JsonPropertyName("name")]
    public string? Name { get; set; }

    [JsonPropertyName("input")]
    public string? Input { get; set; }
}
