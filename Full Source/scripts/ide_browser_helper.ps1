#!/usr/bin/env pwsh
<#
.SYNOPSIS
    IDE Browser Helper - Provides web access for agents, models, and users

.DESCRIPTION
    Integrated browser functionality for the RawrXD IDE ecosystem.
    Agents and models can use this to access documentation, driver downloads,
    and other web resources to provide better answers to user questions.

.EXAMPLE
    .\ide_browser_helper.ps1 -Action OpenDriverPage -Vendor "NVIDIA"
    
.EXAMPLE
    .\ide_browser_helper.ps1 -Action SearchDocumentation -Query "Python asyncio"
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("OpenDriverPage", "SearchDocumentation", "OpenResource", "GetDriverInfo", "OpenChatbot")]
    [string]$Action = "OpenChatbot",
    
    [Parameter(Mandatory=$false)]
    [string]$Vendor = "",
    
    [Parameter(Mandatory=$false)]
    [string]$Query = "",
    
    [Parameter(Mandatory=$false)]
    [string]$URL = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:BrowserRoot = Split-Path $PSScriptRoot -Parent
$script:ChatbotPath = Join-Path $BrowserRoot "gui\ide_chatbot.html"

# ═══════════════════════════════════════════════════════════════════════════════
# DRIVER INFORMATION DATABASE
# ═══════════════════════════════════════════════════════════════════════════════

$script:DriverResources = @{
    NVIDIA = @{
        Name = "NVIDIA Graphics Drivers"
        AutoDetectURL = "https://www.nvidia.com/download/index.aspx"
        ManualURL = "https://www.nvidia.com/Download/Find.aspx"
        ToolURL = "https://www.nvidia.com/en-us/geforce/geforce-experience/"
        CheckCommand = "nvidia-smi"
        Description = "NVIDIA GeForce, RTX, GTX, and Quadro drivers"
    }
    AMD = @{
        Name = "AMD Graphics Drivers"  
        AutoDetectURL = "https://www.amd.com/en/support"
        ManualURL = "https://www.amd.com/en/support/download/drivers.html"
        ToolURL = "https://www.amd.com/en/technologies/software"
        CheckCommand = "Get-WmiObject Win32_VideoController | Where-Object { $_.Name -like '*AMD*' -or $_.Name -like '*Radeon*' }"
        Description = "AMD Radeon graphics drivers"
    }
    Intel = @{
        Name = "Intel Graphics Drivers"
        AutoDetectURL = "https://www.intel.com/content/www/us/en/support/detect.html"
        ManualURL = "https://downloadcenter.intel.com/product/80939/Graphics"
        ToolURL = "https://www.intel.com/content/www/us/en/support/intel-driver-support-assistant.html"
        CheckCommand = "Get-WmiObject Win32_VideoController | Where-Object { $_.Name -like '*Intel*' }"
        Description = "Intel integrated and Arc graphics drivers"
    }
}

$script:DocumentationResources = @{
    Python = "https://docs.python.org/"
    PowerShell = "https://docs.microsoft.com/powershell/"
    Qt = "https://doc.qt.io/"
    PyTorch = "https://pytorch.org/docs/"
    TensorFlow = "https://www.tensorflow.org/api_docs"
    HuggingFace = "https://huggingface.co/docs"
    Ollama = "https://github.com/ollama/ollama/blob/main/README.md"
    LlamaCpp = "https://github.com/ggerganov/llama.cpp"
    GGUF = "https://github.com/ggerganov/ggml/blob/master/docs/gguf.md"
    Vulkan = "https://www.vulkan.org/learn"
    DirectX = "https://docs.microsoft.com/windows/win32/direct3d12/"
    OpenCL = "https://www.khronos.org/opencl/"
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Open-IDEChatbot {
    <#
    .SYNOPSIS
        Opens the IDE chatbot with web browser capabilities
    #>
    
    if (-not (Test-Path $ChatbotPath)) {
        Write-Host "❌ IDE Chatbot not found at: $ChatbotPath" -ForegroundColor Red
        return $false
    }
    
    Write-Host "🌐 Opening IDE Browser Assistant..." -ForegroundColor Cyan
    Start-Process $ChatbotPath
    Start-Sleep -Milliseconds 500
    Write-Host "✓ IDE Browser opened successfully" -ForegroundColor Green
    return $true
}

function Open-DriverPage {
    param([string]$VendorName)
    
    if (-not $DriverResources.ContainsKey($VendorName)) {
        Write-Host "❌ Unknown vendor: $VendorName" -ForegroundColor Red
        Write-Host "Available vendors: $($DriverResources.Keys -join ', ')" -ForegroundColor Yellow
        return $false
    }
    
    $driver = $DriverResources[$VendorName]
    
    Write-Host "`n╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  $($driver.Name.PadRight(59)) ║" -ForegroundColor Cyan
    Write-Host "╚═══════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    Write-Host "Description: $($driver.Description)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Available Options:" -ForegroundColor Yellow
    Write-Host "  [1] Auto-Detect (Recommended)" -ForegroundColor White
    Write-Host "  [2] Manual Download" -ForegroundColor White
    Write-Host "  [3] Software Tool" -ForegroundColor White
    Write-Host "  [4] Check Current Driver" -ForegroundColor White
    Write-Host "  [0] Cancel" -ForegroundColor DarkGray
    Write-Host ""
    
    $choice = Read-Host "Select option"
    
    switch ($choice) {
        '1' {
            Write-Host "🌐 Opening auto-detect page..." -ForegroundColor Cyan
            Start-Process $driver.AutoDetectURL
        }
        '2' {
            Write-Host "🌐 Opening manual download page..." -ForegroundColor Cyan
            Start-Process $driver.ManualURL
        }
        '3' {
            Write-Host "🌐 Opening software tool page..." -ForegroundColor Cyan
            Start-Process $driver.ToolURL
        }
        '4' {
            Write-Host "`nChecking current driver..." -ForegroundColor Cyan
            try {
                if ($VendorName -eq "NVIDIA" -and (Get-Command nvidia-smi -ErrorAction SilentlyContinue)) {
                    nvidia-smi --query-gpu=driver_version --format=csv,noheader
                } else {
                    Get-WmiObject Win32_VideoController | Where-Object { $_.Name -like "*$VendorName*" } | 
                        Select-Object Name, DriverVersion, DriverDate | Format-Table -AutoSize
                }
            } catch {
                Write-Host "Unable to detect driver. Opening detection tool..." -ForegroundColor Yellow
                Start-Process $driver.AutoDetectURL
            }
        }
        '0' {
            Write-Host "Cancelled" -ForegroundColor Gray
        }
        default {
            Write-Host "Invalid choice" -ForegroundColor Red
        }
    }
    
    return $true
}

function Search-Documentation {
    param([string]$SearchQuery)
    
    Write-Host "`n📚 Documentation Resources:" -ForegroundColor Yellow
    Write-Host ""
    
    $i = 1
    $resources = @()
    foreach ($key in $DocumentationResources.Keys | Sort-Object) {
        $resources += @{ Index = $i; Name = $key; URL = $DocumentationResources[$key] }
        Write-Host "  [$i] $key" -ForegroundColor Cyan
        $i++
    }
    
    Write-Host "  [S] Search Google for: `"$SearchQuery`"" -ForegroundColor White
    Write-Host "  [0] Cancel" -ForegroundColor DarkGray
    Write-Host ""
    
    $choice = Read-Host "Select resource (or S to search)"
    
    if ($choice -eq 'S' -or $choice -eq 's') {
        $searchURL = "https://www.google.com/search?q=" + [System.Web.HttpUtility]::UrlEncode($SearchQuery)
        Write-Host "🔍 Searching Google for: $SearchQuery" -ForegroundColor Cyan
        Start-Process $searchURL
        return $true
    }
    
    $idx = [int]$choice
    if ($idx -gt 0 -and $idx -le $resources.Count) {
        $selected = $resources[$idx - 1]
        Write-Host "🌐 Opening $($selected.Name) documentation..." -ForegroundColor Cyan
        Start-Process $selected.URL
        return $true
    }
    
    if ($choice -ne '0') {
        Write-Host "Invalid choice" -ForegroundColor Red
    }
    
    return $false
}

function Open-WebResource {
    param([string]$ResourceURL)
    
    if ([string]::IsNullOrWhiteSpace($ResourceURL)) {
        Write-Host "❌ No URL provided" -ForegroundColor Red
        return $false
    }
    
    # Validate URL
    try {
        $uri = [System.Uri]$ResourceURL
        if ($uri.Scheme -notin @('http', 'https')) {
            Write-Host "❌ Invalid URL scheme. Only http/https allowed." -ForegroundColor Red
            return $false
        }
    } catch {
        Write-Host "❌ Invalid URL format" -ForegroundColor Red
        return $false
    }
    
    Write-Host "🌐 Opening: $ResourceURL" -ForegroundColor Cyan
    Start-Process $ResourceURL
    return $true
}

function Get-DriverInformation {
    <#
    .SYNOPSIS
        Returns driver information for use by agents/models
    #>
    
    $driverInfo = @{
        InstalledDrivers = @()
        AvailableVendors = @()
        Recommendations = @()
    }
    
    # Get installed graphics adapters
    try {
        $adapters = Get-WmiObject Win32_VideoController
        foreach ($adapter in $adapters) {
            $driverInfo.InstalledDrivers += @{
                Name = $adapter.Name
                Driver = $adapter.DriverVersion
                Date = $adapter.DriverDate
                Status = $adapter.Status
            }
            
            # Determine vendor
            if ($adapter.Name -like "*NVIDIA*" -or $adapter.Name -like "*GeForce*" -or $adapter.Name -like "*RTX*" -or $adapter.Name -like "*GTX*") {
                if ("NVIDIA" -notin $driverInfo.AvailableVendors) {
                    $driverInfo.AvailableVendors += "NVIDIA"
                }
            }
            if ($adapter.Name -like "*AMD*" -or $adapter.Name -like "*Radeon*") {
                if ("AMD" -notin $driverInfo.AvailableVendors) {
                    $driverInfo.AvailableVendors += "AMD"
                }
            }
            if ($adapter.Name -like "*Intel*") {
                if ("Intel" -notin $driverInfo.AvailableVendors) {
                    $driverInfo.AvailableVendors += "Intel"
                }
            }
        }
    } catch {
        Write-Host "⚠️ Unable to detect graphics adapters" -ForegroundColor Yellow
    }
    
    # Add recommendations
    foreach ($vendor in $driverInfo.AvailableVendors) {
        $resource = $DriverResources[$vendor]
        $driverInfo.Recommendations += @{
            Vendor = $vendor
            AutoDetectURL = $resource.AutoDetectURL
            Description = $resource.Description
        }
    }
    
    return $driverInfo | ConvertTo-Json -Depth 10
}

# ═══════════════════════════════════════════════════════════════════════════════
# AGENT/MODEL ACCESS FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function Invoke-AgentBrowserAccess {
    <#
    .SYNOPSIS
        Provides programmatic access for agents and models
    .DESCRIPTION
        Agents can call this function to get URLs and information
        without opening browser windows
    #>
    param(
        [Parameter(Mandatory=$true)]
        [ValidateSet("GetDriverURL", "GetDocURL", "SearchQuery", "GetDriverInfo")]
        [string]$Query,
        
        [Parameter(Mandatory=$false)]
        [string]$Parameter = ""
    )
    
    switch ($Query) {
        "GetDriverURL" {
            if ($DriverResources.ContainsKey($Parameter)) {
                return $DriverResources[$Parameter].AutoDetectURL
            }
            return $null
        }
        "GetDocURL" {
            if ($DocumentationResources.ContainsKey($Parameter)) {
                return $DocumentationResources[$Parameter]
            }
            return $null
        }
        "SearchQuery" {
            return "https://www.google.com/search?q=" + [System.Web.HttpUtility]::UrlEncode($Parameter)
        }
        "GetDriverInfo" {
            return Get-DriverInformation
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

switch ($Action) {
    "OpenChatbot" {
        Open-IDEChatbot
    }
    "OpenDriverPage" {
        if ([string]::IsNullOrWhiteSpace($Vendor)) {
            Write-Host "Available vendors:" -ForegroundColor Yellow
            foreach ($v in $DriverResources.Keys | Sort-Object) {
                Write-Host "  • $v" -ForegroundColor Cyan
            }
            Write-Host ""
            $Vendor = Read-Host "Select vendor"
        }
        Open-DriverPage -VendorName $Vendor
    }
    "SearchDocumentation" {
        if ([string]::IsNullOrWhiteSpace($Query)) {
            $Query = Read-Host "Enter search query"
        }
        Search-Documentation -SearchQuery $Query
    }
    "OpenResource" {
        if ([string]::IsNullOrWhiteSpace($URL)) {
            $URL = Read-Host "Enter URL"
        }
        Open-WebResource -ResourceURL $URL
    }
    "GetDriverInfo" {
        $info = Get-DriverInformation
        Write-Host $info -ForegroundColor Cyan
    }
}

Export-ModuleMember -Function @(
    'Open-IDEChatbot',
    'Open-DriverPage',
    'Search-Documentation',
    'Open-WebResource',
    'Get-DriverInformation',
    'Invoke-AgentBrowserAccess'
)
