using Microsoft.AspNetCore.Mvc;
using RawrZSecurityPlatform.Models;
using RawrZSecurityPlatform.Services;

namespace RawrZSecurityPlatform.Controllers
{
    [ApiController]
    [Route("api/stubs")]
    public class StubGeneratorController : ControllerBase
    {
        private readonly IStubGeneratorService _stubGeneratorService;
        private readonly ILogger<StubGeneratorController> _logger;

        public StubGeneratorController(IStubGeneratorService stubGeneratorService, ILogger<StubGeneratorController> logger)
        {
            _stubGeneratorService = stubGeneratorService;
            _logger = logger;
        }

        [HttpPost("generate")]
        public async Task<ActionResult<StubGenerationResponse>> GenerateStub([FromBody] StubGenerationRequest request)
        {
            try
            {
                _logger.LogInformation($"Generating stub: {request.Type} for {request.Architecture}");

                var result = await _stubGeneratorService.GenerateStubAsync(request);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error generating stub");
                return StatusCode(500, new StubGenerationResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpGet("types")]
        public ActionResult<object> GetSupportedStubTypes()
        {
            var types = new
            {
                Windows = new[] { "exe", "dll", "sys", "com", "scr", "pif", "cpl", "msi", "msc" },
                Office = new[] { "xll", "doc", "docx", "xls", "xlsx", "ppt", "pptx", "pdf" },
                Shortcuts = new[] { "lnk", "url", "website" },
                Scripts = new[] { "bat", "cmd", "ps1", "psm1", "psd1", "vbs", "vbe", "js", "jse", "wsf", "wsh" },
                Archives = new[] { "zip", "rar", "7z", "jar" },
                Media = new[] { "mp3", "mp4", "jpg", "png", "ico" },
                System = new[] { "reg", "inf", "ini", "log", "tmp" },
                Network = new[] { "html", "xml", "json", "php", "asp", "aspx" },
                CrossPlatform = new[] { "elf", "macho" }
            };

            return Ok(types);
        }

        [HttpGet("architectures")]
        public ActionResult<object> GetSupportedArchitectures()
        {
            var architectures = new[] { "x64", "x86", "arm64", "any" };
            return Ok(architectures);
        }
    }
}
