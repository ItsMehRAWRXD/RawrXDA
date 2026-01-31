// C# template for the multi-modal, attention-deficient AI agent
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace IDE_AI.Cognitive
{
    // Focus record
    public record Focus(
        string CodeContext,
        string FilePath,
        int LineNumber,
        DateTime StartTime,
        float Intensity = 1.0f
    );

    // Distraction record
    public record Distraction(
        string Origin,
        string Content,
        float Priority,
        DateTime Timestamp
    );

    // Cognitive State Manager
    public class StateManager
    {
        private readonly IAIProvider? _aiProvider;
        private readonly VectorDatabase? _vdb;
        private Focus? _currentFocus;
        private Focus? _previousFocus;
        private readonly ConcurrentQueue<Distraction> _distractions;
        private readonly Random _rng;

        public StateManager(IAIProvider? aiProvider = null, VectorDatabase? vdb = null)
        {
            _aiProvider = aiProvider;
            _vdb = vdb;
            _distractions = new ConcurrentQueue<Distraction>();
            _rng = new Random();
        }

        // Main cognitive cycle
        public async Task RunCycleAsync()
        {
            // Check if we should shift attention
            if (ShouldShiftAttention())
            {
                await ShiftAttentionAsync();
            }

            // Process current focus
            if (_currentFocus != null)
            {
                await ExecuteFocusBurstAsync();
            }

            // Check for impulsive actions
            if (ShouldDoImpulseAction())
            {
                await DoImpulseActionAsync();
            }

            // Process distractions
            ProcessDistractions();
        }

        // Add a distraction
        public void AddDistraction(Distraction distraction)
        {
            _distractions.Enqueue(distraction);
        }

        // Set current focus
        public void SetFocus(Focus focus)
        {
            if (_currentFocus != null)
            {
                _previousFocus = _currentFocus;
            }
            _currentFocus = focus;
        }

        // Get current focus
        public Focus? GetCurrentFocus() => _currentFocus;

        // Get previous focus
        public Focus? GetPreviousFocus() => _previousFocus;

        // Get recent distractions
        public List<Distraction> GetRecentDistractions()
        {
            var recent = new List<Distraction>();
            var tempQueue = new ConcurrentQueue<Distraction>();

            // Copy distractions to temp queue
            while (_distractions.TryDequeue(out var distraction))
            {
                recent.Add(distraction);
                tempQueue.Enqueue(distraction);
            }

            // Restore distractions
            while (tempQueue.TryDequeue(out var distraction))
            {
                _distractions.Enqueue(distraction);
            }

            return recent;
        }

        // Get impulsive refactor idea
        public async Task<string> GetImpulsiveRefactorIdeaAsync(string codeContext)
        {
            if (_aiProvider != null)
            {
                var prompt = $"Give me a quick refactor idea for this code: {codeContext}";
                return await _aiProvider.GenerateCompletionAsync(prompt);
            }
            return "Consider extracting this into a separate function";
        }

        // Check if we should shift attention
        private bool ShouldShiftAttention()
        {
            if (_currentFocus == null)
            {
                return true;
            }

            var focusDuration = DateTime.UtcNow - _currentFocus.StartTime;

            // Probabilistic attention shift based on focus duration
            var shiftProbability = Math.Min(0.1f + (float)focusDuration.TotalSeconds * 0.01f, 0.8f);

            return _rng.NextSingle() < shiftProbability;
        }

        // Shift attention to a new focus
        private async Task ShiftAttentionAsync()
        {
            if (_distractions.IsEmpty)
            {
                return;
            }

            // Get highest priority distraction
            if (_distractions.TryDequeue(out var distraction))
            {
                // Create new focus from distraction
                var newFocus = new Focus(
                    CodeContext: distraction.Content,
                    FilePath: "unknown",
                    LineNumber: 0,
                    StartTime: DateTime.UtcNow,
                    Intensity: distraction.Priority
                );
                SetFocus(newFocus);
            }
        }

        // Execute a burst of focused work
        private async Task ExecuteFocusBurstAsync()
        {
            if (_currentFocus == null)
            {
                return;
            }

            // Simulate focused work
            var workUnits = _rng.Next(1, 6);

            for (int i = 0; i < workUnits; i++)
            {
                // Simulate work on current focus
                if (_aiProvider != null)
                {
                    var prompt = $"Continue working on: {_currentFocus.CodeContext}";
                    await _aiProvider.GenerateCompletionAsync(prompt);
                }
            }
        }

        // Check if we should do an impulsive action
        private bool ShouldDoImpulseAction()
        {
            return _rng.NextSingle() < 0.15f; // 15% chance of impulsive action
        }

        // Do an impulsive action
        private async Task DoImpulseActionAsync()
        {
            var impulsiveActions = new[]
            {
                "Refactor this function",
                "Add error handling",
                "Optimize this loop",
                "Add comments",
                "Extract this into a class",
                "Add unit tests",
                "Improve variable names"
            };

            var action = impulsiveActions[_rng.Next(impulsiveActions.Length)];

            // Create distraction from impulsive action
            var impulse = new Distraction(
                Origin: "impulse",
                Content: action,
                Priority: 0.8f,
                Timestamp: DateTime.UtcNow
            );
            AddDistraction(impulse);
        }

        // Process pending distractions
        private void ProcessDistractions()
        {
            // Limit distraction queue size
            while (_distractions.Count > 10)
            {
                _distractions.TryDequeue(out _);
            }
        }
    }

    // Mock AI Provider interface
    public interface IAIProvider
    {
        Task<string> GenerateCompletionAsync(string prompt);
    }

    // Mock Vector Database
    public class VectorDatabase
    {
        // Implementation would go here
    }
}