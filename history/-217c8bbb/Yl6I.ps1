#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Reverse Engineering Manifest Generator
.DESCRIPTION
    Comprehensive source code analyzer that generates a detailed manifest
    of all components, features, and implementation status.
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath,
    
    [Parameter(Mandatory=$false)]
    [string]$OutputPath = "RawrXD_Manifest.json",
    
    [Parameter(Mandatory=$false)]
    [switch]$Detailed
)

# Manifest structure
$global:Manifest = @{
    Metadata = @{}
    Functions = @{}
    GUIComponents = @{}
    AgenticSystems = @{}
    Integrations = @{}
    Configuration = @{}
    ErrorHandling = @{}
    Testing = @{}
    ProductionReadiness = @{}
    MissingComponents = @{}
    Recommendations = @()
}

function Write-AuditLog {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    Write-Host "[$timestamp] [$Level] $Message"
}

function Get-SourceStatistics {
    param($content)
    
    Write-AuditLog "Analyzing source code statistics..."
    
    $stats = @{
        TotalLines = $content.Count
        TotalFunctions = ($content | Select-String -Pattern '^function\s+\w+').Count
        TotalClasses = ($content | Select-String -Pattern '^class\s+\w+').Count
        TotalComments = ($content | Select-String -Pattern '^#').Count
        TotalTrapBlocks = ($content | Select-String -Pattern '^trap').Count
        TotalTryCatch = ($content | Select-String -Pattern 'try\s*\{').Count
    }
    
    Write-AuditLog "Found $($stats.TotalLines) lines, $($stats.TotalFunctions) functions, $($stats.TotalClasses) classes"
    return $stats
}

function Extract-AllFunctions {
    param($content)
    
    Write-AuditLog "Extracting function definitions..."
    
    $functions = @{}
    $functionPattern = '^function\s+(\w+)\s*\{'
    
    for ($i = 0; $i -lt $content.Count; $i++) {
        $line = $content[$i]
        if ($line -match $functionPattern) {
            $funcName = $matches[1]
            $startLine = $i + 1
            
            # Find function end (matching braces)
            $braceCount = 0
            $inFunction = $false
            $endLine = $startLine
            
            for ($j = $i; $j -lt $content.Count; $j++) {
                $funcLine = $content[$j]
                
                if ($funcLine -match $functionPattern) {
                    $inFunction = $true
                }
                
                if ($inFunction) {
                    $braceCount += ([regex]::Matches($funcLine, '\{').Count)
                    $braceCount -= ([regex]::Matches($funcLine, '\}').Count)
                    
                    if ($braceCount -eq 0 -and $j -gt $i) {
                        $endLine = $j + 1
                        break
                    }
                }
            }
            
            $funcContent = $content[$i..($endLine-1)]
            $functions[$funcName] = @{
                Name = $funcName
                StartLine = $startLine
                EndLine = $endLine
                LineCount = $endLine - $startLine + 1
                Content = $funcContent
                Parameters = Extract-FunctionParameters $funcContent
                IsStub = Test-IsStubFunction $funcContent
                Complexity = Calculate-Complexity $funcContent
                ErrorHandling = Test-ErrorHandling $funcContent
                Documentation = Get-FunctionDocs $funcContent
            }
        }
    }
    
    Write-AuditLog "Extracted $($functions.Count) functions"
    return $functions
}

function Extract-FunctionParameters {
    param($funcContent)
    
    $params = @{}
    $paramPattern = 'param\s*\(([^)]+)\)'
    
    $funcText = $funcContent -join "`n"
    if ($funcText -match $paramPattern) {
        $paramBlock = $matches[1]
        # Extract individual parameters
        $paramMatches = [regex]::Matches($paramBlock, '\[(\w+)\]\s*\$(\w+)')
        foreach ($match in $paramMatches) {
            $params[$match.Groups[2].Value] = $match.Groups[1].Value
        }
    }
    
    return $params
}

function Test-IsStubFunction {
    param($funcContent)
    
    $funcText = ($funcContent -join "`n").ToLower()
    
    # Check for stub indicators
    $stubIndicators = @(
        'not implemented',
        'throw "not implemented"',
        'write-host "stub"',
        '# todo',
        '# fixme',
        'return $null',
        'return $false',
        'return 0'
    )
    
    foreach ($indicator in $stubIndicators) {
        if ($funcText -match $indicator) {
            return $true
        }
    }
    
    # Check if function is too short (potential stub)
    if ($funcContent.Count -lt 5) {
        return $true
    }
    
    return $false
}

function Calculate-Complexity {
    param($funcContent)
    
    $text = $funcContent -join "`n"
    $complexity = 0
    
    # Count control structures
    $complexity += ([regex]::Matches($text, '\bif\b').Count * 2)
    $complexity += ([regex]::Matches($text, '\belse\b').Count * 1)
    $complexity += ([regex]::Matches($text, '\belseif\b').Count * 2)
    $complexity += ([regex]::Matches($text, '\bforeach\b').Count * 3)
    $complexity += ([regex]::Matches($text, '\bfor\b').Count * 3)
    $complexity += ([regex]::Matches($text, '\bwhile\b').Count * 3)
    $complexity += ([regex]::Matches($text, '\bswitch\b').Count * 2)
    $complexity += ([regex]::Matches($text, '\btry\b').Count * 2)
    $complexity += ([regex]::Matches($text, '\bcatch\b').Count * 2)
    
    return $complexity
}

function Test-ErrorHandling {
    param($funcContent)
    
    $text = $funcContent -join "`n"
    $errorHandling = @{
        HasTryCatch = $text -match '\btry\b.*\bcatch\b'
        HasTrap = $text -match '\btrap\b'
        HasErrorAction = $text -match '\$ErrorActionPreference'
        HasErrorVariable = $text -match '-ErrorAction|-ErrorVariable'
    }
    
    return $errorHandling
}

function Get-FunctionDocs {
    param($funcContent)
    
    # Look for comment-based help
    $docs = @{
        Synopsis = ""
        Description = ""
        Parameters = @{}
        Examples = @()
    }
    
    # Extract synopsis from comments above function
    for ($i = 0; $i -lt [Math]::Min(10, $funcContent.Count); $i++) {
        $line = $funcContent[$i]
        if ($line -match '^#\s*\.SYNOPSIS\s*(.+)$') {
            $docs.Synopsis = $matches[1].Trim()
        }
        elseif ($line -match '^#\s*\.DESCRIPTION\s*(.+)$') {
            $docs.Description += $matches[1].Trim() + " "
 }
    }
    
    return $docs
}

function Analyze-GUIComponents {
    param($content)
    
    Write-AuditLog "Analyzing GUI components..."
    
    $gui = @{
        MainWindow = @{
            Implemented = $false
            Features = @{}
        }
        FileExplorer = @{
            Implemented = $false
            Features = @{}
        }
        ChatPanel = @{
            Implemented = $false
            Features = @{}
        }
        WebBrowser = @{
            Implemented = $false
            Features = @{}
        }
        Terminal = @{
            Implemented = $false
            Features = @{}
        }
        StatusBar = @{
            Implemented = $false
            Features = @{}
        }
    }
    
    # Check for WPF/XAML usage
    $hasWPF = $content | Select-String -Pattern 'Add-Type.*PresentationFramework|Windows.Markup.XamlReader'
    $gui.MainWindow.Implemented = $hasWPF.Count -gt 0
    
    # Check for specific controls
    $gui.FileExplorer.Implemented = ($content | Select-String -Pattern 'TreeView.*FileExplorer|Get-ChildItem.*TreeView').Count -gt 0
    $gui.ChatPanel.Implemented = ($content | Select-String -Pattern 'RichTextBox.*Chat|WebView2.*Chat').Count -gt 0
    $gui.WebBrowser.Implemented = ($content | Select-String -Pattern 'WebView2|InternetExplorer|WebBrowser').Count -gt 0
    $gui.Terminal.Implemented = ($content | Select-String -Pattern 'PowerShell.*Terminal|Process.*cmd.exe').Count -gt 0
    $gui.StatusBar.Implemented = ($content | Select-String -Pattern 'StatusBar|StatusText').Count -gt 0
    
    return $gui
}

function Analyze-AgenticSystems {
    param($content, $functions)
    
    Write-AuditLog "Analyzing agentic systems..."
    
    $agentic = @{
        TaskAutomation = @{
            Implemented = $false
            Features = @{}
        }
        ErrorRecovery = @{
            Implemented = $false
            Features = @{}
        }
        Logging = @{
            Implemented = $false
            Features = @{}
        }
        SessionManagement = @{
            Implemented = $false
            Features = @{}
        }
    }
    
    # Check for agent-related functions
    $agentFunctions = $functions.Keys | Where-Object { $_ -match 'Agent|Task|Automation' }
    $agentic.TaskAutomation.Implemented = $agentFunctions.Count -gt 0
    
    # Check for error recovery
    $agentic.ErrorRecovery.Implemented = ($functions.Keys | Where-Object { $_ -match 'Error|Recovery|Handle' }).Count -gt 0
    
    # Check for logging system
    $agentic.Logging.Implemented = ($functions.Keys | Where-Object { $_ -match 'Log|Write|Register' }).Count -gt 0
    
    # Check for session management
    $agentic.SessionManagement.Implemented = ($content | Select-String -Pattern '\$script:CurrentSession').Count -gt 0
    
    return $agentic
}

function Analyze-Integrations {
    param($content)
    
    Write-AuditLog "Analyzing external integrations..."
    
    $integrations = @{
        Ollama = @{
            Implemented = $false
            Features = @{}
        }
        Git = @{
            Implemented = $false
            Features = @{}
        }
        WebView2 = @{
            Implemented = $false
            Features = @{}
        }
        Registry = @{
            Implemented = $false
            Features = @{}
        }
        FileSystem = @{
            Implemented = $false
            Features = @{}
        }
    }
    
    $integrations.Ollama.Implemented = ($content | Select-String -Pattern 'Invoke-RestMethod.*ollama|http://localhost:11434').Count -gt 0
    $integrations.Git.Implemented = ($content | Select-String -Pattern 'git\s+commit|git\s+push|git\s+status').Count -gt 0
    $integrations.WebView2.Implemented = ($content | Select-String -Pattern 'WebView2|Add-Type.*WebView2').Count -gt 0
    $integrations.Registry.Implemented = ($content | Select-String -Pattern 'Get-ItemProperty|Set-ItemProperty|HKCU:|HKLM:').Count -gt 0
    $integrations.FileSystem.Implemented = $true # Always implemented
    
    return $integrations
}

function Identify-MissingComponents {
    param($functions, $gui, $agentic, $integrations)
    
    Write-AuditLog "Identifying missing components..."
    
    $missing = @{
        Critical = @()
        High = @()
        Medium = @()
        Low = @()
    }
    
    # Check for critical GUI components
    if (-not $gui.MainWindow.Implemented) {
        $missing.Critical += "MainWindow WPF implementation"
    }
    if (-not $gui.FileExplorer.Implemented) {
        $missing.Critical += "FileExplorer TreeView control"
    }
    if (-not $gui.ChatPanel.Implemented) {
        $missing.Critical += "ChatPanel RichTextBox/WebView2"
    }
    
    # Check for critical agentic systems
    if (-not $agentic.TaskAutomation.Implemented) {
        $missing.Critical += "Task automation engine"
    }
    if (-not $agentic.ErrorRecovery.Implemented) {
        $missing.High += "Error recovery system"
    }
    
    # Check for critical integrations
    if (-not $integrations.Ollama.Implemented) {
        $missing.Critical += "Ollama AI integration"
    }
    if (-not $integrations.Git.Implemented) {
        $missing.Medium += "Git version control"
    }
    
    # Check for stub functions
    $stubFunctions = $functions.Values | Where-Object { $_.IsStub }
    if ($stubFunctions.Count -gt 0) {
        $missing.High += "$($stubFunctions.Count) stub functions need implementation"
    }
    
    return $missing
}

function Generate-Recommendations {
    param($manifest)
    
    $recommendations = @()
    
    # Production readiness recommendations
    if ($manifest.MissingComponents.Critical.Count -gt 0) {
        $recommendations += "PRIORITY 1: Implement all critical missing components: $($manifest.MissingComponents.Critical -join ', ')"
    }
    
    if ($manifest.Functions.Values | Where-Object { $_.Complexity -gt 20 }) {
        $recommendations += "PRIORITY 2: Refactor high-complexity functions (>20) to reduce maintenance burden"
    }
    
    $functionsWithoutDocs = $manifest.Functions.Values | Where-Object { $_.Documentation.Synopsis -eq "" }
    if ($functionsWithoutDocs.Count -gt 0) {
        $recommendations += "PRIORITY 3: Add documentation for $($functionsWithoutDocs.Count) undocumented functions"
    }
    
    $functionsWithoutErrorHandling = $manifest.Functions.Values | Where-Object { -not $_.ErrorHandling.HasTryCatch -and -not $_.ErrorHandling.HasTrap }
    if ($functionsWithoutErrorHandling.Count -gt 0) {
        $recommendations += "PRIORITY 4: Add error handling for $($functionsWithoutErrorHandling.Count) functions"
    }
    
    $recommendations += "PRIORITY 5: Implement comprehensive test suite for all functions"
    $recommendations += "PRIORITY 6: Add performance monitoring and telemetry"
    $recommendations += "PRIORITY 7: Create deployment automation and CI/CD pipeline"
    
    return $recommendations
}

function Export-Manifest {
    param($manifest, $outputPath)
    
    Write-AuditLog "Exporting manifest to $outputPath..."
    
    # Convert to JSON with proper formatting
    $json = $manifest | ConvertTo-Json -Depth 10
    
    # Save to file
    $json | Out-File -FilePath $outputPath -Encoding UTF8
    
    Write-AuditLog "Manifest exported successfully"
}

function Show-SummaryReport {
    param($manifest)
    
    Write-Host "`n" -ForegroundColor Cyan
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "        RawrXD REVERSE ENGINEERING AUDIT REPORT" -ForegroundColor Cyan
    Write-Host "══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    
    Write-Host "`n📊 SOURCE STATISTICS:" -ForegroundColor Yellow
    Write-Host "   Total Lines: $($manifest.Metadata.Statistics.TotalLines)" -ForegroundColor White
    Write-Host "   Total Functions: $($manifest.Metadata.Statistics.TotalFunctions)" -ForegroundColor White
    Write-Host "   Total Classes: $($manifest.Metadata.Statistics.TotalClasses)" -ForegroundColor White
    Write-Host "   Total Comments: $($manifest.Metadata.Statistics.TotalComments)" -ForegroundColor White
    
    Write-Host "`n🔧 FUNCTION ANALYSIS:" -ForegroundColor Yellow
    $stubCount = ($manifest.Functions.Values | Where-Object { $_.IsStub }).Count
    $complexCount = ($manifest.Functions.Values | Where-Object { $_.Complexity -gt 15 }).Count
    Write-Host "   Stub Functions: $stubCount" -ForegroundColor Red
    Write-Host "   High Complexity: $complexCount" -ForegroundColor Yellow
    $wellDocumented = ($manifest.Functions.Values | Where-Object { $_.Documentation.Synopsis -ne '' }).Count
    Write-Host "   Well Documented: $wellDocumented" -ForegroundColor Green
    
    Write-Host "`n🖥️  GUI COMPONENTS:" -ForegroundColor Yellow
    foreach ($component in $manifest.GUIComponents.Keys) {
        $status = $manifest.GUIComponents[$component].Implemented
        $color = if ($status) { "Green" } else { "Red" }
        Write-Host "   $component`: $(if($status){'✓ Implemented'}else{'✗ Missing'})" -ForegroundColor $color
    }
    
    Write-Host "`n🤖 AGENTIC SYSTEMS:" -ForegroundColor Yellow
    foreach ($system in $manifest.AgenticSystems.Keys) {
        $status = $manifest.AgenticSystems[$system].Implemented
        $color = if ($status) { "Green" } else { "Red" }
        Write-Host "   $system`: $(if($status){'✓ Implemented'}else{'✗ Missing'})" -ForegroundColor $color
    }
    
    Write-Host "`n🔗 INTEGRATIONS:" -ForegroundColor Yellow
    foreach ($integration in $manifest.Integrations.Keys) {
        $status = $manifest.Integrations[$integration].Implemented
        $color = if ($status) { "Green" } else { "Red" }
        Write-Host "   $integration`: $(if($status){'✓ Implemented'}else{'✗ Missing'})" -ForegroundColor $color
    }
    
    Write-Host "`n⚠️  MISSING COMPONENTS:" -ForegroundColor Red
    foreach ($priority in @("Critical", "High", "Medium", "Low")) {
        $items = $manifest.MissingComponents[$priority]
        if ($items.Count -gt 0) {
            Write-Host "   $priority Priority: $($items.Count) items" -ForegroundColor $(if($priority -eq "Critical"){"Red"}elseif($priority -eq "High"){"Yellow"}else{"Gray"})
            if ($Detailed) {
                foreach ($item in $items) {
                    Write-Host "     • $item" -ForegroundColor Gray
                }
            }
        }
    }
    
    Write-Host "`n📋 TOP RECOMMENDATIONS:" -ForegroundColor Cyan
    for ($i = 0; $i -lt [Math]::Min(5, $manifest.Recommendations.Count); $i++) {
        Write-Host "   $($i+1). $($manifest.Recommendations[$i])" -ForegroundColor White
    }
    
    Write-Host "`n══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
}

# Main execution
Write-Host "RawrXD Reverse Engineering Manifest Generator" -ForegroundColor Cyan
Write-Host "Analyzing: $SourcePath" -ForegroundColor Gray

# Read source file
if (-not (Test-Path $SourcePath)) {
    Write-Error "Source file not found: $SourcePath"
    exit 1
}

$content = Get-Content $SourcePath

# Build manifest
$manifest.Metadata = @{
    SourceFile = $SourcePath
    AnalysisDate = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    ToolVersion = "1.0"
    Statistics = Get-SourceStatistics $content
}

$manifest.Functions = Extract-AllFunctions $content
$manifest.GUIComponents = Analyze-GUIComponents $content
$manifest.AgenticSystems = Analyze-AgenticSystems $content $manifest.Functions
$manifest.Integrations = Analyze-Integrations $content
$manifest.MissingComponents = Identify-MissingComponents $manifest.Functions $manifest.GUIComponents $manifest.AgenticSystems $manifest.Integrations
$manifest.Recommendations = Generate-Recommendations $manifest

# Export and display
Export-Manifest $manifest $OutputPath
Show-SummaryReport $manifest

Write-Host "`n✅ Manifest generation complete!" -ForegroundColor Green
Write-Host "📄 Output saved to: $OutputPath" -ForegroundColor Gray
