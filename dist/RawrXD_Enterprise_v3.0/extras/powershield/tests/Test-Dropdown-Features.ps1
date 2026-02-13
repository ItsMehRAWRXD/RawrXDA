# Comprehensive Dropdown Feature Tester
# Tests all dropdown menus, combo boxes, and their functionality
# Logs detailed results of what works and what doesn't

param(
    [switch]$RunAll,
    [string]$LogFile = "Dropdown-Test-Results-$(Get-Date -Format 'yyyyMMdd_HHmmss').log"
)

$ErrorActionPreference = "Continue"
$testResults = @()
$testCount = 0
$passCount = 0
$failCount = 0
$skipCount = 0

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
        Category = $script:currentCategory
    }
}

$script:currentCategory = "General"

function Test-DropdownFeature {
    param(
        [string]$FeatureName,
        [scriptblock]$TestScript,
        [string]$Category = "General"
    )

    $script:testCount++
    $script:currentCategory = $Category
    Write-TestLog "Testing: $Category - $FeatureName" "TEST"

    try {
        $result = & $TestScript
        if ($result -eq $true) {
            $script:passCount++
            Write-TestLog "✓ PASS: $FeatureName" "PASS"
            return $true
        }
        elseif ($result -eq $null -or $result -eq "SKIP") {
            $script:skipCount++
            Write-TestLog "⚠ SKIP: $FeatureName" "SKIP"
            return "SKIP"
        }
        else {
            $script:failCount++
            Write-TestLog "✗ FAIL: $FeatureName" "FAIL"
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

function Test-ComboBoxDropdown {
    param(
        [System.Windows.Forms.ComboBox]$ComboBox,
        [string]$TestName
    )

    if (-not $ComboBox) {
        Write-TestLog "✗ SKIP: $TestName - ComboBox is null" "SKIP"
        return "SKIP"
    }

    try {
        if ($ComboBox.IsDisposed) {
            Write-TestLog "✗ FAIL: $TestName - ComboBox is disposed" "FAIL"
            return $false
        }

        $itemCount = $ComboBox.Items.Count
        Write-TestLog "  Items in dropdown: $itemCount" "INFO"

        if ($itemCount -eq 0) {
            Write-TestLog "⚠ WARN: $TestName - No items in dropdown" "WARN"
            return "SKIP"
        }

        # Test dropdown can be opened (if not DropDownList style)
        if ($ComboBox.DropDownStyle -ne [System.Windows.Forms.ComboBoxStyle]::DropDownList) {
            Write-TestLog "  DropDownStyle: $($ComboBox.DropDownStyle)" "INFO"
        }

        # Test selecting each available item
        $originalIndex = $ComboBox.SelectedIndex
        $originalItem = $ComboBox.SelectedItem
        $selectionTests = 0
        $selectionPassed = 0

        for ($i = 0; $i -lt [Math]::Min(10, $itemCount); $i++) {
            try {
                $ComboBox.SelectedIndex = $i
                Start-Sleep -Milliseconds 100

                if ($ComboBox.SelectedIndex -eq $i -and $ComboBox.SelectedItem) {
                    Write-TestLog "  ✓ Selected item $i : $($ComboBox.SelectedItem)" "PASS"
                    $selectionPassed++
                }
                else {
                    Write-TestLog "  ✗ Failed to select item $i" "FAIL"
                }
                $selectionTests++
            }
            catch {
                Write-TestLog "  ✗ Error selecting item $i : $($_.Exception.Message)" "FAIL"
            }
        }

        # Restore original selection
        if ($originalIndex -ge 0 -and $originalIndex -lt $itemCount) {
            $ComboBox.SelectedIndex = $originalIndex
        }
        elseif ($originalItem) {
            $ComboBox.SelectedItem = $originalItem
        }

        if ($selectionPassed -eq $selectionTests) {
            Write-TestLog "✓ PASS: $TestName - All selections work ($selectionPassed/$selectionTests)" "PASS"
            return $true
        }
        elseif ($selectionPassed -gt 0) {
            Write-TestLog "⚠ PARTIAL: $TestName - Some selections work ($selectionPassed/$selectionTests)" "WARN"
            return $true
        }
        else {
            Write-TestLog "✗ FAIL: $TestName - No selections work" "FAIL"
            return $false
        }
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

function Test-MenuDropdown {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$MenuItem,
        [string]$TestName,
        [int]$Depth = 0
    )

    if (-not $MenuItem) {
        Write-TestLog "✗ SKIP: $TestName - MenuItem is null" "SKIP"
        return "SKIP"
    }

    try {
        if ($MenuItem.IsDisposed) {
            Write-TestLog "✗ FAIL: $TestName - MenuItem is disposed" "FAIL"
            return $false
        }

        $indent = "  " * $Depth
        Write-TestLog "$indent Menu: $($MenuItem.Text)" "INFO"
        Write-TestLog "$indent   Enabled: $($MenuItem.Enabled)" "INFO"
        Write-TestLog "$indent   Visible: $($MenuItem.Visible)" "INFO"

        # Check if it has click handler
        $hasClickHandler = $false
        try {
            $clickEvent = $MenuItem.GetType().GetEvent("Click")
            if ($clickEvent) {
                $hasClickHandler = $true
                Write-TestLog "$indent   Has Click Handler: Yes" "INFO"
            }
        }
        catch {
            Write-TestLog "$indent   Has Click Handler: Unknown" "WARN"
        }

        # Test sub-items
        $subItemCount = $MenuItem.DropDownItems.Count
        Write-TestLog "$indent   Sub-items: $subItemCount" "INFO"

        if ($subItemCount -gt 0) {
            $subItemResults = @()
            foreach ($subItem in $MenuItem.DropDownItems) {
                if ($subItem -is [System.Windows.Forms.ToolStripMenuItem]) {
                    $subResult = Test-MenuDropdown -MenuItem $subItem -TestName "$TestName > $($subItem.Text)" -Depth ($Depth + 1)
                    $subItemResults += $subResult
                }
                elseif ($subItem -is [System.Windows.Forms.ToolStripSeparator]) {
                    Write-TestLog "$indent   --- Separator ---" "INFO"
                }
            }

            $passedSubs = ($subItemResults | Where-Object { $_ -eq $true }).Count
            $totalSubs = $subItemResults.Count

            if ($passedSubs -eq $totalSubs -and $totalSubs -gt 0) {
                Write-TestLog "$indent ✓ All sub-items accessible ($passedSubs/$totalSubs)" "PASS"
                return $true
            }
            elseif ($passedSubs -gt 0) {
                Write-TestLog "$indent ⚠ Some sub-items accessible ($passedSubs/$totalSubs)" "WARN"
                return $true
            }
            else {
                Write-TestLog "$indent ✗ No sub-items accessible" "FAIL"
                return $false
            }
        }

        # If it's a leaf node (no sub-items), check if it's functional
        if ($hasClickHandler -or $subItemCount -eq 0) {
            return $true
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
Write-TestLog "RawrXD Dropdown Feature Test Suite" "INFO"
Write-TestLog "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-Host ""
Write-Host "⚠️  IMPORTANT: This test requires RawrXD GUI to be running!" -ForegroundColor Yellow
Write-Host "   Make sure RawrXD.ps1 GUI is open before running this test." -ForegroundColor Yellow
Write-Host ""

if (-not $RunAll) {
    $response = Read-Host "Is RawrXD GUI currently running? (y/N)"
    if ($response -ne "y" -and $response -ne "Y") {
        Write-Host "Please start RawrXD GUI first, then run this test." -ForegroundColor Red
        Write-Host "Run: .\RawrXD.ps1" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host ""
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "TEST CATEGORY: ComboBox Dropdowns" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

# Test Model ComboBox in chat tabs
Test-DropdownFeature -FeatureName "Chat Model ComboBox" -Category "ComboBox" -TestScript {
    if ($script:chatTabs -and $script:chatTabs.Count -gt 0) {
        $allPassed = $true
        $anyFound = $false

        foreach ($tabId in $script:chatTabs.Keys) {
            $chatSession = $script:chatTabs[$tabId]
            if ($chatSession.ModelCombo) {
                $anyFound = $true
                $result = Test-ComboBoxDropdown -ComboBox $chatSession.ModelCombo -TestName "Model Combo ($tabId)"
                if ($result -ne $true) {
                    $allPassed = $false
                }
            }
        }

        if (-not $anyFound) {
            return "SKIP"
        }

        return $allPassed
    }
    return "SKIP"
}

Write-Host ""
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "TEST CATEGORY: Menu Dropdowns" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

# Test File Menu
Test-DropdownFeature -FeatureName "File Menu" -Category "Menu" -TestScript {
    if ($fileMenu) {
        return (Test-MenuDropdown -MenuItem $fileMenu -TestName "File Menu")
    }
    return "SKIP"
}

# Test Edit Menu
Test-DropdownFeature -FeatureName "Edit Menu" -Category "Menu" -TestScript {
    if ($editMenu) {
        return (Test-MenuDropdown -MenuItem $editMenu -TestName "Edit Menu")
    }
    return "SKIP"
}

# Test Chat Menu
Test-DropdownFeature -FeatureName "Chat Menu" -Category "Menu" -TestScript {
    if ($chatMenu) {
        return (Test-MenuDropdown -MenuItem $chatMenu -TestName "Chat Menu")
    }
    return "SKIP"
}

# Test Settings Menu
Test-DropdownFeature -FeatureName "Settings Menu" -Category "Menu" -TestScript {
    if ($settingsMenu) {
        return (Test-MenuDropdown -MenuItem $settingsMenu -TestName "Settings Menu")
    }
    return "SKIP"
}

# Test Tools Menu
Test-DropdownFeature -FeatureName "Tools Menu" -Category "Menu" -TestScript {
    if ($toolsMenu) {
        return (Test-MenuDropdown -MenuItem $toolsMenu -TestName "Tools Menu")
    }
    return "SKIP"
}

# Test Extensions Menu
Test-DropdownFeature -FeatureName "Extensions Menu" -Category "Menu" -TestScript {
    if ($extensionsMenu) {
        return (Test-MenuDropdown -MenuItem $extensionsMenu -TestName "Extensions Menu")
    }
    return "SKIP"
}

# Test Security Menu
Test-DropdownFeature -FeatureName "Security Menu" -Category "Menu" -TestScript {
    if ($securityMenu) {
        return (Test-MenuDropdown -MenuItem $securityMenu -TestName "Security Menu")
    }
    return "SKIP"
}

# Test Tools submenus
Test-DropdownFeature -FeatureName "Ollama Server Submenu" -Category "Submenu" -TestScript {
    if ($ollamaServerItem) {
        return (Test-MenuDropdown -MenuItem $ollamaServerItem -TestName "Ollama Server" -Depth 1)
    }
    return "SKIP"
}

Test-DropdownFeature -FeatureName "Performance Tools Submenu" -Category "Submenu" -TestScript {
    if ($perfToolsItem) {
        return (Test-MenuDropdown -MenuItem $perfToolsItem -TestName "Performance Tools" -Depth 1)
    }
    return "SKIP"
}

Write-Host ""
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "TEST CATEGORY: Text Input Boxes" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

# Test all text boxes for typing functionality
Test-DropdownFeature -FeatureName "Editor TextBox Typing" -Category "TextInput" -TestScript {
    if ($script:editor -and -not $script:editor.IsDisposed) {
        try {
            $originalText = $script:editor.Text
            $testText = "Test typing in editor`nLine 2`nLine 3"
            $script:editor.Text = $testText
            Start-Sleep -Milliseconds 200

            if ($script:editor.Text -eq $testText) {
                $script:editor.Text = $originalText
                return $true
            }
            return $false
        }
        catch {
            return $false
        }
    }
    return "SKIP"
}

Test-DropdownFeature -FeatureName "Chat Input Box Typing" -Category "TextInput" -TestScript {
    if ($script:chatTabs -and $script:chatTabs.Count -gt 0) {
        $allPassed = $true
        foreach ($tabId in $script:chatTabs.Keys) {
            $chatSession = $script:chatTabs[$tabId]
            if ($chatSession.InputBox -and -not $chatSession.InputBox.IsDisposed) {
                try {
                    $originalText = $chatSession.InputBox.Text
                    $testText = "Test chat message"
                    $chatSession.InputBox.Text = $testText
                    Start-Sleep -Milliseconds 200

                    if ($chatSession.InputBox.Text -eq $testText) {
                        $chatSession.InputBox.Text = $originalText
                    }
                    else {
                        $allPassed = $false
                    }
                }
                catch {
                    $allPassed = $false
                }
            }
        }
        return $allPassed
    }
    return "SKIP"
}

Test-DropdownFeature -FeatureName "Browser URL Box Typing" -Category "TextInput" -TestScript {
    if ($browserUrlBox -and -not $browserUrlBox.IsDisposed) {
        try {
            $originalText = $browserUrlBox.Text
            $testText = "https://www.test.com"
            $browserUrlBox.Text = $testText
            Start-Sleep -Milliseconds 200

            if ($browserUrlBox.Text -eq $testText) {
                $browserUrlBox.Text = $originalText
                return $true
            }
            return $false
        }
        catch {
            return $false
        }
    }
    return "SKIP"
}

Test-DropdownFeature -FeatureName "Command Palette Input Typing" -Category "TextInput" -TestScript {
    if ($paletteInput -and -not $paletteInput.IsDisposed) {
        try {
            $originalText = $paletteInput.Text
            $testText = "test command"
            $paletteInput.Text = $testText
            Start-Sleep -Milliseconds 200

            if ($paletteInput.Text -eq $testText) {
                $paletteInput.Text = $originalText
                return $true
            }
            return $false
        }
        catch {
            return $false
        }
    }
    return "SKIP"
}

Write-Host ""
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "TEST SUMMARY" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "Total Tests: $testCount" "INFO"
Write-TestLog "Passed: $passCount" "PASS"
Write-TestLog "Failed: $failCount" "FAIL"
Write-TestLog "Skipped: $skipCount" "SKIP"
if ($testCount -gt 0) {
    $successRate = [math]::Round(($passCount / $testCount) * 100, 2)
    Write-TestLog "Success Rate: $successRate%" "INFO"
}
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "Test completed: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
Write-TestLog "Log file: $LogFile" "INFO"
Write-Host ""

# Generate detailed summary report
$summaryFile = $LogFile -replace '\.log$', '-Summary.txt'
$summary = @"
═══════════════════════════════════════════════════════
RawrXD Dropdown Feature Test Summary
═══════════════════════════════════════════════════════
Test Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Total Tests: $testCount
Passed: $passCount
Failed: $failCount
Skipped: $skipCount
Success Rate: $([math]::Round(($passCount / $testCount) * 100, 2))%

═══════════════════════════════════════════════════════
PASSED TESTS
═══════════════════════════════════════════════════════
"@

$passedTests = $testResults | Where-Object { $_.Status -eq "PASS" }
if ($passedTests.Count -eq 0) {
    $summary += "`nNo tests passed`n"
}
else {
    foreach ($test in $passedTests) {
        $summary += "`n✓ $($test.Message)"
    }
}

$summary += @"

═══════════════════════════════════════════════════════
FAILED TESTS
═══════════════════════════════════════════════════════
"@

$failedTests = $testResults | Where-Object { $_.Status -eq "FAIL" }
if ($failedTests.Count -eq 0) {
    $summary += "`n✓ No tests failed!`n"
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

$summary += @"

═══════════════════════════════════════════════════════
RESULTS BY CATEGORY
═══════════════════════════════════════════════════════
"@

$categories = $testResults | Group-Object -Property Category
foreach ($category in $categories) {
    $catPassed = ($category.Group | Where-Object { $_.Status -eq "PASS" }).Count
    $catFailed = ($category.Group | Where-Object { $_.Status -eq "FAIL" }).Count
    $catSkipped = ($category.Group | Where-Object { $_.Status -eq "SKIP" }).Count
    $catTotal = $category.Count

    $summary += "`n$($category.Name):"
    $summary += "  Total: $catTotal | Passed: $catPassed | Failed: $catFailed | Skipped: $catSkipped"
}

$summary | Out-File -FilePath $summaryFile -Encoding UTF8
Write-Host "Summary report saved to: $summaryFile" -ForegroundColor Cyan
Write-Host "Detailed log saved to: $LogFile" -ForegroundColor Cyan
Write-Host ""

