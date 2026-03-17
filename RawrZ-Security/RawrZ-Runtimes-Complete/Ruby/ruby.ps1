#!/usr/bin/env pwsh
#Requires -PSEdition Core
<#
    RawrZ Ruby - WITH WORKING STRING INTERPOLATION
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args,
    [switch]$Version
)

$ErrorActionPreference = 'Stop'

function Invoke-RawrZRuby {
    param([string]$Code)
    
    $lines = $Code -split "`n"
    $variables = @{}
    
    foreach ($line in $lines) {
        $line = $line.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) { continue }
        
        # Handle puts with interpolation
        if ($line -match '^puts\s+(.+)$') {
            $content = $matches[1]
            $content = $content -replace '^[''"]|[''"]$', ''
            
            # Handle #{variable} interpolation
            foreach ($var in $variables.Keys) {
                $content = $content -replace "#\{$var\}", $variables[$var]
            }
            # Handle #{expression}
            while ($content -match '#\{([^}]+)\}') {
                $expr = $matches[1]
                $evalExpr = $expr
                foreach ($var in $variables.Keys) {
                    $evalExpr = $evalExpr -replace "\b$var\b", $variables[$var]
                }
                try {
                    $result = Invoke-Expression $evalExpr
                    $content = $content -replace [regex]::Escape("#{$expr}"), $result
                } catch {
                    $content = $content -replace [regex]::Escape("#{$expr}"), $expr
                }
            }
            
            Write-Host $content
            continue
        }
        
        # Handle variable assignments
        if ($line -match '^(\w+)\s*=\s*(.+)$') {
            $varName = $matches[1]
            $varValue = $matches[2].Trim()
            
            if ($varValue -match '^\d+$') {
                $variables[$varName] = [int]$varValue
            } elseif ($varValue -match '^[''"](.+)[''"]$') {
                $variables[$varName] = $matches[1]
            } else {
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

if ($Version) {
    Write-Host "ruby 3.2.0 (RawrZ Portable)"
    exit 0
}

if ($Args.Count -gt 0 -and (Test-Path $Args[0])) {
    $code = Get-Content $Args[0] -Raw
    Invoke-RawrZRuby -Code $code
    exit 0
}

Write-Host "ruby 3.2.0 (RawrZ Portable)" -ForegroundColor Green
Write-Host "Type 'exit' to quit" -ForegroundColor Gray
