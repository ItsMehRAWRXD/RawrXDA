# PowerShell Script to Find Missing Function Definitions in IDEre2.html
# This script analyzes function calls and checks if they are defined

param(
    [string]$FilePath = "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html"
)

Write-Host "=== Missing Function Analyzer ===" -ForegroundColor Cyan
Write-Host "Analyzing: $FilePath" -ForegroundColor Yellow
Write-Host ""

# Read the file content
$content = Get-Content -Path $FilePath -Raw

# Extract all function definitions
# Patterns: function name(), const name = function(), const name = () =>, async function name()
$functionDefinitions = @{}

# Match: function functionName(
[regex]::Matches($content, 'function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: const functionName = function(
[regex]::Matches($content, 'const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*function\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: const functionName = (
[regex]::Matches($content, 'const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*\([^)]*\)\s*=>') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: const functionName = async (
[regex]::Matches($content, 'const\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*async\s*\([^)]*\)\s*=>') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: async function functionName(
[regex]::Matches($content, 'async\s+function\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: let functionName = function(
[regex]::Matches($content, 'let\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*function\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: var functionName = function(
[regex]::Matches($content, 'var\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*function\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: window.functionName = function(
[regex]::Matches($content, 'window\.([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*function\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

# Match: window.functionName = async function(
[regex]::Matches($content, 'window\.([a-zA-Z_$][a-zA-Z0-9_$]*)\s*=\s*async\s+function\s*\(') | ForEach-Object {
    $functionDefinitions[$_.Groups[1].Value] = $true
}

Write-Host "Found $($functionDefinitions.Count) function definitions" -ForegroundColor Green
Write-Host ""

# Extract function calls from onclick, onchange, etc. attributes
$onclickFunctions = @{}

# Match onclick="functionName(...)"
[regex]::Matches($content, 'on(?:click|change|input|submit|load|keydown|keyup|keypress|blur|focus)\s*=\s*"([a-zA-Z_$][a-zA-Z0-9_$]*)\s*\(') | ForEach-Object {
    $funcName = $_.Groups[1].Value
    if (-not $onclickFunctions.ContainsKey($funcName)) {
        $onclickFunctions[$funcName] = 0
    }
    $onclickFunctions[$funcName]++
}

Write-Host "Found $($onclickFunctions.Count) unique functions called from inline event handlers" -ForegroundColor Green
Write-Host ""

# Find missing functions from onclick handlers
$missingOnclickFunctions = @{}
foreach ($call in $onclickFunctions.Keys) {
    if (-not $functionDefinitions.ContainsKey($call)) {
        $missingOnclickFunctions[$call] = $onclickFunctions[$call]
    }
}

# Sort by call count (descending)
$sortedMissing = $missingOnclickFunctions.GetEnumerator() | Sort-Object -Property Value -Descending

Write-Host "=== MISSING FUNCTIONS FROM EVENT HANDLERS ===" -ForegroundColor Red
Write-Host "Found $($missingOnclickFunctions.Count) undefined functions called from inline handlers" -ForegroundColor Yellow
Write-Host ""

if ($sortedMissing.Count -gt 0) {
    Write-Host "Function Name                          | Call Count" -ForegroundColor Cyan
    Write-Host "-------------------------------------------------------" -ForegroundColor Cyan
    
    foreach ($item in $sortedMissing) {
        $padding = " " * (40 - $item.Key.Length)
        Write-Host "$($item.Key)$padding| $($item.Value)" -ForegroundColor White
    }
    
    Write-Host ""
    Write-Host "=== GENERATING REPORT ===" -ForegroundColor Cyan
    
    # Generate detailed report
    $reportPath = "C:\Users\HiH8e\OneDrive\Desktop\missing-functions-report.txt"
    $report = @"
MISSING FUNCTIONS REPORT
Generated: $(Get-Date)
File: $FilePath

SUMMARY:
- Total function definitions found: $($functionDefinitions.Count)
- Total functions called from inline handlers: $($onclickFunctions.Count)
- Missing function definitions: $($missingOnclickFunctions.Count)

MISSING FUNCTIONS (sorted by usage):
"@
    
    foreach ($item in $sortedMissing) {
        $report += "`n$($item.Key) - Called $($item.Value) time(s) from inline event handlers"
    }
    
    $report += "`n`nDEFINED FUNCTIONS:`n"
    $report += ($functionDefinitions.Keys | Sort-Object) -join "`n"
    
    $report | Out-File -FilePath $reportPath -Encoding UTF8
    
    Write-Host "Report saved to: $reportPath" -ForegroundColor Green
} else {
    Write-Host "No missing functions found! All event handler functions appear to be defined." -ForegroundColor Green
}

Write-Host ""
Write-Host "=== ANALYSIS COMPLETE ===" -ForegroundColor Cyan
