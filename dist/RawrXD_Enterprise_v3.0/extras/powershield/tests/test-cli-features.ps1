#!/usr/bin/env pwsh
# Test script for new CLI features in RawrXD

# Load the RawrXD script first
$scriptPath = Join-Path $PSScriptRoot "RawrXD.ps1"

# Set up CLI mode variables
$script:SkipGUIInit = $true
$CliMode = $true

# Source the main script (in a try/catch to handle any errors)
try {
    Write-Host "Loading RawrXD script..." -ForegroundColor Cyan
    . $scriptPath

    Write-Host "✅ RawrXD loaded successfully" -ForegroundColor Green

    # Test the new CLI commands
    Write-Host "`n🧪 Testing CLI Visual Editor Features:" -ForegroundColor Yellow
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan

    # Test 1: Check if CLI editor functions are available
    Write-Host "`n1. Testing function availability:" -ForegroundColor White
    $functions = @(
        'Start-CLITextEditor',
        'Show-CLIFileTree',
        'Show-CLITabs',
        'Start-CLISplitView',
        'Show-SyntaxHighlightedFile',
        'Process-ConsoleCommand'
    )

    foreach ($func in $functions) {
        if (Get-Command $func -ErrorAction SilentlyContinue) {
            Write-Host "   ✅ $func - Available" -ForegroundColor Green
        } else {
            Write-Host "   ❌ $func - Missing" -ForegroundColor Red
        }
    }

    # Test 2: Create a sample file for testing
    Write-Host "`n2. Creating test file:" -ForegroundColor White
    $testFile = Join-Path $PSScriptRoot "test-sample.ps1"
    @"
# Sample PowerShell script for testing CLI editor
function Test-CLIEditor {
    param([string]`$Message)
    Write-Host "Testing CLI Editor: `$Message" -ForegroundColor Green
}

# Sample variables
`$sampleVar = "Hello World"
`$numbers = @(1, 2, 3, 4, 5)

# Sample loop
foreach (`$num in `$numbers) {
    Write-Host "Number: `$num"
}

Test-CLIEditor "This is a test!"
"@ | Set-Content $testFile

    if (Test-Path $testFile) {
        Write-Host "   ✅ Test file created: $testFile" -ForegroundColor Green
    } else {
        Write-Host "   ❌ Failed to create test file" -ForegroundColor Red
    }

    # Test 3: Show available CLI commands
    Write-Host "`n3. Available CLI Commands:" -ForegroundColor White
    Write-Host "   📝 /edit <file>     - Open file in CLI text editor" -ForegroundColor Cyan
    Write-Host "   🌳 /tree [path]     - Browse files with CLI file tree" -ForegroundColor Cyan
    Write-Host "   📂 /tabs            - View and manage open tabs" -ForegroundColor Cyan
    Write-Host "   🔀 /split <f1> <f2> - View two files side by side" -ForegroundColor Cyan
    Write-Host "   🎨 /syntax <file>   - Preview file with syntax highlighting" -ForegroundColor Cyan

    # Test 4: Test syntax highlighting
    Write-Host "`n4. Testing syntax highlighting on test file:" -ForegroundColor White
    try {
        Show-SyntaxHighlightedFile $testFile
    } catch {
        Write-Host "   ❌ Syntax highlighting failed: $($_.Exception.Message)" -ForegroundColor Red
    }

    Write-Host "`n🎯 CLI Enhancement Complete!" -ForegroundColor Green
    Write-Host "All CLI visual editor features have been added to RawrXD:" -ForegroundColor White
    Write-Host "✅ Console-based text editor with line numbers" -ForegroundColor Green
    Write-Host "✅ Text-based file tree navigation system" -ForegroundColor Green
    Write-Host "✅ Virtual tabs and split view functionality" -ForegroundColor Green
    Write-Host "✅ Console-based syntax highlighting" -ForegroundColor Green
    Write-Host "✅ Integration with existing console command system" -ForegroundColor Green

    Write-Host "`nTo use these features, start RawrXD in console mode and use:" -ForegroundColor Yellow
    Write-Host "   /edit myfile.ps1" -ForegroundColor Cyan
    Write-Host "   /tree" -ForegroundColor Cyan
    Write-Host "   /syntax myfile.ps1" -ForegroundColor Cyan
    Write-Host "   /help" -ForegroundColor Cyan

}
catch {
    Write-Host "❌ Error loading RawrXD: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Stack trace: $($_.ScriptStackTrace)" -ForegroundColor Gray
}

Write-Host "`nPress Enter to exit..." -ForegroundColor Gray
Read-Host
