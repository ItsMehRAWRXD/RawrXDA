# MASM-AutoPatch-Builder.ps1
# DIRECT LINK ARCHITECTURE (LINK-ONLY)
# - No cl / clang / ml64
# - No MSVC link.exe
# - Uses in-house PowerLang linker: G:\Everything\powlang\tools\linker.ps1
# - DisableRecompile = link-only (no assemble/compile)

param(
    [string]$Root = "D:\RawrXD",
    [string]$DiscoverRoot = "",
    [string]$OutDir = "",
    [int]$BatchSize = 50,
    [int]$BatchId = 1,
    [switch]$AutoDiscover,
    [switch]$LinkOnly,
    [string]$ObjectsFile = "",
    [string]$LinkOut = "",
    [string]$PowerLangLinker = "G:\Everything\powlang\tools\linker.ps1",
    [ValidateSet("PE32+", "ELF64")]
    [string]$Format = "PE32+",
    [string]$EntrySymbol = "_start"
)

$ErrorActionPreference = "Stop"

function Resolve-Default([string]$value, [string]$fallback) {
    if([string]::IsNullOrWhiteSpace($value)) { return $fallback }
    return $value
}

function Require-Path([string]$path, [string]$label) {
    if(-not (Test-Path -LiteralPath $path)) {
        throw "$label not found: $path"
    }
}

function Import-PowerLangLinker([string]$linkerPath) {
    Require-Path $linkerPath "PowerLang linker"
    Import-Module $linkerPath -Force -Global -ErrorAction Stop | Out-Null
}

function Invoke-PowerLangLinker([
    string[]]$InputFiles,
    [string]$OutputFile,
    [string]$Format,
    [string]$EntrySymbol
) {
    # PowerLang's Invoke-Linker is an advanced function that defines a conflicting
    # Verbose parameter; call its ScriptBlock directly.
    $cmd = Get-Command Invoke-Linker -ErrorAction Stop
    if(-not $cmd.ScriptBlock) {
        throw "Invoke-Linker does not expose a ScriptBlock"
    }
    & $cmd.ScriptBlock -InputFiles $InputFiles -OutputFile $OutputFile -Format $Format -EntrySymbol $EntrySymbol | Out-Null
}

function Read-ObjectsFile([string]$path) {
    $raw = Get-Content -LiteralPath $path -ErrorAction Stop |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith('#') }

    if(-not $raw -or $raw.Count -lt 1) {
        throw "Objects file is empty: $path"
    }

    $objs = New-Object System.Collections.Generic.List[string]
    foreach($line in $raw) {
        foreach($tok in ($line -split "\s+")) {
            $t = $tok.Trim('"')
            if(-not [string]::IsNullOrWhiteSpace($t)) { $objs.Add($t) | Out-Null }
        }
    }
    return $objs.ToArray()
}

function Write-ObjectsFile([string]$path, [string[]]$objs) {
    $dir = Split-Path -Parent $path
    if($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $objs | Out-File -FilePath $path -Encoding UTF8
}

$Root = (Resolve-Path -LiteralPath $Root).Path
$DiscoverRoot = Resolve-Default $DiscoverRoot (Join-Path $Root "src\asm")
$OutDir = Resolve-Default $OutDir (Join-Path $Root "direct_link")
if(-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$ObjectsFile = Resolve-Default $ObjectsFile (Join-Path $OutDir "objects.txt")
$LinkOut = Resolve-Default $LinkOut (Join-Path $OutDir ("RawrXD_batch_{0}.exe" -f $BatchId))

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  RAWRXD DIRECT LINK (LINK-ONLY)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "Root        : $Root" -ForegroundColor Gray
Write-Host "DiscoverRoot: $DiscoverRoot" -ForegroundColor Gray
Write-Host "OutDir      : $OutDir" -ForegroundColor Gray
Write-Host "BatchId     : $BatchId (size=$BatchSize)" -ForegroundColor Gray
Write-Host "ObjectsFile : $ObjectsFile" -ForegroundColor Gray
Write-Host "Output      : $LinkOut" -ForegroundColor Gray

Import-PowerLangLinker $PowerLangLinker

if(-not $LinkOnly) { $AutoDiscover = $true }

if($AutoDiscover) {
    Require-Path $DiscoverRoot "DiscoverRoot"

    $allObjs = Get-ChildItem -LiteralPath $DiscoverRoot -Recurse -File -Filter "*.obj" -ErrorAction Stop |
        Sort-Object FullName |
        Select-Object -ExpandProperty FullName

    if(-not $allObjs -or $allObjs.Count -lt 1) {
        throw "No .obj files found under: $DiscoverRoot"
    }

    $start = ($BatchId - 1) * $BatchSize
    if($start -ge $allObjs.Count) {
        throw "BatchId $BatchId is out of range. objs=$($allObjs.Count) batchSize=$BatchSize"
    }
    $end = [Math]::Min($start + $BatchSize - 1, $allObjs.Count - 1)
    $batchObjs = $allObjs[$start..$end]

    Write-ObjectsFile $ObjectsFile $batchObjs
    Write-Host "Wrote objects list: $ObjectsFile ($($batchObjs.Count) objs)" -ForegroundColor Green
}

# Link-only
$objsToLink = Read-ObjectsFile $ObjectsFile
$missing = @($objsToLink | Where-Object { -not (Test-Path -LiteralPath $_) })
if($missing.Count -gt 0) {
    throw "Missing input .obj (first 10): $((@($missing) | Select-Object -First 10) -join '; ')"
}

Write-Host "[LINK] PowerLang -> $LinkOut" -ForegroundColor Cyan
Invoke-PowerLangLinker -InputFiles $objsToLink -OutputFile $LinkOut -Format $Format -EntrySymbol $EntrySymbol

if(-not (Test-Path -LiteralPath $LinkOut)) {
    throw "Link did not produce output: $LinkOut"
}

$report = [pscustomobject]@{
    deliverable = "direct_link_batch"
    root = $Root
    discover_root = $DiscoverRoot
    out_dir = $OutDir
    objects_file = $ObjectsFile
    output = $LinkOut
    batch_id = $BatchId
    batch_size = $BatchSize
    obj_count = $objsToLink.Count
    format = $Format
    entry_symbol = $EntrySymbol
    toolchain = [pscustomobject]@{
        no_recompile = $true
        no_assemble = $true
        no_msvc_link = $true
        powerlang_linker = $PowerLangLinker
    }
}

$reportPath = Join-Path $OutDir ("direct_link_batch_{0}_report.json" -f $BatchId)
$report | ConvertTo-Json -Depth 6 | Out-File -FilePath $reportPath -Encoding UTF8

Write-Host "[OK] Output: $LinkOut" -ForegroundColor Green
Write-Host "[OK] Report: $reportPath" -ForegroundColor Green
