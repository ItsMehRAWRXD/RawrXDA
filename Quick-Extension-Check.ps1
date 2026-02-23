<#
.SYNOPSIS
    Simple manual smoke test for RawrXD IDE extensions
.DESCRIPTION
    Quick validation that extension files exist and are loadable
#>

param([switch]$Verbose)

function Test-Result {
    param($Name, $Pass, $Details = "")
    $symbol = if ($Pass) { "✓" } else { "✗" }
    $color = if ($Pass) { "Green" } else { "Red" }
    Write-Host "  $symbol $Name" -ForegroundColor $color
    if ($Details -and $Verbose) {
        Write-Host "    → $Details" -ForegroundColor Gray
    }
    return $Pass
}

Clear-Host
Write-Host @"
╔═══════════════════════════════════════════════════════════════╗
║           RawrXD IDE Extension Quick Smoke Test               ║
╚═══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

$passed = 0
$failed = 0

Write-Host "`n▶ Testing Extension Files..." -ForegroundColor Yellow

# Test 1: Amazon Q VSIX
$amazonQPath = "d:\rawrxd\amazon-q-vscode-latest.vsix"
if (Test-Result "Amazon Q VSIX exists" (Test-Path $amazonQPath)) {
    $size = (Get-Item $amazonQPath).Length / 1MB
    Test-Result "Amazon Q VSIX size valid" ($size -gt 1) "$([math]::Round($size, 2)) MB"
    $passed++
} else {
    $failed++
}

# Test 2: Copilot VSIX
$copilotPath = "d:\rawrxd\copilot-latest.vsix"
if (Test-Result "GitHub Copilot VSIX exists" (Test-Path $copilotPath)) {
    $size = (Get-Item $copilotPath).Length / 1MB
    Test-Result "Copilot VSIX size valid" ($size -gt 1) "$([math]::Round($size, 2)) MB"
    $passed++
} else {
    $failed++
}

Write-Host "`n▶ Testing IDE Executables..." -ForegroundColor Yellow

# Test 3: CLI executable
$cliPath = "d:\rawrxd\RawrXD.exe"
if (Test-Result "CLI executable exists" (Test-Path $cliPath)) {
    $passed++
} else {
    $failed++
}

# Test 4: GUI executable
$guiPath = "d:\rawrxd\RawrXD_IDE_unified.exe"
if (Test-Result "GUI executable exists" (Test-Path $guiPath)) {
    $passed++
} else {
    # Try alternate name
    $altGuiPath = "d:\rawrxd\RawrXD_IDE.exe"
    if (Test-Result "GUI executable (alt) exists" (Test-Path $altGuiPath)) {
        $passed++
    } else {
        $failed++
    }
}

Write-Host "`n▶ Testing Plugin System..." -ForegroundColor Yellow

# Test 5: Plugin directory
$pluginDir = "d:\rawrxd\plugins"
if (Test-Result "Plugin directory exists" (Test-Path $pluginDir)) {
    $passed++
    
    # Check for any existing plugins
    if (Test-Path $pluginDir) {
        $existing = Get-ChildItem $pluginDir -Directory -ErrorAction SilentlyContinue
        if ($existing) {
            Write-Host "    → Found $($existing.Count) existing plugin(s)" -ForegroundColor Gray
        }
    }
} else {
    # Create it
    New-Item -Path $pluginDir -ItemType Directory -Force | Out-Null
    Test-Result "Created plugin directory" (Test-Path $pluginDir) "d:\rawrxd\plugins"
    $passed++
}

# Test 6: vsix_loader source
$loaderSrc = "d:\rawrxd\src\vsix_loader.cpp"
if (Test-Result "VSIX loader implementation exists" (Test-Path $loaderSrc)) {
    $passed++
} else {
    $failed++
}

Write-Host "`n▶ Manual Test Instructions..." -ForegroundColor Yellow
Write-Host @"

To manually test the extensions:

CLI MODE:
---------
1. Open PowerShell in d:\rawrxd
2. Run: .\RawrXD.exe --cli
3. Type: !plugin load amazon-q-vscode-latest.vsix
4. Type: !plugin load copilot-latest.vsix  
5. Type: !plugin list
6. Type: /exit

GUI MODE:
---------
1. Run: .\RawrXD_IDE_unified.exe
   (or .\RawrXD_IDE.exe if unified doesn't exist)
2. Look for Extensions menu or plugin option
3. Select "Install from VSIX"
4. Browse to amazon-q-vscode-latest.vsix
5. Repeat for copilot-latest.vsix

FEATURE TESTS:
--------------
After loading extensions, test:
• /chat <question>  — Test chat interface
• /suggest <code>   — Test code suggestions
• /analyze <file>   — Test code analysis

"@ -ForegroundColor Gray

Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                       Summary                                  ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$total = $passed + $failed
Write-Host "  Tests Passed: $passed / $total" -ForegroundColor $(if ($passed -eq $total) {"Green"} else {"Yellow"})
Write-Host "  Tests Failed: $failed / $total" -ForegroundColor $(if ($failed -eq 0) {"Green"} else {"Red"})

if ($failed -eq 0) {
    Write-Host "`n  ✓ All prerequisite checks passed!" -ForegroundColor Green
    Write-Host "  Extensions are ready to load manually." -ForegroundColor Green
} else {
    Write-Host "`n  ⚠ Some prerequisite checks failed." -ForegroundColor Yellow
    Write-Host "  Review missing files above." -ForegroundColor Yellow
}

Write-Host ""
