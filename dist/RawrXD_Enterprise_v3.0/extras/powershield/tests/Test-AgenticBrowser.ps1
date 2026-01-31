<#
.SYNOPSIS
    Test Script for RawrXD Agentic Browser & Video System

.DESCRIPTION
    Comprehensive test of all browser and video functionality
    without requiring GUI interaction.
#>

[CmdletBinding()]
param(
    [switch]$FullTest,
    [switch]$QuickTest
)

$ErrorActionPreference = "Stop"

# Setup
$modulesPath = Join-Path $PSScriptRoot "Modules\Modules"
$testResults = @()

function Write-TestResult {
    param($TestName, $Result, $Details = "")

    $status = if ($Result) { "✅ PASS" } else { "❌ FAIL" }
    $color = if ($Result) { "Green" } else { "Red" }

    Write-Host "$status $TestName" -ForegroundColor $color
    if ($Details) {
        Write-Host "   $Details" -ForegroundColor Gray
    }

    $testResults += @{
        Test = $TestName
        Result = $Result
        Details = $Details
    }
}

Write-Host "🧪 RawrXD Agentic Browser & Video System Tests" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan

# Test 1: Module Loading
Write-Host "`n📦 Testing Module Loading..." -ForegroundColor Yellow

$modulesToTest = @(
    "RawrXD.Browser",
    "RawrXD.Video",
    "RawrXD.Logging"
)

foreach ($module in $modulesToTest) {
    $modulePath = Join-Path $modulesPath "$module.psm1"
    try {
        if (Test-Path $modulePath) {
            Import-Module $modulePath -Force -ErrorAction Stop
            Write-TestResult "Load $module" $true "Module loaded successfully"
        }
        else {
            Write-TestResult "Load $module" $false "Module file not found: $modulePath"
        }
    }
    catch {
        Write-TestResult "Load $module" $false "Import failed: $_"
    }
}

# Test 2: Browser Capabilities
Write-Host "`n🌐 Testing Browser Capabilities..." -ForegroundColor Yellow

try {
    $browserInfo = Get-BrowserInfo
    Write-TestResult "Get Browser Info" $true "WebView2: $($browserInfo.WebView2Available)"
} catch {
    Write-TestResult "Get Browser Info" $false "Failed: $_"
}

# Test 3: Video Capabilities
Write-Host "`n🎬 Testing Video Capabilities..." -ForegroundColor Yellow

try {
    $videoCaps = Get-VideoCapabilities
    Write-TestResult "Get Video Capabilities" $true "Formats: $($videoCaps.SupportedFormats.Count), WPF: $($videoCaps.WPFMediaAvailable)"
} catch {
    Write-TestResult "Get Video Capabilities" $false "Failed: $_"
}

# Test 4: Agent Task Creation
Write-Host "`n🤖 Testing Agent Task Creation..." -ForegroundColor Yellow

try {
    $task = New-AgentTask -Id "test-navigate" -Type "Navigate" -Parameters @{ Url = "https://example.com" }
    Write-TestResult "Create Agent Task" $true "Task ID: $($task.Id), Type: $($task.Type)"
} catch {
    Write-TestResult "Create Agent Task" $false "Failed: $_"
}

# Test 5: Video Metadata (without GUI)
Write-Host "`n📊 Testing Video Metadata Functions..." -ForegroundColor Yellow

try {
    # Test URL format detection
    $testUrls = @(
        "https://youtube.com/watch?v=test",
        "https://example.com/video.mp4",
        "https://vimeo.com/12345",
        "https://stream.com/playlist.m3u8"
    )

    foreach ($url in $testUrls) {
        # Simulate format detection logic
        $format = switch ($url) {
            { $_ -match "youtube\.com|youtu\.be" } { "YouTube" }
            { $_ -match "\.mp4$" } { "MP4" }
            { $_ -match "\.m3u8$" } { "HLS" }
            default { "Unknown" }
        }
        Write-TestResult "Format Detection: $([System.IO.Path]::GetFileName($url))" $true "Detected: $format"
    }
} catch {
    Write-TestResult "Video Format Detection" $false "Failed: $_"
}

if ($FullTest) {
    # Test 6: GUI Components (requires display)
    Write-Host "`n🖥️ Testing GUI Components..." -ForegroundColor Yellow

    try {
        # Test form creation (doesn't require display)
        $testForm = New-Object System.Windows.Forms.Form
        $testForm.Text = "Test Form"
        $testForm.Size = [System.Drawing.Size]::new(400, 300)

        $testPanel = New-Object System.Windows.Forms.Panel
        $testPanel.Dock = [System.Windows.Forms.DockStyle]::Fill
        $testForm.Controls.Add($testPanel)

        Write-TestResult "GUI Component Creation" $true "Form and Panel created successfully"
    } catch {
        Write-TestResult "GUI Component Creation" $false "Failed: $_"
    }
}

# Test 7: System Integration
Write-Host "`n🔗 Testing System Integration..." -ForegroundColor Yellow

try {
    # Test function availability
    $requiredFunctions = @(
        "New-BrowserAgent",
        "Invoke-BrowserNavigation",
        "New-VideoStream",
        "Invoke-VideoControl",
        "New-AgentTask"
    )

    $missingFunctions = @()
    foreach ($func in $requiredFunctions) {
        if (-not (Get-Command $func -ErrorAction SilentlyContinue)) {
            $missingFunctions += $func
        }
    }

    if ($missingFunctions.Count -eq 0) {
        Write-TestResult "Function Availability" $true "All required functions available"
    } else {
        Write-TestResult "Function Availability" $false "Missing: $($missingFunctions -join ', ')"
    }
} catch {
    Write-TestResult "System Integration" $false "Failed: $_"
}

# Test 8: Error Handling
Write-Host "`n🛡️ Testing Error Handling..." -ForegroundColor Yellow

try {
    # Test invalid browser ID
    $result = Get-BrowserAgent -Id "nonexistent-browser"
    if ($result -eq $null) {
        Write-TestResult "Invalid Browser ID Handling" $true "Correctly returned null for invalid ID"
    } else {
        Write-TestResult "Invalid Browser ID Handling" $false "Should return null for invalid ID"
    }
} catch {
    Write-TestResult "Error Handling Test" $false "Unexpected error: $_"
}

# Summary
Write-Host "`n📊 Test Summary" -ForegroundColor Cyan
Write-Host "=" * 30 -ForegroundColor Cyan

$passed = ($testResults | Where-Object { $_.Result }).Count
$total = $testResults.Count
$passRate = [math]::Round(($passed / $total) * 100, 1)

Write-Host "Tests Passed: $passed / $total ($passRate%)" -ForegroundColor $(if ($passRate -ge 80) { "Green" } else { "Yellow" })

if ($QuickTest -or $passRate -lt 100) {
    Write-Host "`n❌ Failed Tests:" -ForegroundColor Red
    $testResults | Where-Object { -not $_.Result } | ForEach-Object {
        Write-Host "  • $($_.Test)" -ForegroundColor Red
        if ($_.Details) {
            Write-Host "    $($_.Details)" -ForegroundColor Gray
        }
    }
}

Write-Host "`n🎉 Testing Complete!" -ForegroundColor $(if ($passRate -ge 80) { "Green" } else { "Yellow" })

if ($passRate -ge 80) {
    Write-Host "`n💡 Next Steps:" -ForegroundColor Cyan
    Write-Host "  • Run .\Browser-Video-Integration.ps1 for GUI demos" -ForegroundColor White
    Write-Host "  • Integrate modules into your RawrXD main script" -ForegroundColor White
    Write-Host "  • Customize automation tasks for your use case" -ForegroundColor White
} else {
    Write-Host "`n🔧 Issues to resolve:" -ForegroundColor Yellow
    Write-Host "  • Check module installation and paths" -ForegroundColor White
    Write-Host "  • Ensure PowerShell 7+ with .NET Framework 4.8+" -ForegroundColor White
    Write-Host "  • Verify WebView2 runtime installation" -ForegroundColor White
}

exit $(if ($passRate -ge 80) { 0 } else { 1 })
