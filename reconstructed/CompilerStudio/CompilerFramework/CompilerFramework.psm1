# CompilerFramework.psm1

# Define the module's public functions
Export-ModuleMember -Function `
    New-CompilerContext, `
    Get-CompilerContext, `
    Set-CompilerContext, `
    Invoke-Compiler, `
    Get-CompilerVersion

# Import the private modules
Import-Module "$PSScriptRoot\Private\CompilerContext.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerLexer.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerParser.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerIRGenerator.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerOptimizer.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerCodeGenerator.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerErrorHandling.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerUtilities.ps1" -Force
Import-Module "$PSScriptRoot\Private\CompilerLogging.ps1" -Force

# Initialize the compiler context
$global:CompilerContext = New-CompilerContext

# Define the compiler version
$global:CompilerVersion = "0.1.0"

# Function to get the compiler version
function Get-CompilerVersion {
    return $global:CompilerVersion
}
