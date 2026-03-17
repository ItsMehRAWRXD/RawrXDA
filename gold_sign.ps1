# ============================================================================
# gold_sign.ps1 — GOLD_SIGN: EV Certificate Signing for Production Release
# ============================================================================
# Signs EXE/DLL binaries using an EV (Extended Validation) code signing
# certificate for instant Windows SmartScreen reputation.
#
# EV certificates use hardware-protected private keys (HSM / USB token /
# cloud KMS) — the key never exists as a PFX file.  Signing uses either:
#   1. Certificate-store thumbprint  (/sha1)
#   2. Cloud HSM via DigiCert KeyLocker / Azure Trusted Signing
#   3. Hardware token via CSP + key-container (/csp /kc)
#
# Environment Variables:
#   GOLD_SIGN_THUMBPRINT   — SHA-1 thumbprint of the EV cert in cert store
#   GOLD_SIGN_SUBJECT      — Subject name (/n) for auto-select from store
#   GOLD_SIGN_CSP           — Cryptographic Service Provider (SafeNet, etc.)
#   GOLD_SIGN_KC            — Key container name on the CSP
#   GOLD_SIGN_TOKEN_PIN     — Hardware token PIN (SafeNet / YubiKey)
#   GOLD_SIGN_TIMESTAMP     — RFC 3161 timestamp URL (default: DigiCert)
#   GOLD_SIGN_STORE         — Certificate store name (default: My)
#   GOLD_SIGN_DIGEST        — Digest algorithm (default: SHA256)
#   GOLD_SIGN_AZURE_TENANT  — Azure Trusted Signing tenant ID
#   GOLD_SIGN_AZURE_ENDPOINT — Azure Trusted Signing endpoint
#   GOLD_SIGN_AZURE_PROFILE  — Azure Trusted Signing profile name
#   GOLD_SIGN_CROSS_CERT    — Path to cross-signing certificate (legacy)
#   SIGNTOOL_PATH           — Explicit path to signtool.exe
#
# Usage:
#   .\gold_sign.ps1                                      # Auto-detect EV cert
#   .\gold_sign.ps1 -Thumbprint "ABC123..."              # Explicit thumbprint
#   .\gold_sign.ps1 -AzureTrustedSigning                 # Azure cloud HSM
#   .\gold_sign.ps1 -DualSign                            # SHA1 + SHA256
#   .\gold_sign.ps1 -Verify                              # Verify only
#   .\gold_sign.ps1 -BuildDir .\build\Release -DryRun    # Preview
# ============================================================================

[CmdletBinding()]
param(
    [string]$Thumbprint       = "",
    [string]$SubjectName      = "",
    [string]$CSP              = "",
    [string]$KeyContainer     = "",
    [string]$TokenPin         = "",
    [string]$TimestampServer  = "",
    [string]$CertStore        = "",
    [string]$DigestAlg        = "",
    [string]$CrossCert        = "",
    [string]$BuildDir         = "",
    [switch]$AzureTrustedSigning,
    [switch]$DualSign,
    [switch]$Verify,
    [switch]$DryRun,
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ============================================================================
# Counters
# ============================================================================
$script:Signed  = 0
$script:Failed  = 0
$script:Skipped = 0
$script:Verified = 0

# ============================================================================
# Resolve configuration from params → env → defaults
# ============================================================================
function Resolve-Config {
    $script:cfg = @{
        Thumbprint     = if ($Thumbprint)      { $Thumbprint }      elseif ($env:GOLD_SIGN_THUMBPRINT)  { $env:GOLD_SIGN_THUMBPRINT }  else { "" }
        SubjectName    = if ($SubjectName)      { $SubjectName }     elseif ($env:GOLD_SIGN_SUBJECT)     { $env:GOLD_SIGN_SUBJECT }     else { "" }
        CSP            = if ($CSP)              { $CSP }             elseif ($env:GOLD_SIGN_CSP)         { $env:GOLD_SIGN_CSP }         else { "" }
        KeyContainer   = if ($KeyContainer)     { $KeyContainer }    elseif ($env:GOLD_SIGN_KC)          { $env:GOLD_SIGN_KC }          else { "" }
        TokenPin       = if ($TokenPin)         { $TokenPin }        elseif ($env:GOLD_SIGN_TOKEN_PIN)   { $env:GOLD_SIGN_TOKEN_PIN }   else { "" }
        Timestamp      = if ($TimestampServer)  { $TimestampServer } elseif ($env:GOLD_SIGN_TIMESTAMP)   { $env:GOLD_SIGN_TIMESTAMP }   else { "http://timestamp.digicert.com" }
        Store          = if ($CertStore)        { $CertStore }       elseif ($env:GOLD_SIGN_STORE)       { $env:GOLD_SIGN_STORE }       else { "My" }
        Digest         = if ($DigestAlg)        { $DigestAlg }       elseif ($env:GOLD_SIGN_DIGEST)      { $env:GOLD_SIGN_DIGEST }      else { "SHA256" }
        CrossCert      = if ($CrossCert)        { $CrossCert }       elseif ($env:GOLD_SIGN_CROSS_CERT)  { $env:GOLD_SIGN_CROSS_CERT }  else { "" }
        AzureTenant    = if ($env:GOLD_SIGN_AZURE_TENANT)   { $env:GOLD_SIGN_AZURE_TENANT }   else { "" }
        AzureEndpoint  = if ($env:GOLD_SIGN_AZURE_ENDPOINT) { $env:GOLD_SIGN_AZURE_ENDPOINT } else { "" }
        AzureProfile   = if ($env:GOLD_SIGN_AZURE_PROFILE)  { $env:GOLD_SIGN_AZURE_PROFILE }  else { "" }
    }
}

# ============================================================================
# Find signtool.exe
# ============================================================================
function Find-SignTool {
    # Explicit override
    if ($env:SIGNTOOL_PATH -and (Test-Path $env:SIGNTOOL_PATH)) {
        return $env:SIGNTOOL_PATH
    }

    # PATH
    $inPath = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($inPath) { return $inPath.Source }

    # Windows SDK (newest first)
    $sdkRoots = @(
        "${env:ProgramFiles(x86)}\Windows Kits\10\bin",
        "${env:ProgramFiles}\Windows Kits\10\bin"
    )
    foreach ($root in $sdkRoots) {
        if (Test-Path $root) {
            $versions = Get-ChildItem $root -Directory | Sort-Object Name -Descending
            foreach ($ver in $versions) {
                $tool = Join-Path $ver.FullName "x64\signtool.exe"
                if (Test-Path $tool) { return $tool }
            }
        }
    }

    # VS2022 bundled
    $vsPaths = @(
        "C:\VS2022Enterprise\Common7\Tools",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools"
    )
    foreach ($vs in $vsPaths) {
        if (Test-Path $vs) {
            $found = Get-ChildItem $vs -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue |
                     Select-Object -First 1
            if ($found) { return $found.FullName }
        }
    }

    return $null
}

# ============================================================================
# Find Azure Sign Tool (for Azure Trusted Signing)
# ============================================================================
function Find-AzureSignTool {
    $inPath = Get-Command AzureSignTool.exe -ErrorAction SilentlyContinue
    if ($inPath) { return $inPath.Source }

    # Try dotnet tool
    $dotnetTool = Get-Command dotnet -ErrorAction SilentlyContinue
    if ($dotnetTool) {
        $toolList = & dotnet tool list -g 2>$null
        if ($toolList -match "azuresigntool") {
            $home = if ($env:USERPROFILE) { $env:USERPROFILE } else { $env:HOME }
            $azTool = Join-Path $home ".dotnet\tools\AzureSignTool.exe"
            if (Test-Path $azTool) { return $azTool }
        }
    }

    return $null
}

# ============================================================================
# Detect EV certificate in the certificate store
# ============================================================================
function Find-EVCertificate {
    Write-Host "[GOLD_SIGN] Scanning certificate store for EV code-signing certificates..." -ForegroundColor Gray

    $store = $script:cfg.Store
    $certs = Get-ChildItem "Cert:\CurrentUser\$store" -CodeSigningCert -ErrorAction SilentlyContinue
    $certs += Get-ChildItem "Cert:\LocalMachine\$store" -CodeSigningCert -ErrorAction SilentlyContinue

    # Filter: EV certs have specific OID policies
    # OID 2.23.140.1.3 = Extended Validation Code Signing
    $evOid = "2.23.140.1.3"
    $evCerts = @()

    foreach ($cert in $certs) {
        if (-not $cert.HasPrivateKey) { continue }
        if ($cert.NotAfter -lt (Get-Date)) { continue }

        $isEV = $false
        foreach ($ext in $cert.Extensions) {
            if ($ext.Oid.Value -eq "2.5.29.32") {
                # Certificate Policies extension
                $raw = $ext.Format($false)
                if ($raw -match $evOid) { $isEV = $true; break }
            }
        }

        if ($isEV) {
            $evCerts += $cert
            Write-Host "  [EV] $($cert.Subject) | Thumbprint: $($cert.Thumbprint) | Expires: $($cert.NotAfter)" -ForegroundColor Green
        }
    }

    if ($evCerts.Count -eq 0) {
        Write-Host "  [WARN] No EV code-signing certificates found in store." -ForegroundColor Yellow
        # Fall back to any code signing cert
        $fallback = $certs | Where-Object { $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } |
                    Sort-Object NotAfter -Descending | Select-Object -First 1
        if ($fallback) {
            Write-Host "  [INFO] Using standard code-signing cert as fallback: $($fallback.Subject)" -ForegroundColor Cyan
            return $fallback.Thumbprint
        }
        return $null
    }

    # Pick the one expiring latest
    $best = $evCerts | Sort-Object NotAfter -Descending | Select-Object -First 1
    Write-Host "  [SELECTED] $($best.Subject) | Thumbprint: $($best.Thumbprint)" -ForegroundColor Cyan
    return $best.Thumbprint
}

# ============================================================================
# Build signtool argument list
# ============================================================================
function Build-SignArgs {
    param([string]$TargetFile, [string]$Algo)

    $args = @("sign")

    # Digest algorithm
    $args += "/fd"
    $args += $Algo

    # Certificate selection — priority: thumbprint > subject > CSP > auto
    if ($script:cfg.Thumbprint) {
        $args += "/sha1"
        $args += $script:cfg.Thumbprint
        $args += "/s"
        $args += $script:cfg.Store
    }
    elseif ($script:cfg.SubjectName) {
        $args += "/n"
        $args += $script:cfg.SubjectName
        $args += "/s"
        $args += $script:cfg.Store
    }
    elseif ($script:cfg.CSP -and $script:cfg.KeyContainer) {
        # Hardware token (SafeNet eToken, YubiKey, etc.)
        $args += "/csp"
        $args += $script:cfg.CSP
        $args += "/kc"
        $args += $script:cfg.KeyContainer
        if ($script:cfg.Thumbprint) {
            $args += "/sha1"
            $args += $script:cfg.Thumbprint
        }
    }
    else {
        # Auto-select best cert
        $args += "/a"
        $args += "/s"
        $args += $script:cfg.Store
    }

    # Cross-certificate (for kernel-mode drivers or legacy chains)
    if ($script:cfg.CrossCert -and (Test-Path $script:cfg.CrossCert)) {
        $args += "/ac"
        $args += $script:cfg.CrossCert
    }

    # RFC 3161 Timestamp
    $args += "/tr"
    $args += $script:cfg.Timestamp
    $args += "/td"
    $args += $Algo

    # Verbose
    $args += "/v"

    # Target
    $args += $TargetFile

    return $args
}

# ============================================================================
# Sign with Azure Trusted Signing (cloud HSM)
# ============================================================================
function Invoke-AzureTrustedSign {
    param([string]$SignToolPath, [string]$TargetFile)

    $azTool = Find-AzureSignTool
    if (-not $azTool) {
        Write-Host "[GOLD_SIGN] AzureSignTool not found. Install with: dotnet tool install -g AzureSignTool" -ForegroundColor Red
        return $false
    }

    $azArgs = @(
        "sign",
        "-kvu", $script:cfg.AzureEndpoint,
        "-kvt", $script:cfg.AzureTenant,
        "-kvc", $script:cfg.AzureProfile,
        "-fd", $script:cfg.Digest,
        "-tr", $script:cfg.Timestamp,
        "-td", $script:cfg.Digest,
        "-v",
        $TargetFile
    )

    if ($DryRun) {
        Write-Host "  [DRY-RUN] $azTool $($azArgs -join ' ')" -ForegroundColor DarkGray
        return $true
    }

    $output = & $azTool @azArgs 2>&1
    return ($LASTEXITCODE -eq 0)
}

# ============================================================================
# Sign a single binary (standard signtool path)
# ============================================================================
function Invoke-GoldSign {
    param([string]$SignToolPath, [string]$TargetFile)

    $fileName = Split-Path $TargetFile -Leaf

    if ($DualSign) {
        # Pass 1: SHA-1 for legacy Windows (7/8)
        Write-Host "  [SHA1] $fileName" -ForegroundColor Gray -NoNewline
        $sha1Args = Build-SignArgs -TargetFile $TargetFile -Algo "SHA1"

        if (-not $DryRun) {
            $out1 = & $SignToolPath @sha1Args 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host " FAIL" -ForegroundColor Red
                Write-Host "       $out1" -ForegroundColor DarkRed
                $script:Failed++
                return $false
            }
            Write-Host " OK" -ForegroundColor Green
        } else {
            Write-Host " (dry-run)" -ForegroundColor DarkGray
        }

        # Pass 2: SHA-256 appended signature /as
        Write-Host "  [SHA256/as] $fileName" -ForegroundColor Gray -NoNewline
        $sha256Args = Build-SignArgs -TargetFile $TargetFile -Algo "SHA256"
        # Replace /fd at index and insert /as for append
        $sha256Args = @("sign", "/as") + ($sha256Args | Select-Object -Skip 1)

        if (-not $DryRun) {
            $out2 = & $SignToolPath @sha256Args 2>&1
            if ($LASTEXITCODE -ne 0) {
                Write-Host " FAIL" -ForegroundColor Red
                Write-Host "       $out2" -ForegroundColor DarkRed
                $script:Failed++
                return $false
            }
            Write-Host " OK" -ForegroundColor Green
        } else {
            Write-Host " (dry-run)" -ForegroundColor DarkGray
        }
    }
    else {
        # Single SHA-256 signature (standard modern signing)
        $signArgs = Build-SignArgs -TargetFile $TargetFile -Algo $script:cfg.Digest

        if ($DryRun) {
            Write-Host "  [DRY-RUN] $fileName → $SignToolPath $($signArgs -join ' ')" -ForegroundColor DarkGray
            $script:Signed++
            return $true
        }

        $output = & $SignToolPath @signArgs 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "  [GOLD] $fileName" -ForegroundColor Green
            $script:Signed++
            return $true
        } else {
            Write-Host "  [FAIL] $fileName" -ForegroundColor Red
            Write-Host "         $output" -ForegroundColor DarkRed
            $script:Failed++
            return $false
        }
    }

    $script:Signed++
    return $true
}

# ============================================================================
# Verify a signed binary
# ============================================================================
function Test-Signature {
    param([string]$SignToolPath, [string]$TargetFile)

    $fileName = Split-Path $TargetFile -Leaf

    # signtool verify /pa /v <file>
    $verifyArgs = @("verify", "/pa", "/v", $TargetFile)
    $output = & $SignToolPath @verifyArgs 2>&1
    $ok = ($LASTEXITCODE -eq 0)

    # Also check with PowerShell Authenticode
    $sig = Get-AuthenticodeSignature -FilePath $TargetFile -ErrorAction SilentlyContinue

    if ($ok -and $sig -and $sig.Status -eq "Valid") {
        $issuer = $sig.SignerCertificate.Issuer
        $subject = $sig.SignerCertificate.Subject
        $expiry = $sig.SignerCertificate.NotAfter
        $isTimestamped = ($sig.TimeStamperCertificate -ne $null)

        Write-Host "  [VERIFIED] $fileName" -ForegroundColor Green
        Write-Host "     Subject:     $subject" -ForegroundColor Gray
        Write-Host "     Issuer:      $issuer" -ForegroundColor Gray
        Write-Host "     Expires:     $expiry" -ForegroundColor Gray
        Write-Host "     Timestamped: $isTimestamped" -ForegroundColor $(if ($isTimestamped) { "Green" } else { "Yellow" })
        $script:Verified++
        return $true
    } else {
        $status = if ($sig) { $sig.Status } else { "NoSignature" }
        Write-Host "  [INVALID] $fileName → Status: $status" -ForegroundColor Red
        $script:Failed++
        return $false
    }
}

# ============================================================================
# Discover build output directory
# ============================================================================
function Get-BuildDirectory {
    if ($BuildDir -and (Test-Path $BuildDir)) { return $BuildDir }

    $candidates = @(
        (Join-Path $PSScriptRoot "build" "Release"),
        (Join-Path $PSScriptRoot "build" "RelWithDebInfo"),
        (Join-Path $PSScriptRoot "build"),
        (Join-Path $PSScriptRoot "bin" "Release"),
        (Join-Path $PSScriptRoot "bin"),
        (Join-Path $PSScriptRoot "out" "Release"),
        $PSScriptRoot
    )

    foreach ($c in $candidates) {
        if ((Test-Path $c) -and
            (Get-ChildItem $c -Filter "*.exe" -ErrorAction SilentlyContinue)) {
            return $c
        }
    }

    return $PSScriptRoot
}

# ============================================================================
# Collect binaries to sign
# ============================================================================
function Get-SignTargets {
    param([string]$Dir)

    $targets = @()

    # RawrXD executables
    $targets += Get-ChildItem $Dir -Filter "RawrXD*.exe" -Recurse -ErrorAction SilentlyContinue
    # Other project executables
    $targets += Get-ChildItem $Dir -Filter "rawrxd-*.exe" -Recurse -ErrorAction SilentlyContinue
    # DLLs
    $targets += Get-ChildItem $Dir -Filter "*.dll" -Recurse -ErrorAction SilentlyContinue
    # MSI installers
    $targets += Get-ChildItem $Dir -Filter "RawrXD*.msi" -Recurse -ErrorAction SilentlyContinue

    # Deduplicate
    $targets = $targets | Sort-Object FullName -Unique

    # Skip already-signed unless -Force
    if (-not $Force) {
        $unsigned = @()
        foreach ($t in $targets) {
            $sig = Get-AuthenticodeSignature -FilePath $t.FullName -ErrorAction SilentlyContinue
            if ($sig -and $sig.Status -eq "Valid") {
                Write-Host "  [SKIP] $($t.Name) — already signed" -ForegroundColor DarkGray
                $script:Skipped++
            } else {
                $unsigned += $t
            }
        }
        return $unsigned
    }

    return $targets
}

# ============================================================================
# Generate signing attestation (JSON manifest of what was signed)
# ============================================================================
function Write-SigningAttestation {
    param([string[]]$SignedFiles)

    $attestation = @{
        schema    = "gold_sign_attestation_v1"
        timestamp = (Get-Date -Format "yyyy-MM-ddTHH:mm:ss.fffZ")
        hostname  = $env:COMPUTERNAME
        user      = $env:USERNAME
        digest    = $script:cfg.Digest
        thumbprint = $script:cfg.Thumbprint
        subject    = $script:cfg.SubjectName
        timestamp_server = $script:cfg.Timestamp
        dual_sign  = $DualSign.IsPresent
        files     = @()
    }

    foreach ($f in $SignedFiles) {
        $hash = (Get-FileHash -Path $f -Algorithm SHA256).Hash
        $sig  = Get-AuthenticodeSignature -FilePath $f -ErrorAction SilentlyContinue
        $attestation.files += @{
            path       = (Resolve-Path $f -Relative -ErrorAction SilentlyContinue) ?? $f
            sha256     = $hash
            size_bytes = (Get-Item $f).Length
            signer     = if ($sig -and $sig.SignerCertificate) { $sig.SignerCertificate.Subject } else { "unknown" }
            status     = if ($sig) { $sig.Status.ToString() } else { "Unknown" }
        }
    }

    $outPath = Join-Path $PSScriptRoot "GOLD_SIGN_ATTESTATION.json"
    $attestation | ConvertTo-Json -Depth 5 | Out-File $outPath -Encoding utf8
    Write-Host "[GOLD_SIGN] Attestation written: $outPath" -ForegroundColor Cyan
}

# ============================================================================
# MAIN
# ============================================================================

Write-Host ""
Write-Host "================================================================" -ForegroundColor Yellow
Write-Host "  GOLD_SIGN — EV Certificate Signing for Production Release" -ForegroundColor Yellow
Write-Host "  RawrXD IDE / ItsMehRAWRXD" -ForegroundColor Yellow
Write-Host "================================================================" -ForegroundColor Yellow
Write-Host ""

# --- Resolve configuration ---
Resolve-Config

# --- Find signtool ---
$signtool = Find-SignTool
if (-not $signtool) {
    Write-Host "[GOLD_SIGN] FATAL: signtool.exe not found." -ForegroundColor Red
    Write-Host "  Install Windows 10/11 SDK or set SIGNTOOL_PATH env var." -ForegroundColor Yellow
    exit 1
}
Write-Host "[GOLD_SIGN] signtool: $signtool" -ForegroundColor Gray

# --- Auto-detect EV certificate if no explicit thumbprint/subject ---
if (-not $script:cfg.Thumbprint -and -not $script:cfg.SubjectName -and
    -not $script:cfg.CSP -and -not $AzureTrustedSigning) {
    $detected = Find-EVCertificate
    if ($detected) {
        $script:cfg.Thumbprint = $detected
    } else {
        Write-Host "[GOLD_SIGN] FATAL: No EV certificate available." -ForegroundColor Red
        Write-Host "  Set GOLD_SIGN_THUMBPRINT, GOLD_SIGN_SUBJECT, or provide -Thumbprint." -ForegroundColor Yellow
        Write-Host "  For Azure Trusted Signing, use -AzureTrustedSigning." -ForegroundColor Yellow
        exit 1
    }
}

# --- Display signing mode ---
if ($AzureTrustedSigning) {
    Write-Host "[GOLD_SIGN] Mode: Azure Trusted Signing (cloud HSM)" -ForegroundColor Cyan
    Write-Host "  Endpoint: $($script:cfg.AzureEndpoint)" -ForegroundColor Gray
    Write-Host "  Profile:  $($script:cfg.AzureProfile)" -ForegroundColor Gray
} elseif ($script:cfg.CSP) {
    Write-Host "[GOLD_SIGN] Mode: Hardware Token (CSP)" -ForegroundColor Cyan
    Write-Host "  CSP:      $($script:cfg.CSP)" -ForegroundColor Gray
    Write-Host "  KC:       $($script:cfg.KeyContainer)" -ForegroundColor Gray
} elseif ($script:cfg.Thumbprint) {
    Write-Host "[GOLD_SIGN] Mode: Certificate Store (Thumbprint)" -ForegroundColor Cyan
    Write-Host "  Store:    $($script:cfg.Store)" -ForegroundColor Gray
    Write-Host "  SHA1:     $($script:cfg.Thumbprint)" -ForegroundColor Gray
} else {
    Write-Host "[GOLD_SIGN] Mode: Subject Name" -ForegroundColor Cyan
    Write-Host "  Subject:  $($script:cfg.SubjectName)" -ForegroundColor Gray
}

Write-Host "[GOLD_SIGN] Digest:    $($script:cfg.Digest)" -ForegroundColor Gray
Write-Host "[GOLD_SIGN] Timestamp: $($script:cfg.Timestamp)" -ForegroundColor Gray
if ($DualSign) { Write-Host "[GOLD_SIGN] Dual-sign: SHA1 + SHA256" -ForegroundColor Cyan }
if ($DryRun)   { Write-Host "[GOLD_SIGN] *** DRY RUN — no files will be modified ***" -ForegroundColor Magenta }
Write-Host ""

# --- Collect targets ---
$buildDir = Get-BuildDirectory
Write-Host "[GOLD_SIGN] Build directory: $buildDir" -ForegroundColor Gray

if ($Verify) {
    # Verify-only mode
    Write-Host "[GOLD_SIGN] Verification mode — checking existing signatures..." -ForegroundColor Cyan
    $targets = Get-ChildItem $buildDir -Include @("*.exe","*.dll","*.msi") -Recurse -ErrorAction SilentlyContinue
    foreach ($t in $targets) {
        Test-Signature -SignToolPath $signtool -TargetFile $t.FullName
    }
} else {
    # Signing mode
    $targets = Get-SignTargets -Dir $buildDir

    if ($targets.Count -eq 0) {
        Write-Host "[GOLD_SIGN] No unsigned binaries found in $buildDir" -ForegroundColor Yellow
        exit 0
    }

    Write-Host "[GOLD_SIGN] Signing $($targets.Count) binary/ies..." -ForegroundColor Cyan
    Write-Host ""

    $signedPaths = @()

    foreach ($t in $targets) {
        $ok = $false
        if ($AzureTrustedSigning) {
            $ok = Invoke-AzureTrustedSign -SignToolPath $signtool -TargetFile $t.FullName
        } else {
            $ok = Invoke-GoldSign -SignToolPath $signtool -TargetFile $t.FullName
        }

        if ($ok) { $signedPaths += $t.FullName }
    }

    # Post-sign verification pass
    if ($signedPaths.Count -gt 0 -and -not $DryRun) {
        Write-Host ""
        Write-Host "[GOLD_SIGN] Post-sign verification..." -ForegroundColor Cyan
        foreach ($sp in $signedPaths) {
            Test-Signature -SignToolPath $signtool -TargetFile $sp
        }

        # Write attestation manifest
        Write-SigningAttestation -SignedFiles $signedPaths
    }
}

# --- Summary ---
Write-Host ""
Write-Host "================================================================" -ForegroundColor Yellow
Write-Host "  GOLD_SIGN Summary" -ForegroundColor Yellow
Write-Host "================================================================" -ForegroundColor Yellow
Write-Host "  Signed:   $($script:Signed)" -ForegroundColor Green
Write-Host "  Verified: $($script:Verified)" -ForegroundColor Green
Write-Host "  Skipped:  $($script:Skipped)" -ForegroundColor DarkGray
Write-Host "  Failed:   $($script:Failed)" -ForegroundColor $(if ($script:Failed -gt 0) { "Red" } else { "Green" })
Write-Host "================================================================" -ForegroundColor Yellow

if ($script:Failed -gt 0) { exit 1 }
exit 0
