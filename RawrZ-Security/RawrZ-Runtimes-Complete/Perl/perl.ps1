#!/usr/bin/env pwsh
#Requires -PSEdition Core
<#
    RawrZ Perl - WITH WORKING STRING INTERPOLATION
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args,
    [switch]$Version
)

$ErrorActionPreference = 'Stop'

function Invoke-RawrZPerl {
    param([string]$Code)
    
    $lines = $Code -split "`n"
    $variables = @{}
    
    foreach ($line in $lines) {
        $line = $line.Trim() -replace ';$', ''
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) { continue }
        
        # Handle print with concatenation
        if ($line -match '^print\s+(.+)$') {
            $content = $matches[1]
            
            # Handle string concatenation with .
            $parts = $content -split '\s*\.\s*'
            $output = ""
            foreach ($part in $parts) {
                $part = $part.Trim() -replace '^[''"]|[''"]$', ''
                
                # Replace \n with actual newlines
                $part = $part -replace '\\n', "`n"
                
                # Check if it's a variable
                if ($part -match '^\$(\w+)$') {
                    $varName = $matches[1]
                    if ($variables.ContainsKey($varName)) {
                        $output += $variables[$varName]
                    } else {
                        $output += $part
                    }
                }
                # Check if it's an expression like ($x + $y)
                elseif ($part -match '^\((.+)\)$') {
                    $expr = $matches[1]
                    foreach ($var in $variables.Keys) {
                        $expr = $expr -replace [regex]::Escape("`$$var"), $variables[$var]
                    }
                    try {
                        $result = Invoke-Expression $expr
                        $output += $result
                    } catch {
                        $output += $part
                    }
                }
                else {
                    $output += $part
                }
            }
            
            Write-Host $output -NoNewline
            continue
        }
        
        # Handle variable declarations
        if ($line -match '^my\s+\$(\w+)\s*=\s*(.+)$') {
            $varName = $matches[1]
            $varValue = $matches[2].Trim()
            
            if ($varValue -match '^\d+$') {
                $variables[$varName] = [int]$varValue
            } elseif ($varValue -match '^[''"](.+)[''"]$') {
                $variables[$varName] = $matches[1]
            } else {
                $variables[$varName] = $varValue
            }
            continue
        }
    }
}

if ($Version) {
    Write-Host "perl 5.36.0 (RawrZ Portable)"
    exit 0
}

if ($Args.Count -gt 0 -and (Test-Path $Args[0])) {
    $code = Get-Content $Args[0] -Raw
    Invoke-RawrZPerl -Code $code
    exit 0
}
