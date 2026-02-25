@{
    RootModule = 'OllamaTools.psm1'
    ModuleVersion = '1.0.0'
    GUID = 'a1b2c3d4-e5f6-7890-abcd-ef1234567890'
    Author = 'BigDaddyG IDE'
    CompanyName = 'BigDaddyG'
    Copyright = '(c) 2024 BigDaddyG. All rights reserved.'
    Description = 'PowerShell module for interacting with Ollama server'
    PowerShellVersion = '7.2'
    FunctionsToExport = @('Invoke-OllamaGenerate')
    PrivateData = @{
        PSData = @{
            Tags = @('Ollama', 'AI', 'LLM')
            LicenseUri = ''
            ProjectUri = ''
            ReleaseNotes = 'Initial release'
        }
    }
}

