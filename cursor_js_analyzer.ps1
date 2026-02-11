# Cursor JavaScript Analysis & De-obfuscation Tool
# Extracts function signatures, exports, and API surface from compiled JS

$ErrorActionPreference = "Continue"

Write-Host "=== CURSOR JS ANALYZER ===" -ForegroundColor Cyan
Write-Host ""

$mainFile = "D:\Cursor_Critical_Source\cursor-agent\dist\main.js"
$outputDir = "D:\Cursor_Analysis_Results"
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

Write-Host "[1] Reading main.js (3.5MB)..." -ForegroundColor Yellow
$content = Get-Content $mainFile -Raw

Write-Host "[2] Extracting function signatures..." -ForegroundColor Yellow
$functions = [regex]::Matches($content, 'function\s+(\w+)\s*\([^)]*\)') | 
    ForEach-Object { $_.Groups[1].Value } | 
    Sort-Object -Unique |
    Select-Object -First 500

Write-Host "  Found $($functions.Count) unique function names"
$functions | Out-File "$outputDir\functions.txt"

Write-Host "[3] Extracting class definitions..." -ForegroundColor Yellow
$classes = [regex]::Matches($content, 'class\s+(\w+)') |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique |
    Select-Object -First 200

Write-Host "  Found $($classes.Count) unique class names"
$classes | Out-File "$outputDir\classes.txt"

Write-Host "[4] Extracting exports..." -ForegroundColor Yellow
$exports = [regex]::Matches($content, 'exports\.(\w+)') |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique |
    Select-Object -First 300

Write-Host "  Found $($exports.Count) unique exports"
$exports | Out-File "$outputDir\exports.txt"

Write-Host "[5] Searching for API endpoints..." -ForegroundColor Yellow
$urls = [regex]::Matches($content, '(https?://[^\s"''<>]+)') |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique

Write-Host "  Found $($urls.Count) URL patterns"
$urls | Out-File "$outputDir\urls.txt"

Write-Host "[6] Extracting Claude/Anthropic references..." -ForegroundColor Yellow
$claudeRefs = [regex]::Matches($content, '(claude|anthropic|api[_-]key)[^\s,;]*', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase) |
    ForEach-Object { $_.Groups[0].Value } |
    Sort-Object -Unique |
    Select-Object -First 100

Write-Host "  Found $($claudeRefs.Count) Claude/Anthropic references"
$claudeRefs | Out-File "$outputDir\claude_refs.txt"

Write-Host "[7] Extracting tool/command definitions..." -ForegroundColor Yellow
$tools = [regex]::Matches($content, '"(name|type|description)"\s*:\s*"([^"]+)"') |
    ForEach-Object { "$($_.Groups[1].Value): $($_.Groups[2].Value)" } |
    Select-Object -First 200

Write-Host "  Found $($tools.Count) tool/command definitions"
$tools | Out-File "$outputDir\tools_commands.txt"

Write-Host "[8] Searching for MCP protocol patterns..." -ForegroundColor Yellow
$mcpPatterns = @(
    "protocol",
    "request",
    "response", 
    "tool_use",
    "content_block",
    "message",
    "role"
)

$mcpFindings = @()
foreach ($pattern in $mcpPatterns) {
    $matches = [regex]::Matches($content, "[\w]*$pattern[\w]*", [System.Text.RegularExpressions.RegexOptions]::IgnoreCase) |
        ForEach-Object { $_.Value } |
        Sort-Object -Unique
    $mcpFindings += $matches
}

$mcpFindings = $mcpFindings | Sort-Object -Unique | Select-Object -First 200
Write-Host "  Found $($mcpFindings.Count) MCP-related identifiers"
$mcpFindings | Out-File "$outputDir\mcp_patterns.txt"

Write-Host "[9] Extracting import statements..." -ForegroundColor Yellow
$imports = [regex]::Matches($content, 'require\([''"]([^''"]+)[''"]\)') |
    ForEach-Object { $_.Groups[1].Value } |
    Sort-Object -Unique

Write-Host "  Found $($imports.Count) require() imports"
$imports | Out-File "$outputDir\imports.txt"

Write-Host "[10] Analyzing cursor-mcp extension..." -ForegroundColor Yellow
$mcpFile = "D:\Cursor_Critical_Source\cursor-mcp\dist\main.js"
if (Test-Path $mcpFile) {
    $mcpContent = Get-Content $mcpFile -Raw
    
    # Extract MCP-specific patterns
    $mcpTools = [regex]::Matches($mcpContent, 'tool[_-]name[''"]?\s*[:=]\s*[''"]([^''"]+)') |
        ForEach-Object { $_.Groups[1].Value } |
        Sort-Object -Unique
    
    Write-Host "  Found $($mcpTools.Count) MCP tool names"
    $mcpTools | Out-File "$outputDir\mcp_tools.txt"
}

Write-Host "[11] Creating analysis summary..." -ForegroundColor Yellow

$summary = @"
CURSOR JAVASCRIPT ANALYSIS RESULTS
===================================
Generated: $(Get-Date)
Source: D:\Cursor_Critical_Source\

FILES ANALYZED:
- cursor-agent/dist/main.js (3.5MB)
- cursor-mcp/dist/main.js (3.4MB)

EXTRACTED DATA:
--------------
Functions:      $($functions.Count)
Classes:        $($classes.Count)  
Exports:        $($exports.Count)
URLs:           $($urls.Count)
Claude refs:    $($claudeRefs.Count)
Tools/Commands: $($tools.Count)
MCP Patterns:   $($mcpFindings.Count)
Imports:        $($imports.Count)
MCP Tools:      $($mcpTools.Count)

OUTPUT FILES:
-------------
$outputDir\functions.txt         - All extracted function names
$outputDir\classes.txt           - All extracted class names
$outputDir\exports.txt           - Module exports
$outputDir\urls.txt              - API endpoints and URLs
$outputDir\claude_refs.txt       - Claude/Anthropic references
$outputDir\tools_commands.txt    - Tool/command definitions
$outputDir\mcp_patterns.txt      - MCP protocol identifiers
$outputDir\imports.txt           - Required modules
$outputDir\mcp_tools.txt         - MCP tool names

NOTABLE FINDINGS:
-----------------

Top Functions (First 20):
$($functions | Select-Object -First 20 | ForEach-Object { "- $_" } | Out-String)

Top Classes (First 10):
$($classes | Select-Object -First 10 | ForEach-Object { "- $_" } | Out-String)

Sample URLs (First 10):
$($urls | Select-Object -First 10 | ForEach-Object { "- $_" } | Out-String)

NEXT STEPS:
-----------
1. De-minify JavaScript with js-beautify or prettier
2. Manually review extracted functions for agent logic
3. Analyze MCP protocol implementation
4. Extract complete API surface
5. Build working clones/reimplementations

"@

$summary | Out-File "$outputDir\ANALYSIS_SUMMARY.txt"

Write-Host "`n=== COMPLETE ===" -ForegroundColor Green
Write-Host "Results: $outputDir" -ForegroundColor Yellow
Write-Host ""
Write-Host "Key Files:"
Get-ChildItem $outputDir -Filter "*.txt" | ForEach-Object {
    $size = "{0:N0}" -f $_.Length
    Write-Host "  $($_.Name) ($size bytes)"
}
