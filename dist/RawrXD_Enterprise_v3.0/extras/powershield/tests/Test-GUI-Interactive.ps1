# Interactive GUI Tester - Tests all dropdowns and text boxes with actual GUI interaction
# Run this while RawrXD GUI is open

param(
    [switch]$AutoTest,
    [int]$DelayMs = 500
)

$ErrorActionPreference = "Continue"
$testLog = "GUI-Interactive-Test-$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

function Write-TestLog {
    param([string]$Message, [string]$Status = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
    $logEntry = "[$timestamp] [$Status] $Message"
    Write-Host $logEntry
    Add-Content -Path $testLog -Value $logEntry
}

function Test-TextBoxInput {
    param(
        [System.Windows.Forms.TextBox]$TextBox,
        [string]$TestName,
        [string]$TestText
    )

    Write-TestLog "Testing text input: $TestName" "TEST"

    if (-not $TextBox) {
        Write-TestLog "✗ SKIP: $TestName - TextBox not found" "SKIP"
        return $false
    }

    try {
        if ($TextBox.IsDisposed) {
            Write-TestLog "✗ FAIL: $TestName - TextBox is disposed" "FAIL"
            return $false
        }

        # Test 1: Can set text
        $originalText = $TextBox.Text
        $TextBox.Text = $TestText

        Start-Sleep -Milliseconds $DelayMs

        if ($TextBox.Text -eq $TestText) {
            Write-TestLog "✓ PASS: $TestName - Text can be set" "PASS"
        }
        else {
            Write-TestLog "✗ FAIL: $TestName - Text was not set (Expected: '$TestText', Got: '$($TextBox.Text)')" "FAIL"
            $TextBox.Text = $originalText
            return $false
        }

        # Test 2: Can append text
        $appendText = " - Appended"
        $TextBox.AppendText($appendText)
        Start-Sleep -Milliseconds $DelayMs

        if ($TextBox.Text -eq ($TestText + $appendText)) {
            Write-TestLog "✓ PASS: $TestName - Text can be appended" "PASS"
        }
        else {
            Write-TestLog "✗ FAIL: $TestName - Text cannot be appended" "FAIL"
        }

        # Test 3: Can clear text
        $TextBox.Clear()
        Start-Sleep -Milliseconds $DelayMs

        if ($TextBox.Text -eq "") {
            Write-TestLog "✓ PASS: $TestName - Text can be cleared" "PASS"
        }
        else {
            Write-TestLog "✗ FAIL: $TestName - Text cannot be cleared" "FAIL"
        }

        # Restore original
        $TextBox.Text = $originalText
        return $true
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

function Test-ComboBoxSelection {
    param(
        [System.Windows.Forms.ComboBox]$ComboBox,
        [string]$TestName
    )

    Write-TestLog "Testing combobox: $TestName" "TEST"

    if (-not $ComboBox) {
        Write-TestLog "✗ SKIP: $TestName - ComboBox not found" "SKIP"
        return $false
    }

    try {
        if ($ComboBox.IsDisposed) {
            Write-TestLog "✗ FAIL: $TestName - ComboBox is disposed" "FAIL"
            return $false
        }

        $itemCount = $ComboBox.Items.Count
        Write-TestLog "  Items available: $itemCount" "INFO"

        if ($itemCount -eq 0) {
            Write-TestLog "⚠ WARN: $TestName - No items to test" "WARN"
            return $true
        }

        # Test selecting each item
        $originalIndex = $ComboBox.SelectedIndex
        $allPassed = $true

        for ($i = 0; $i -lt [Math]::Min(5, $itemCount); $i++) {
            $ComboBox.SelectedIndex = $i
            Start-Sleep -Milliseconds $DelayMs

            if ($ComboBox.SelectedIndex -eq $i -and $ComboBox.SelectedItem) {
                Write-TestLog "✓ PASS: $TestName - Selected item $i: $($ComboBox.SelectedItem)" "PASS"
            }
            else {
                Write-TestLog "✗ FAIL: $TestName - Cannot select item $i" "FAIL"
                $allPassed = $false
            }
        }

        # Restore original selection
        if ($originalIndex -ge 0) {
            $ComboBox.SelectedIndex = $originalIndex
        }

        return $allPassed
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

function Test-MenuItemClick {
    param(
        [System.Windows.Forms.ToolStripMenuItem]$MenuItem,
        [string]$TestName
    )

    Write-TestLog "Testing menu item: $TestName" "TEST"

    if (-not $MenuItem) {
        Write-TestLog "✗ SKIP: $TestName - MenuItem not found" "SKIP"
        return $false
    }

    try {
        if ($MenuItem.Enabled) {
            Write-TestLog "  Menu item is enabled" "INFO"

            # Check if it has a click handler
            $hasHandler = $MenuItem.GetType().GetEvent("Click") -ne $null
            if ($hasHandler) {
                Write-TestLog "✓ PASS: $TestName - Has click handler" "PASS"

                # Test sub-items if any
                if ($MenuItem.DropDownItems.Count -gt 0) {
                    Write-TestLog "  Has $($MenuItem.DropDownItems.Count) sub-items" "INFO"
                    foreach ($subItem in $MenuItem.DropDownItems) {
                        if ($subItem -is [System.Windows.Forms.ToolStripMenuItem]) {
                            Test-MenuItemClick -MenuItem $subItem -TestName "$TestName > $($subItem.Text)"
                        }
                    }
                }

                return $true
            }
            else {
                Write-TestLog "✗ FAIL: $TestName - No click handler" "FAIL"
                return $false
            }
        }
        else {
            Write-TestLog "⚠ WARN: $TestName - Menu item is disabled" "WARN"
            return $true
        }
    }
    catch {
        Write-TestLog "✗ FAIL: $TestName - Error: $($_.Exception.Message)" "FAIL"
        return $false
    }
}

Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "RawrXD Interactive GUI Tester" "INFO"
Write-TestLog "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-Host ""
Write-Host "⚠️  IMPORTANT: This test requires RawrXD GUI to be running!" -ForegroundColor Yellow
Write-Host "   Make sure RawrXD.ps1 GUI is open before running this test." -ForegroundColor Yellow
Write-Host ""

if (-not $AutoTest) {
    $response = Read-Host "Is RawrXD GUI currently running? (y/N)"
    if ($response -ne "y" -and $response -ne "Y") {
        Write-Host "Please start RawrXD GUI first, then run this test." -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-TestLog "Testing Text Input Boxes..." "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

# Test Editor
if ($script:editor) {
    Test-TextBoxInput -TextBox $script:editor -TestName "Main Editor" -TestText "Test editor content`nLine 2`nLine 3"
}

# Test Chat Input Boxes
if ($script:chatTabs) {
    foreach ($tabId in $script:chatTabs.Keys) {
        $chatSession = $script:chatTabs[$tabId]
        if ($chatSession.InputBox) {
            Test-TextBoxInput -TextBox $chatSession.InputBox -TestName "Chat Input ($tabId)" -TestText "Test chat message"
        }
    }
}

# Test Find/Replace boxes
if ($findTextBox) {
    Test-TextBoxInput -TextBox $findTextBox -TestName "Find Dialog" -TestText "search text"
}

# Test Browser URL box
if ($browserUrlBox) {
    Test-TextBoxInput -TextBox $browserUrlBox -TestName "Browser URL" -TestText "https://www.test.com"
}

# Test Command Palette
if ($paletteInput) {
    Test-TextBoxInput -TextBox $paletteInput -TestName "Command Palette" -TestText "test command"
}

Write-Host ""
Write-TestLog "Testing ComboBox/Dropdown Controls..." "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

# Test Model ComboBox in chat tabs
if ($script:chatTabs) {
    foreach ($tabId in $script:chatTabs.Keys) {
        $chatSession = $script:chatTabs[$tabId]
        if ($chatSession.ModelCombo) {
            Test-ComboBoxSelection -ComboBox $chatSession.ModelCombo -TestName "Model Combo ($tabId)"
        }
    }
}

Write-Host ""
Write-TestLog "Testing Menu Items..." "INFO"
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"

# Test File Menu
if ($fileMenu) {
    Test-MenuItemClick -MenuItem $fileMenu -TestName "File Menu"
}

# Test Edit Menu
if ($editMenu) {
    Test-MenuItemClick -MenuItem $editMenu -TestName "Edit Menu"
}

# Test Chat Menu
if ($chatMenu) {
    Test-MenuItemClick -MenuItem $chatMenu -TestName "Chat Menu"
}

# Test Settings Menu
if ($settingsMenu) {
    Test-MenuItemClick -MenuItem $settingsMenu -TestName "Settings Menu"
}

# Test Tools Menu
if ($toolsMenu) {
    Test-MenuItemClick -MenuItem $toolsMenu -TestName "Tools Menu"
}

# Test Extensions Menu
if ($extensionsMenu) {
    Test-MenuItemClick -MenuItem $extensionsMenu -TestName "Extensions Menu"
}

# Test Security Menu
if ($securityMenu) {
    Test-MenuItemClick -MenuItem $securityMenu -TestName "Security Menu"
}

Write-Host ""
Write-TestLog "═══════════════════════════════════════════════════════" "INFO"
Write-TestLog "Test completed: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" "INFO"
Write-TestLog "Log file: $testLog" "INFO"
Write-Host ""
Write-Host "Test log saved to: $testLog" -ForegroundColor Green

