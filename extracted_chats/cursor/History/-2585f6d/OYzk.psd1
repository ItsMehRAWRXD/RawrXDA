@{
    RootModule = 'OllamaTools.psm1'
    ModuleVersion = '1.0.0'
    GUID = 'a1b2c3d4-e5f6-7890-abcd-ef1234567890'
    Author = 'BigDaddyG IDE'
    CompanyName = 'BigDaddyG'
    Copyright = '(c) 2024 BigDaddyG. All rights reserved.'
    Description = 'PowerShell module for interacting with Ollama server. Provides Invoke-OllamaGenerate function for local LLM inference with streaming and non-streaming support.'
    PowerShellVersion = '7.2'
    FunctionsToExport = @('Invoke-OllamaGenerate')
    CmdletsToExport = @()
    VariablesToExport = @()
    AliasesToExport = @()
    PrivateData = @{
        PSData = @{
            Tags = @('Ollama', 'AI', 'LLM', 'Local', 'Inference')
            LicenseUri = ''
            ProjectUri = ''
            ReleaseNotes = @'
Initial release - v1.0.0
- Invoke-OllamaGenerate function with streaming and non-streaming modes
- Advanced options support (temperature, top_p, top_k, etc.)
- Robust error handling and timeout protection
'@
        }
    }
}

