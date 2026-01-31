using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using AIDesktopHelper.Api.Services;
using System.Security.Claims;

namespace AIDesktopHelper.Api.Controllers
{
    [ApiController]
    [Route("api/[controller]")]
    [Authorize]
    public class AIController : ControllerBase
    {
        private readonly IAIService _aiService;
        private readonly IFileService _fileService;
        private readonly ILogger<AIController> _logger;

        public AIController(IAIService aiService, IFileService fileService, ILogger<AIController> logger)
        {
            _aiService = aiService;
            _fileService = fileService;
            _logger = logger;
        }

        [HttpPost("analyze")]
        public async Task<IActionResult> AnalyzeCode([FromBody] AnalyzeCodeRequest request)
        {
            try
            {
                var userId = GetCurrentUserId();
                var response = await _aiService.AnalyzeFileAsync(request.FileId, request.AnalysisType);
                
                _logger.LogInformation($"Code analysis completed for user {userId}, file {request.FileId}");
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error analyzing code");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("generate")]
        public async Task<IActionResult> GenerateCode([FromBody] GenerateCodeRequest request)
        {
            try
            {
                var userId = GetCurrentUserId();
                var codeRequest = new CodeGenerationRequest
                {
                    Description = request.Description,
                    Language = request.Language,
                    Framework = request.Framework,
                    Requirements = request.Requirements,
                    UserId = userId,
                    Style = request.Style
                };

                var response = await _aiService.GenerateCodeAsync(codeRequest);
                
                _logger.LogInformation($"Code generation completed for user {userId}");
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error generating code");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("refactor")]
        public async Task<IActionResult> RefactorCode([FromBody] RefactorCodeRequest request)
        {
            try
            {
                var userId = GetCurrentUserId();
                var refactorRequest = new RefactorRequest
                {
                    Code = request.Code,
                    Language = request.Language,
                    Type = request.Type,
                    Goals = request.Goals,
                    UserId = userId
                };

                var response = await _aiService.RefactorCodeAsync(refactorRequest);
                
                _logger.LogInformation($"Code refactoring completed for user {userId}");
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error refactoring code");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("explain")]
        public async Task<IActionResult> ExplainCode([FromBody] ExplainCodeRequest request)
        {
            try
            {
                var response = await _aiService.ExplainCodeAsync(request.Code, request.Language);
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error explaining code");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("generate-tests")]
        public async Task<IActionResult> GenerateTests([FromBody] GenerateTestsRequest request)
        {
            try
            {
                var response = await _aiService.GenerateTestsAsync(request.Code, request.Language, request.TestType);
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error generating tests");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("optimize")]
        public async Task<IActionResult> OptimizePerformance([FromBody] OptimizeCodeRequest request)
        {
            try
            {
                var response = await _aiService.OptimizePerformanceAsync(request.Code, request.Language);
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error optimizing code");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("fix-vulnerabilities")]
        public async Task<IActionResult> FixVulnerabilities([FromBody] FixVulnerabilitiesRequest request)
        {
            try
            {
                var response = await _aiService.FixVulnerabilitiesAsync(request.FileId, request.Issues);
                return Ok(response);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error fixing vulnerabilities");
                return StatusCode(500, "Internal server error");
            }
        }

        [HttpPost("proactive-analysis")]
        public async Task<IActionResult> ProactiveAnalysis([FromBody] ProactiveAnalysisRequest request)
        {
            try
            {
                var userId = GetCurrentUserId();
                
                // Advanced proactive analysis combining multiple AI capabilities
                var tasks = new List<Task<AIResponse>>();
                
                // Security analysis
                tasks.Add(_aiService.AnalyzeFileAsync(request.FileId, AnalysisType.Security));
                
                // Performance analysis
                tasks.Add(_aiService.AnalyzeFileAsync(request.FileId, AnalysisType.Performance));
                
                // Code quality analysis
                tasks.Add(_aiService.AnalyzeFileAsync(request.FileId, AnalysisType.CodeQuality));
                
                var results = await Task.WhenAll(tasks);
                
                var combinedResponse = new AIResponse
                {
                    Content = "Proactive analysis completed",
                    Type = AIResponseType.Analysis,
                    Confidence = 0.95,
                    GeneratedAt = DateTime.UtcNow,
                    RequestId = Guid.NewGuid().ToString(),
                    SecurityIssues = results.SelectMany(r => r.SecurityIssues).ToList(),
                    PerformanceIssues = results.SelectMany(r => r.PerformanceIssues).ToList()
                };
                
                _logger.LogInformation($"Proactive analysis completed for user {userId}, file {request.FileId}");
                return Ok(combinedResponse);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error in proactive analysis");
                return StatusCode(500, "Internal server error");
            }
        }

        private string GetCurrentUserId()
        {
            return User.FindFirst(ClaimTypes.NameIdentifier)?.Value ?? "unknown";
        }
    }

    // Request DTOs
    public class AnalyzeCodeRequest
    {
        public string FileId { get; set; } = string.Empty;
        public AnalysisType AnalysisType { get; set; }
    }

    public class GenerateCodeRequest
    {
        public string Description { get; set; } = string.Empty;
        public string Language { get; set; } = string.Empty;
        public string Framework { get; set; } = string.Empty;
        public List<string> Requirements { get; set; } = new();
        public CodeStyle Style { get; set; } = CodeStyle.Standard;
    }

    public class RefactorCodeRequest
    {
        public string Code { get; set; } = string.Empty;
        public string Language { get; set; } = string.Empty;
        public RefactorType Type { get; set; }
        public List<string> Goals { get; set; } = new();
    }

    public class ExplainCodeRequest
    {
        public string Code { get; set; } = string.Empty;
        public string Language { get; set; } = string.Empty;
    }

    public class GenerateTestsRequest
    {
        public string Code { get; set; } = string.Empty;
        public string Language { get; set; } = string.Empty;
        public TestType TestType { get; set; }
    }

    public class OptimizeCodeRequest
    {
        public string Code { get; set; } = string.Empty;
        public string Language { get; set; } = string.Empty;
    }

    public class FixVulnerabilitiesRequest
    {
        public string FileId { get; set; } = string.Empty;
        public List<SecurityIssue> Issues { get; set; } = new();
    }

    public class ProactiveAnalysisRequest
    {
        public string FileId { get; set; } = string.Empty;
        public List<AnalysisType> AnalysisTypes { get; set; } = new();
    }
}
