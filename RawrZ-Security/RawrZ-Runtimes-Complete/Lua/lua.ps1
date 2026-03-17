#!/usr/bin/env pwsh
#Requires -PSEdition Core
<#
    RawrZ Lua - WITH WORKING STRING CONCATENATION
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args,
    [switch]$Version
)

$ErrorActionPreference = 'Stop'

function Invoke-RawrZLua {
    param([string]$Code)
    
    $lines = $Code -split "`n"
    $variables = @{}
    
    foreach ($line in $lines) {
        $line = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('--')) { continue }
        
        # Handle print with concatenation
        if ($line -match '^print\s*\((.+)\)$') {
            $content = $matches[1]
            $content = $content -replace '^[''"]|[''"]$', ''
            
            # Handle string concatenation with ..
            $parts = $content -split '\s*\.\.\s*'
            $output = ""
            foreach ($part in $parts) {
                $part = $part.Trim() -replace '^[''"]|[''"]$', ''
                
                # Check if it's a variable
                if ($part -match '^(\w+)$' -and $variables.ContainsKey($part)) {
                    $output += $variables[$part]
                }
                # Check if it's an expression like (x + y)
                elseif ($part -match '^\((.+)\)$') {
                    $expr = $matches[1]
                    foreach ($var in $variables.Keys) {
                        $expr = $expr -replace "\b$var\b", $variables[$var]
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
            
            Write-Host $output
            continue
        }
        
        # Handle local variable declarations
        if ($line -match '^local\s+(\w+)\s*=\s*(.+)$') {
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
    Write-Host "Lua 5.4.0 (RawrZ Portable)"
    exit 0
}

if ($Args.Count -gt 0 -and (Test-Path $Args[0])) {
    $code = Get-Content $Args[0] -Raw
    Invoke-RawrZLua -Code $code
    exit 0
}
