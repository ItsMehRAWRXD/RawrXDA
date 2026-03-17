# Fix VS Code Extensions - Recycle Bin Permissions Issue
# Prevents extensions from accessing protected Windows directories

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "VS Code Extension Permissions Fix" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Protected directories that should be excluded
$protectedDirs = @(
    '$RECYCLE.BIN',
    'System Volume Information',
    'Recovery',
    'ProgramData\Microsoft\Windows\WER'
)

Write-Host "[INFO] Protected directories:" -ForegroundColor Yellow
$protectedDirs | ForEach-Object { Write-Host "  - $_" -ForegroundColor Gray }
Write-Host ""

# VS Code settings location
$vscodeSettingsPath = "$env:APPDATA\Code\User\settings.json"
$cursorSettingsPath = "$env:APPDATA\Cursor\User\settings.json"

function Add-ExclusionPatterns {
    param([string]$settingsPath)
    
    if (-not (Test-Path $settingsPath)) {
        Write-Host "[WARN] Settings file not found: $settingsPath" -ForegroundColor Yellow
        return
    }
    
    Write-Host "[INFO] Reading settings: $settingsPath" -ForegroundColor Cyan
    
    try {
        $settings = Get-Content $settingsPath -Raw | ConvertFrom-Json
        
        # Add files.exclude patterns
        if (-not $settings.'files.exclude') {
            $settings | Add-Member -MemberType NoteProperty -Name 'files.exclude' -Value @{} -Force
        }
        
        $protectedDirs | ForEach-Object {
            $pattern = "**/$_/**"
            if (-not $settings.'files.exclude'.$pattern) {
                $settings.'files.exclude' | Add-Member -MemberType NoteProperty -Name $pattern -Value $true -Force
                Write-Host "  [+] Added exclusion: $pattern" -ForegroundColor Green
            }
        }
        
        # Add search.exclude patterns
        if (-not $settings.'search.exclude') {
            $settings | Add-Member -MemberType NoteProperty -Name 'search.exclude' -Value @{} -Force
        }
        
        $protectedDirs | ForEach-Object {
            $pattern = "**/$_/**"
            if (-not $settings.'search.exclude'.$pattern) {
                $settings.'search.exclude' | Add-Member -MemberType NoteProperty -Name $pattern -Value $true -Force
                Write-Host "  [+] Added search exclusion: $pattern" -ForegroundColor Green
            }
        }
        
        # Save settings
        $settings | ConvertTo-Json -Depth 10 | Set-Content $settingsPath
        Write-Host "[SUCCESS] Settings updated!" -ForegroundColor Green
        
    } catch {
        Write-Host "[ERROR] Failed to update settings: $_" -ForegroundColor Red
    }
}

# Update VS Code settings
Write-Host ""
Write-Host "[STEP 1] Updating VS Code settings..." -ForegroundColor Cyan
Add-ExclusionPatterns -settingsPath $vscodeSettingsPath

# Update Cursor settings
Write-Host ""
Write-Host "[STEP 2] Updating Cursor settings..." -ForegroundColor Cyan
Add-ExclusionPatterns -settingsPath $cursorSettingsPath

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "ADDITIONAL RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Restart VS Code / Cursor" -ForegroundColor Yellow
Write-Host "2. If issues persist, run VS Code as Administrator" -ForegroundColor Yellow
Write-Host "3. Check extension settings for custom file scanning" -ForegroundColor Yellow
Write-Host ""
Write-Host "[DONE] Permissions fix applied!" -ForegroundColor Green
Write-Host ""
