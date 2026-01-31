# RawrXD Pattern Bridge - Generated Module
# Toolchain: PowerShell
# Generated: 2026-01-26T06:40:12.5498260-05:00
# Configuration: Optimized

$script:ToolchainBackend = 'PowerShell'

function Initialize-RawrXDPatternEngine {
    [CmdletBinding()]
    param()
    
    Write-Verbose "[RawrXD] Initializing pattern engine (Backend: $script:ToolchainBackend)"
    
    switch ($script:ToolchainBackend) {
        'PowerShell' {
            # Ensure C# types are loaded in this session
            if (-not ([System.Management.Automation.PSTypeName]'RawrXD.PatternBridge.PatternEngine').Type) {
                $sourcePath = Join-Path (Split-Path $PSScriptRoot -Parent) "src\RawrXD_PatternEngine.cs"
                if (-not (Test-Path $sourcePath)) {
                    throw "Pattern engine source not found: $sourcePath"
                }
                $code = Get-Content $sourcePath -Raw
                $compileParams = @{ TypeDefinition = $code; Language = 'CSharp'; WarningAction = 'SilentlyContinue' }
                Add-Type @compileParams
            }
            $null = [RawrXD.PatternBridge.PatternEngine]
        }
        default {
            # Load native DLL
            $dllPath = Join-Path $PSScriptRoot 'RawrXD_PatternEngine.dll'
            if (Test-Path $dllPath) {
                Add-Type -Path $dllPath
            } else {
                throw "Backend DLL not found: $dllPath"
            }
        }
    }
    
    Write-Verbose "[RawrXD] Pattern engine ready"
    return $true
}

function Invoke-RawrXDClassification {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Code,
        
        [Parameter(Mandatory=$false)]
        [string]$Context = ""
    )
    
    if ($script:ToolchainBackend -eq 'PowerShell') {
        $confidence = 0.0
        $type = [RawrXD.PatternBridge.PatternEngine]::ClassifyPattern($Code, $Context, [ref]$confidence)
        
        return [PSCustomObject]@{
            Type = [int]$type
            TypeName = $type.ToString()
            Confidence = $confidence
            IsPattern = ($type -in @(1, 3))  # Template or Learned
            RequiresManualReview = ($type -eq 2)  # NonPattern
            Backend = $script:ToolchainBackend
        }
    } else {
        # Call native DLL function
        throw "Native backend invocation not yet implemented"
    }
}

function Get-RawrXDPatternStats {
    [CmdletBinding()]
    param()
    
    if ($script:ToolchainBackend -eq 'PowerShell') {
        $stats = [RawrXD.PatternBridge.PatternEngine]::GetStats()
        return [PSCustomObject]@{
            TotalClassifications = $stats.TotalClassifications
            TemplateMatches = $stats.TemplateMatches
            NonPatternMatches = $stats.NonPatternMatches
            LearnedMatches = $stats.LearnedMatches
            AvgConfidence = $stats.AvgConfidence
            Backend = $script:ToolchainBackend
        }
    } else {
        throw "Native backend stats not yet implemented"
    }
}

function Get-RawrXDToolchainInfo {
    [CmdletBinding()]
    param()
    
    return [PSCustomObject]@{
        Backend = $script:ToolchainBackend
        Configuration = 'Optimized'
        ModulePath = $PSScriptRoot
        BuildDate = '2026-01-26T06:40:12.5506428-05:00'
    }
}

# Auto-initialize
try {
    Initialize-RawrXDPatternEngine | Out-Null
} catch {
    Write-Warning "[RawrXD] Initialization failed: $_"
}

Export-ModuleMember -Function @(
    'Initialize-RawrXDPatternEngine',
    'Invoke-RawrXDClassification',
    'Get-RawrXDPatternStats',
    'Get-RawrXDToolchainInfo'
)
