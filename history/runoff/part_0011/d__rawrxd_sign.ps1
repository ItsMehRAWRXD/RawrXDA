# ============================================================================
# sign.ps1 — Binary Signing for RawrXD IDE
# ============================================================================
# Signs EXE/DLL binaries using Windows signtool.exe for SmartScreen compliance.
# Supports both certificate file (.pfx) and certificate store signing.
#
# Usage:
#   .\sign.ps1                          # Sign with defaults
#   .\sign.ps1 -CertPath .\cert.pfx    # Sign with specific certificate
#   .\sign.ps1 -SelfSign                # Generate and use self-signed cert
# ============================================================================

param(
    [string]$CertPath = "",
    [string]$CertPassword = "",
    [string]$CertSubject = "CN=RawrXD IDE, O=ItsMehRAWRXD",
    [string]$TimestampServer = "http://timestamp.digicert.com",
    [switch]$SelfSign,
    [switch]$Verbose,
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"
$script:SignedCount = 0
$script:FailedCount = 0

# ============================================================================
# Discover build output directory
# ============================================================================
function Get-BuildDirectory {
    if ($BuildDir -and (Test-Path $BuildDir)) { return $BuildDir }

    $candidates = @(
        (Join-Path $PSScriptRoot "build" "Release"),
        (Join-Path $PSScriptRoot "build"),
        (Join-Path $PSScriptRoot "bin" "Release"),
        (Join-Path $PSScriptRoot "bin"),
        (Join-Path $PSScriptRoot "out" "Release"),
        $PSScriptRoot
    )

    foreach ($c in $candidates) {
        if ((Test-Path $c) -and (Get-ChildItem $c -Filter "*.exe" -ErrorAction SilentlyContinue)) {
            return $c
        }
    }

    return $PSScriptRoot
}

# ============================================================================
# Find signtool.exe
# ============================================================================
function Find-SignTool {
    # Check PATH first
    $inPath = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($inPath) { return $inPath.Source }

    # Search Windows SDK directories
    $sdkPaths = @(
        "C:\Program Files (x86)\Windows Kits\10\bin",
        "C:\Program Files\Windows Kits\10\bin"
    )

    foreach ($sdkBase in $sdkPaths) {
        if (Test-Path $sdkBase) {
            $versions = Get-ChildItem $sdkBase -Directory | Sort-Object Name -Descending
            foreach ($ver in $versions) {
                $tool = Join-Path $ver.FullName "x64\signtool.exe"
                if (Test-Path $tool) { return $tool }
            }
        }
    }

    # VS2022 bundled
    $vs = "C:\VS2022Enterprise\Common7\Tools"
    if (Test-Path $vs) {
        $tool = Get-ChildItem $vs -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($tool) { return $tool.FullName }
    }

    return $null
}

# ============================================================================
# Generate self-signed certificate (for development/testing)
# ============================================================================
function New-SelfSignedCodeCert {
    param([string]$Subject)

    Write-Host "[sign] Generating self-signed code signing certificate..." -ForegroundColor Yellow

    $cert = New-SelfSignedCertificate `
        -Subject $Subject `
        -Type CodeSigningCert `
        -CertStoreLocation "Cert:\CurrentUser\My" `
        -KeyExportPolicy Exportable `
        -KeyUsage DigitalSignature `
        -NotAfter (Get-Date).AddYears(3) `
        -HashAlgorithm SHA256

    $pfxPath = Join-Path $PSScriptRoot "rawrxd_selfsigned.pfx"
    $securePassword = ConvertTo-SecureString -String "RawrXD2026!" -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $securePassword | Out-Null

    Write-Host "[sign] Self-signed certificate created: $pfxPath" -ForegroundColor Green
    Write-Host "[sign] Thumbprint: $($cert.Thumbprint)" -ForegroundColor Cyan

    return @{
        Path = $pfxPath
        Password = "RawrXD2026!"
        Thumbprint = $cert.Thumbprint
    }
}

# ============================================================================
# Sign a single binary
# ============================================================================
function Sign-Binary {
    param(
        [string]$SignToolPath,
        [string]$FilePath,
        [string]$CertFile,
        [string]$Password,
        [string]$Timestamp
    )

    $fileName = Split-Path $FilePath -Leaf

    $signArgs = @("sign", "/fd", "SHA256", "/v")

    if ($CertFile) {
        $signArgs += "/f"
        $signArgs += $CertFile
        if ($Password) {
            $signArgs += "/p"
            $signArgs += $Password
        }
    }

    if ($Timestamp) {
        $signArgs += "/tr"
        $signArgs += $Timestamp
        $signArgs += "/td"
        $signArgs += "SHA256"
    }

    $signArgs += $FilePath

    try {
        $output = & $SignToolPath @signArgs 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  [OK] $fileName" -ForegroundColor Green
            $script:SignedCount++
            return $true
        } else {
            Write-Host "  [FAIL] $fileName : $output" -ForegroundColor Red
            $script:FailedCount++
            return $false
        }
    } catch {
        Write-Host "  [ERROR] $fileName : $($_.Exception.Message)" -ForegroundColor Red
        $script:FailedCount++
        return $false
    }
}

# ============================================================================
# Main
# ============================================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  RawrXD IDE — Binary Signing Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find signtool
$signtool = Find-SignTool
if (-not $signtool) {
    Write-Host "[sign] ERROR: signtool.exe not found." -ForegroundColor Red
    Write-Host "[sign] Install Windows SDK or add signtool to PATH." -ForegroundColor Yellow
    exit 1
}
Write-Host "[sign] Using signtool: $signtool" -ForegroundColor Gray

# Handle certificate
$certFile = $CertPath
$certPwd = $CertPassword

if ($SelfSign) {
    $selfCert = New-SelfSignedCodeCert -Subject $CertSubject
    $certFile = $selfCert.Path
    $certPwd = $selfCert.Password
} elseif (-not $certFile) {
    # Look for .pfx in project root
    $pfxFiles = Get-ChildItem $PSScriptRoot -Filter "*.pfx" -ErrorAction SilentlyContinue
    if ($pfxFiles) {
        $certFile = $pfxFiles[0].FullName
        Write-Host "[sign] Found certificate: $certFile" -ForegroundColor Cyan
    } else {
        Write-Host "[sign] No certificate found. Use -CertPath or -SelfSign." -ForegroundColor Yellow
        Write-Host "[sign] Running with -SelfSign for development..." -ForegroundColor Yellow
        $selfCert = New-SelfSignedCodeCert -Subject $CertSubject
        $certFile = $selfCert.Path
        $certPwd = $selfCert.Password
    }
}

# Find binaries to sign
$buildDir = Get-BuildDirectory
Write-Host "[sign] Build directory: $buildDir" -ForegroundColor Gray

$binaries = @()
$binaries += Get-ChildItem $buildDir -Filter "RawrXD*.exe" -Recurse -ErrorAction SilentlyContinue
$binaries += Get-ChildItem $buildDir -Filter "*.dll" -Recurse -ErrorAction SilentlyContinue

if ($binaries.Count -eq 0) {
    Write-Host "[sign] No binaries found to sign in $buildDir" -ForegroundColor Yellow
    exit 0
}

Write-Host "[sign] Signing $($binaries.Count) binary/ies..." -ForegroundColor Cyan
Write-Host ""

foreach ($bin in $binaries) {
    Sign-Binary -SignToolPath $signtool -FilePath $bin.FullName `
                -CertFile $certFile -Password $certPwd `
                -Timestamp $TimestampServer
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Signed: $($script:SignedCount)  |  Failed: $($script:FailedCount)" -ForegroundColor $(if ($script:FailedCount -gt 0) { "Yellow" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan

if ($script:FailedCount -gt 0) { exit 1 }
