# Fix-AmazonQ-Connection.ps1
# Fixes Amazon Q extension connection error (ECONNREFUSED 127.0.0.1:443)
# Also configures GitHub Copilot extension

param(
    [switch]$FixProxy,
    [switch]$AddCopilot,
    [switch]$ShowStatus
)

$CursorSettingsPath = "$env:APPDATA\Cursor\User\settings.json"
$VSCodeSettingsPath = "$env:APPDATA\Code\User\settings.json"
$WorkspaceSettingsPath = ".vscode\settings.json"

Write-Host "🔧 Amazon Q & GitHub Copilot Configuration Fix" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Gray

# Function to update settings file
function Update-SettingsFile {
    param(
        [string]$SettingsPath,
        [string]$Description
    )
    
    if (-not (Test-Path $SettingsPath)) {
        Write-Host "⚠️  Settings file not found: $SettingsPath" -ForegroundColor Yellow
        return $false
    }
    
    Write-Host "`n📝 Updating $Description..." -ForegroundColor Cyan
    
    try {
        $settings = Get-Content $SettingsPath -Raw | ConvertFrom-Json
        
        # Fix proxy setting that causes Amazon Q connection issues
        if ($FixProxy -and $settings.'aiSuite.simple.provider' -eq "proxy") {
            Write-Host "  🔧 Removing problematic proxy setting..." -ForegroundColor Yellow
            $settings.PSObject.Properties.Remove('aiSuite.simple.provider')
            Write-Host "  ✅ Removed 'aiSuite.simple.provider' setting" -ForegroundColor Green
        }
        
        # Ensure Amazon Q settings are properly configured
        if (-not $settings.'amazonQ.telemetry') {
            $settings | Add-Member -MemberType NoteProperty -Name 'amazonQ.telemetry' -Value $true -Force
        }
        
        if (-not $settings.'amazonQ.workspaceIndex') {
            $settings | Add-Member -MemberType NoteProperty -Name 'amazonQ.workspaceIndex' -Value $true -Force
        }
        
        # Add GitHub Copilot settings if requested
        if ($AddCopilot) {
            Write-Host "  🔧 Configuring GitHub Copilot..." -ForegroundColor Yellow
            
            # GitHub Copilot settings
            if (-not $settings.'github.copilot.enable') {
                $settings | Add-Member -MemberType NoteProperty -Name 'github.copilot.enable' -Value @{
                    "*" = $true
                } -Force
            }
            
            if (-not $settings.'github.copilot.editor.enableAutoCompletions') {
                $settings | Add-Member -MemberType NoteProperty -Name 'github.copilot.editor.enableAutoCompletions' -Value $true -Force
            }
            
            if (-not $settings.'github.copilot.chat.enabled') {
                $settings | Add-Member -MemberType NoteProperty -Name 'github.copilot.chat.enabled' -Value $true -Force
            }
            
            Write-Host "  ✅ GitHub Copilot settings configured" -ForegroundColor Green
        }
        
        # Save updated settings
        $settings | ConvertTo-Json -Depth 20 | Set-Content $SettingsPath -Encoding UTF8
        Write-Host "  ✅ Settings file updated successfully" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Host "  ❌ Error updating settings: $_" -ForegroundColor Red
        return $false
    }
}

# Show current status
if ($ShowStatus) {
    Write-Host "`n📊 Current Configuration Status:" -ForegroundColor Cyan
    
    if (Test-Path $CursorSettingsPath) {
        $cursorSettings = Get-Content $CursorSettingsPath -Raw | ConvertFrom-Json
        Write-Host "`n  Cursor Settings:" -ForegroundColor Yellow
        Write-Host "    Amazon Q Telemetry: $($cursorSettings.'amazonQ.telemetry')" -ForegroundColor Gray
        Write-Host "    Amazon Q Workspace Index: $($cursorSettings.'amazonQ.workspaceIndex')" -ForegroundColor Gray
        Write-Host "    Proxy Setting: $($cursorSettings.'aiSuite.simple.provider')" -ForegroundColor $(if ($cursorSettings.'aiSuite.simple.provider' -eq "proxy") { "Red" } else { "Green" })
        Write-Host "    GitHub Copilot Enabled: $($cursorSettings.'github.copilot.enable')" -ForegroundColor Gray
    }
    
    return
}

# Update Cursor settings
if (Test-Path $CursorSettingsPath) {
    Update-SettingsFile -SettingsPath $CursorSettingsPath -Description "Cursor User Settings"
}

# Update VS Code settings
if (Test-Path $VSCodeSettingsPath) {
    Update-SettingsFile -SettingsPath $VSCodeSettingsPath -Description "VS Code User Settings"
}

# Update workspace settings
if (Test-Path $WorkspaceSettingsPath) {
    Update-SettingsFile -SettingsPath $WorkspaceSettingsPath -Description "Workspace Settings"
}

Write-Host "`n✅ Configuration fix complete!" -ForegroundColor Green
Write-Host "`n📋 Next Steps:" -ForegroundColor Cyan
Write-Host "  1. Restart Cursor/VS Code" -ForegroundColor Yellow
Write-Host "  2. Sign in to Amazon Q (if not already signed in)" -ForegroundColor Yellow
Write-Host "  3. Sign in to GitHub Copilot (if not already signed in)" -ForegroundColor Yellow
Write-Host "  4. Run this script with -ShowStatus to verify configuration" -ForegroundColor Yellow

