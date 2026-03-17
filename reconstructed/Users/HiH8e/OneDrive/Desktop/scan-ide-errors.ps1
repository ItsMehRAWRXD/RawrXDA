# ============================================================================
# IDE Error Scanner - Finds undefined functions, syntax errors, and missing handlers
# ============================================================================

param(
    [string]$HtmlFile = "C:\Users\HiH8e\OneDrive\Desktop\IDEre2.html",
    [switch]$ShowDetails,
    [switch]$ExportJSON,
    [string]$OutputFile = "C:\Users\HiH8e\OneDrive\Desktop\scan-results.json"
)

$ErrorActionPreference = 'SilentlyContinue'

# Read the entire HTML file
Write-Host "📖 Reading HTML file..." -ForegroundColor Cyan
$content = Get-Content $HtmlFile -Raw
$lines = Get-Content $HtmlFile

# Initialize results
$results = @{
    SyntaxErrors = @()
    UndefinedFunctions = @()
    UndefinedHandlers = @()
    MissingFunctionBodies = @()
    BracesMismatch = @()
    OnclickWithoutFunction = @()
    OnchangeWithoutFunction = @()
    OnkeyWithoutFunction = @()
    UnusedVariables = @()
    TotalIssues = 0
}

Write-Host "`n🔍 Scanning for issues...`n" -ForegroundColor Yellow

# ============================================================================
# 1. DETECT SYNTAX ERRORS - Mismatched braces
# ============================================================================
Write-Host "1️⃣  Checking brace balance..." -ForegroundColor Cyan
$openBraces = ($content | Select-String -Pattern '\{' -AllMatches).Matches.Count
$closeBraces = ($content | Select-String -Pattern '\}' -AllMatches).Matches.Count
$difference = $openBraces - $closeBraces

if ($difference -ne 0) {
    $results.SyntaxErrors += @{
        Type = "BraceImbalance"
        OpenCount = $openBraces
        CloseCount = $closeBraces
        Difference = $difference
        Severity = if ($difference -gt 0) { "CRITICAL - Missing closing braces" } else { "CRITICAL - Extra closing braces" }
    }
    Write-Host "   ⚠️  Brace mismatch: $openBraces open vs $closeBraces close (diff: $difference)" -ForegroundColor Red
}

# ============================================================================
# 2. FIND UNDEFINED FUNCTION REFERENCES
# ============================================================================
Write-Host "2️⃣  Scanning for undefined function calls..." -ForegroundColor Cyan

# Extract all defined functions
$definedFunctions = @{}
$functionPattern = '(?:function\s+(\w+)|const\s+(\w+)\s*=\s*(?:async\s*)?(?:\(|function)|\b(\w+)\s*:\s*(?:async\s*)?function)'

foreach ($match in [regex]::Matches($content, $functionPattern)) {
    $funcName = $match.Groups[1].Value
    if ([string]::IsNullOrEmpty($funcName)) { $funcName = $match.Groups[2].Value }
    if ([string]::IsNullOrEmpty($funcName)) { $funcName = $match.Groups[3].Value }
    if (-not [string]::IsNullOrEmpty($funcName)) {
        $definedFunctions[$funcName] = $true
    }
}

Write-Host "   ✅ Found $($definedFunctions.Count) defined functions" -ForegroundColor Green

# Extract all function calls
$callPattern = '([a-zA-Z_]\w*)\s*\('
$undefinedCalls = @{}

foreach ($match in [regex]::Matches($content, $callPattern)) {
    $funcCall = $match.Groups[1].Value
    
    # Skip common JS keywords and built-ins
    $builtins = @('if', 'for', 'while', 'switch', 'catch', 'function', 'return', 'new', 'typeof', 'instanceof',
                  'Array', 'Object', 'String', 'Number', 'Boolean', 'Date', 'Math', 'JSON', 'Promise', 'setTimeout',
                  'setInterval', 'clearTimeout', 'clearInterval', 'console', 'document', 'window', 'this', 'super',
                  'async', 'await', 'yield', 'delete', 'void', 'class', 'export', 'import', 'let', 'const', 'var',
                  'parseInt', 'parseFloat', 'isNaN', 'isFinite', 'eval', 'encodeURI', 'decodeURI', 'fetch',
                  'alert', 'confirm', 'prompt', 'requestAnimationFrame', 'cancelAnimationFrame', 'RegExp')
    
    if ($builtins -notcontains $funcCall -and -not $definedFunctions.ContainsKey($funcCall)) {
        if (-not $undefinedCalls.ContainsKey($funcCall)) {
            $undefinedCalls[$funcCall] = 0
        }
        $undefinedCalls[$funcCall]++
    }
}

foreach ($func in $undefinedCalls.GetEnumerator() | Sort-Object Value -Descending) {
    $results.UndefinedFunctions += @{
        FunctionName = $func.Key
        CallCount = $func.Value
        LineNumbers = @()  # Would need more complex parsing to find exact lines
    }
}

Write-Host "   ⚠️  Found $($undefinedCalls.Count) potentially undefined functions" -ForegroundColor Yellow
$top10 = $undefinedCalls.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 10
foreach ($item in $top10) {
    Write-Host "      • $($item.Key) (called $($item.Value) times)" -ForegroundColor Yellow
}

# ============================================================================
# 3. FIND HANDLERS WITHOUT FUNCTIONS
# ============================================================================
Write-Host "3️⃣  Scanning for orphaned event handlers..." -ForegroundColor Cyan

$handlers = @{
    onclick = 'onclick="([^"]+)"'
    onchange = 'onchange="([^"]+)"'
    onkeypress = 'onkeypress="([^"]+)"'
    onkeydown = 'onkeydown="([^"]+)"'
    onkeyup = 'onkeyup="([^"]+)"'
    onload = 'onload="([^"]+)"'
    onfocus = 'onfocus="([^"]+)"'
    onblur = 'onblur="([^"]+)"'
    onsubmit = 'onsubmit="([^"]+)"'
}

foreach ($handler in $handlers.GetEnumerator()) {
    $pattern = $handler.Value
    $matches = [regex]::Matches($content, $pattern)
    
    foreach ($match in $matches) {
        $code = $match.Groups[1].Value
        # Extract function name from handler code
        $funcName = [regex]::Match($code, '([a-zA-Z_]\w*)\s*\(').Groups[1].Value
        
        if (-not [string]::IsNullOrEmpty($funcName) -and -not $definedFunctions.ContainsKey($funcName)) {
            $results.UndefinedHandlers += @{
                HandlerType = $handler.Key
                FunctionName = $funcName
                Code = $code
            }
        }
    }
}

Write-Host "   ⚠️  Found $($results.UndefinedHandlers.Count) orphaned event handlers" -ForegroundColor Yellow
$results.UndefinedHandlers | ForEach-Object {
    Write-Host "      • $($_.HandlerType): $($_.FunctionName) - `"$($_.Code)`"" -ForegroundColor Yellow
}

# ============================================================================
# 4. FIND FUNCTION DEFINITIONS WITHOUT BODIES
# ============================================================================
Write-Host "4️⃣  Scanning for incomplete function definitions..." -ForegroundColor Cyan

$incompletePattern = 'function\s+(\w+)\s*\([^)]*\)\s*(?:\{|async|\s|;)'
$matches = [regex]::Matches($content, $incompletePattern)

foreach ($match in $matches) {
    $lineNum = ($content.Substring(0, $match.Index) -split '\n').Count
    # Check if function has a body
    $afterFunc = $content.Substring($match.Index + $match.Length, [Math]::Min(100, $content.Length - $match.Index - $match.Length))
    if ($afterFunc -notmatch '\{' -or $afterFunc -match '^\s*;') {
        $results.MissingFunctionBodies += @{
            FunctionName = $match.Groups[1].Value
            Line = $lineNum
        }
    }
}

Write-Host "   ✅ Found $($results.MissingFunctionBodies.Count) incomplete definitions" -ForegroundColor Green

# ============================================================================
# 5. SPECIFIC ERRORS FROM CONSOLE
# ============================================================================
Write-Host "5️⃣  Cross-referencing known errors..." -ForegroundColor Cyan

$knownErrors = @(
    'initOPFS',
    'closeMultiChat',
    'toggleMultiChatSettings',
    'toggleMultiChatResearch',
    'createNewChat',
    'createCheckpoint',
    'restoreLastCheckpoint',
    'retryLastCheckpoint'
)

foreach ($func in $knownErrors) {
    $isDefined = $definedFunctions.ContainsKey($func)
    if (-not $isDefined) {
        $count = ([regex]::Matches($content, "\b$func\s*\(")).Count
        if ($count -gt 0) {
            Write-Host "   ❌ CRITICAL: `$func` is called $count times but never defined!" -ForegroundColor Red
        }
    } else {
        Write-Host "   ✅ `$func` is defined" -ForegroundColor Green
    }
}

# ============================================================================
# 6. CHECK SYNTAX ERRORS AT SPECIFIC LINES
# ============================================================================
Write-Host "6️⃣  Checking specific error lines..." -ForegroundColor Cyan

$errorLines = @(21672, 21689, 9772, 2572)
foreach ($lineNum in $errorLines) {
    if ($lineNum -le $lines.Count) {
        Write-Host "   Line $lineNum`:" -ForegroundColor Cyan
        Write-Host "      $($lines[$lineNum - 1])" -ForegroundColor Gray
    }
}

# ============================================================================
# SUMMARY
# ============================================================================
$results.TotalIssues = $results.SyntaxErrors.Count + $results.UndefinedFunctions.Count + $results.UndefinedHandlers.Count

Write-Host "`n" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "📊 SCAN SUMMARY" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "   Syntax Errors:              $($results.SyntaxErrors.Count)" -ForegroundColor $(if ($results.SyntaxErrors.Count -gt 0) { 'Red' } else { 'Green' })
Write-Host "   Undefined Functions:        $($results.UndefinedFunctions.Count)" -ForegroundColor $(if ($results.UndefinedFunctions.Count -gt 0) { 'Yellow' } else { 'Green' })
Write-Host "   Orphaned Event Handlers:    $($results.UndefinedHandlers.Count)" -ForegroundColor $(if ($results.UndefinedHandlers.Count -gt 0) { 'Yellow' } else { 'Green' })
Write-Host "   Incomplete Definitions:     $($results.MissingFunctionBodies.Count)" -ForegroundColor $(if ($results.MissingFunctionBodies.Count -gt 0) { 'Yellow' } else { 'Green' })
Write-Host "   ────────────────────────────" -ForegroundColor Cyan
Write-Host "   TOTAL ISSUES:               $($results.TotalIssues)" -ForegroundColor $(if ($results.TotalIssues -gt 0) { 'Red' } else { 'Green' })
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# ============================================================================
# EXPORT RESULTS
# ============================================================================
if ($ExportJSON) {
    $results | ConvertTo-Json | Set-Content $OutputFile
    Write-Host "`n📁 Results exported to: $OutputFile" -ForegroundColor Green
}

if ($ShowDetails) {
    Write-Host "`n📋 DETAILED REPORT:" -ForegroundColor Yellow
    Write-Host $results | Format-List
}

Write-Host "`n✨ Scan complete!" -ForegroundColor Green
