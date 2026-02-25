<#
.SYNOPSIS
    Production SourceCodeSummarizer - Comprehensive code documentation and analysis engine

.DESCRIPTION
    AST-based source code analyzer that extracts comprehensive documentation including
    function signatures, parameter details, dependencies, complexity metrics, inline
    documentation, and generates multiple output formats (Markdown, HTML, JSON).

.PARAMETER SourceDir
    Directory containing source files to summarize

.PARAMETER FilePattern
    File pattern to match (default: *.ps1, *.psm1)

.PARAMETER IncludePrivate
    Include private/internal functions in summary

.PARAMETER OutputFormat
    Output format: JSON, Markdown, HTML, or All

.PARAMETER GroupByModule
    Group functions by module/file

.EXAMPLE
    Invoke-SourceCodeSummarizer -SourceDir 'D:/project' -OutputFormat 'All' -GroupByModule
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('Info','Warning','Error','Debug')][string]$Level = 'Info',
            [hashtable]$Data = $null
        )
        $color = switch ($Level) { 'Error' { 'Red' } 'Warning' { 'Yellow' } 'Debug' { 'DarkGray' } default { 'Cyan' } }
        Write-Host "[$Level] $Message" -ForegroundColor $color
    }
}

function Get-FunctionDocumentation {
    <#
    .SYNOPSIS
        Extract comprehensive documentation from a function AST
    #>
    param(
        [System.Management.Automation.Language.FunctionDefinitionAst]$FunctionAst,
        [string]$SourceContent
    )

    $doc = @{
        Name = $FunctionAst.Name
        StartLine = $FunctionAst.Extent.StartLineNumber
        EndLine = $FunctionAst.Extent.EndLineNumber
        LineCount = ($FunctionAst.Extent.EndLineNumber - $FunctionAst.Extent.StartLineNumber) + 1
        IsAdvancedFunction = $false
        IsPublic = -not ($FunctionAst.Name -match '^_|^private|^internal')
        Parameters = @()
        OutputType = @()
        Synopsis = $null
        Description = $null
        Examples = @()
        Notes = $null
        Links = @()
        Attributes = @()
    }

    # Check for CmdletBinding (advanced function)
    $paramBlock = $FunctionAst.Body.ParamBlock
    if ($paramBlock) {
        $doc.IsAdvancedFunction = $paramBlock.Attributes | Where-Object {
            $_.TypeName.Name -eq 'CmdletBinding'
        }

        # Extract output type attributes
        $outputTypes = $paramBlock.Attributes | Where-Object { $_.TypeName.Name -eq 'OutputType' }
        foreach ($ot in $outputTypes) {
            $doc.OutputType += $ot.PositionalArguments | ForEach-Object { $_.Extent.Text }
        }
    }

    # Extract parameters
    $params = if ($FunctionAst.Parameters) { $FunctionAst.Parameters } elseif ($paramBlock) { $paramBlock.Parameters } else { @() }

    foreach ($param in $params) {
        $paramInfo = @{
            Name = $param.Name.VariablePath.UserPath
            Type = if ($param.StaticType) { $param.StaticType.Name } else { 'Object' }
            Mandatory = $false
            Position = $null
            DefaultValue = if ($param.DefaultValue) { $param.DefaultValue.Extent.Text } else { $null }
            ValidateSet = @()
            HelpMessage = $null
            Aliases = @()
            ParameterSets = @()
        }

        foreach ($attr in $param.Attributes) {
            switch ($attr.TypeName.Name) {
                'Parameter' {
                    foreach ($na in $attr.NamedArguments) {
                        switch ($na.ArgumentName) {
                            'Mandatory' { $paramInfo.Mandatory = $na.Argument.Extent.Text -eq '$true' }
                            'Position' { $paramInfo.Position = [int]$na.Argument.Extent.Text }
                            'HelpMessage' { $paramInfo.HelpMessage = $na.Argument.Value }
                            'ParameterSetName' { $paramInfo.ParameterSets += $na.Argument.Value }
                        }
                    }
                }
                'ValidateSet' {
                    $paramInfo.ValidateSet = $attr.PositionalArguments | ForEach-Object { $_.Value }
                }
                'Alias' {
                    $paramInfo.Aliases = $attr.PositionalArguments | ForEach-Object { $_.Value }
                }
            }
        }

        $doc.Parameters += $paramInfo
    }

    # Extract comment-based help
    $functionText = $FunctionAst.Extent.Text
    $helpMatch = [regex]::Match($functionText, '<#[\s\S]*?#>', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)

    if ($helpMatch.Success) {
        $helpText = $helpMatch.Value

        # Synopsis
        $synopsisMatch = [regex]::Match($helpText, '\.SYNOPSIS\s*\r?\n\s*(.+?)(?=\r?\n\s*\.|\r?\n\s*#>)', [System.Text.RegularExpressions.RegexOptions]::Singleline)
        if ($synopsisMatch.Success) {
            $doc.Synopsis = $synopsisMatch.Groups[1].Value.Trim()
        }

        # Description
        $descMatch = [regex]::Match($helpText, '\.DESCRIPTION\s*\r?\n\s*([\s\S]+?)(?=\r?\n\s*\.|\r?\n\s*#>)', [System.Text.RegularExpressions.RegexOptions]::Singleline)
        if ($descMatch.Success) {
            $doc.Description = $descMatch.Groups[1].Value.Trim()
        }

        # Examples
        $exampleMatches = [regex]::Matches($helpText, '\.EXAMPLE\s*\r?\n\s*([\s\S]+?)(?=\r?\n\s*\.|\r?\n\s*#>)', [System.Text.RegularExpressions.RegexOptions]::Singleline)
        foreach ($ex in $exampleMatches) {
            $doc.Examples += $ex.Groups[1].Value.Trim()
        }

        # Notes
        $notesMatch = [regex]::Match($helpText, '\.NOTES\s*\r?\n\s*([\s\S]+?)(?=\r?\n\s*\.|\r?\n\s*#>)', [System.Text.RegularExpressions.RegexOptions]::Singleline)
        if ($notesMatch.Success) {
            $doc.Notes = $notesMatch.Groups[1].Value.Trim()
        }

        # Links
        $linkMatches = [regex]::Matches($helpText, '\.LINK\s*\r?\n\s*(.+?)(?=\r?\n)', [System.Text.RegularExpressions.RegexOptions]::Singleline)
        foreach ($link in $linkMatches) {
            $doc.Links += $link.Groups[1].Value.Trim()
        }
    }

    return $doc
}

function Get-FileComplexityMetrics {
    <#
    .SYNOPSIS
        Calculate file-level complexity metrics
    #>
    param(
        [System.Management.Automation.Language.ScriptBlockAst]$Ast,
        [string]$Content
    )

    $metrics = @{
        TotalLines = ($Content -split "`n").Count
        CodeLines = 0
        CommentLines = 0
        BlankLines = 0
        FunctionCount = 0
        ClassCount = 0
        MaxNestingDepth = 0
        CyclomaticComplexity = 0
    }

    $lines = $Content -split "`n"
    foreach ($line in $lines) {
        $trimmed = $line.Trim()
        if ($trimmed -eq '') {
            $metrics.BlankLines++
        } elseif ($trimmed.StartsWith('#') -or $trimmed.StartsWith('<#')) {
            $metrics.CommentLines++
        } else {
            $metrics.CodeLines++
        }
    }

    # Count functions
    $functions = $Ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true)
    $metrics.FunctionCount = $functions.Count

    # Count classes
    $classes = $Ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.TypeDefinitionAst] }, $true)
    $metrics.ClassCount = $classes.Count

    # Calculate cyclomatic complexity (simplified)
    $decisionPoints = $Ast.FindAll({
        param($n)
        $n -is [System.Management.Automation.Language.IfStatementAst] -or
        $n -is [System.Management.Automation.Language.SwitchStatementAst] -or
        $n -is [System.Management.Automation.Language.ForStatementAst] -or
        $n -is [System.Management.Automation.Language.ForEachStatementAst] -or
        $n -is [System.Management.Automation.Language.WhileStatementAst] -or
        $n -is [System.Management.Automation.Language.CatchClauseAst]
    }, $true)
    $metrics.CyclomaticComplexity = $decisionPoints.Count + 1

    # Calculate max nesting depth
    function Get-NestingDepth {
        param($Node, $CurrentDepth = 0)
        $maxDepth = $CurrentDepth

        $nestingNodes = @(
            [System.Management.Automation.Language.IfStatementAst]
            [System.Management.Automation.Language.ForStatementAst]
            [System.Management.Automation.Language.ForEachStatementAst]
            [System.Management.Automation.Language.WhileStatementAst]
            [System.Management.Automation.Language.SwitchStatementAst]
            [System.Management.Automation.Language.TryStatementAst]
        )

        if ($Node -is [System.Management.Automation.Language.Ast]) {
            $isNestingNode = $nestingNodes | Where-Object { $Node -is $_ }
            if ($isNestingNode) { $CurrentDepth++ }

            foreach ($child in $Node.PSObject.Properties.Value) {
                if ($child -is [System.Management.Automation.Language.Ast]) {
                    $childDepth = Get-NestingDepth -Node $child -CurrentDepth $CurrentDepth
                    $maxDepth = [Math]::Max($maxDepth, $childDepth)
                } elseif ($child -is [System.Collections.IEnumerable] -and $child -isnot [string]) {
                    foreach ($item in $child) {
                        if ($item -is [System.Management.Automation.Language.Ast]) {
                            $childDepth = Get-NestingDepth -Node $item -CurrentDepth $CurrentDepth
                            $maxDepth = [Math]::Max($maxDepth, $childDepth)
                        }
                    }
                }
            }
        }

        return $maxDepth
    }

    $metrics.MaxNestingDepth = Get-NestingDepth -Node $Ast

    return $metrics
}

function Export-DocumentationMarkdown {
    <#
    .SYNOPSIS
        Export documentation as Markdown
    #>
    param(
        [PSCustomObject]$Report,
        [string]$OutputPath
    )

    $md = @"
# Source Code Documentation

Generated: $($Report.Timestamp)
Source Directory: $($Report.SourceDirectory)

## Summary

| Metric | Value |
|--------|-------|
| Total Files | $($Report.Summary.TotalFiles) |
| Total Functions | $($Report.Summary.TotalFunctions) |
| Public Functions | $($Report.Summary.PublicFunctions) |
| Total Lines | $($Report.Summary.TotalLines) |
| Code Lines | $($Report.Summary.CodeLines) |
| Average Complexity | $($Report.Summary.AverageComplexity) |

---

"@

    foreach ($file in $Report.Files) {
        $md += @"

## $($file.FileName)

**Path:** ``$($file.FullPath)``
**Lines:** $($file.Metrics.TotalLines) | **Functions:** $($file.Metrics.FunctionCount) | **Complexity:** $($file.Metrics.CyclomaticComplexity)

"@

        foreach ($func in $file.Functions) {
            $md += @"

### $($func.Name)

$(if ($func.Synopsis) { "> $($func.Synopsis)`n" })
$(if ($func.Description) { "$($func.Description)`n" })

**Lines:** $($func.StartLine)-$($func.EndLine) ($($func.LineCount) lines)
$(if ($func.IsAdvancedFunction) { "**Advanced Function:** Yes`n" })

"@

            if ($func.Parameters.Count -gt 0) {
                $md += @"

#### Parameters

| Name | Type | Mandatory | Default | Description |
|------|------|-----------|---------|-------------|
"@
                foreach ($param in $func.Parameters) {
                    $mandatory = if ($param.Mandatory) { '✓' } else { '' }
                    $default = if ($param.DefaultValue) { "``$($param.DefaultValue)``" } else { '-' }
                    $desc = if ($param.HelpMessage) { $param.HelpMessage } else { '-' }
                    $md += "| $($param.Name) | $($param.Type) | $mandatory | $default | $desc |`n"
                }
            }

            if ($func.Examples.Count -gt 0) {
                $md += "`n#### Examples`n`n"
                foreach ($ex in $func.Examples) {
                    $md += "``````powershell`n$ex`n```````n`n"
                }
            }
        }
    }

    $md | Set-Content $OutputPath -Encoding UTF8
}

function Export-DocumentationHTML {
    <#
    .SYNOPSIS
        Export documentation as HTML
    #>
    param(
        [PSCustomObject]$Report,
        [string]$OutputPath
    )

    $html = @"
<!DOCTYPE html>
<html>
<head>
    <title>RawrXD Source Documentation</title>
    <style>
        body { font-family: 'Segoe UI', sans-serif; max-width: 1200px; margin: 0 auto; padding: 20px; background: #f5f5f5; }
        .header { background: linear-gradient(135deg, #667eea, #764ba2); color: white; padding: 30px; border-radius: 10px; margin-bottom: 20px; }
        .summary { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 15px; margin-bottom: 30px; }
        .stat { background: white; padding: 20px; border-radius: 8px; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .stat-value { font-size: 2em; font-weight: bold; color: #667eea; }
        .stat-label { color: #666; font-size: 0.9em; }
        .file { background: white; margin-bottom: 20px; border-radius: 8px; overflow: hidden; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .file-header { background: #667eea; color: white; padding: 15px 20px; }
        .file-content { padding: 20px; }
        .function { border-left: 4px solid #667eea; padding-left: 20px; margin-bottom: 20px; }
        .function h3 { color: #333; margin-bottom: 10px; }
        .function-meta { color: #666; font-size: 0.9em; margin-bottom: 10px; }
        .params table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        .params th, .params td { padding: 8px; text-align: left; border-bottom: 1px solid #eee; }
        .params th { background: #f8f9fa; }
        code { background: #f4f4f4; padding: 2px 6px; border-radius: 3px; }
        pre { background: #2d2d2d; color: #f8f8f2; padding: 15px; border-radius: 5px; overflow-x: auto; }
        .synopsis { background: #e8f4fd; border-left: 4px solid #667eea; padding: 10px 15px; margin-bottom: 15px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>📚 RawrXD Source Documentation</h1>
        <p>Generated: $($Report.Timestamp)</p>
    </div>

    <div class="summary">
        <div class="stat"><div class="stat-value">$($Report.Summary.TotalFiles)</div><div class="stat-label">Files</div></div>
        <div class="stat"><div class="stat-value">$($Report.Summary.TotalFunctions)</div><div class="stat-label">Functions</div></div>
        <div class="stat"><div class="stat-value">$($Report.Summary.PublicFunctions)</div><div class="stat-label">Public APIs</div></div>
        <div class="stat"><div class="stat-value">$($Report.Summary.TotalLines)</div><div class="stat-label">Total Lines</div></div>
        <div class="stat"><div class="stat-value">$($Report.Summary.AverageComplexity)</div><div class="stat-label">Avg Complexity</div></div>
    </div>

$(foreach ($file in $Report.Files) {
@"
    <div class="file">
        <div class="file-header">
            <h2>$($file.FileName)</h2>
            <div>$($file.Metrics.TotalLines) lines | $($file.Metrics.FunctionCount) functions | Complexity: $($file.Metrics.CyclomaticComplexity)</div>
        </div>
        <div class="file-content">
$(foreach ($func in $file.Functions) {
@"
            <div class="function">
                <h3>$($func.Name)</h3>
                $(if ($func.Synopsis) { "<div class='synopsis'>$($func.Synopsis)</div>" })
                <div class="function-meta">
                    Lines $($func.StartLine)-$($func.EndLine) ($($func.LineCount) lines)
                    $(if ($func.IsAdvancedFunction) { '| <strong>Advanced Function</strong>' })
                </div>
                $(if ($func.Description) { "<p>$($func.Description)</p>" })
                $(if ($func.Parameters.Count -gt 0) {
                    "<div class='params'><h4>Parameters</h4><table><tr><th>Name</th><th>Type</th><th>Mandatory</th><th>Default</th></tr>" +
                    ($func.Parameters | ForEach-Object { "<tr><td><code>$($_.Name)</code></td><td>$($_.Type)</td><td>$(if ($_.Mandatory) { '✓' } else { '' })</td><td>$(if ($_.DefaultValue) { "<code>$($_.DefaultValue)</code>" } else { '-' })</td></tr>" }) +
                    "</table></div>"
                })
            </div>
"@
})
        </div>
    </div>
"@
})
</body>
</html>
"@

    $html | Set-Content $OutputPath -Encoding UTF8
}

function Invoke-SourceCodeSummarizer {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateScript({Test-Path $_ -PathType 'Container'})]
        [string]$SourceDir = "D:/lazy init ide",

        [string[]]$FilePatterns = @('*.ps1', '*.psm1'),

        [switch]$IncludePrivate,

        [ValidateSet('JSON','Markdown','HTML','All')]
        [string]$OutputFormat = 'All',

        [switch]$GroupByModule,

        [string]$OutputPath = "D:/lazy init ide/auto_generated_methods/SourceSummaries.json"
    )

    $functionName = 'Invoke-SourceCodeSummarizer'
    $startTime = Get-Date

    try {
        Write-StructuredLog -Message "Starting comprehensive source code analysis for $SourceDir" -Level Info

        # Discover files
        $allFiles = @()
        foreach ($pattern in $FilePatterns) {
            $allFiles += Get-ChildItem -Path $SourceDir -Filter $pattern -Recurse -ErrorAction SilentlyContinue
        }
        $allFiles = $allFiles | Select-Object -Unique

        Write-StructuredLog -Message "Analyzing $($allFiles.Count) source files" -Level Info

        $fileSummaries = @()
        $totalFunctions = 0
        $publicFunctions = 0
        $totalLines = 0
        $totalCodeLines = 0
        $totalComplexity = 0

        foreach ($file in $allFiles) {
            Write-StructuredLog -Message "Processing: $($file.Name)" -Level Debug

            try {
                $content = Get-Content $file.FullName -Raw -ErrorAction Stop
                $tokens = $null
                $errors = $null
                $ast = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$tokens, [ref]$errors)

                # Get file metrics
                $fileMetrics = Get-FileComplexityMetrics -Ast $ast -Content $content
                $totalLines += $fileMetrics.TotalLines
                $totalCodeLines += $fileMetrics.CodeLines
                $totalComplexity += $fileMetrics.CyclomaticComplexity

                # Extract function documentation
                $functions = $ast.FindAll({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] }, $true)
                $functionDocs = @()

                foreach ($func in $functions) {
                    $funcDoc = Get-FunctionDocumentation -FunctionAst $func -SourceContent $content

                    # Filter private functions if not including them
                    if (-not $IncludePrivate -and -not $funcDoc.IsPublic) {
                        continue
                    }

                    $functionDocs += $funcDoc
                    $totalFunctions++
                    if ($funcDoc.IsPublic) { $publicFunctions++ }
                }

                # Extract module-level documentation
                $moduleDoc = $null
                $moduleHelpMatch = [regex]::Match($content, '^<#[\s\S]*?#>', [System.Text.RegularExpressions.RegexOptions]::Multiline)
                if ($moduleHelpMatch.Success) {
                    $helpText = $moduleHelpMatch.Value
                    $synopsisMatch = [regex]::Match($helpText, '\.SYNOPSIS\s*\r?\n\s*(.+?)(?=\r?\n\s*\.|\r?\n\s*#>)')
                    $descMatch = [regex]::Match($helpText, '\.DESCRIPTION\s*\r?\n\s*([\s\S]+?)(?=\r?\n\s*\.|\r?\n\s*#>)')
                    $moduleDoc = @{
                        Synopsis = if ($synopsisMatch.Success) { $synopsisMatch.Groups[1].Value.Trim() } else { $null }
                        Description = if ($descMatch.Success) { $descMatch.Groups[1].Value.Trim() } else { $null }
                    }
                }

                $fileSummary = @{
                    FileName = $file.Name
                    FullPath = $file.FullName
                    RelativePath = $file.FullName.Replace($SourceDir, '').TrimStart('\', '/')
                    Metrics = $fileMetrics
                    ModuleDocumentation = $moduleDoc
                    Functions = $functionDocs
                    ParseErrors = $errors | ForEach-Object { @{ Message = $_.Message; Line = $_.Extent.StartLineNumber } }
                }

                $fileSummaries += $fileSummary

            } catch {
                Write-StructuredLog -Message "Error processing $($file.Name): $_" -Level Warning
            }
        }

        # Calculate averages
        $avgComplexity = if ($allFiles.Count -gt 0) { [Math]::Round($totalComplexity / $allFiles.Count, 2) } else { 0 }

        # Build comprehensive report
        $report = [PSCustomObject]@{
            Timestamp = (Get-Date).ToString('o')
            SourceDirectory = $SourceDir
            AnalysisDuration = ((Get-Date) - $startTime).TotalSeconds
            Configuration = @{
                FilePatterns = $FilePatterns
                IncludePrivate = $IncludePrivate.IsPresent
                OutputFormat = $OutputFormat
            }
            Summary = @{
                TotalFiles = $allFiles.Count
                TotalFunctions = $totalFunctions
                PublicFunctions = $publicFunctions
                PrivateFunctions = $totalFunctions - $publicFunctions
                TotalLines = $totalLines
                CodeLines = $totalCodeLines
                TotalComplexity = $totalComplexity
                AverageComplexity = $avgComplexity
            }
            Files = $fileSummaries
        }

        # Ensure output directory exists
        $outputDir = Split-Path $OutputPath -Parent
        if (-not (Test-Path $outputDir)) {
            New-Item -ItemType Directory -Path $outputDir -Force | Out-Null
        }

        # Export in requested formats
        if ($OutputFormat -in @('JSON', 'All')) {
            $report | ConvertTo-Json -Depth 20 | Set-Content $OutputPath -Encoding UTF8
            Write-StructuredLog -Message "JSON documentation saved to $OutputPath" -Level Info
        }

        if ($OutputFormat -in @('Markdown', 'All')) {
            $mdPath = $OutputPath -replace '\.json$', '.md'
            Export-DocumentationMarkdown -Report $report -OutputPath $mdPath
            Write-StructuredLog -Message "Markdown documentation saved to $mdPath" -Level Info
        }

        if ($OutputFormat -in @('HTML', 'All')) {
            $htmlPath = $OutputPath -replace '\.json$', '.html'
            Export-DocumentationHTML -Report $report -OutputPath $htmlPath
            Write-StructuredLog -Message "HTML documentation saved to $htmlPath" -Level Info
        }

        $duration = ((Get-Date) - $startTime).TotalSeconds
        Write-StructuredLog -Message "Source code analysis complete in $([Math]::Round($duration, 2))s" -Level Info
        Write-StructuredLog -Message "Summary: $($allFiles.Count) files, $totalFunctions functions ($publicFunctions public), $totalLines total lines" -Level Info

        return $report

    } catch {
        Write-StructuredLog -Message "SourceCodeSummarizer error: $_" -Level Error
        throw
    }
}

