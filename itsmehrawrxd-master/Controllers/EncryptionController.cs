using Microsoft.AspNetCore.Mvc;
using RawrZSecurityPlatform.Models;
using RawrZSecurityPlatform.Services;
using System.Security.Cryptography;

namespace RawrZSecurityPlatform.Controllers
{
    [ApiController]
    [Route("api/[controller]")]
    public class EncryptionController : ControllerBase
    {
        private readonly IEncryptionService _encryptionService;
        private readonly ILogger<EncryptionController> _logger;

        public EncryptionController(IEncryptionService encryptionService, ILogger<EncryptionController> logger)
        {
            _encryptionService = encryptionService;
            _logger = logger;
        }

        [HttpPost("encrypt-file")]
        public async Task<ActionResult<EncryptionResponse>> EncryptFile([FromForm] IFormFile file, [FromForm] EncryptionRequest request)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new EncryptionResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Encrypting file: {file.FileName} with algorithm: {request.Algorithm}");

                var result = await _encryptionService.EncryptFileAsync(file, request);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error encrypting file");
                return StatusCode(500, new EncryptionResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("decrypt-file")]
        public async Task<ActionResult<DecryptionResponse>> DecryptFile([FromForm] IFormFile file, [FromForm] DecryptionRequest request)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new DecryptionResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Decrypting file: {file.FileName}");

                var result = await _encryptionService.DecryptFileAsync(file, request);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error decrypting file");
                return StatusCode(500, new DecryptionResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("hash-file")]
        public async Task<ActionResult<HashResponse>> HashFile([FromForm] IFormFile file, [FromForm] HashRequest request)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new HashResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Hashing file: {file.FileName} with algorithm: {request.Algorithm}");

                var result = await _encryptionService.HashFileAsync(file, request);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error hashing file");
                return StatusCode(500, new HashResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("analyze-file")]
        public async Task<ActionResult<FileAnalysisResponse>> AnalyzeFile([FromForm] IFormFile file, [FromForm] FileAnalysisRequest request)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new FileAnalysisResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Analyzing file: {file.FileName}");

                var result = await _encryptionService.AnalyzeFileAsync(file, request);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error analyzing file");
                return StatusCode(500, new FileAnalysisResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("malware-scan")]
        public async Task<ActionResult<FileAnalysisResponse>> ScanForMalware([FromForm] IFormFile file)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new FileAnalysisResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Scanning file for malware: {file.FileName}");

                var result = await _encryptionService.ScanForMalwareAsync(file);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error scanning for malware");
                return StatusCode(500, new FileAnalysisResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("entropy-analysis")]
        public async Task<ActionResult<FileAnalysisResponse>> EntropyAnalysis([FromForm] IFormFile file)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new FileAnalysisResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Performing entropy analysis on file: {file.FileName}");

                var result = await _encryptionService.EntropyAnalysisAsync(file);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error performing entropy analysis");
                return StatusCode(500, new FileAnalysisResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("integrity-check")]
        public async Task<ActionResult<FileAnalysisResponse>> IntegrityCheck([FromForm] IFormFile file)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new FileAnalysisResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Performing integrity check on file: {file.FileName}");

                var result = await _encryptionService.IntegrityCheckAsync(file);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error performing integrity check");
                return StatusCode(500, new FileAnalysisResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("generate-keys")]
        public async Task<ActionResult<KeyGenerationResponse>> GenerateKeys([FromBody] KeyGenerationRequest request)
        {
            try
            {
                _logger.LogInformation($"Generating keys with algorithm: {request.Algorithm}, size: {request.KeySize}");

                var result = await _encryptionService.GenerateKeysAsync(request);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error generating keys");
                return StatusCode(500, new KeyGenerationResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpGet("export-keys")]
        public async Task<ActionResult<KeyExportResponse>> ExportKeys()
        {
            try
            {
                _logger.LogInformation("Exporting encryption keys");

                var result = await _encryptionService.ExportKeysAsync();
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error exporting keys");
                return StatusCode(500, new KeyExportResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpPost("import-keys")]
        public async Task<ActionResult<KeyImportResponse>> ImportKeys([FromForm] IFormFile file)
        {
            try
            {
                if (file == null || file.Length == 0)
                    return BadRequest(new KeyImportResponse { Success = false, Error = "No file provided" });

                _logger.LogInformation($"Importing keys from file: {file.FileName}");

                var result = await _encryptionService.ImportKeysAsync(file);
                return Ok(result);
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error importing keys");
                return StatusCode(500, new KeyImportResponse { Success = false, Error = ex.Message });
            }
        }

        [HttpGet("algorithms")]
        public ActionResult<object> GetSupportedAlgorithms()
        {
            var algorithms = new
            {
                AES = new[] { "AES-256-GCM", "AES-256-CBC", "AES-256-CTR", "AES-192-GCM", "AES-192-CBC", "AES-128-GCM", "AES-128-CBC" },
                ChaCha20 = new[] { "ChaCha20-Poly1305", "ChaCha20-IETF", "ChaCha20-Original" },
                ARIA = new[] { "ARIA-256-GCM", "ARIA-256-CBC", "ARIA-192-GCM", "ARIA-128-GCM" },
                Camellia = new[] { "Camellia-256-CBC", "Camellia-256-GCM", "Camellia-192-CBC", "Camellia-128-CBC" },
                Twofish = new[] { "Twofish-256-CBC", "Twofish-256-GCM", "Twofish-192-CBC", "Twofish-128-CBC" },
                Serpent = new[] { "Serpent-256-CBC", "Serpent-256-GCM", "Serpent-192-CBC", "Serpent-128-CBC" },
                Blowfish = new[] { "Blowfish-CBC", "Blowfish-CTR", "Blowfish-CFB", "Blowfish-OFB", "Blowfish-ECB" },
                TripleDES = new[] { "3DES-CBC", "3DES-CTR", "3DES-CFB", "3DES-OFB", "3DES-ECB" },
                CAST5 = new[] { "CAST5-CBC", "CAST5-CTR", "CAST5-CFB", "CAST5-OFB", "CAST5-ECB" },
                IDEA = new[] { "IDEA-CBC", "IDEA-CTR", "IDEA-CFB", "IDEA-OFB", "IDEA-ECB" },
                RC4 = new[] { "RC4", "RC4-40", "RC4-128" },
                Advanced = new[] { "DUAL-AES-CAMELLIA", "TRIPLE-AES-SERPENT-TWOFISH", "CASCADE-ALL", "CUSTOM-COMBINATION" },
                PostQuantum = new[] { "KYBER-768", "DILITHIUM-2", "FALCON-512" },
                Lightweight = new[] { "PRESENT-80", "PRESENT-128", "SIMON-64-128", "SIMON-128-128", "SPECK-64-128", "SPECK-128-128" }
            };

            return Ok(algorithms);
        }

        [HttpGet("extensions")]
        public ActionResult<object> GetSupportedExtensions()
        {
            var extensions = new
            {
                Generic = new[] { ".enc", ".secure", ".locked", ".protected", ".encrypted", ".safe", ".private" },
                Windows = new[] { ".exe", ".dll", ".sys", ".com", ".scr", ".pif", ".cpl", ".msi", ".msc" },
                Office = new[] { ".xll", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx", ".pdf" },
                Shortcuts = new[] { ".lnk", ".url", ".website" },
                Scripts = new[] { ".bat", ".cmd", ".ps1", ".psm1", ".psd1", ".vbs", ".vbe", ".js", ".jse", ".wsf", ".wsh" },
                Archives = new[] { ".zip", ".rar", ".7z", ".tar", ".gz", ".jar", ".war", ".ear" },
                Media = new[] { ".mp3", ".mp4", ".avi", ".jpg", ".png", ".gif", ".bmp", ".ico" },
                System = new[] { ".reg", ".inf", ".ini", ".cfg", ".conf", ".log", ".tmp", ".temp" },
                Network = new[] { ".html", ".htm", ".xml", ".json", ".css", ".php", ".asp", ".aspx" }
            };

            return Ok(extensions);
        }
    }
}
