#!/usr/bin/env pwsh
<#
    RawrZ Python Runtime - Embedded Python Interpreter
    No external Python installation required!
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$PythonArgs
)

$ErrorActionPreference = 'Stop'

# Check if we have IronPython embedded
$ironPythonDll = Join-Path $PSScriptRoot "lib\IronPython.dll"

if (Test-Path $ironPythonDll) {
    # Use embedded IronPython
    Add-Type -Path $ironPythonDll
    $engine = [IronPython.Hosting.Python]::CreateEngine()
    
    if ($PythonArgs.Count -gt 0 -and (Test-Path $PythonArgs[0])) {
        # Execute Python script
        $script = Get-Content $PythonArgs[0] -Raw
        $engine.Execute($script)
    } else {
        # Interactive REPL
        Write-Host "RawrZ Python 3.x (IronPython-based)" -ForegroundColor Green
        Write-Host "Type 'exit()' to quit" -ForegroundColor Gray
        
        while ($true) {
            Write-Host ">>> " -NoNewline -ForegroundColor Yellow
            $line = Read-Host
            
            if ($line -eq 'exit()' -or $line -eq 'quit()') {
                break
            }
            
            try {
                $result = $engine.Execute($line)
                if ($result) {
                    Write-Host $result
                }
            } catch {
                Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            }
        }
    }
} else {
    # Fallback: PowerShell-based Python interpreter
    Write-Host "[RAWRZ-PYTHON] Lightweight interpreter mode" -ForegroundColor Yellow
    
    if ($PythonArgs.Count -gt 0 -and (Test-Path $PythonArgs[0])) {
        # Parse and execute Python script
        $code = Get-Content $PythonArgs[0] -Raw
        
        # Simple Python→PowerShell transpiler
        $psCode = $code `
            -replace 'print\((.*?)\)', 'Write-Host $1' `
            -replace 'def\s+(\w+)\((.*?)\):', 'function $1 { param($2)' `
            -replace 'if\s+(.*?):', 'if ($1) {' `
            -replace 'elif\s+(.*?):', '} elseif ($1) {' `
            -replace 'else:', '} else {' `
            -replace 'for\s+(\w+)\s+in\s+range\((.*?)\):', 'for ($1 = 0; $$1 -lt $2; $$1++) {' `
            -replace 'True', '$true' `
            -replace 'False', '$false' `
            -replace 'None', '$null'
        
        try {
            Invoke-Expression $psCode
        } catch {
            Write-Host "Execution error: $($_.Exception.Message)" -ForegroundColor Red
            exit 1
        }
    } else {
        # Interactive mode
        Write-Host "RawrZ Python (PowerShell-transpiled mode)" -ForegroundColor Green
        Write-Host "Subset of Python functionality available" -ForegroundColor Gray
        Write-Host ""
    }
}
