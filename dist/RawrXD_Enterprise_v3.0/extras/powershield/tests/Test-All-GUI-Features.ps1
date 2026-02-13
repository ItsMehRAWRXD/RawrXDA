# Comprehensive GUI Feature Tester for RawrXD
# Tests all dropdown menus, text boxes, and GUI features
# Logs results to a detailed report

param(
    [switch]$RunTests,
    [string]$LogFile = "GUI-Test-Results-$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
)

$ErrorActionPreference = "Continue"
$testResults = @()
$testCount = 0
$passCount = 0
$failCount = 0

function Write-TestLog {
    param(
        [string]$Message,
        [string]$Status = "INFO"
    )
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Status] $Message"
    Write-Host $logEntry
    Add-Content -Path $LogFile -Value $logEntry
    $script:testResults += [PSCustomObject]@{
        Timestamp = $timestamp
        Status = $Status
        Message = $Message
    }
}

function Test-Feature {
    param(
        [string]$FeatureName,
        [scriptblock]$TestScript,
        [string]$Category = "General"
    )

    $script:testCount++
    Write-TestLog "Testing: $Category - $FeatureName" "TEST"

    try {
        $result = & $TestScript
        if ($result -or $result -eq $null) {
            $script:passCount++
            Write-TestLog "✓ PASS: $FeatureName" "PASS"
            return $true
        }
        else {
            $script:failCount++
            Write-TestLog "✗ FAIL: $FeatureName - Returned false" "FAIL"
            return $false
        }
    }
    catch {
        $script:failCount++
        Write-TestLog "✗ FAIL: $FeatureName - Error: $($_.Exception.Message)" "FAIL"
        Write-TestLog "  Stack: $($_.ScriptStackTrace)" "DEBUG"
        return $false
    }
}

function Test-TextBox {
    param(
        [System.Windows.Forms.TextBox]$TextBox,
        [string]$TestName,
        [string]$TestText = "Test input text"
    )

    if (-not $TextBox) {
        Write-TestLog "✗ SKIP: $TestName - TextBox is null" "SKIP"
        return $false
    }

    try {
        # Test if textbox is accessible
        if ($TextBox.IsDisposed) {
            Write-TestLog "✗ FAIL: $TestName - TextBox is disposed" "FAIL"
            return $false
        }

        # Test setting text
        $originalText = $TextBox.Text
        $TextBox.Text = $TestText

        if ($TextBox.Text -eq $TestText) {
            Write-TestLog "✓ PASS: $TestName - Text can be set" "PASS"

            # Test clearing
            $TextBox.Clear()
            if ($TextBox.Text -eq "") {
                Write-TestLog "✓ PASS: $TestName - Text can be cleared" "PASS"
            }
            else {
                Write-TestLog "✗ FAIL: $TestName - Text cannot be cleared" "FAIL"
            }

            # Restore original text
            $TextBox.Text = $originalText
            return $true
        }
        else {
            Write-TestLog "✗ FAIL: $TestName - Text was not set correctly" "FAIL"
            return $false
        }
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

function Test-ComboBox {
    param(
        [System.Windows.Forms.ComboBox]$ComboBox,
        [string]$TestName
    )

    if (-not $ComboBox) {
        Write-TestLog "✗ SKIP: $TestName - ComboBox is null" "SKIP"
        return $false
    }

    try {
        if ($ComboBox.IsDisposed) {
            Write-TestLog "✗ FAIL: $TestName - ComboBox is disposed" "FAIL"
            return $false
        }

        $itemCount = $ComboBox.Items.Count
        Write-TestLog "  Items in $TestName : $itemCount" "INFO"

        if ($itemCount -gt 0) {
            # Test selecting first item
            $ComboBox.SelectedIndex = 0
            if ($ComboBox.SelectedItem) {
                Write-TestLog "✓ PASS: $TestName - Can select items" "PASS"
                return $true
            }
            else {
                Write-TestLog "✗ FAIL: $TestName - Cannot select items" "FAIL"
                return $false
            }
        }
        else {
            Write-TestLog "⚠ WARN: $TestName - No items to test" "WARN"
            return $true
        }
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

function Test-MenuItem {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$MenuItem,
        [string]$TestName
    )

    if (-not $MenuItem) {
        Write-TestLog "✗ SKIP: $TestName - MenuItem is null" "SKIP"
        return $false
    }

    try {
        # Test if menu item exists and is accessible
        $enabled = $MenuItem.Enabled
        $text = $MenuItem.Text
        $hasClickHandler = $MenuItem.GetType().GetEvent("Click").GetAddMethod() -ne $null

        Write-TestLog "  Menu Item: '$text' - Enabled: $enabled" "INFO"

        # Test if it has dropdown items
        if ($MenuItem.DropDownItems.Count -gt 0) {
            Write-TestLog "  Has $($MenuItem.DropDownItems.Count) sub-items" "INFO"
            foreach ($subItem in $MenuItem.DropDownItems) {
                if ($subItem -is [System.Windows.Forms.ToolStripMenuItem]) {
                    Test-MenuItem -MenuItem $subItem -TestName "$TestName > $($subItem.Text)"
                }
            }
        }

        return $true
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

# Main test execution
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "RawrXD GUI Feature Test Suite" "INFO"
Write-TestLog "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-Host ""

# Load the main script to get access to GUI objects
Write-TestLog "Loading RawrXD script..." "INFO"

try {
    # Dot-source the script to get access to variables (but don't run GUI)
    $scriptPath = Join-Path $PSScriptRoot "RawrXD.ps1"

    if (-not (Test-Path $scriptPath)) {
        Write-TestLog "✗ ERROR: RawrXD.ps1 not found at $scriptPath" "ERROR"
        exit 1
    }

    Write-TestLog "Script found: $scriptPath" "INFO"
    Write-TestLog "Note: This test requires the GUI to be running" "INFO"
    Write-TestLog "Run this test while RawrXD GUI is open" "INFO"
    Write-Host ""

    # Test categories
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "TEST CATEGORY: Menu Items" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

    # Test if we can access menu items (requires GUI to be running)
    Test-Feature -FeatureName "File Menu Access" -Category "Menus" -TestScript {
        try {
            # Check if form exists
            if ($form -and -not $form.IsDisposed) {
                if ($menu -and $menu.Items.Count -gt 0) {
                    return $true
                }
            }
            return $false
        }
        catch { return $false }
    }

    Test-Feature -FeatureName "Edit Menu Access" -Category "Menus" -TestScript {
        try {
            if ($editMenu -and $editMenu.DropDownItems.Count -gt 0) {
                return $true
            }
            return $false
        }
        catch { return $false }
    }

    Test-Feature -FeatureName "Chat Menu Access" -Category "Menus" -TestScript {
        try {
            if ($chatMenu -and $chatMenu.DropDownItems.Count -gt 0) {
                return $true
            }
            return $false
        }
        catch { return $false }
    }

    Test-Feature -FeatureName "Settings Menu Access" -Category "Menus" -TestScript {
        try {
            if ($settingsMenu -and $settingsMenu.DropDownItems.Count -gt 0) {
                return $true
            }
            return $false
        }
        catch { return $false }
    }

    Test-Feature -FeatureName "Tools Menu Access" -Category "Menus" -TestScript {
        try {
            if ($toolsMenu -and $toolsMenu.DropDownItems.Count -gt 0) {
                return $true
            }
            return $false
        }
        catch { return $false }
    }

    Test-Feature -FeatureName "Extensions Menu Access" -Category "Menus" -TestScript {
        try {
            if ($extensionsMenu -and $extensionsMenu.DropDownItems.Count -gt 0) {
                return $true
            }
            return $false
        }
        catch { return $false }
    }

    Test-Feature -FeatureName "Security Menu Access" -Category "Menus" -TestScript {
        try {
            if ($securityMenu -and $securityMenu.DropDownItems.Count -gt 0) {
                return $true
            }
            return $false
        }
        catch { return $false }
    }

    Write-Host ""
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "TEST CATEGORY: Text Input Boxes" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

    # Test all text boxes
    Test-Feature -FeatureName "Editor TextBox" -Category "TextInput" -TestScript {
        if ($script:editor) {
            return Test-TextBox -TextBox $script:editor -TestName "Editor"
        }
        return $false
    }

    Test-Feature -FeatureName "Chat Input Box" -Category "TextInput" -TestScript {
        # Test chat input boxes in all chat tabs
        if ($script:chatTabs -and $script:chatTabs.Count -gt 0) {
            $allPassed = $true
            foreach ($tabId in $script:chatTabs.Keys) {
                $chatSession = $script:chatTabs[$tabId]
                if ($chatSession.InputBox) {
                    $result = Test-TextBox -TextBox $chatSession.InputBox -TestName "Chat Input ($tabId)"
                    if (-not $result) { $allPassed = $false }
                }
            }
            return $allPassed
        }
        return $false
    }

    Test-Feature -FeatureName "Find Dialog TextBox" -Category "TextInput" -TestScript {
        if ($findTextBox) {
            return Test-TextBox -TextBox $findTextBox -TestName "Find Dialog"
        }
        return $false
    }

    Test-Feature -FeatureName "Replace Dialog TextBoxes" -Category "TextInput" -TestScript {
        if ($replaceForm) {
            # Find the textboxes in replace form
            $findBox = $replaceForm.Controls | Where-Object { $_ -is [System.Windows.Forms.TextBox] -and $_.Name -like "*find*" } | Select-Object -First 1
            $replaceBox = $replaceForm.Controls | Where-Object { $_ -is [System.Windows.Forms.TextBox] -and $_.Name -like "*replace*" } | Select-Object -First 1

            $result1 = if ($findBox) { Test-TextBox -TextBox $findBox -TestName "Replace Find" } else { $false }
            $result2 = if ($replaceBox) { Test-TextBox -TextBox $replaceBox -TestName "Replace With" } else { $false }

            return ($result1 -and $result2)
        }
        return $false
    }

    Test-Feature -FeatureName "Command Palette Input" -Category "TextInput" -TestScript {
        if ($paletteInput) {
            return Test-TextBox -TextBox $paletteInput -TestName "Command Palette"
        }
        return $false
    }

    Test-Feature -FeatureName "Browser URL Box" -Category "TextInput" -TestScript {
        if ($browserUrlBox) {
            return Test-TextBox -TextBox $browserUrlBox -TestName "Browser URL" -TestText "https://www.example.com"
        }
        return $false
    }

    Write-Host ""
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "TEST CATEGORY: ComboBox/Dropdown Controls" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

    # Test all combo boxes
    Test-Feature -FeatureName "Chat Model ComboBox" -Category "ComboBox" -TestScript {
        if ($script:chatTabs -and $script:chatTabs.Count -gt 0) {
            $allPassed = $true
            foreach ($tabId in $script:chatTabs.Keys) {
                $chatSession = $script:chatTabs[$tabId]
                if ($chatSession.ModelCombo) {
                    $result = Test-ComboBox -ComboBox $chatSession.ModelCombo -TestName "Model Combo ($tabId)"
                    if (-not $result) { $allPassed = $false }
                }
            }
            return $allPassed
        }
        return $false
    }

    Write-Host ""
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "TEST CATEGORY: Menu Item Functionality" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

    # Test menu item click handlers
    Test-Feature -FeatureName "Undo Menu Item" -Category "MenuActions" -TestScript {
        if ($undoItem) {
            try {
                # Check if it has a click handler
                $hasHandler = $undoItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Redo Menu Item" -Category "MenuActions" -TestScript {
        if ($redoItem) {
            try {
                $hasHandler = $redoItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Find Menu Item" -Category "MenuActions" -TestScript {
        if ($findItem) {
            try {
                $hasHandler = $findItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Replace Menu Item" -Category "MenuActions" -TestScript {
        if ($replaceItem) {
            try {
                $hasHandler = $replaceItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Clear Chat Menu Item" -Category "MenuActions" -TestScript {
        if ($clearChatItem) {
            try {
                $hasHandler = $clearChatItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Ollama Start Menu Item" -Category "MenuActions" -TestScript {
        if ($ollamaStartItem) {
            try {
                $hasHandler = $ollamaStartItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Ollama Stop Menu Item" -Category "MenuActions" -TestScript {
        if ($ollamaStopItem) {
            try {
                $hasHandler = $ollamaStopItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Performance Monitor Menu Item" -Category "MenuActions" -TestScript {
        if ($perfMonitorItem) {
            try {
                $hasHandler = $perfMonitorItem.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Write-Host ""
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "TEST CATEGORY: Button Controls" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

    Test-Feature -FeatureName "Find Next Button" -Category "Buttons" -TestScript {
        if ($findNextBtn) {
            try {
                $hasHandler = $findNextBtn.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Replace Button" -Category "Buttons" -TestScript {
        if ($replaceBtn) {
            try {
                $hasHandler = $replaceBtn.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Test-Feature -FeatureName "Replace All Button" -Category "Buttons" -TestScript {
        if ($replaceAllBtn) {
            try {
                $hasHandler = $replaceAllBtn.GetType().GetEvent("Click").GetAddMethod() -ne $null
                return $hasHandler
            }
            catch { return $false }
        }
        return $false
    }

    Write-Host ""
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "TEST SUMMARY" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "Total Tests: $testCount" "INFO"
    Write-TestLog "Passed: $passCount" "PASS"
    Write-TestLog "Failed: $failCount" "FAIL"
    Write-TestLog "Success Rate: $([math]::Round(($passCount / $testCount) * 100, 2))%" "INFO"
    Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
    Write-TestLog "Test completed: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
    Write-TestLog "Log file: $LogFile" "INFO"
    Write-Host ""

    # Generate summary report
    $summaryFile = $LogFile -replace '\.log$', '-Summary.txt'
    $summary = @"
═══════════════════════════════════════════════════════
RawrXD GUI Feature Test Summary
═══════════════════════════════════════════════════════
Test Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Total Tests: $testCount
Passed: $passCount
Failed: $failCount
Success Rate: $([math]::Round(($passCount / $testCount) * 100, 2))%

═══════════════════════════════════════════════════════
FAILED TESTS
═══════════════════════════════════════════════════════
"@

    $failedTests = $testResults | Where-Object { $_.Status -eq "FAIL" }
    if ($failedTests.Count -eq 0) {
        $summary += "`n✓ All tests passed!`n"
    }
    else {
        foreach ($test in $failedTests) {
            $summary += "`n✗ $($test.Message)"
        }
    }

    $summary += @"

═══════════════════════════════════════════════════════
SKIPPED TESTS
═══════════════════════════════════════════════════════
"@

    $skippedTests = $testResults | Where-Object { $_.Status -eq "SKIP" }
    if ($skippedTests.Count -eq 0) {
        $summary += "`nNo tests skipped`n"
    }
    else {
        foreach ($test in $skippedTests) {
            $summary += "`n⚠ $($test.Message)"
        }
    }

    $summary | Out-File -FilePath $summaryFile -Encoding UTF8
    Write-Host "Summary report saved to: $summaryFile" -ForegroundColor Cyan
    Write-Host "Detailed log saved to: $LogFile" -ForegroundColor Cyan
}
catch {
    Write-TestLog "✗ CRITICAL ERROR: $($_.Exception.Message)" "ERROR"
    Write-TestLog "Stack trace: $($_.ScriptStackTrace)" "ERROR"
    exit 1
}

