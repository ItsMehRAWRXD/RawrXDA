#!/usr/bin/env pwsh
#Requires -PSEdition Core
<#
    RawrZ Node.js - WITH WORKING TEMPLATE LITERALS
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args,
    [switch]$Version
)

$ErrorActionPreference = 'Stop'

# Function with working template literals
function Invoke-RawrZJavaScript {
    param([string]$Code)
    
    $lines = $Code -split "`n"
    $variables = @{}
    
    foreach ($line in $lines) {
        $line = $line.Trim() -replace ';$', ''
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('//')) { continue }
        
        # Handle console.log with template literals
        if ($line -match '^console\.log\s*\((.*)\)') {
            $content = $matches[1]
            
            # Handle template literals: `text ${var} more text`
            if ($content -match '^`(.+)`$') {
                $template = $matches[1]
                # Replace ${variable} with actual values
                foreach ($var in $variables.Keys) {
                    $template = $template -replace "\$\{$var\}", $variables[$var]
                }
                # Handle expressions like ${x + y}
                while ($template -match '\$\{([^}]+)\}') {
                    $expr = $matches[1]
                    # Replace variables in expression
                    $evalExpr = $expr
                    foreach ($var in $variables.Keys) {
                        $evalExpr = $evalExpr -replace "\b$var\b", $variables[$var]
                    }
                    try {
                        $result = Invoke-Expression $evalExpr
                        $template = $template -replace [regex]::Escape("`${$expr}"), $result
                    } catch {
                        $template = $template -replace [regex]::Escape("`${$expr}"), $expr
                    }
                }
                Write-Host $template
            }
            # Regular strings
            else {
                $content = $content -replace '^[''"`]|[''"`]$', ''
                Write-Host $content
            }
            continue
        }
        
        # Handle variable declarations
        if ($line -match '^(const|let|var)\s+(\w+)\s*=\s*(.+)$') {
            $varName = $matches[2]
            $varValue = $matches[3].Trim()
            
            # Integer
            if ($varValue -match '^\d+$') {
                $variables[$varName] = [int]$varValue
            }
            # Float
            elseif ($varValue -match '^\d+\.\d+$') {
                $variables[$varName] = [double]$varValue
            }
            # String
            elseif ($varValue -match '^[''"`](.+)[''"`]$') {
                $variables[$varName] = $matches[1]
            }
            # Expression
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
    Write-Host "v20.0.0 (RawrZ Portable)"
    exit 0
}

if ($Args.Count -gt 0 -and (Test-Path $Args[0])) {
    $code = Get-Content $Args[0] -Raw
    Invoke-RawrZJavaScript -Code $code
    exit 0
}

# Interactive REPL
Write-Host "Welcome to Node.js v20.0.0 (RawrZ Portable)" -ForegroundColor Green
Write-Host "Type '.exit' to quit" -ForegroundColor Gray
Write-Host ""

while ($true) {
    Write-Host "> " -NoNewline -ForegroundColor Yellow
    $line = Read-Host
    
    if ($line -in @('.exit', 'process.exit()')) { break }
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    
    try {
        Invoke-RawrZJavaScript -Code $line
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    }
}
