<#
.SYNOPSIS
    Settings-related CLI command handlers
.DESCRIPTION
    Handles get-settings and set-setting commands
#>

function Invoke-GetSettingsHandler {
    param([string]$SettingName)
    
    if ($SettingName) {
        if (-not (Invoke-CliGetSettings -SettingName $SettingName)) { return 1 }
    }
    else {
        if (-not (Invoke-CliGetSettings)) { return 1 }
    }
    return 0
}

function Invoke-SetSettingHandler {
    param(
        [string]$SettingName,
        [string]$SettingValue
    )
    
    if (-not $SettingName -or -not $SettingValue) {
        Write-Host "Error: -SettingName and -SettingValue are required for set-setting command" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command set-setting -SettingName <name> -SettingValue <value>" -ForegroundColor Yellow
        return 1
    }
    
    if (-not (Invoke-CliSetSetting -SettingName $SettingName -SettingValue $SettingValue)) { return 1 }
    return 0
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
    'Invoke-GetSettingsHandler',
    'Invoke-SetSettingHandler'
)






































