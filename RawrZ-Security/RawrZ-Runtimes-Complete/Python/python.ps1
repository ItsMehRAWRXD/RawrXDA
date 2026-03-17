#!/usr/bin/env pwsh
#Requires -PSEdition Core
<#
    RawrZ Python - WITH WORKING STRING INTERPOLATION
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args,
    [switch]$Version
)

$ErrorActionPreference = 'Stop'

# Function with working string interpolation
function Invoke-RawrZPython {
    param([string]$Code)
    
    $lines = $Code -split "`n"
    $variables = @{}
    
    foreach ($line in $lines) {
        $line = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) { continue }
        
        # Handle print statements with f-strings
        if ($line -match '^print\s*\((.*)\)') {
            $content = $matches[1]
            
            # Handle f-strings: f"text {var} more text"
            if ($content -match '^f[''"](.+)[''"]$') {
                $fstring = $matches[1]
                # Replace {variable} with actual values
                foreach ($var in $variables.Keys) {
                    $fstring = $fstring -replace "\{$var\}", $variables[$var]
                }
                # Handle expressions like {x + y}
                while ($fstring -match '\{([^}]+)\}') {
                    $expr = $matches[1]
                    # Replace variables in expression
                    $evalExpr = $expr
                    foreach ($var in $variables.Keys) {
                        $evalExpr = $evalExpr -replace "\b$var\b", $variables[$var]
                    }
                    try {
                        $result = Invoke-Expression $evalExpr
                        $fstring = $fstring -replace [regex]::Escape("{$expr}"), $result
                    } catch {
                        $fstring = $fstring -replace [regex]::Escape("{$expr}"), $expr
                    }
                }
                Write-Host $fstring
            }
            # Regular strings
            else {
                $content = $content -replace '^[''"]|[''"]$', ''
                Write-Host $content
            }
            continue
        }
        
        # Handle variable assignments
        if ($line -match '^(\w+)\s*=\s*(.+)$') {
            $varName = $matches[1]
            $varValue = $matches[2].Trim()
            
            # Integer
            if ($varValue -match '^\d+$') {
                $variables[$varName] = [int]$varValue
            }
            # Float
            elseif ($varValue -match '^\d+\.\d+$') {
                $variables[$varName] = [double]$varValue
            }
            # String
            elseif ($varValue -match '^[''"](.+)[''"]$') {
                $variables[$varName] = $matches[1]
            }
            # Expression (math, etc.)
            else {
                $evalValue = $varValue
                foreach ($var in $variables.Keys) {
                    $evalValue = $evalValue -replace "\b$var\b", $variables[$var]
                }
                try {
                    $variables[$varName] = Invoke-Expression $evalValue
                } catch {
                    $variables[$varName] = $varValue
                }
            }
            continue
        }
    }
}

# Main logic
if ($Version) {
    Write-Host "Python 3.11.0 (RawrZ Portable)"
    exit 0
}

if ($Args.Count -gt 0 -and (Test-Path $Args[0])) {
    $code = Get-Content $Args[0] -Raw
    Invoke-RawrZPython -Code $code
    exit 0
}

# Interactive REPL
Write-Host "Python 3.11.0 (RawrZ Portable)" -ForegroundColor Green
Write-Host "Type 'exit()' to quit" -ForegroundColor Gray
Write-Host ""

while ($true) {
    Write-Host ">>> " -NoNewline -ForegroundColor Yellow
    $line = Read-Host
    
    if ($line -in @('exit()', 'quit()')) { break }
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    
    try {
        Invoke-RawrZPython -Code $line
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    }
}
