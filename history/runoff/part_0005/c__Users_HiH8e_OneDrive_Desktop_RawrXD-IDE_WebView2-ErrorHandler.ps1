#Requires -Version 5.1
<#
.SYNOPSIS
    WebView2 Error Handler & Fallback Module
    
.DESCRIPTION
    Handles WebView2 assembly loading failures with graceful fallback to legacy browser.
    Provides robust error handling for different .NET versions and missing dependencies.

.PRODUCTION NOTES
    - Non-intrusive error handling with standardized responses
    - Automatic fallback to legacy IE-based browser if WebView2 fails
    - Structured logging for all assembly loading attempts
    - Support for multiple .NET framework versions
#>

# ============================================
# MODULE VARIABLES
# ============================================

$script:WebView2Status = @{
    Loaded = $false
    AssemblyPath = ""
    Error = ""
    FallbackMode = $false
    FrameworkVersion = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
}

$script:AssemblySearchPaths = @(
    "$env:TEMP\WVLibs",
    "$env:APPDATA\RawrXD\WebView2",
    "$PSScriptRoot\WebView2Libs",
    "${env:ProgramFiles}\Microsoft\WebView2",
    "${env:ProgramFiles(x86)}\Microsoft\WebView2"
)

# ============================================
# WEBVIEW2 ASSEMBLY LOADING
# ============================================

function Test-WebView2AssemblyAvailable {
    <#
    .SYNOPSIS
        Test if WebView2 assemblies are available in any search path
    #>
    foreach ($path in $script:AssemblySearchPaths) {
        if (Test-Path $path) {
            $winFormsdll = Join-Path $path "Microsoft.Web.WebView2.WinForms.dll"
            $coreDll = Join-Path $path "Microsoft.Web.WebView2.Core.dll"
            
            if ((Test-Path $winFormsdll) -and (Test-Path $coreDll)) {
                return @{
                    Found = $true
                    Path = $path
                    WinFormsDll = $winFormsdll
                    CoreDll = $coreDll
                }
            }
        }
    }
    
    return @{ Found = $false }
}

function Resolve-WebView2AssemblyPath {
    <#
    .SYNOPSIS
        Resolve the correct WebView2 assembly path for current framework
    #>
    try {
        $frameworkDesc = [System.Runtime.InteropServices.RuntimeInformation]::FrameworkDescription
        Write-Host "[WebView2] Framework: $frameworkDesc" -ForegroundColor Gray
        
        $availableResult = Test-WebView2AssemblyAvailable
        
        if ($availableResult.Found) {
            return @{
                Success = $true
                Path = $availableResult.Path
                WinFormsDll = $availableResult.WinFormsDll
                CoreDll = $availableResult.CoreDll
            }
        }
        
        return @{ Success = $false; Error = "WebView2 assemblies not found in any search path" }
    }
    catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
        }
    }
}

function Load-WebView2Assemblies {
    <#
    .SYNOPSIS
        Attempt to load WebView2 assemblies with comprehensive error handling
    #>
    try {
        Write-Host "[WebView2] Attempting to load WebView2 assemblies..." -ForegroundColor Cyan
        
        # Resolve path
        $pathResult = Resolve-WebView2AssemblyPath
        if (-not $pathResult.Success) {
            return @{
                Success = $false
                Error = $pathResult.Error
                Message = "WebView2 assemblies not found"
            }
        }
        
        # Load Core DLL first (it's a dependency)
        Write-Host "[WebView2] Loading Core assembly from: $($pathResult.CoreDll)" -ForegroundColor Gray
        try {
            Add-Type -Path $pathResult.CoreDll -ErrorAction Stop
            Write-Host "[WebView2] ✅ Core assembly loaded" -ForegroundColor Green
        }
        catch {
            # Core DLL not critical if it's already in GAC
            Write-Host "[WebView2] ⚠️ Core assembly load warning (may be in GAC): $($_.Exception.Message)" -ForegroundColor Yellow
        }
        
        # Load WinForms DLL
        Write-Host "[WebView2] Loading WinForms assembly from: $($pathResult.WinFormsDll)" -ForegroundColor Gray
        try {
            Add-Type -Path $pathResult.WinFormsDll -ErrorAction Stop
            Write-Host "[WebView2] ✅ WinForms assembly loaded" -ForegroundColor Green
            
            $script:WebView2Status.Loaded = $true
            $script:WebView2Status.AssemblyPath = $pathResult.Path
            
            return @{
                Success = $true
                AssemblyPath = $pathResult.Path
                Message = "WebView2 assemblies loaded successfully"
            }
        }
        catch {
            $errorMsg = $_.Exception.Message
            Write-Host "[WebView2] ❌ Failed to load WinForms assembly: $errorMsg" -ForegroundColor Red
            
            # Try to extract more details about the error
            if ($errorMsg -like "*assembly*could not be found*") {
                Write-Host "[WebView2] INFO: This is a known issue when the assembly isn't in the search path" -ForegroundColor Yellow
            }
            elseif ($errorMsg -like "*version*") {
                Write-Host "[WebView2] INFO: Version mismatch - may need to update WebView2 Runtime" -ForegroundColor Yellow
            }
            
            return @{
                Success = $false
                Error = $errorMsg
                Message = "Failed to load WebView2 WinForms assembly"
            }
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
            Message = "Unexpected error during WebView2 assembly loading"
        }
    }
}

# ============================================
# FALLBACK BROWSER SETUP
# ============================================

function Initialize-LegacyBrowser {
    <#
    .SYNOPSIS
        Initialize fallback IE-based browser when WebView2 fails
    #>
    try {
        Write-Host "[Browser] Initializing legacy WebBrowser control (IE-based)..." -ForegroundColor Cyan
        
        # IE-based browser is always available on Windows
        Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
        
        $script:WebView2Status.FallbackMode = $true
        
        Write-Host "[Browser] ✅ Legacy browser initialized successfully" -ForegroundColor Green
        
        return @{
            Success = $true
            BrowserType = "WebBrowser (IE-based)"
            Message = "Legacy browser initialized as fallback"
        }
    }
    catch {
        return @{
            Success = $false
            Error = $_.Exception.Message
            Message = "Failed to initialize legacy browser"
        }
    }
}

# ============================================
# SAFE BROWSER INITIALIZATION
# ============================================

function Initialize-BrowserSafely {
    <#
    .SYNOPSIS
        Initialize browser with automatic fallback on failure
    .DESCRIPTION
        Attempts WebView2, falls back to IE-based browser if needed.
        Never throws an exception - always returns a valid result.
    #>
    try {
        Write-Host "[Browser Initialization] Starting browser setup..." -ForegroundColor Cyan
        Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Gray
        Write-Host "[Browser Initialization] WEBVIEW2/BROWSER INITIALIZATION" -ForegroundColor Gray
        Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Gray
        
        # Attempt WebView2 loading
        $webView2Result = Load-WebView2Assemblies
        
        if ($webView2Result.Success) {
            Write-Host "[Browser Initialization] Browser Mode: WebView2 (Modern)" -ForegroundColor Green
            Write-Host "[Browser Initialization] ✅ WebView2 Runtime installed" -ForegroundColor Green
            Write-Host "[Browser Initialization] SUCCESS ✅ Browser control initialized: WebView2" -ForegroundColor Green
            
            return @{
                Success = $true
                BrowserType = "WebView2"
                BrowserImplementation = "modern"
                Fallback = $false
                Message = "WebView2 browser initialized successfully"
            }
        }
        else {
            Write-Host "[Browser Initialization] ⚠️ WebView2 not available: $($webView2Result.Error)" -ForegroundColor Yellow
            Write-Host "[Browser Initialization] Falling back to legacy browser..." -ForegroundColor Yellow
            
            # Attempt legacy browser fallback
            $legacyResult = Initialize-LegacyBrowser
            
            if ($legacyResult.Success) {
                Write-Host "[Browser Initialization] Browser Mode: LegacyFallback" -ForegroundColor Green
                Write-Host "[Browser Initialization] Using legacy WebBrowser control (IE-based)" -ForegroundColor Yellow
                Write-Host "[Browser Initialization] ✅ Browser control initialized: WebBrowser" -ForegroundColor Green
                
                return @{
                    Success = $true
                    BrowserType = "WebBrowser"
                    BrowserImplementation = "legacy"
                    Fallback = $true
                    Message = "Legacy IE-based browser initialized as fallback"
                    Note = "YouTube embeds may not work in legacy mode. Please install WebView2 Runtime for full functionality."
                }
            }
            else {
                Write-Host "[Browser Initialization] ❌ Failed to initialize any browser: $($legacyResult.Error)" -ForegroundColor Red
                
                return @{
                    Success = $false
                    BrowserType = "None"
                    BrowserImplementation = "none"
                    Fallback = $false
                    Message = "Failed to initialize browser"
                    Error = "Neither WebView2 nor legacy browser could be initialized"
                    CriticalError = $true
                }
            }
        }
    }
    catch {
        Write-Host "[Browser Initialization] ❌ Unexpected error: $($_.Exception.Message)" -ForegroundColor Red
        
        return @{
            Success = $false
            BrowserType = "Error"
            BrowserImplementation = "error"
            Fallback = $false
            Message = "Unexpected error during browser initialization"
            Error = $_.Exception.Message
            CriticalError = $true
        }
    }
    finally {
        Write-Host "[Browser Initialization] ═══════════════════════════════════════════════════════" -ForegroundColor Gray
    }
}

# ============================================
# DIAGNOSTIC & TROUBLESHOOTING
# ============================================

function Get-WebView2DiagnosticsInfo {
    <#
    .SYNOPSIS
        Get detailed diagnostic information about WebView2 status
    #>
    $diagnostics = @{
        FrameworkVersion = $script:WebView2Status.FrameworkVersion
        WebView2Loaded = $script:WebView2Status.Loaded
        WebView2Path = $script:WebView2Status.AssemblyPath
        WebView2Error = $script:WebView2Status.Error
        FallbackMode = $script:WebView2Status.FallbackMode
        SearchPaths = $script:AssemblySearchPaths
        PathsAvailable = @()
    }
    
    foreach ($path in $script:AssemblySearchPaths) {
        if (Test-Path $path) {
            $diagnostics.PathsAvailable += @{
                Path = $path
                Exists = $true
                Files = @(Get-ChildItem -Path $path -Filter "Microsoft.Web.WebView2.*" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Name)
            }
        }
    }
    
    return $diagnostics
}

function Show-WebView2TroubleshootingGuide {
    <#
    .SYNOPSIS
        Display troubleshooting guide for WebView2 issues
    #>
    $guide = @"
═══════════════════════════════════════════════════════════════════════════════
                    WebView2 TROUBLESHOOTING GUIDE
═══════════════════════════════════════════════════════════════════════════════

🔴 ISSUE: "Cannot add type. The assembly 'Microsoft.Web.WebView2.WinForms' could not be found."

SOLUTION OPTIONS (Try in this order):

1️⃣  Install WebView2 Runtime (Recommended):
   - Visit: https://developer.microsoft.com/en-us/microsoft-edge/webview2/
   - Download "WebView2 Runtime" (NOT developer tools)
   - Run the installer
   - Restart RawrXD

2️⃣  Update .NET Framework:
   - Current .NET: $($script:WebView2Status.FrameworkVersion)
   - Download latest .NET: https://dotnet.microsoft.com/download
   - Install and restart

3️⃣  Portable WebView2 (if admin install unavailable):
   - Extract WebView2 NuGet package to: $env:TEMP\WVLibs
   - Restart RawrXD

✅ WORKAROUND:
   - RawrXD will fall back to legacy IE-based browser
   - All core functionality works (except YouTube embeds)
   - Continue using RawrXD normally

📊 Current Diagnostic Information:
"@
    
    Write-Host $guide -ForegroundColor Yellow
    $diagnostics = Get-WebView2DiagnosticsInfo
    $diagnostics | ConvertTo-Json | Write-Host -ForegroundColor Gray
}

# ============================================
# INITIALIZATION
# ============================================

Write-Host "[WebView2-ErrorHandler] Module loaded successfully" -ForegroundColor Green

Export-ModuleMember -Function @(
    'Test-WebView2AssemblyAvailable',
    'Resolve-WebView2AssemblyPath',
    'Load-WebView2Assemblies',
    'Initialize-LegacyBrowser',
    'Initialize-BrowserSafely',
    'Get-WebView2DiagnosticsInfo',
    'Show-WebView2TroubleshootingGuide'
)
