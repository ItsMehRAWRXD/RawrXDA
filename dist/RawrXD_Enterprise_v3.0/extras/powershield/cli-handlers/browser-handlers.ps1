<#
.SYNOPSIS
    Browser automation CLI command handlers
.DESCRIPTION
    Handles browser-navigate, browser-screenshot, and browser-click commands
#>

function Invoke-BrowserNavigateHandler {
    param([string]$URL)
    
    Write-Host "`n=== 🌐 Browser Navigate ===" -ForegroundColor Cyan
    if (-not $URL) {
        Write-Host "Error: -URL parameter required" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command browser-navigate -URL 'https://...'" -ForegroundColor Yellow
        return 1
    }
    
    try {
        Write-Host "Opening: $URL" -ForegroundColor Yellow
        Start-Process $URL
        Write-Host "✅ URL opened in default browser" -ForegroundColor Green
        @{ status = "success"; action = "navigate"; url = $URL } | ConvertTo-Json | Write-Host -ForegroundColor Gray
        return 0
    }
    catch {
        Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
        return 1
    }
}

function Invoke-BrowserScreenshotHandler {
    param([string]$OutputPath)
    
    Write-Host "`n=== 📸 Browser Screenshot ===" -ForegroundColor Cyan
    Write-Host "Note: Screenshots require GUI mode with WebView2" -ForegroundColor Yellow
    Write-Host ""
    
    if (Get-Command Get-BrowserScreenshot -ErrorAction SilentlyContinue) {
        $outputFile = if ($OutputPath) { $OutputPath } else { "$env:TEMP\browser_screenshot_$(Get-Date -Format 'yyyyMMdd_HHmmss').png" }
        try {
            $result = Get-BrowserScreenshot -OutputPath $outputFile
            if ($result) {
                Write-Host "✅ Screenshot saved: $outputFile" -ForegroundColor Green
                @{ status = "success"; path = $outputFile } | ConvertTo-Json | Write-Host -ForegroundColor Gray
                return 0
            }
            else {
                Write-Host "❌ Screenshot failed (WebView2 not available in CLI mode)" -ForegroundColor Red
                return 1
            }
        }
        catch {
            Write-Host "❌ Error: $($_.Exception.Message)" -ForegroundColor Red
            return 1
        }
    }
    else {
        Write-Host "❌ BrowserAutomation module not loaded" -ForegroundColor Red
        return 1
    }
}

function Invoke-BrowserClickHandler {
    param([string]$Selector)
    
    Write-Host "`n=== 🖱️ Browser Click ===" -ForegroundColor Cyan
    Write-Host "Note: Click requires GUI mode with WebView2" -ForegroundColor Yellow
    
    if (-not $Selector) {
        Write-Host "Error: -Selector parameter required (CSS selector)" -ForegroundColor Red
        Write-Host "Usage: .\RawrXD.ps1 -CliMode -Command browser-click -Selector '#button-id'" -ForegroundColor Yellow
        return 1
    }
    
    Write-Host "Selector: $Selector" -ForegroundColor Gray
    Write-Host "⚠️ Browser click only works in GUI mode with active WebView2" -ForegroundColor Yellow
    @{ status = "info"; message = "Click requires GUI mode"; selector = $Selector } | ConvertTo-Json | Write-Host -ForegroundColor Gray
    return 0
}

# Note: Export-ModuleMember removed - this file is dot-sourced, not imported as a module
    'Invoke-BrowserNavigateHandler',
    'Invoke-BrowserScreenshotHandler',
    'Invoke-BrowserClickHandler'
)






































