using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace RawrXD.Ollama.Agentic;

public sealed class AgentMemoryStore
{
    private readonly string _memoryPath;
    private readonly object _sync = new();
    private readonly List<AgentMemoryEntry> _entries = new();
    private readonly JsonSerializerOptions _jsonOptions = new()
    {
        WriteIndented = true,
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
    };

    public AgentMemoryStore(string? memoryPath = null)
    {
        _memoryPath = string.IsNullOrWhiteSpace(memoryPath)
            ? Path.Combine(AppContext.BaseDirectory, "agent-memory.json")
            : Path.GetFullPath(memoryPath);
        LoadFromDisk();
    }

    public IReadOnlyList<AgentMemoryEntry> Entries
    {
        get
        {
            lock (_sync)
            {
                return _entries.OrderByDescending(e => e.Timestamp).ToList();
            }
        }
    }

    public void AddEntry(string goal, string note, string? tag = null)
    {
        if (string.IsNullOrWhiteSpace(note)) return;

        lock (_sync)
        {
            _entries.Add(new AgentMemoryEntry
            {
                Goal = goal,
                Note = note.Trim(),
                Tag = tag,
                Timestamp = DateTimeOffset.UtcNow
            });
            Persist();
        }
    }

    private void LoadFromDisk()
    {
        try
        {
            if (!File.Exists(_memoryPath)) return;
            var json = File.ReadAllText(_memoryPath);
            var entries = JsonSerializer.Deserialize<List<AgentMemoryEntry>>(json, _jsonOptions);
            if (entries == null) return;
            lock (_sync)
            {
                _entries.Clear();
                _entries.AddRange(entries);
            }
        }
        catch
        {
            // Corrupted memory should not block the agent; start fresh instead.
        }
    }

    private void Persist()
    {
        try
        {
            var directory = Path.GetDirectoryName(_memoryPath);
            if (!string.IsNullOrWhiteSpace(directory) && !Directory.Exists(directory))
            {
                Directory.CreateDirectory(directory);
            }

            var json = JsonSerializer.Serialize(_entries, _jsonOptions);
            File.WriteAllText(_memoryPath, json);
        }
        catch
        {
            // Persistence failures are logged by the caller; ignore here to avoid crashes.
        }
    }
}

public sealed class AgentMemoryEntry
{
    public required string Goal { get; init; }
    public required string Note { get; init; }
    public string? Tag { get; init; }
    public DateTimeOffset Timestamp { get; init; }
}
