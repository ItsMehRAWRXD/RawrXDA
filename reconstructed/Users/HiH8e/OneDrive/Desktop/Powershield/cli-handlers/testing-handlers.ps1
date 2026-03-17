<#
.SYNOPSIS
    Testing-related CLI command handlers
.DESCRIPTION
    Handles test-editor-settings, test-file-operations, test-settings-persistence, 
    test-all-features, test-gui, test-gui-interactive, and test-dropdowns commands
#>

function Invoke-TestEditorSettingsHandler {
    if (-not (Invoke-CliTestEditorSettings)) { return 1 }
    return 0
}

function Invoke-TestFileOperationsHandler {
    if (-not (Invoke-CliTestFileOperations)) { return 1 }
    return 0
}

function Invoke-TestSettingsPersistenceHandler {
    Write-Host "`n=== Settings Persistence Test ===" -ForegroundColor Cyan
    try {
        $originalFontSize = $global:settings.EditorFontSize
        Invoke-CliSetSetting -SettingName "EditorFontSize" -SettingValue "14"
        Start-Sleep -Milliseconds 100
        Invoke-CliGetSettings -SettingName "EditorFontSize"
        if ($global:settings.EditorFontSize -eq 14) {
            Write-Host "✓ Settings persistence works" -ForegroundColor Green
            Invoke-CliSetSetting -SettingName "EditorFontSize" -SettingValue $originalFontSize
            return 0
        }
        else {
            Write-Host "✗ Settings did not persist" -ForegroundColor Red
            return 1
        }
    }
    catch {
        Write-Host "✗ Error: $_" -ForegroundColor Red
        return 1
    }
}

function Invoke-TestAllFeaturesHandler {
    if (-not (Invoke-CliTestAllFeatures)) { return 1 }
    return 0
}

function Invoke-TestGUIHandler {
    Write-Host "`n=== GUI Feature Tester ===" -ForegroundColor Cyan
    Write-Host "Running comprehensive GUI tests..." -ForegroundColor Yellow
    Write-Host ""
    
    if (Test-Path "Test-All-GUI-Features.ps1") {
        & ".\Test-All-GUI-Features.ps1" -RunTests
        return 0
    }
    else {
        Write-Host "Error: Test-All-GUI-Features.ps1 not found" -ForegroundColor Red
        Write-Host "Please ensure the test script is in the same directory" -ForegroundColor Yellow
        return 1
    }
}

function Invoke-TestGUIInteractiveHandler {
    Write-Host "`n=== Interactive GUI Tester ===" -ForegroundColor Cyan
    Write-Host "This will test GUI features while the IDE is running" -ForegroundColor Yellow
    Write-Host ""
    
    if (Test-Path "Test-GUI-Interactive.ps1") {
        & ".\Test-GUI-Interactive.ps1" -AutoTest
        return 0
    }
    else {
        Write-Host "Error: Test-GUI-Interactive.ps1 not found" -ForegroundColor Red
        Write-Host "Please ensure the test script is in the same directory" -ForegroundColor Yellow
        return 1
    }
}

function Invoke-TestDropdownsHandler {
    Write-Host "`n=== Dropdown Feature Tester ===" -ForegroundColor Cyan
    Write-Host "Testing all dropdown menus and combo boxes..." -ForegroundColor Yellow
    Write-Host ""
    
    if (Test-Path "Test-Dropdown-Features.ps1") {
        & ".\Test-Dropdown-Features.ps1" -RunAll
        return 0
    }
    else {
        Write-Host "Error: Test-Dropdown-Features.ps1 not found" -ForegroundColor Red
        Write-Host "Please ensure the test script is in the same directory" -ForegroundColor Yellow
        return 1
    }
}

Export-ModuleMember -Function @(
    'Invoke-TestEditorSettingsHandler',
    'Invoke-TestFileOperationsHandler',
    'Invoke-TestSettingsPersistenceHandler',
    'Invoke-TestAllFeaturesHandler',
    'Invoke-TestGUIHandler',
    'Invoke-TestGUIInteractiveHandler',
    'Invoke-TestDropdownsHandler'
)
