# RawrXD Full Brace Error Module
# Provides analysis utilities for detecting curly brace balance issues across PowerShell sources.

#Requires -Version 5.1

<#+
.SYNOPSIS
    Detects and reports unbalanced curly braces in PowerShell source files.

.DESCRIPTION
    Scans PowerShell scripts and modules for mismatched opening/closing braces.
    Generates structured reports suitable for integration with existing observability tooling.

.NOTES
    Author: RawrXD Auto-Generation System
    Version: 1.0.0
    Created: 2026-01-24
#>

if (-not (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue)) {
    function Write-StructuredLog {
        param(
            [Parameter(Mandatory=$true)][string]$Message,
            [ValidateSet('DEBUG','INFO','WARN','ERROR')][string]$Level = 'INFO',
            [string]$Function = $null,
            [hashtable]$Data = $null
        )

        $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff'
        $caller = if ($Function) { $Function } else { (Get-PSCallStack)[1].FunctionName }
        $payload = if ($Data) { ($Data.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join '; ' } else { '' }
        if ($payload) {
            Write-Host "[$timestamp][$caller][$Level] $Message :: $payload"
        } else {
            Write-Host "[$timestamp][$caller][$Level] $Message"
        }
    }
}

$script:BraceErrorConfig = @{
    SupportedExtensions = @('.ps1', '.psm1', '.psd1', '.ps1xml')
    EnableVerboseLogging = $false
}

function New-BraceViolation {
    param(
        [string]$Path,
        [int]$Line,
        [int]$Column,
        [string]$Type,
        [int]$Depth
    )

    return [PSCustomObject]@{
        Path = $Path
        Line = $Line
        Column = $Column
        Type = $Type
        Depth = $Depth
    }
}

function Test-BraceBalance {
    <#
    .SYNOPSIS
        Evaluates brace balance for provided content.

    .PARAMETER Content
        Raw text content to analyze.

    .PARAMETER Path
        Logical path identifier for reporting.

    .OUTPUTS
        PSCustomObject summarizing brace balance metrics and violations.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$Content,
        [Parameter(Mandatory=$true)][string]$Path
    )

    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $openCount = 0
    $closeCount = 0
    $depth = 0
    $maxDepth = 0
    $violations = New-Object System.Collections.Generic.List[object]

    $lines = $Content -split "`r?`n"
    for ($lineIndex = 0; $lineIndex -lt $lines.Length; $lineIndex++) {
        $line = $lines[$lineIndex]
        for ($columnIndex = 0; $columnIndex -lt $line.Length; $columnIndex++) {
            $char = $line[$columnIndex]
            if ($char -eq '{') {
                $openCount++
                $depth++
                if ($depth -gt $maxDepth) { $maxDepth = $depth }
            } elseif ($char -eq '}') {
                $closeCount++
                $depth--
                if ($depth -lt 0) {
                    $violations.Add((New-BraceViolation -Path $Path -Line ($lineIndex + 1) -Column ($columnIndex + 1) -Type 'UnexpectedClosing' -Depth $depth)) | Out-Null
                    $depth = 0
                }
            }
        }
    }

    if ($depth -gt 0) {
        $violations.Add((New-BraceViolation -Path $Path -Line $lines.Length -Column ($lines[$lines.Length-1].Length + 1) -Type 'UnclosedOpening' -Depth $depth)) | Out-Null
    }

    $stopwatch.Stop()
    $result = [PSCustomObject]@{
        Path = $Path
        Balanced = ($violations.Count -eq 0 -and $openCount -eq $closeCount)
        OpeningCount = $openCount
        ClosingCount = $closeCount
        NetBalance = $openCount - $closeCount
        MaxDepth = $maxDepth
        Violations = $violations.ToArray()
        DurationMs = [Math]::Round($stopwatch.Elapsed.TotalMilliseconds, 3)
    }

    return $result
}

function Get-BraceTargetFiles {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string[]]$Path,
        [switch]$Recurse
    )

    $files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    foreach ($entry in $Path) {
        if (-not $entry) { continue }
        if (-not (Test-Path $entry)) {
            Write-StructuredLog -Level 'WARN' -Message 'Path not found' -Function 'Get-BraceTargetFiles' -Data @{ Path = $entry }
            continue
        }

        $item = Get-Item -LiteralPath $entry
        if ($item.PSIsContainer) {
            $childItems = Get-ChildItem -LiteralPath $item.FullName -File -ErrorAction SilentlyContinue -Recurse:$Recurse.IsPresent
            foreach ($child in $childItems) {
                if ($script:BraceErrorConfig.SupportedExtensions -contains $child.Extension) {
                    $files.Add($child) | Out-Null
                }
            }
        } else {
            if ($script:BraceErrorConfig.SupportedExtensions -contains $item.Extension) {
                $files.Add($item) | Out-Null
            } else {
                Write-StructuredLog -Level 'DEBUG' -Message 'Skipping unsupported file' -Function 'Get-BraceTargetFiles' -Data @{ Path = $item.FullName }
            }
        }
    }

    return $files
}

function Invoke-FullBraceErrorCheck {
    <#
    .SYNOPSIS
        Scans files and reports brace balance violations.

    .PARAMETER Path
        File or directory paths to inspect.

    .PARAMETER Recurse
        Include nested directories when provided directories are scanned.

    .PARAMETER OutputPath
        Optional path to persist JSON results.

    .PARAMETER FailOnViolation
        Throw terminating error when any violation is detected.

    .OUTPUTS
        PSCustomObject describing the scan summary.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string[]]$Path,
        [switch]$Recurse,
        [string]$OutputPath,
        [switch]$FailOnViolation
    )

    $functionName = 'Invoke-FullBraceErrorCheck'
    $startTime = Get-Date
    $scanTimer = [System.Diagnostics.Stopwatch]::StartNew()

    Write-StructuredLog -Level 'INFO' -Function $functionName -Message 'Starting brace validation scan' -Data @{
        PathCount = $Path.Count
        Recurse = $Recurse.IsPresent
        OutputPath = $OutputPath
    }

    $targetFiles = Get-BraceTargetFiles -Path $Path -Recurse:$Recurse
    Write-StructuredLog -Level 'INFO' -Function $functionName -Message 'Resolved target files' -Data @{ FileCount = $targetFiles.Count }

    $results = New-Object System.Collections.Generic.List[object]
    $filesWithViolations = 0
    $totalViolations = 0

    foreach ($file in $targetFiles) {
        try {
            $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction Stop
            $analysis = Test-BraceBalance -Content $content -Path $file.FullName
            if ($analysis.Violations.Count -gt 0) {
                $filesWithViolations++
                $totalViolations += $analysis.Violations.Count
                if ($script:BraceErrorConfig.EnableVerboseLogging) {
                    Write-StructuredLog -Level 'WARN' -Function $functionName -Message 'Brace violations detected' -Data @{
                        Path = $file.FullName
                        ViolationCount = $analysis.Violations.Count
                    }
                }
            }
            $results.Add($analysis) | Out-Null
        } catch {
            Write-StructuredLog -Level 'ERROR' -Function $functionName -Message 'Failed to analyze file' -Data @{ Path = $file.FullName; Error = $_.Exception.Message }
        }
    }

    $scanTimer.Stop()
    $endTime = Get-Date
    $summary = [PSCustomObject]@{
        StartTime = $startTime
        EndTime = $endTime
        DurationMs = [Math]::Round($scanTimer.Elapsed.TotalMilliseconds, 3)
        FilesScanned = $targetFiles.Count
        FilesWithViolations = $filesWithViolations
        TotalViolations = $totalViolations
        Results = $results.ToArray()
    }

    Write-StructuredLog -Level 'INFO' -Function $functionName -Message 'Brace validation completed' -Data @{
        DurationMs = $summary.DurationMs
        FilesScanned = $summary.FilesScanned
        FilesWithViolations = $summary.FilesWithViolations
        TotalViolations = $summary.TotalViolations
    }

    if ($OutputPath) {
        try {
            $directory = Split-Path -Parent $OutputPath
            if ($directory -and -not (Test-Path $directory)) {
                New-Item -Path $directory -ItemType Directory -Force | Out-Null
            }
            $summary | ConvertTo-Json -Depth 6 | Out-File -FilePath $OutputPath -Encoding UTF8
            Write-StructuredLog -Level 'INFO' -Function $functionName -Message 'Persisted brace report' -Data @{ OutputPath = $OutputPath }
        } catch {
            Write-StructuredLog -Level 'ERROR' -Function $functionName -Message 'Failed to persist brace report' -Data @{ OutputPath = $OutputPath; Error = $_.Exception.Message }
        }
    }

    if ($FailOnViolation.IsPresent -and $totalViolations -gt 0) {
        throw "Brace validation failed: $totalViolations violation(s) detected across $filesWithViolations file(s)."
    }

    return $summary
}

Export-ModuleMember -Function Test-BraceBalance, Invoke-FullBraceErrorCheck
