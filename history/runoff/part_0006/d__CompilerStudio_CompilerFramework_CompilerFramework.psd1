@{
    RootModule = 'CompilerFramework.psm1'
    ModuleVersion = '0.1.0'
    GUID = 'B5C4E4E8-7B9A-4F9E-8C0A-9B0A0A0A0A0D'
    Author = 'Your Name'
    CompanyName = 'Your Company'
    Copyright = '(c) 2025 Your Company. All rights reserved.'
    Description = 'A PowerShell-based compiler framework.'
    PowerShellVersion = '5.1'
    FunctionsToExport = @('New-CompilerContext', 'Get-CompilerContext', 'Set-CompilerContext', 'Invoke-Compiler', 'Get-CompilerVersion')
    CmdletsToExport = @()
    VariablesToExport = @()
    AliasesToExport = @()
    PrivateData = @{
        PSData = @{
            Tags = @('PowerShell', 'Compiler', 'Framework')
            LicenseUri = 'https://github.com/yourusername/CompilerFramework/blob/main/LICENSE'
            ProjectUri = 'https://github.com/yourusername/CompilerFramework'
            IconUri = 'https://github.com/yourusername/CompilerFramework/blob/main/icon.png'
            ReleaseNotes = 'Initial release.'
        }
    }
}
