[CmdletBinding()]
param(
    [string]$Version = "14.6.0",
    [string]$Channel = "stable",
    [string]$BuildDir = "build_replacement_check_ssot",
    [string]$OutputRoot = "release_out",
    [string]$BaseUrl = "https://releases.rawrxd.dev",
    [string]$CertThumbprint = "",
    [switch]$SkipBuild,
    [switch]$SkipSign,
    [switch]$SkipMsi
)

$ErrorActionPreference = "Stop"

function Resolve-RepoPath([string]$p) {
    if ([System.IO.Path]::IsPathRooted($p)) { return $p }
    return Join-Path $PSScriptRoot "..\..\$p"
}

function Require-File([string]$path) {
    if (-not (Test-Path $path -PathType Leaf)) {
        throw "Required file not found: $path"
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$buildDirAbs = Resolve-RepoPath $BuildDir
$outputRootAbs = Resolve-RepoPath $OutputRoot
$releaseDir = Join-Path $outputRootAbs "v$Version"
$stagingDir = Join-Path $releaseDir "staging"
$manifestPath = Join-Path $releaseDir "update-manifest.json"
$msiPath = Join-Path $releaseDir "RawrXD-v$Version-win64.msi"

New-Item -ItemType Directory -Path $releaseDir -Force | Out-Null
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

if (-not $SkipBuild) {
    Write-Host "[Phase15] Building release binaries..."
    ninja -C $buildDirAbs RawrXD-Win32IDE RawrXD_Gold
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
}

$win32Exe = Join-Path $buildDirAbs "bin\RawrXD-Win32IDE.exe"
$goldExe = Join-Path $buildDirAbs "gold\RawrXD_Gold.exe"
Require-File $win32Exe
Require-File $goldExe

Copy-Item $win32Exe (Join-Path $stagingDir "RawrXD-Win32IDE.exe") -Force
Copy-Item $goldExe (Join-Path $stagingDir "RawrXD_Gold.exe") -Force

if (-not $SkipSign) {
    Write-Host "[Phase15] Signing binaries..."
    & (Join-Path $PSScriptRoot "Sign-ReleaseArtifacts.ps1") `
        -Files @(
            (Join-Path $stagingDir "RawrXD-Win32IDE.exe"),
            (Join-Path $stagingDir "RawrXD_Gold.exe")
        ) `
        -CertThumbprint $CertThumbprint
}

if (-not $SkipMsi) {
    $wix = Get-Command wix -ErrorAction SilentlyContinue
    if (-not $wix) {
        Write-Warning "WiX CLI not found. Skipping MSI build."
    } else {
        Write-Host "[Phase15] Building MSI..."
        $wxs = Join-Path $repoRoot "installer\RawrXD_Installer_v14.6.0.wxs"
        Require-File $wxs

        wix build `
            -d BuildDir=$buildDirAbs `
            -d SourceDir=$repoRoot `
            -d ProductVersion=$Version `
            -o $msiPath `
            $wxs
        if ($LASTEXITCODE -ne 0) { throw "MSI build failed." }

        if (-not $SkipSign) {
            & (Join-Path $PSScriptRoot "Sign-ReleaseArtifacts.ps1") `
                -Files @($msiPath) `
                -CertThumbprint $CertThumbprint
        }
    }
}

$manifestArtifacts = @(
    (Join-Path $stagingDir "RawrXD-Win32IDE.exe"),
    (Join-Path $stagingDir "RawrXD_Gold.exe")
)
if (Test-Path $msiPath) {
    $manifestArtifacts += $msiPath
}

& (Join-Path $PSScriptRoot "New-UpdateManifest.ps1") `
    -Version $Version `
    -Channel $Channel `
    -BaseUrl $BaseUrl `
    -OutputPath $manifestPath `
    -Artifacts $manifestArtifacts

if (-not $SkipSign) {
    & (Join-Path $PSScriptRoot "Sign-ReleaseArtifacts.ps1") `
        -Files @($manifestPath) `
        -CertThumbprint $CertThumbprint `
        -AllowUnsigned
}

Write-Host "[Phase15] Complete."
Write-Host "  Release dir: $releaseDir"
Write-Host "  Manifest:    $manifestPath"
if (Test-Path $msiPath) {
    Write-Host "  MSI:         $msiPath"
}
