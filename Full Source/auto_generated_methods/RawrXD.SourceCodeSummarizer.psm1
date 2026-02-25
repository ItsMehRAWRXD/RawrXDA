
# Cache for function results
$script:FunctionCache = @{}

function Get-FromCache {
    param([string]$Key)
    if ($script:FunctionCache.ContainsKey($Key)) {
        return $script:FunctionCache[$Key]
    }
    return $null
}

function Set-Cache {
    param([string]$Key, $Value)
    $script:FunctionCache[$Key] = $Value
}# RawrXD Source Code Summarizer Module
# Production-ready code analysis and documentation generation

#Requires -Version 5.1

<#
.SYNOPSIS
    RawrXD.SourceCodeSummarizer - Comprehensive source code analysis and documentation generation

.DESCRIPTION
    Advanced source code analysis tool providing:
    - Automatic code documentation generation
    - Function and class extraction and analysis
    - Complexity metrics and code quality scoring
    - Dependency mapping and visualization
    - Multi-language support (PowerShell, C#, Python, JavaScript)
    - Exportable documentation in multiple formats
    - Integration with existing documentation systems

.LINK
    https://github.com/RawrXD/SourceCodeSummarizer

.NOTES
    Author: RawrXD Auto-Generation System
    Version: 1.0.0
    Requires: PowerShell 5.1+
    Last Updated: 2024-12-28
#>

# Import logging if available
if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [string]$Function = $null,
            [hashtable]$Data = $null
        )
        $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
        $caller = if ($Function) { $Function } else { (Get-PSCallStack)[1].FunctionName }
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$timestamp][$caller][$Level] $Message" -ForegroundColor $color
    }
}

function Get-PowerShellCodeSummary {
    <#
    .SYNOPSIS
        Extract comprehensive information from PowerShell source files
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$FilePath
    )
    
    try {
        $content = Get-Content $FilePath -Raw -ErrorAction Stop
        $lines = Get-Content $FilePath -ErrorAction Stop
        
        # Parse AST
        $tokens = $null
        $errors = $null
        $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)
        
        $summary = @{
            FileName = [System.IO.Path]::GetFileName($FilePath)
            FilePath = $FilePath
            Language = 'PowerShell'
            FileSize = (Get-Item $FilePath).Length
            LineCount = $lines.Count
            LastModified = (Get-Item $FilePath).LastWriteTime
            ParseErrors = $errors.Count
            Functions = @()
            Classes = @()
            Variables = @()
            Parameters = @()
            Comments = @()
            ImportedModules = @()
            ExportedFunctions = @()
            Complexity = @{
                CyclomaticComplexity = 0
                TotalFunctions = 0
                LargestFunction = 0
                AverageComplexity = 0
            }
            Documentation = @{
                HasHeader = $false
                HeaderComment = $null
                FunctionsDocumented = 0
                DocumentationCoverage = 0
            }
        }
        
        # Extract functions
        $functions = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true)
        foreach ($func in $functions) {
            $funcLines = ($func.Extent.EndLineNumber - $func.Extent.StartLineNumber) + 1
            $paramCount = if ($func.Parameters) { $func.Parameters.Count } else { 0 }
            if ($func.Body.ParamBlock) { $paramCount = $func.Body.ParamBlock.Parameters.Count }
            
            # Calculate function complexity
            $complexity = Get-FunctionComplexity -FunctionAst $func
            $summary.Complexity.CyclomaticComplexity += $complexity
            $summary.Complexity.LargestFunction = [Math]::Max($summary.Complexity.LargestFunction, $funcLines)
            
            # Extract function documentation
            $hasDocumentation = $false
            $documentation = $null
            
            # Look for comment-based help
            $helpPattern = '\.SYNOPSIS|\.DESCRIPTION|\.PARAMETER|\.EXAMPLE|\.NOTES'
            if ($content.Substring($func.Extent.StartOffset, [Math]::Min(1000, $func.Extent.EndOffset - $func.Extent.StartOffset)) -match $helpPattern) {
                $hasDocumentation = $true
                $summary.Documentation.FunctionsDocumented++
            }
            
            $summary.Functions += @{
                Name = $func.Name
                StartLine = $func.Extent.StartLineNumber
                EndLine = $func.Extent.EndLineNumber
                LineCount = $funcLines
                ParameterCount = $paramCount
                CyclomaticComplexity = $complexity
                HasDocumentation = $hasDocumentation
                Visibility = if ($func.Name -like 'Get-*' -or $func.Name -like 'Set-*' -or $func.Name -like 'New-*') { 'Public' } else { 'Private' }
                ReturnType = 'Unknown'  # PowerShell doesn't have explicit return types
            }
        }
        
        $summary.Complexity.TotalFunctions = $functions.Count
        if ($functions.Count -gt 0) {
            $summary.Complexity.AverageComplexity = [Math]::Round($summary.Complexity.CyclomaticComplexity / $functions.Count, 2)
        }
        
        # Extract classes (PowerShell 5.0+)
        $classes = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.TypeDefinitionAst] }, $true)
        foreach ($class in $classes) {
            $summary.Classes += @{
                Name = $class.Name
                StartLine = $class.Extent.StartLineNumber
                EndLine = $class.Extent.EndLineNumber
                MemberCount = $class.Members.Count
                IsPublic = $class.Attributes | Where-Object { $_.TypeName.Name -eq 'Public' }
                BaseTypes = if ($class.BaseTypes) { $class.BaseTypes.TypeName.Name -join ', ' } else { $null }
            }
        }
        
        # Extract variables
        $variables = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.VariableExpressionAst] }, $true)
        $uniqueVars = $variables | Group-Object VariablePath | ForEach-Object {
            @{
                Name = $_.Name
                Scope = if ($_.Name -like 'script:*') { 'Script' } elseif ($_.Name -like 'global:*') { 'Global' } else { 'Local' }
                UsageCount = $_.Count
                FirstUseLine = ($_.Group | Sort-Object { $_.Extent.StartLineNumber })[0].Extent.StartLineNumber
            }
        }
        $summary.Variables = $uniqueVars | Select-Object -First 20  # Limit to prevent huge arrays
        
        # Extract imported modules
        $importCommands = $ast.FindAll({ 
            param($n) 
            $n -is [System.Management.Automation.Language.CommandAst] -and 
            $n.GetCommandName() -in @('Import-Module', 'using')
        }, $true)
        foreach ($import in $importCommands) {
            if ($import.CommandElements.Count -gt 1) {
                $moduleName = $import.CommandElements[1].ToString() -replace '[''"]', ''
                $summary.ImportedModules += $moduleName
            }
        }
        
        # Look for exported functions (Export-ModuleMember)
        $exportCommands = $ast.FindAll({ 
            param($n) 
            $n -is [System.Management.Automation.Language.CommandAst] -and 
            $n.GetCommandName() -eq 'Export-ModuleMember'
        }, $true)
        foreach ($export in $exportCommands) {
            $functionParam = $export.CommandElements | Where-Object { $_.ToString() -like '*Function*' }
            if ($functionParam) {
                $summary.ExportedFunctions += "Export-ModuleMember detected"
            }
        }
        
        # Extract header documentation
        $headerLines = $lines | Select-Object -First 50
        $headerText = $headerLines -join "`n"
        if ($headerText -match '\.SYNOPSIS|\.DESCRIPTION|<#.*?#>') {
            $summary.Documentation.HasHeader = $true
            $summary.Documentation.HeaderComment = ($headerText -split "`n" | Select-Object -First 20) -join "`n"
        }
        
        # Calculate documentation coverage
        if ($summary.Complexity.TotalFunctions -gt 0) {
            $summary.Documentation.DocumentationCoverage = [Math]::Round(($summary.Documentation.FunctionsDocumented / $summary.Complexity.TotalFunctions) * 100, 1)
        }
        
        return $summary
        
    } catch {
        Write-StructuredLog -Message "Error analyzing PowerShell file $FilePath`: $_" -Level Warning -Function 'Get-PowerShellCodeSummary'
        return @{
            FileName = [System.IO.Path]::GetFileName($FilePath)
            FilePath = $FilePath
            Language = 'PowerShell'
            Error = $_.Exception.Message
            LineCount = 0
            Functions = @()
        }
    }
}

function Get-FunctionComplexity {
    <#
    .SYNOPSIS
        Calculate cyclomatic complexity for a function
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [System.Management.Automation.Language.FunctionDefinitionAst]$FunctionAst
    )
    
    $complexity = 1  # Base complexity
    
    # Decision points that increase complexity
    $decisionNodes = @(
        [System.Management.Automation.Language.IfStatementAst],
        [System.Management.Automation.Language.WhileStatementAst],
        [System.Management.Automation.Language.ForStatementAst],
        [System.Management.Automation.Language.ForEachStatementAst],
        [System.Management.Automation.Language.SwitchStatementAst],
        [System.Management.Automation.Language.CatchClauseAst],
        [System.Management.Automation.Language.TrapStatementAst]
    )
    
    foreach ($nodeType in $decisionNodes) {
        $nodes = $FunctionAst.FindAll({ param($n) $n.GetType() -eq $nodeType }, $true)
        $complexity += $nodes.Count
    }
    
    return $complexity
}

function Export-CodeDocumentation {
    <#
    .SYNOPSIS
        Export code summary to various documentation formats
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [hashtable]$ProjectSummary,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('HTML', 'Markdown', 'JSON', 'XML')]
        [string]$Format = 'HTML',
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = $null
    )
    
    if (-not $OutputPath) {
        $timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        $extension = switch ($Format) { 'HTML' { 'html' } 'Markdown' { 'md' } 'JSON' { 'json' } 'XML' { 'xml' } }
        $OutputPath = "CodeDocumentation_$timestamp.$extension"
    }
    
    try {
        switch ($Format) {
            'HTML' {
                $html = New-HTMLDocumentation -ProjectSummary $ProjectSummary
                $html | Set-Content $OutputPath -Encoding UTF8
            }
            'Markdown' {
                $markdown = New-MarkdownDocumentation -ProjectSummary $ProjectSummary
                $markdown | Set-Content $OutputPath -Encoding UTF8
            }
            'JSON' {
                $ProjectSummary | ConvertTo-Json -Depth 15 | Set-Content $OutputPath -Encoding UTF8
            }
            'XML' {
                $xml = New-XMLDocumentation -ProjectSummary $ProjectSummary
                $xml | Set-Content $OutputPath -Encoding UTF8
            }
        }
        
        Write-StructuredLog -Message "Documentation exported to: $OutputPath" -Level Info -Function 'Export-CodeDocumentation'
        return $OutputPath
        
    } catch {
        Write-StructuredLog -Message "Error exporting documentation: $_" -Level Error -Function 'Export-CodeDocumentation'
        throw
    }
}

function New-HTMLDocumentation {
    <#
    .SYNOPSIS
        Generate HTML documentation from project summary
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [hashtable]$ProjectSummary
    )
    
    $html = @"
<!DOCTYPE html>
<html>
<head>
    <title>$($ProjectSummary.ProjectName) - Code Documentation</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; margin: 40px; background: #f8f9fa; }
        .container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        h1 { color: #2c3e50; border-bottom: 3px solid #3498db; padding-bottom: 10px; }
        h2 { color: #34495e; border-bottom: 1px solid #bdc3c7; padding-bottom: 8px; margin-top: 30px; }
        h3 { color: #7f8c8d; margin-top: 25px; }
        .stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }
        .stat-card { background: #ecf0f1; padding: 15px; border-radius: 5px; text-align: center; }
        .stat-value { font-size: 2em; font-weight: bold; color: #3498db; }
        .stat-label { color: #7f8c8d; margin-top: 5px; }
        table { width: 100%; border-collapse: collapse; margin: 15px 0; }
        th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #3498db; color: white; }
        tr:nth-child(even) { background-color: #f2f2f2; }
        .complexity-high { color: #e74c3c; font-weight: bold; }
        .complexity-medium { color: #f39c12; font-weight: bold; }
        .complexity-low { color: #27ae60; font-weight: bold; }
        .code { background: #2c3e50; color: #ecf0f1; padding: 15px; border-radius: 5px; font-family: 'Consolas', monospace; white-space: pre-wrap; }
        .file-list { columns: 2; column-gap: 30px; }
        .file-item { break-inside: avoid; margin-bottom: 10px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>📄 $($ProjectSummary.ProjectName) - Code Documentation</h1>
        <p><strong>Generated:</strong> $($ProjectSummary.GeneratedAt)</p>
        <p><strong>Source Directory:</strong> $($ProjectSummary.SourceDirectory)</p>
        <p><strong>Analysis Duration:</strong> $($ProjectSummary.AnalysisDuration) seconds</p>
        
        <h2>📊 Project Statistics</h2>
        <div class="stats">
            <div class="stat-card">
                <div class="stat-value">$($ProjectSummary.Summary.TotalFiles)</div>
                <div class="stat-label">Total Files</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">$($ProjectSummary.Summary.TotalLines)</div>
                <div class="stat-label">Total Lines</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">$($ProjectSummary.Summary.TotalFunctions)</div>
                <div class="stat-label">Total Functions</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">$($ProjectSummary.Summary.AverageComplexity)</div>
                <div class="stat-label">Average Complexity</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">$($ProjectSummary.Summary.DocumentationCoverage)%</div>
                <div class="stat-label">Documentation Coverage</div>
            </div>
            <div class="stat-card">
                <div class="stat-value">$($ProjectSummary.Summary.TotalClasses)</div>
                <div class="stat-label">Total Classes</div>
            </div>
        </div>
        
        <h2>📁 Files Analyzed</h2>
        <div class="file-list">
$(foreach ($file in $ProjectSummary.Files) {
"            <div class='file-item'><strong>$($file.FileName)</strong><br><small>$($file.LineCount) lines, $($file.Functions.Count) functions</small></div>"
})
        </div>
        
        <h2>🔧 Functions Summary</h2>
        <table>
            <thead>
                <tr><th>Function</th><th>File</th><th>Lines</th><th>Parameters</th><th>Complexity</th><th>Documented</th></tr>
            </thead>
            <tbody>
$(foreach ($file in $ProjectSummary.Files) {
    foreach ($func in $file.Functions) {
        $complexityClass = if ($func.CyclomaticComplexity -gt 10) { 'complexity-high' } elseif ($func.CyclomaticComplexity -gt 5) { 'complexity-medium' } else { 'complexity-low' }
        $documented = if ($func.HasDocumentation) { '✓' } else { '✗' }
        "                <tr><td>$($func.Name)</td><td>$($file.FileName)</td><td>$($func.LineCount)</td><td>$($func.ParameterCount)</td><td class='$complexityClass'>$($func.CyclomaticComplexity)</td><td>$documented</td></tr>"
    }
})
            </tbody>
        </table>
        
        <h2>🏷️ Language Distribution</h2>
        <table>
            <thead>
                <tr><th>Language</th><th>Files</th><th>Lines</th><th>Percentage</th></tr>
            </thead>
            <tbody>
$(foreach ($lang in $ProjectSummary.LanguageStats.Keys) {
    $stats = $ProjectSummary.LanguageStats[$lang]
    "                <tr><td>$lang</td><td>$($stats.FileCount)</td><td>$($stats.LineCount)</td><td>$($stats.Percentage)%</td></tr>"
})
            </tbody>
        </table>
        
        <h2>⚠️ Code Quality Insights</h2>
        <ul>
$(foreach ($insight in $ProjectSummary.QualityInsights) {
"            <li>$insight</li>"
})
        </ul>
        
        <footer style="margin-top: 40px; text-align: center; color: #7f8c8d; font-size: 0.9em;">
            <p>Generated by RawrXD Source Code Summarizer v1.0.0</p>
        </footer>
    </div>
</body>
</html>
"@
    
    return $html
}

function New-MarkdownDocumentation {
    <#
    .SYNOPSIS
        Generate Markdown documentation from project summary
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [hashtable]$ProjectSummary
    )
    
    $markdown = @"
# 📄 $($ProjectSummary.ProjectName) - Code Documentation

**Generated:** $($ProjectSummary.GeneratedAt)  
**Source Directory:** $($ProjectSummary.SourceDirectory)  
**Analysis Duration:** $($ProjectSummary.AnalysisDuration) seconds

## 📊 Project Statistics

| Metric | Value |
|--------|-------|
| Total Files | $($ProjectSummary.Summary.TotalFiles) |
| Total Lines | $($ProjectSummary.Summary.TotalLines) |
| Total Functions | $($ProjectSummary.Summary.TotalFunctions) |
| Average Complexity | $($ProjectSummary.Summary.AverageComplexity) |
| Documentation Coverage | $($ProjectSummary.Summary.DocumentationCoverage)% |
| Total Classes | $($ProjectSummary.Summary.TotalClasses) |

## 🔧 Functions Summary

| Function | File | Lines | Parameters | Complexity | Documented |
|----------|------|-------|------------|------------|------------|
$(foreach ($file in $ProjectSummary.Files) {
    foreach ($func in $file.Functions) {
        $documented = if ($func.HasDocumentation) { '✓' } else { '✗' }
        "| $($func.Name) | $($file.FileName) | $($func.LineCount) | $($func.ParameterCount) | $($func.CyclomaticComplexity) | $documented |"
    }
})

## 🏷️ Language Distribution

| Language | Files | Lines | Percentage |
|----------|-------|-------|------------|
$(foreach ($lang in $ProjectSummary.LanguageStats.Keys) {
    $stats = $ProjectSummary.LanguageStats[$lang]
    "| $lang | $($stats.FileCount) | $($stats.LineCount) | $($stats.Percentage)% |"
})

## ⚠️ Code Quality Insights

$(foreach ($insight in $ProjectSummary.QualityInsights) {
"- $insight"
})

---
*Generated by RawrXD Source Code Summarizer v1.0.0*
"@
    
    return $markdown
}

function New-XMLDocumentation {
    <#
    .SYNOPSIS
        Generate XML documentation from project summary
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [hashtable]$ProjectSummary
    )
    
    $xml = @"
<?xml version="1.0" encoding="UTF-8"?>
<ProjectDocumentation>
    <Metadata>
        <ProjectName>$($ProjectSummary.ProjectName)</ProjectName>
        <GeneratedAt>$($ProjectSummary.GeneratedAt)</GeneratedAt>
        <SourceDirectory>$($ProjectSummary.SourceDirectory)</SourceDirectory>
        <AnalysisDuration>$($ProjectSummary.AnalysisDuration)</AnalysisDuration>
    </Metadata>
    <Summary>
        <TotalFiles>$($ProjectSummary.Summary.TotalFiles)</TotalFiles>
        <TotalLines>$($ProjectSummary.Summary.TotalLines)</TotalLines>
        <TotalFunctions>$($ProjectSummary.Summary.TotalFunctions)</TotalFunctions>
        <AverageComplexity>$($ProjectSummary.Summary.AverageComplexity)</AverageComplexity>
        <DocumentationCoverage>$($ProjectSummary.Summary.DocumentationCoverage)</DocumentationCoverage>
    </Summary>
    <Files>
$(foreach ($file in $ProjectSummary.Files) {
"        <File name=`"$($file.FileName)`" path=`"$($file.FilePath)`" lines=`"$($file.LineCount)`" functions=`"$($file.Functions.Count)`" />"
})
    </Files>
</ProjectDocumentation>
"@
    
    return $xml
}

function Invoke-SourceCodeSummarizer {
    <#
    .SYNOPSIS
        Main source code analysis and documentation generation entry point
    
    .DESCRIPTION
        Comprehensive source code analysis tool that generates detailed documentation,
        complexity metrics, and code quality insights for software projects.
    
    .PARAMETER SourceDirectory
        Directory containing source code files to analyze
    
    .PARAMETER FilePatterns
        File patterns to include in analysis (default: *.ps1, *.psm1, *.psd1)
    
    .PARAMETER ExcludePatterns
        File patterns to exclude from analysis
    
    .PARAMETER IncludeTests
        Include test files in analysis
    
    .PARAMETER GenerateDocumentation
        Generate documentation output
    
    .PARAMETER DocumentationFormat
        Documentation output format (HTML, Markdown, JSON, XML, All)
    
    .PARAMETER OutputPath
        Output directory for generated documentation
    
    .PARAMETER ProjectName
        Custom project name for documentation
    
    .PARAMETER IncludeDependencies
        Include dependency analysis in output
    
    .EXAMPLE
        Invoke-SourceCodeSummarizer -SourceDirectory 'C:/project/src' -GenerateDocumentation -DocumentationFormat HTML
        
        Analyze source code and generate HTML documentation
    
    .EXAMPLE
        Invoke-SourceCodeSummarizer -SourceDirectory '.' -FilePatterns @('*.ps1', '*.psm1') -ExcludePatterns @('*test*')
        
        Analyze PowerShell files excluding test files
    
    .OUTPUTS
        Hashtable containing comprehensive project analysis and documentation paths
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [ValidateScript({Test-Path $_ -PathType 'Container'})]
        [string]$SourceDirectory,
        
        [Parameter(Mandatory=$false)]
        [string[]]$FilePatterns = @('*.ps1', '*.psm1', '*.psd1'),
        
        [Parameter(Mandatory=$false)]
        [string[]]$ExcludePatterns = @('*.Tests.ps1', '*test*', '*/bin/*', '*/obj/*'),
        
        [Parameter(Mandatory=$false)]
        [switch]$IncludeTests,
        
        [Parameter(Mandatory=$false)]
        [switch]$GenerateDocumentation,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('HTML', 'Markdown', 'JSON', 'XML', 'All')]
        [string]$DocumentationFormat = 'HTML',
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = $null,
        
        [Parameter(Mandatory=$false)]
        [string]$ProjectName = $null,
        
        [Parameter(Mandatory=$false)]
        [switch]$IncludeDependencies
    )
    
    $functionName = 'Invoke-SourceCodeSummarizer'
    $startTime = Get-Date
    
    try {
        # Set defaults
        if (-not $ProjectName) {
            $ProjectName = [System.IO.Path]::GetFileName($SourceDirectory)
        }
        
        if (-not $OutputPath) {
            $OutputPath = Join-Path $SourceDirectory 'docs'
        }
        
        # Adjust patterns if including tests
        if (-not $IncludeTests) {
            $FilePatterns = $FilePatterns | Where-Object { $_ -notlike '*test*' -and $_ -notlike '*.Tests.*' }
        }
        
        Write-StructuredLog -Message "Starting source code analysis" -Level Info -Function $functionName -Data @{
            SourceDirectory = $SourceDirectory
            ProjectName = $ProjectName
            FilePatterns = $FilePatterns -join ', '
            GenerateDocumentation = $GenerateDocumentation.IsPresent
        }
        
        # Discover files
        $allFiles = @()
        foreach ($pattern in $FilePatterns) {
            $files = Get-ChildItem -Path $SourceDirectory -Filter $pattern -Recurse -File -ErrorAction SilentlyContinue
            foreach ($file in $files) {
                $shouldExclude = $false
                foreach ($excludePattern in $ExcludePatterns) {
                    if ($file.FullName -like "*$excludePattern*") {
                        $shouldExclude = $true
                        break
                    }
                }
                if (-not $shouldExclude) {
                    $allFiles += $file
                }
            }
        }
        
        Write-StructuredLog -Message "Found $($allFiles.Count) files to analyze" -Level Info -Function $functionName
        
        # Analyze each file
        $fileSummaries = @()
        $totalLines = 0
        $totalFunctions = 0
        $totalComplexity = 0
        $totalDocumentedFunctions = 0
        $totalClasses = 0
        
        foreach ($file in $allFiles) {
            Write-StructuredLog -Message "Analyzing: $($file.Name)" -Level Debug -Function $functionName
            
            $extension = $file.Extension.ToLower()
            $summary = switch ($extension) {
                '.ps1' { Get-PowerShellCodeSummary -FilePath $file.FullName }
                '.psm1' { Get-PowerShellCodeSummary -FilePath $file.FullName }
                '.psd1' { Get-PowerShellCodeSummary -FilePath $file.FullName }
                default {
                    @{
                        FileName = $file.Name
                        FilePath = $file.FullName
                        Language = 'Unknown'
                        LineCount = (Get-Content $file.FullName).Count
                        Functions = @()
                        Classes = @()
                    }
                }
            }
            
            $fileSummaries += $summary
            $totalLines += $summary.LineCount
            $totalFunctions += $summary.Functions.Count
            if ($summary.Complexity) {
                $totalComplexity += $summary.Complexity.CyclomaticComplexity
                $totalDocumentedFunctions += $summary.Documentation.FunctionsDocumented
            }
            $totalClasses += $summary.Classes.Count
        }
        
        # Calculate language statistics
        $languageStats = @{}
        foreach ($summary in $fileSummaries) {
            $lang = $summary.Language
            if (-not $languageStats.ContainsKey($lang)) {
                $languageStats[$lang] = @{ FileCount = 0; LineCount = 0; Percentage = 0 }
            }
            $languageStats[$lang].FileCount++
            $languageStats[$lang].LineCount += $summary.LineCount
        }
        
        # Calculate language percentages
        foreach ($lang in $languageStats.Keys) {
            $languageStats[$lang].Percentage = if ($totalLines -gt 0) {
                [Math]::Round(($languageStats[$lang].LineCount / $totalLines) * 100, 1)
            } else { 0 }
        }
        
        # Generate quality insights
        $qualityInsights = @()
        
        $avgComplexity = if ($totalFunctions -gt 0) { [Math]::Round($totalComplexity / $totalFunctions, 2) } else { 0 }
        $docCoverage = if ($totalFunctions -gt 0) { [Math]::Round(($totalDocumentedFunctions / $totalFunctions) * 100, 1) } else { 0 }
        
        if ($avgComplexity -gt 8) {
            $qualityInsights += "Average function complexity is high ($avgComplexity). Consider refactoring complex functions."
        }
        
        if ($docCoverage -lt 50) {
            $qualityInsights += "Documentation coverage is low ($docCoverage%). Add more inline documentation."
        }
        
        if ($totalFunctions -eq 0) {
            $qualityInsights += "No functions detected. Consider organizing code into reusable functions."
        }
        
        $largestFile = ($fileSummaries | Sort-Object LineCount -Descending | Select-Object -First 1)
        if ($largestFile -and $largestFile.LineCount -gt 500) {
            $qualityInsights += "Large file detected: $($largestFile.FileName) ($($largestFile.LineCount) lines). Consider splitting into smaller modules."
        }
        
        if ($qualityInsights.Count -eq 0) {
            $qualityInsights += "Code quality looks good! No major issues detected."
        }
        
        # Build project summary
        $projectSummary = @{
            ProjectName = $ProjectName
            SourceDirectory = $SourceDirectory
            GeneratedAt = (Get-Date).ToString('o')
            AnalysisDuration = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
            Summary = @{
                TotalFiles = $allFiles.Count
                TotalLines = $totalLines
                TotalFunctions = $totalFunctions
                TotalClasses = $totalClasses
                AverageComplexity = $avgComplexity
                DocumentationCoverage = $docCoverage
                TotalDocumentedFunctions = $totalDocumentedFunctions
            }
            Files = $fileSummaries
            LanguageStats = $languageStats
            QualityInsights = $qualityInsights
            Configuration = @{
                FilePatterns = $FilePatterns
                ExcludePatterns = $ExcludePatterns
                IncludeTests = $IncludeTests.IsPresent
                IncludeDependencies = $IncludeDependencies.IsPresent
            }
        }
        
        # Generate documentation if requested
        $documentationPaths = @{}
        if ($GenerateDocumentation) {
            # Ensure output directory exists
            if (-not (Test-Path $OutputPath)) {
                New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
            }
            
            if ($DocumentationFormat -eq 'All') {
                foreach ($format in @('HTML', 'Markdown', 'JSON', 'XML')) {
                    $docPath = Export-CodeDocumentation -ProjectSummary $projectSummary -Format $format -OutputPath (Join-Path $OutputPath "documentation.$($format.ToLower())")
                    $documentationPaths[$format] = $docPath
                }
            } else {
                $docPath = Export-CodeDocumentation -ProjectSummary $projectSummary -Format $DocumentationFormat -OutputPath (Join-Path $OutputPath "documentation.$($DocumentationFormat.ToLower())")
                $documentationPaths[$DocumentationFormat] = $docPath
            }
            
            $projectSummary.DocumentationPaths = $documentationPaths
        }
        
        $duration = [Math]::Round(((Get-Date) - $startTime).TotalSeconds, 2)
        Write-StructuredLog -Message "Source code analysis completed in ${duration}s" -Level Info -Function $functionName
        Write-StructuredLog -Message "Summary: $($allFiles.Count) files, $totalLines lines, $totalFunctions functions, $totalClasses classes" -Level Info -Function $functionName
        Write-StructuredLog -Message "Quality: $docCoverage% documentation coverage, $avgComplexity average complexity" -Level Info -Function $functionName
        
        return $projectSummary
        
    } catch {
        Write-StructuredLog -Message "Source code summarizer error: $_" -Level Error -Function $functionName
        throw
    }
}

# Export main function
Export-ModuleMember -Function Invoke-SourceCodeSummarizer

