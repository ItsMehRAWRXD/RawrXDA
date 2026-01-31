using System.Text;
using System.Text.Json;
using AIDesktopHelper.Api.Data;

namespace AIDesktopHelper.Api.Services
{
    public class AIService : IAIService
    {
        private readonly HttpClient _httpClient;
        private readonly IConfiguration _configuration;
        private readonly ILogger<AIService> _logger;
        private readonly IFileService _fileService;

        public AIService(HttpClient httpClient, IConfiguration configuration, ILogger<AIService> logger, IFileService fileService)
        {
            _httpClient = httpClient;
            _configuration = configuration;
            _logger = logger;
            _fileService = fileService;
        }

        public async Task<AIResponse> ProcessCodeRequestAsync(CodeRequest request)
        {
            var prompt = BuildPrompt(request);
            var response = await CallOpenAIAsync(prompt, request.UserId);
            
            return new AIResponse
            {
                Content = response,
                Type = MapActionToResponseType(request.Action),
                Confidence = 0.85,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> AnalyzeFileAsync(string fileId, AnalysisType analysisType)
        {
            var fileData = await _fileService.DownloadEncryptedFileAsync(fileId);
            var code = Encoding.UTF8.GetString(fileData);
            
            var prompt = BuildAnalysisPrompt(code, analysisType);
            var response = await CallOpenAIAsync(prompt, "system");
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Analysis,
                Confidence = 0.90,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> GenerateCodeAsync(CodeGenerationRequest request)
        {
            var prompt = BuildCodeGenerationPrompt(request);
            var response = await CallOpenAIAsync(prompt, request.UserId);
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Code,
                Confidence = 0.80,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> RefactorCodeAsync(RefactorRequest request)
        {
            var prompt = BuildRefactorPrompt(request);
            var response = await CallOpenAIAsync(prompt, request.UserId);
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Code,
                Confidence = 0.85,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> FixVulnerabilitiesAsync(string fileId, List<SecurityIssue> issues)
        {
            var fileData = await _fileService.DownloadEncryptedFileAsync(fileId);
            var code = Encoding.UTF8.GetString(fileData);
            
            var prompt = BuildVulnerabilityFixPrompt(code, issues);
            var response = await CallOpenAIAsync(prompt, "system");
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Fix,
                Confidence = 0.95,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> ExplainCodeAsync(string code, string language)
        {
            var prompt = BuildExplanationPrompt(code, language);
            var response = await CallOpenAIAsync(prompt, "system");
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Explanation,
                Confidence = 0.90,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> GenerateTestsAsync(string code, string language, TestType testType)
        {
            var prompt = BuildTestGenerationPrompt(code, language, testType);
            var response = await CallOpenAIAsync(prompt, "system");
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Test,
                Confidence = 0.85,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        public async Task<AIResponse> OptimizePerformanceAsync(string code, string language)
        {
            var prompt = BuildOptimizationPrompt(code, language);
            var response = await CallOpenAIAsync(prompt, "system");
            
            return new AIResponse
            {
                Content = response,
                Type = AIResponseType.Code,
                Confidence = 0.80,
                GeneratedAt = DateTime.UtcNow,
                RequestId = Guid.NewGuid().ToString()
            };
        }

        private async Task<string> CallOpenAIAsync(string prompt, string userId)
        {
            var apiKey = _configuration["OpenAI:ApiKey"];
            if (string.IsNullOrEmpty(apiKey))
                throw new InvalidOperationException("OpenAI API key not configured");

            var messages = new[]
            {
                new { role = "system", content = "You are an expert software engineer and security specialist." },
                new { role = "user", content = prompt }
            };

            var requestBody = new
            {
                model = "gpt-4",
                messages = messages,
                max_tokens = 4000,
                temperature = 0.1
            };

            var json = JsonSerializer.Serialize(requestBody);
            var content = new StringContent(json, Encoding.UTF8, "application/json");

            _httpClient.DefaultRequestHeaders.Authorization = 
                new System.Net.Http.Headers.AuthenticationHeaderValue("Bearer", apiKey);

            var response = await _httpClient.PostAsync("https://api.openai.com/v1/chat/completions", content);
            
            if (!response.IsSuccessStatusCode)
            {
                var errorContent = await response.Content.ReadAsStringAsync();
                throw new Exception($"OpenAI API error: {response.StatusCode} - {errorContent}");
            }

            var responseContent = await response.Content.ReadAsStringAsync();
            var openAIResponse = JsonSerializer.Deserialize<OpenAIResponse>(responseContent);
            
            return openAIResponse?.choices?.FirstOrDefault()?.message?.content ?? "No response generated";
        }

        private string BuildPrompt(CodeRequest request)
        {
            return $"Language: {request.Language}\nAction: {request.Action}\nContext: {request.Context}\n\nCode:\n{request.Code}";
        }

        private string BuildAnalysisPrompt(string code, AnalysisType analysisType)
        {
            return $"Analyze the following {analysisType} code for security vulnerabilities and performance issues:\n\n```csharp\n{code}\n```";
        }

        private string BuildCodeGenerationPrompt(CodeGenerationRequest request)
        {
            return $"Generate {request.Language} code: {request.Description}\nFramework: {request.Framework}\nStyle: {request.Style}";
        }

        private string BuildRefactorPrompt(RefactorRequest request)
        {
            return $"Refactor this {request.Language} code using {request.Type}:\n\n```{request.Language.ToLower()}\n{request.Code}\n```";
        }

        private string BuildVulnerabilityFixPrompt(string code, List<SecurityIssue> issues)
        {
            return $"Fix security vulnerabilities in this code:\n\n```csharp\n{code}\n```\n\nIssues: {string.Join(", ", issues.Select(i => i.Type))}";
        }

        private string BuildExplanationPrompt(string code, string language)
        {
            return $"Explain this {language} code:\n\n```{language.ToLower()}\n{code}\n```";
        }

        private string BuildTestGenerationPrompt(string code, string language, TestType testType)
        {
            return $"Generate {testType} tests for this {language} code:\n\n```{language.ToLower()}\n{code}\n```";
        }

        private string BuildOptimizationPrompt(string code, string language)
        {
            return $"Optimize this {language} code for performance:\n\n```{language.ToLower()}\n{code}\n```";
        }

        private AIResponseType MapActionToResponseType(AIAction action)
        {
            return action switch
            {
                AIAction.Analyze => AIResponseType.Analysis,
                AIAction.Generate => AIResponseType.Code,
                AIAction.Refactor => AIResponseType.Code,
                AIAction.Fix => AIResponseType.Fix,
                AIAction.Explain => AIResponseType.Explanation,
                AIAction.Test => AIResponseType.Test,
                AIAction.Optimize => AIResponseType.Code,
                _ => AIResponseType.Suggestion
            };
        }
    }

    public class OpenAIResponse
    {
        public List<OpenAIChoice> choices { get; set; } = new();
    }

    public class OpenAIChoice
    {
        public OpenAIMessage message { get; set; } = new();
    }

    public class OpenAIMessage
    {
        public string content { get; set; } = string.Empty;
    }
}
