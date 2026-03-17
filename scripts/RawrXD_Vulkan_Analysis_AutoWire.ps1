<#
.SYNOPSIS
    Auto-discovers Vulkan SDK and GPU analysis tools, patches build, generates analysis.ical manifest.
.DESCRIPTION
    RawrXD Vulkan & Analysis Auto-Wire. Run from repo root or with -ProjectRoot.
    Creates: analysis.ical.json, diff_config.asm (optional), patches env for build.
.EXAMPLE
    .\scripts\RawrXD_Vulkan_Analysis_AutoWire.ps1 -ProjectRoot "D:\rawrxd" -GenerateIcal
#>
param(
    [string]$ProjectRoot = (Get-Location).Path,
    [switch]$InstallHooks,
    [switch]$GenerateIcal
)

$ErrorActionPreference = "Stop"

$RegPaths = @(
    "HKLM:\SOFTWARE\Khronos\Vulkan\CurrentVersion",
    "HKLM:\SOFTWARE\WOW6432Node\Khronos\Vulkan\CurrentVersion",
    "HKCU:\SOFTWARE\Khronos\Vulkan\CurrentVersion"
)

$VulkanSdk = $null
$AnalysisTools = @{}

# Resolve project root
if (-not [System.IO.Path]::IsPathRooted($ProjectRoot)) {
    $ProjectRoot = Join-Path (Get-Location) $ProjectRoot
}
$ProjectRoot = [System.IO.Path]::GetFullPath($ProjectRoot)

Write-Host "[*] RawrXD Vulkan & Analysis AutoWire" -ForegroundColor Cyan
Write-Host "    ProjectRoot: $ProjectRoot" -ForegroundColor Gray

# 1. VULKAN DISCOVERY
Write-Host "[*] Probing for Vulkan SDK..." -ForegroundColor Cyan
foreach ($reg in $RegPaths) {
    if (Test-Path $reg) {
        try {
            $VulkanSdk = (Get-ItemProperty $reg -Name "VK_SDK_PATH" -ErrorAction SilentlyContinue).VK_SDK_PATH
            if (-not $VulkanSdk) { $VulkanSdk = (Get-ItemProperty $reg -Name "VulkanSDK" -ErrorAction SilentlyContinue).VulkanSDK }
            if ($VulkanSdk -and (Test-Path "$VulkanSdk\Include\vulkan\vulkan.h")) { break }
        } catch { }
    }
}

if (-not $VulkanSdk) { $VulkanSdk = $env:VULKAN_SDK }

if (-not $VulkanSdk -or -not (Test-Path "$VulkanSdk\Include\vulkan\vulkan.h")) {
    $HuntPaths = @(
        "${env:ProgramFiles}\VulkanSDK",
        "${env:ProgramFiles(x86)}\VulkanSDK",
        "${env:LOCALAPPDATA}\VulkanSDK",
        "C:\VulkanSDK",
        "D:\VulkanSDK"
    )
    foreach ($path in $HuntPaths) {
        if (Test-Path $path) {
            $Latest = Get-ChildItem $path -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending | Select-Object -First 1
            if ($Latest -and (Test-Path "$($Latest.FullName)\Include\vulkan\vulkan.h")) {
                $VulkanSdk = $Latest.FullName
                break
            }
        }
    }
}

if (-not $VulkanSdk) {
    Write-Host "[-] Vulkan SDK not found. Install from https://vulkan.lunarg.com/" -ForegroundColor Yellow
    $VulkanSdk = ""
} else {
    Write-Host "[+] Vulkan SDK: $VulkanSdk" -ForegroundColor Green
}

# 2. ANALYSIS TOOL DISCOVERY
Write-Host "[*] Hunting GPU analysis tools..." -ForegroundColor Cyan
$ToolRegistry = @{
    "RenderDoc" = @(
        "${env:ProgramFiles}\RenderDoc\qrenderdoc.exe",
        "${env:ProgramData}\chocolatey\bin\qrenderdoc.exe"
    )
    "Nsight"    = @(
        (Get-ChildItem "${env:ProgramFiles}\NVIDIA Corporation\Nsight*" -Directory -ErrorAction SilentlyContinue | ForEach-Object { Join-Path $_.FullName "nsight-gfx.exe" }),
        (Get-ChildItem "${env:ProgramFiles(x86)}\NVIDIA Corporation\Nsight*" -Directory -ErrorAction SilentlyContinue | ForEach-Object { Join-Path $_.FullName "NVidia.Nsight.VS.exe" })
    )
    "PIX"       = @(
        "${env:ProgramFiles}\Microsoft PIX\WinPixGpuCapturer.dll",
        "${env:LOCALAPPDATA}\Microsoft\PIX\*\WinPixGpuCapturer.dll"
    )
}

foreach ($Tool in $ToolRegistry.Keys) {
    $Found = $null
    foreach ($p in $ToolRegistry[$Tool]) {
        if ($p -match '\*') {
            $resolved = Get-Item $p -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($resolved) { $Found = $resolved.FullName; break }
        } elseif (Test-Path $p) {
            $Found = $p
            break
        }
    }
    if ($Found) {
        $AnalysisTools[$Tool] = $Found
        Write-Host "  [+] $Tool -> $Found" -ForegroundColor Green
    } else {
        Write-Host "  [-] $Tool not found" -ForegroundColor DarkGray
    }
}

# 3. GENERATE analysis.ical.json (Intelligent Computation Analysis Layer)
if ($GenerateIcal) {
    $Ical = @{
        schema   = "RawrXD-Analysis-1.0"
        generated = (Get-Date -Format "o")
        vulkan   = @{
            sdk_path = if ($VulkanSdk) { $VulkanSdk } else { $null }
            version  = if ($VulkanSdk -and (Test-Path "$VulkanSdk\version.txt")) { (Get-Content "$VulkanSdk\version.txt" -Raw -ErrorAction SilentlyContinue) -replace "`r`n", "" } else { $null }
            loader   = if ($VulkanSdk) { "$VulkanSdk\Bin\vulkan-1.dll" } else { $null }
        }
        analysis_tools = $AnalysisTools
        targets = @(
            @{ name = "RawrXD-Win32IDE"; executable = "RawrXD-Win32IDE.exe"; capture_mode = "frame" }
            @{ name = "RawrXD-InferenceEngine"; executable = "RawrXD-InferenceEngine.exe"; capture_mode = "frame" }
            @{ name = "RawrXD-ModelAnalysis"; executable = "RawrXD-ModelAnalysis.exe"; capture_mode = "none"; stream_to_terminal = $true }
        )
    }
    $IcalPath = Join-Path $ProjectRoot "analysis.ical.json"
    $Ical | ConvertTo-Json -Depth 6 | Set-Content $IcalPath -Force -Encoding UTF8
    Write-Host "[+] Generated: $IcalPath" -ForegroundColor Green
}

# 4. SET ENV FOR CURRENT SESSION (so next build sees Vulkan)
if ($VulkanSdk) {
    $env:VULKAN_SDK = $VulkanSdk
    $env:PATH = "$VulkanSdk\Bin;$env:PATH"
    $env:INCLUDE = "$VulkanSdk\Include;$env:INCLUDE"
    $env:LIB = "$VulkanSdk\Lib;$env:LIB"
}

# 5. VALIDATION
if ($VulkanSdk) {
    $vulkaninfo = Join-Path $VulkanSdk "Bin\vulkaninfo.exe"
    if (Test-Path $vulkaninfo) {
        Write-Host "[*] Validating Vulkan..." -ForegroundColor Cyan
        & $vulkaninfo --summary 2>$null | Select-String "GPU" | ForEach-Object { Write-Host "  [OK] $_" -ForegroundColor Green }
    }
}

Write-Host "`n[+] AutoWire complete. SDK: $($VulkanSdk ?? 'none'); Tools: $($AnalysisTools.Count)" -ForegroundColor Green
