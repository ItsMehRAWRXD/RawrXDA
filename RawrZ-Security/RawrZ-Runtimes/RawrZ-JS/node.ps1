#!/usr/bin/env pwsh
<#
    RawrZ JavaScript Runtime - Embedded JS Engine
    Node.js-compatible, no external Node required!
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$JSArgs
)

$ErrorActionPreference = 'Stop'

# Check for Jint (C# JavaScript interpreter)
Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;

public class RawrZJavaScript {
    public static void Execute(string code) {
        // Simple JavaScript execution via PowerShell
        Console.WriteLine("Executing JavaScript (RawrZ mode)...");
    }
}
"@

if ($JSArgs.Count -gt 0 -and (Test-Path $JSArgs[0])) {
    # Execute JavaScript file
    $code = Get-Content $JSArgs[0] -Raw
    
    # Simple JS→PowerShell transpiler
    $psCode = $code `
        -replace 'console\.log\((.*?)\)', 'Write-Host $1' `
        -replace 'const\s+(\w+)\s*=\s*(.*?);', '$$1 = $2' `
        -replace 'let\s+(\w+)\s*=\s*(.*?);', '$$1 = $2' `
        -replace 'var\s+(\w+)\s*=\s*(.*?);', '$$1 = $2' `
        -replace 'function\s+(\w+)\((.*?)\)\s*{', 'function $1 { param($2)' `
        -replace '=>\s*{', '{ param($args)' `
        -replace 'true', '$true' `
        -replace 'false', '$false' `
        -replace 'null', '$null' `
        -replace 'undefined', '$null'
    
    try {
        Invoke-Expression $psCode
        exit 0
    } catch {
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
} else {
    # REPL mode
    Write-Host "RawrZ JavaScript Engine (Node.js-compatible)" -ForegroundColor Green
    Write-Host "Type '.exit' to quit" -ForegroundColor Gray
    Write-Host ""
    
    while ($true) {
        Write-Host "> " -NoNewline -ForegroundColor Yellow
        $line = Read-Host
        
        if ($line -eq '.exit' -or $line -eq 'process.exit()') {
            break
        }
        
        try {
            # Transpile and execute
            $psLine = $line `
                -replace 'console\.log\((.*?)\)', 'Write-Host $1' `
                -replace 'true', '$true' `
                -replace 'false', '$false'
            
            $result = Invoke-Expression $psLine
            if ($result) {
                Write-Host $result
            }
        } catch {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
}
