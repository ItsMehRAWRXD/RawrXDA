#Requires -Version 7.4

<#
.SYNOPSIS
    Directory Scanning and Task Generation Module.
.DESCRIPTION
    Recursively scans a directory, identifies source files, and generates tasks for analysis and improvement.
#>

function Scan-Directory {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path
    )

    if (-Not (Test-Path $Path)) {
        throw "The specified path '$Path' does not exist."
    }

    Write-Host "Scanning directory: $Path" -ForegroundColor Cyan

    $files = Get-ChildItem -Path $Path -Recurse -File |
        Select-Object FullName, Name, Extension, Length, LastWriteTime

    Write-Host "Found $($files.Count) files." -ForegroundColor Green

    return $files
}

function Generate-Tasks {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [array]$Files
    )

    $tasks = @()

    foreach ($file in $Files) {
        Write-Host "Analyzing file: $($file.Name)" -ForegroundColor Yellow

        switch ($file.Extension) {
            '.ps1' {
                $tasks += [PSCustomObject]@{
                    FileName = $file.Name
                    Task     = "Check for syntax errors and best practices."
                }
            }
            '.psm1' {
                $tasks += [PSCustomObject]@{
                    FileName = $file.Name
                    Task     = "Verify module exports and dependencies."
                }
            }
            '.json' {
                $tasks += [PSCustomObject]@{
                    FileName = $file.Name
                    Task     = "Validate JSON structure."
                }
            }
            default {
                $tasks += [PSCustomObject]@{
                    FileName = $file.Name
                    Task     = "No specific task defined for this file type."
                }
            }
        }
    }

    Write-Host "Generated $($tasks.Count) tasks." -ForegroundColor Green

    return $tasks
}

# Example usage
if ($MyInvocation.InvocationName -ne '.') {
    $directory = Read-Host "Enter the directory to scan"
    $files = Scan-Directory -Path $directory
    $tasks = Generate-Tasks -Files $files

    $reportPath = Join-Path $directory "TaskReport_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
    $tasks | ConvertTo-Json -Depth 10 | Set-Content -Path $reportPath -Encoding UTF8

    Write-Host "Task report saved to: $reportPath" -ForegroundColor Cyan
}