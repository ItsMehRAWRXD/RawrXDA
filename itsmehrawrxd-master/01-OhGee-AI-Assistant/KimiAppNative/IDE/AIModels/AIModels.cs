using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace KimiAppNative.IDE.AIModels
{
    /// <summary>
    /// Shared models and interfaces for AI integrations
    /// </summary>
    
    public interface IAIProvider
    {
        event Action<string>? OnResponse;
        event Action<string>? OnError;
        
        Task<string> SendMessage(string message, bool includeContext = true);
        Task<string> GenerateCode(string description, string language, string context = "");
        Task<string> ExplainCode(string code, string language = "");
        Task<List<CodeIssue>> AnalyzeCode(string code, string language = "");
    }

    public class ConversationMessage
    {
        public string Role { get; set; } = "";
        public string Content { get; set; } = "";
        public DateTime Timestamp { get; set; } = DateTime.Now;
    }

    public class CodeIssue
    {
        public string Severity { get; set; } = "";
        public string Description { get; set; } = "";
        public int Line { get; set; }
        public int Column { get; set; }
    }

    public class ChatMessage
    {
        public string Role { get; set; } = "";
        public string Content { get; set; } = "";
        public DateTime Timestamp { get; set; } = DateTime.Now;
    }

    public class ConversationTurn
    {
        public string Role { get; set; } = "";
        public string Content { get; set; } = "";
        public DateTime Timestamp { get; set; } = DateTime.Now;
    }

    public class ConversationContext
    {
        public string Role { get; set; } = "";
        public string Content { get; set; } = "";
        public DateTime Timestamp { get; set; } = DateTime.Now;
        public string ProjectContext { get; set; } = "";
    }

    public class CodeComplexityAnalysis
    {
        public string TimeComplexity { get; set; } = "";
        public string SpaceComplexity { get; set; } = "";
        public string Error { get; set; } = "";
    }

    public class PerformanceIssue
    {
        public string Severity { get; set; } = "";
        public string Description { get; set; } = "";
        public int Line { get; set; }
    }

    public class SecurityIssue
    {
        public string Severity { get; set; } = "";
        public string Description { get; set; } = "";
        public int Line { get; set; }
    }

    public class CodeSuggestion
    {
        public string Content { get; set; } = "";
        public float Confidence { get; set; }
        public string Language { get; set; } = "";
        public int StartPosition { get; set; }
        public int EndPosition { get; set; }
    }

    public class AIModelConfig
    {
        public string Name { get; set; } = "";
        public string Provider { get; set; } = "";
        public string ModelId { get; set; } = "";
        public int MaxTokens { get; set; }
        public float Temperature { get; set; }
        public string[] Capabilities { get; set; } = Array.Empty<string>();
        public bool IsConfigured { get; set; }
    }

    public class AIModelInfo
    {
        public string Name { get; set; } = "";
        public string Provider { get; set; } = "";
        public bool IsConfigured { get; set; }
        public string[] Capabilities { get; set; } = Array.Empty<string>();
    }
}
