# Test-Video-Engine-CLI.ps1
# Quick CLI test suite for the Agentic Video Engine
# Run: .\Test-Video-Engine-CLI.ps1

param(
  [switch]$RunAll,
  [switch]$TestSearch,
  [switch]$TestPlay,
  [switch]$TestDownload,
  [switch]$TestHelp,
  [switch]$JsonOnly
)

$ErrorActionPreference = "Continue"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Write-TestHeader {
  param([string]$TestName)
  if (-not $JsonOnly) {
    Write-Host "`n" -NoNewline
    Write-Host "═" * 60 -ForegroundColor Cyan
    Write-Host "  TEST: $TestName" -ForegroundColor Yellow
    Write-Host "═" * 60 -ForegroundColor Cyan
  }
}

function Write-TestResult {
  param([bool]$Success, [string]$Message)
  if (-not $JsonOnly) {
    if ($Success) {
      Write-Host "  ✅ PASS: $Message" -ForegroundColor Green
    }
    else {
      Write-Host "  ❌ FAIL: $Message" -ForegroundColor Red
    }
  }
}

# ============================================
# TEST 1: Video Help Command
# ============================================
function Test-VideoHelp {
  Write-TestHeader "video-help command"

  $output = & powershell.exe -ExecutionPolicy Bypass -File "$scriptDir\RawrXD.ps1" -CliMode -Command video-help 2>&1
  $outputText = $output -join "`n"

  $hasCommands = $outputText -match "COMMANDS:"
  $hasSearch = $outputText -match "video-search"
  $hasDownload = $outputText -match "video-download"
  $hasPlay = $outputText -match "video-play"

  Write-TestResult $hasCommands "Contains COMMANDS section"
  Write-TestResult $hasSearch "Lists video-search"
  Write-TestResult $hasDownload "Lists video-download"
  Write-TestResult $hasPlay "Lists video-play"

  return ($hasCommands -and $hasSearch -and $hasDownload -and $hasPlay)
}

# ============================================
# TEST 2: Video Play Command (opens browser)
# ============================================
function Test-VideoPlay {
  Write-TestHeader "video-play command"

  # Test with a safe YouTube URL
  $testUrl = "https://www.youtube.com/watch?v=jNQXAC9IVRw"  # "Me at the zoo" - first YouTube video

  $output = & powershell.exe -ExecutionPolicy Bypass -File "$scriptDir\RawrXD.ps1" -CliMode -Command video-play -URL $testUrl 2>&1
  $outputText = $output -join "`n"

  $hasSuccess = $outputText -match "success"
  $hasUrl = $outputText -match $testUrl.Replace("?", "\?")
  $hasJson = $outputText -match '"status"'

  Write-TestResult $hasSuccess "Returns success status"
  Write-TestResult $hasUrl "Contains correct URL"
  Write-TestResult $hasJson "Outputs JSON format"

  # Try to parse JSON
  if ($outputText -match '\{[\s\S]*"status"[\s\S]*\}') {
    try {
      $jsonMatch = [regex]::Match($outputText, '\{[^{}]*"status"[^{}]*\}')
      if ($jsonMatch.Success) {
        $json = $jsonMatch.Value | ConvertFrom-Json
        Write-TestResult ($json.status -eq "success") "JSON parses correctly"
      }
    }
    catch {
      Write-TestResult $false "JSON parsing: $_"
    }
  }

  return ($hasSuccess -and $hasJson)
}

# ============================================
# TEST 3: Browser Navigate Command
# ============================================
function Test-BrowserNavigate {
  Write-TestHeader "browser-navigate command"

  $testUrl = "https://github.com"

  $output = & powershell.exe -ExecutionPolicy Bypass -File "$scriptDir\RawrXD.ps1" -CliMode -Command browser-navigate -URL $testUrl 2>&1
  $outputText = $output -join "`n"

  $hasSuccess = $outputText -match "success"
  $hasUrl = $outputText -match "github.com"

  Write-TestResult $hasSuccess "Returns success status"
  Write-TestResult $hasUrl "Contains URL"

  return ($hasSuccess -and $hasUrl)
}

# ============================================
# TEST 4: Help Command (shows video commands)
# ============================================
function Test-MainHelp {
  Write-TestHeader "Main help includes video commands"

  $output = & powershell.exe -ExecutionPolicy Bypass -File "$scriptDir\RawrXD.ps1" -CliMode -Command help 2>&1
  $outputText = $output -join "`n"

  $hasVideoSearch = $outputText -match "video-search"
  $hasVideoDownload = $outputText -match "video-download"
  $hasVideoPlay = $outputText -match "video-play"
  $hasVideoHelp = $outputText -match "video-help"
  $hasBrowserNav = $outputText -match "browser-navigate"

  Write-TestResult $hasVideoSearch "Lists video-search in help"
  Write-TestResult $hasVideoDownload "Lists video-download in help"
  Write-TestResult $hasVideoPlay "Lists video-play in help"
  Write-TestResult $hasVideoHelp "Lists video-help in help"
  Write-TestResult $hasBrowserNav "Lists browser-navigate in help"

  return ($hasVideoSearch -and $hasVideoDownload -and $hasVideoPlay)
}

# ============================================
# TEST 5: Video Search (requires browser/network)
# ============================================
function Test-VideoSearch {
  Write-TestHeader "video-search command (requires network)"

  Write-Host "  ⚠️  Note: This test requires active browser automation" -ForegroundColor Yellow
  Write-Host "  ⚠️  May fail in headless CLI mode without WebView2" -ForegroundColor Yellow

  $output = & powershell.exe -ExecutionPolicy Bypass -File "$scriptDir\RawrXD.ps1" -CliMode -Command video-search -Prompt "test" 2>&1
  $outputText = $output -join "`n"

  # Check if it at least tries to search
  $hasSearchAttempt = $outputText -match "Searching YouTube" -or $outputText -match "video-search"
  $hasError = $outputText -match "not loaded" -or $outputText -match "Error"

  if ($hasError) {
    Write-TestResult $false "Search requires GUI mode with WebView2"
    Write-Host "  ℹ️  This is expected in pure CLI mode" -ForegroundColor Gray
  }
  else {
    Write-TestResult $hasSearchAttempt "Search command executed"
  }

  return $hasSearchAttempt
}

# ============================================
# RUN TESTS
# ============================================

if (-not $JsonOnly) {
  Write-Host "`n" -NoNewline
  Write-Host "╔" + ("═" * 58) + "╗" -ForegroundColor Magenta
  Write-Host "║  AGENTIC VIDEO ENGINE - CLI TEST SUITE                   ║" -ForegroundColor Magenta
  Write-Host "╚" + ("═" * 58) + "╝" -ForegroundColor Magenta
  Write-Host ""
  Write-Host "Testing video engine commands via CLI (no GUI required)" -ForegroundColor Gray
  Write-Host ""
}

$results = @{}

if ($RunAll -or $TestHelp -or (-not $TestSearch -and -not $TestPlay -and -not $TestDownload)) {
  $results["video-help"] = Test-VideoHelp
  $results["main-help"] = Test-MainHelp
}

if ($RunAll -or $TestPlay) {
  $results["video-play"] = Test-VideoPlay
  $results["browser-navigate"] = Test-BrowserNavigate
}

if ($RunAll -or $TestSearch) {
  $results["video-search"] = Test-VideoSearch
}

# ============================================
# SUMMARY
# ============================================
if (-not $JsonOnly) {
  Write-Host "`n" -NoNewline
  Write-Host "═" * 60 -ForegroundColor Cyan
  Write-Host "  TEST SUMMARY" -ForegroundColor Yellow
  Write-Host "═" * 60 -ForegroundColor Cyan

  $passed = ($results.Values | Where-Object { $_ -eq $true }).Count
  $total = $results.Count

  foreach ($test in $results.Keys) {
    $status = if ($results[$test]) { "✅ PASS" } else { "❌ FAIL" }
    Write-Host "  $status : $test" -ForegroundColor $(if ($results[$test]) { "Green" } else { "Red" })
  }

  Write-Host ""
  Write-Host "  Total: $passed / $total passed" -ForegroundColor $(if ($passed -eq $total) { "Green" } else { "Yellow" })
  Write-Host ""
}

# Output JSON summary
$jsonSummary = @{
  timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
  tests     = $results
  passed    = ($results.Values | Where-Object { $_ -eq $true }).Count
  total     = $results.Count
  success   = (($results.Values | Where-Object { $_ -eq $true }).Count -eq $results.Count)
}

if ($JsonOnly) {
  $jsonSummary | ConvertTo-Json -Depth 3
}
else {
  Write-Host "JSON Summary:" -ForegroundColor Gray
  $jsonSummary | ConvertTo-Json -Depth 3 | Write-Host -ForegroundColor DarkGray
}

# Return exit code
exit $(if ($jsonSummary.success) { 0 } else { 1 })
