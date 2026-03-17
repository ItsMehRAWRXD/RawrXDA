#Requires -RunAsAdministrator

<#
.SYNOPSIS
    Cursor Authentication Bypass - Force Local AI Usage

.DESCRIPTION
    Patches Cursor's JavaScript to bypass authentication checks
    and redirect all API calls to local Ollama instance.

.NOTES
    Run as Administrator
    Backs up original files automatically
    Re-run after each Cursor update
#>

$ErrorActionPreference = "Stop"

Write-Host "`n╔═══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║   CURSOR AUTHENTICATION BYPASS & LOCAL REDIRECT  ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

# Configuration
$cursorDir = "E:\Everything\cursor"
$cursorJs = "$cursorDir\resources\app\out\vs\workbench\workbench.desktop.main.js"
$hostsFile = "$env:SystemRoot\System32\drivers\etc\hosts"

# Check prerequisites
Write-Host "[1/6] Checking prerequisites..." -ForegroundColor Yellow

if (-not (Test-Path $cursorDir)) {
    Write-Host "❌ Cursor directory not found: $cursorDir" -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $cursorJs)) {
    Write-Host "❌ Cursor JavaScript not found: $cursorJs" -ForegroundColor Red
    Write-Host "   Cursor might not be installed or path is incorrect" -ForegroundColor Yellow
    exit 1
}

Write-Host "   ✓ Cursor found at: $cursorDir" -ForegroundColor Green
Write-Host "   ✓ JavaScript file: $cursorJs" -ForegroundColor Green

# Create backup
Write-Host "`n[2/6] Creating backup..." -ForegroundColor Yellow

$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$backup = "$cursorJs.backup.$timestamp"

try {
    Copy-Item $cursorJs $backup -Force
    Write-Host "   ✓ Backup created: $backup" -ForegroundColor Green
} catch {
    Write-Host "   ❌ Failed to create backup: $_" -ForegroundColor Red
    exit 1
}

# Read file
Write-Host "`n[3/6] Reading Cursor JavaScript (this may take a moment)..." -ForegroundColor Yellow

try {
    $content = Get-Content $cursorJs -Raw
    $originalSize = $content.Length
    Write-Host "   ✓ File size: $([math]::Round($originalSize / 1MB, 2)) MB" -ForegroundColor Green
} catch {
    Write-Host "   ❌ Failed to read file: $_" -ForegroundColor Red
    exit 1
}

# Apply authentication bypass patches
Write-Host "`n[4/6] Applying authentication bypass patches..." -ForegroundColor Yellow

$patches = @(
    @{ 
        Name = "Auth Error Override"
        Pattern = 'ERROR_NOT_LOGGED_IN' 
        Replace = 'BYPASS_AUTH_OK'
        Critical = $true
    }
    @{ 
        Name = "Unauthenticated Flag"
        Pattern = '\[unauthenticated\]' 
        Replace = '[authenticated]'
        Critical = $true
    }
    @{ 
        Name = "Authentication Status"
        Pattern = 'isAuthenticated:!1' 
        Replace = 'isAuthenticated:!0'
        Critical = $false
    }
    @{ 
        Name = "Login Status"
        Pattern = 'isLoggedIn:!1' 
        Replace = 'isLoggedIn:!0'
        Critical = $false
    }
    @{ 
        Name = "Pro Access Check"
        Pattern = 'hasProAccess:!1' 
        Replace = 'hasProAccess:!0'
        Critical = $true
    }
    @{ 
        Name = "Pro Status"
        Pattern = 'isPro:!1' 
        Replace = 'isPro:!0'
        Critical = $true
    }
    @{ 
        Name = "Auth Requirement"
        Pattern = 'requiresAuth:!0' 
        Replace = 'requiresAuth:!1'
        Critical = $false
    }
    @{ 
        Name = "Composer Auth"
        Pattern = '"composer".*?requiresAuth:!0' 
        Replace = '"composer",requiresAuth:!1'
        Critical = $false
    }
    @{ 
        Name = "Agent Auth"
        Pattern = '"agent".*?requiresAuth:!0' 
        Replace = '"agent",requiresAuth:!1'
        Critical = $false
    }
)

$patchedCount = 0
$criticalPatched = 0

foreach ($patch in $patches) {
    $before = $content
    $content = $content -replace $patch.Pattern, $patch.Replace
    
    if ($before -ne $content) {
        Write-Host "   ✓ $($patch.Name)" -ForegroundColor Green
        $patchedCount++
        if ($patch.Critical) { $criticalPatched++ }
    } else {
        $status = if ($patch.Critical) { "⚠" } else { "·" }
        $color = if ($patch.Critical) { "Yellow" } else { "Gray" }
        Write-Host "   $status $($patch.Name) - No matches found" -ForegroundColor $color
    }
}

Write-Host "`n   📊 Patches applied: $patchedCount / $($patches.Count)" -ForegroundColor Cyan
Write-Host "   📊 Critical patches: $criticalPatched" -ForegroundColor $(if($criticalPatched -ge 2){"Green"}else{"Yellow"})

# Write patched file
Write-Host "`n[5/6] Writing patched file..." -ForegroundColor Yellow

try {
    # Remove read-only attribute if present
    if ((Get-ItemProperty $cursorJs).IsReadOnly) {
        Set-ItemProperty $cursorJs -Name IsReadOnly -Value $false
    }
    
    $content | Set-Content $cursorJs -NoNewline -Force
    $newSize = (Get-Item $cursorJs).Length
    Write-Host "   ✓ File written successfully" -ForegroundColor Green
    Write-Host "   ✓ New size: $([math]::Round($newSize / 1MB, 2)) MB" -ForegroundColor Green
} catch {
    Write-Host "   ❌ Failed to write file: $_" -ForegroundColor Red
    Write-Host "   ⚠ Restoring from backup..." -ForegroundColor Yellow
    Copy-Item $backup $cursorJs -Force
    exit 1
}

# Block OpenAI domains in hosts file
Write-Host "`n[6/6] Blocking OpenAI API (redirecting to localhost)..." -ForegroundColor Yellow

$openaiBlock = @"

# CURSOR LOCAL BYPASS - Block OpenAI API
127.0.0.1 api.openai.com
127.0.0.1 openai.com
127.0.0.1 www.openai.com
127.0.0.1 platform.openai.com
127.0.0.1 chat.openai.com
"@

try {
    $hostsContent = Get-Content $hostsFile -Raw
    
    if ($hostsContent -notmatch "CURSOR LOCAL BYPASS") {
        Add-Content -Path $hostsFile -Value $openaiBlock -Force
        Write-Host "   ✓ OpenAI domains blocked in hosts file" -ForegroundColor Green
    } else {
        Write-Host "   · OpenAI domains already blocked" -ForegroundColor Gray
    }
} catch {
    Write-Host "   ⚠ Could not modify hosts file: $_" -ForegroundColor Yellow
    Write-Host "   This is optional - Cursor will still work" -ForegroundColor Gray
}

# Verification
Write-Host "`n╔═══════════════════════════════════════════════════╗" -ForegroundColor Green
Write-Host "║              VERIFICATION                        ║" -ForegroundColor Green
Write-Host "╚═══════════════════════════════════════════════════╝`n" -ForegroundColor Green

$verifyPatterns = @(
    @{ Name = "Auth bypass"; Pattern = 'BYPASS_AUTH_OK'; Critical = $true }
    @{ Name = "Authenticated flag"; Pattern = '\[authenticated\]'; Critical = $true }
    @{ Name = "Pro access"; Pattern = 'hasProAccess:!0'; Critical = $true }
    @{ Name = "Pro status"; Pattern = 'isPro:!0'; Critical = $true }
)

$verifyPassed = 0
foreach ($check in $verifyPatterns) {
    if ($content -match $check.Pattern) {
        Write-Host "   ✓ $($check.Name): PRESENT" -ForegroundColor Green
        $verifyPassed++
    } else {
        $symbol = if($check.Critical) { "❌" } else { "⚠" }
        $color = if($check.Critical) { "Red" } else { "Yellow" }
        Write-Host "   $symbol $($check.Name): MISSING" -ForegroundColor $color
    }
}

# Final summary
Write-Host "`n╔═══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                   SUMMARY                        ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

if ($verifyPassed -ge 2) {
    Write-Host "✅ SUCCESS: Cursor authentication bypass applied!" -ForegroundColor Green
    Write-Host "✅ Verification: $verifyPassed/$($verifyPatterns.Count) checks passed" -ForegroundColor Green
    Write-Host "✅ Backup saved: $backup" -ForegroundColor Green
    
    Write-Host "`n📝 What happens now:" -ForegroundColor Yellow
    Write-Host "   1. Cursor will think you're authenticated" -ForegroundColor White
    Write-Host "   2. Pro features (Composer, Agent) will be enabled" -ForegroundColor White
    Write-Host "   3. OpenAI API calls are blocked (hosts file)" -ForegroundColor White
    Write-Host "   4. You need to redirect API calls to localhost:11434" -ForegroundColor White
    
    Write-Host "`n🔄 Next steps:" -ForegroundColor Yellow
    Write-Host "   1. Restart Cursor" -ForegroundColor White
    Write-Host "   2. Try using Composer or Agent" -ForegroundColor White
    Write-Host "   3. Monitor logs for 'authenticated: true'" -ForegroundColor White
    Write-Host "   4. Set up API redirect to Ollama (I can help with this)" -ForegroundColor White
    
    $success = $true
} else {
    Write-Host "⚠ WARNING: Bypass may be incomplete" -ForegroundColor Yellow
    Write-Host "⚠ Verification: Only $verifyPassed/$($verifyPatterns.Count) checks passed" -ForegroundColor Yellow
    Write-Host "⚠ Cursor might still show authentication errors" -ForegroundColor Yellow
    
    Write-Host "`n📝 Possible reasons:" -ForegroundColor Yellow
    Write-Host "   - Cursor version is too new (different code structure)" -ForegroundColor White
    Write-Host "   - File was already patched (re-patching)" -ForegroundColor White
    Write-Host "   - Cursor uses different authentication method" -ForegroundColor White
    
    Write-Host "`n🔄 What to try:" -ForegroundColor Yellow
    Write-Host "   1. Restore backup: Copy-Item '$backup' '$cursorJs' -Force" -ForegroundColor White
    Write-Host "   2. Use asar unpacking method (see documentation)" -ForegroundColor White
    Write-Host "   3. Check Cursor version: Help > About" -ForegroundColor White
    
    $success = $false
}

# Restart prompt
Write-Host "`n╔═══════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║              RESTART REQUIRED                    ║" -ForegroundColor Magenta
Write-Host "╚═══════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

$restart = Read-Host "Restart Cursor now? (Y/N)"

if ($restart -eq 'Y' -or $restart -eq 'y') {
    Write-Host "`nRestarting Cursor..." -ForegroundColor Cyan
    
    # Kill all Cursor processes
    Stop-Process -Name "Cursor" -Force -ErrorAction SilentlyContinue
    Start-Sleep 2
    
    # Start Cursor
    Start-Process "$cursorDir\Cursor.exe"
    
    Write-Host "✅ Cursor restarted" -ForegroundColor Green
    Write-Host "`n📊 Monitor the console output for:" -ForegroundColor Yellow
    Write-Host "   - Look for: 'authenticated: true'" -ForegroundColor White
    Write-Host "   - Look for: 'hasProAccess: true'" -ForegroundColor White
    Write-Host "   - Should NOT see: 'ERROR_NOT_LOGGED_IN'" -ForegroundColor White
} else {
    Write-Host "`n⚠ Remember to restart Cursor manually for changes to take effect" -ForegroundColor Yellow
}

Write-Host "`n╔═══════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    DONE                          ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

if ($success) {
    Write-Host "🎉 Bypass complete! Check Cursor logs to verify." -ForegroundColor Green
} else {
    Write-Host "⚠ Bypass incomplete. See troubleshooting steps above." -ForegroundColor Yellow
}

# Save log
$logFile = "$cursorDir\bypass_log_$timestamp.txt"
@"
Cursor Authentication Bypass Log
Generated: $(Get-Date)

Cursor Directory: $cursorDir
JavaScript File: $cursorJs
Backup File: $backup

Patches Applied: $patchedCount / $($patches.Count)
Critical Patches: $criticalPatched
Verification Passed: $verifyPassed / $($verifyPatterns.Count)

Status: $(if($success){"SUCCESS"}else{"INCOMPLETE"})
"@ | Set-Content $logFile

Write-Host "`n📄 Log saved: $logFile" -ForegroundColor Gray
