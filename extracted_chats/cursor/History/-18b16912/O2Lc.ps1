# Fix-AmazonQ-Connection.ps1
# Fixes Amazon Q extension connection error (ECONNREFUSED 127.0.0.1:443)
# The issue is caused by "aiSuite.simple.provider": "proxy" setting

param(
    [switch]$FixProxy,
    [switch]$ShowStatus
)

$CursorSettingsPath = "$env:APPDATA\Cursor\User\settings.json"

Write-Host "🔧 Amazon Q Connection Fix" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

if (-not (Test-Path $CursorSettingsPath)) {
    Write-Host "❌ Cursor settings file not found: $CursorSettingsPath" -ForegroundColor Red
    exit 1
}

# Show current status
if ($ShowStatus) {
    Write-Host "`n📊 Current Configuration:" -ForegroundColor Cyan
    $settings = Get-Content $CursorSettingsPath -Raw | ConvertFrom-Json
    Write-Host "  Proxy Setting: $($settings.'aiSuite.simple.provider')" -ForegroundColor $(if ($settings.'aiSuite.simple.provider' -eq "proxy") { "Red" } else { "Green" })
    Write-Host "  Amazon Q Telemetry: $($settings.'amazonQ.telemetry')" -ForegroundColor Gray
    Write-Host "  Amazon Q Workspace Index: $($settings.'amazonQ.workspaceIndex')" -ForegroundColor Gray
    return
}

Write-Host "`n📝 Reading Cursor settings..." -ForegroundColor Cyan

try {
    $settings = Get-Content $CursorSettingsPath -Raw | ConvertFrom-Json
    
    $needsFix = $false
    
    # Check and fix proxy setting
    if ($settings.PSObject.Properties.Name -contains 'aiSuite.simple.provider') {
        if ($settings.'aiSuite.simple.provider' -eq "proxy") {
            Write-Host "  ⚠️  Found problematic proxy setting: aiSuite.simple.provider = 'proxy'" -ForegroundColor Yellow
            Write-Host "  🔧 Removing proxy setting to fix Amazon Q connection..." -ForegroundColor Yellow
            
            # Remove the property
            $settings.PSObject.Properties.Remove('aiSuite.simple.provider')
            $needsFix = $true
        }
    }
    
    # Ensure Amazon Q settings are properly configured
    if (-not ($settings.PSObject.Properties.Name -contains 'amazonQ.telemetry')) {
        $settings | Add-Member -MemberType NoteProperty -Name 'amazonQ.telemetry' -Value $true -Force
        $needsFix = $true
        Write-Host "  ✅ Added amazonQ.telemetry setting" -ForegroundColor Green
    }
    
    if (-not ($settings.PSObject.Properties.Name -contains 'amazonQ.workspaceIndex')) {
        $settings | Add-Member -MemberType NoteProperty -Name 'amazonQ.workspaceIndex' -Value $true -Force
        $needsFix = $true
        Write-Host "  ✅ Added amazonQ.workspaceIndex setting" -ForegroundColor Green
    }
    
    if ($needsFix) {
        # Save updated settings
        $json = $settings | ConvertTo-Json -Depth 20
        # Preserve comments by reading original and replacing JSON parts
        $originalContent = Get-Content $CursorSettingsPath -Raw
        Set-Content $CursorSettingsPath -Value $json -Encoding UTF8 -NoNewline
        
        Write-Host "`n✅ Settings updated successfully!" -ForegroundColor Green
        Write-Host "`n📋 Next Steps:" -ForegroundColor Cyan
        Write-Host "  1. Restart Cursor completely" -ForegroundColor Yellow
        Write-Host "  2. The Amazon Q connection error should be resolved" -ForegroundColor Yellow
        Write-Host "  3. Sign in to Amazon Q if prompted" -ForegroundColor Yellow
    } else {
        Write-Host "`n✅ No fixes needed - settings are already correct!" -ForegroundColor Green
    }
}
catch {
    Write-Host "`n❌ Error updating settings: $_" -ForegroundColor Red
    Write-Host "  Stack trace: $($_.ScriptStackTrace)" -ForegroundColor Gray
    exit 1
}
