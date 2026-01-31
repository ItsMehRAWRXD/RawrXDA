@{
    RootModule        = 'AgenticFramework.psm1'
    ModuleVersion     = '0.1.0'
    GUID              = '2f2e0fa0-7d7d-4b6e-b222-222222222222'
    Author            = 'RawrXD'
    CompanyName       = 'RawrXD'
    Description       = 'Minimal PowerShell agentic loop scaffolding for external models (Ollama, etc).'
    PowerShellVersion = '7.0'
    FunctionsToExport = 'Set-AgentModel','Register-AgentTool','Get-AgentTools','Invoke-AgentTool','Invoke-ExternalModel','Start-AgentLoop'
    CmdletsToExport   = @()
    AliasesToExport   = @()
    PrivateData       = @{}
}
