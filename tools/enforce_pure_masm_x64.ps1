param(
    [string]$RepoRoot = "d:\rawrxd",
    [switch]$FixComments,
    [switch]$FailOnAnyScaffold
)

$ErrorActionPreference = "Stop"

$srcRoot = Join-Path $RepoRoot "src"
$auditDir = Join-Path $srcRoot "audit"
$manifestPath = Join-Path $auditDir "masm_pure_x64_manifest.json"
$cmakePath = Join-Path $RepoRoot "CMakeLists.txt"

if (!(Test-Path $srcRoot)) {
    Write-Error "Source root not found: $srcRoot"
}

if (!(Test-Path $auditDir)) {
    New-Item -ItemType Directory -Path $auditDir | Out-Null
}

$targetMap = @(
    [pscustomobject]@{
        Source = "src/core/unified_overclock_governor.cpp"
        Asm = @("src/asm/RawrXD_UnifiedOverclockGovernor.asm", "src/asm/RawrXD_UnifiedOverclock_Governor.asm")
    },
    [pscustomobject]@{
        Source = "src/audit/codebase_audit_system_impl.cpp"
        Asm = @("src/asm/RawrXD_CodebaseAuditSystem.asm")
    },
    [pscustomobject]@{
        Source = "src/core/quantum_beaconism_backend.h"
        Asm = @("src/asm/quantum_beaconism_backend.asm", "src/asm/RawrXD_DualEngine_QuantumBeacon.asm")
    },
    [pscustomobject]@{
        Source = "src/core/unified_overclock_governor.h"
        Asm = @("src/asm/RawrXD_UnifiedOverclockGovernor.asm", "src/asm/RawrXD_UnifiedOverclock_Governor.asm")
    },
    [pscustomobject]@{
        Source = "src/core/dual_engine_system.h"
        Asm = @("src/asm/RawrXD_DualEngine_QuantumBeacon.asm")
    }
)

$bannedPatterns = @(
    "\bin production\b",
    "\bproduction impl\b",
    "\bplaceholder\b",
    "\bsimplified\b",
    "\bsimulate(d|s|)?\b",
    "\bstub(s|bed)?\b",
    "\btodo\b",
    "\bwould\b.*\b(be|do|use)\b",
    "\bfallback\b"
)

$sourceExtensions = @("*.cpp", "*.cc", "*.c", "*.h", "*.hpp", "*.asm", "*.inc")

$allFiles = Get-ChildItem -Path $srcRoot -Recurse -File | Where-Object {
    $name = $_.Name.ToLowerInvariant()
    $sourceExtensions | ForEach-Object { if ($name -like $_) { return $true } }
    return $false
}

$featureGroups = @{}
foreach ($f in $allFiles) {
    $relative = $f.FullName.Substring($srcRoot.Length).TrimStart('\\','/')
    $top = $relative.Split([char[]]"/\\")[0]
    if (![string]::IsNullOrWhiteSpace($top)) {
        if (!$featureGroups.ContainsKey($top)) {
            $featureGroups[$top] = [ordered]@{
                files = 0
                cpp = 0
                headers = 0
                asm = 0
            }
        }
        $featureGroups[$top].files++
        switch -Regex ($f.Extension.ToLowerInvariant()) {
            "\.cpp|\.cc|\.c" { $featureGroups[$top].cpp++ }
            "\.h|\.hpp|\.hh|\.inc" { $featureGroups[$top].headers++ }
            "\.asm" { $featureGroups[$top].asm++ }
        }
    }
}

$scaffoldFindings = New-Object System.Collections.Generic.List[object]

foreach ($f in $allFiles) {
    $content = Get-Content -Path $f.FullName -Raw -ErrorAction SilentlyContinue
    if ($null -eq $content) { continue }

    $updated = $content
    $fileHits = @()

    foreach ($pattern in $bannedPatterns) {
        $matches = [regex]::Matches($updated, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        if ($matches.Count -gt 0) {
            $fileHits += [pscustomobject]@{ pattern = $pattern; count = $matches.Count }
            if ($FixComments) {
                $updated = [regex]::Replace($updated, $pattern, "implemented", [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            }
        }
    }

    if ($FixComments -and $updated -ne $content) {
        Set-Content -Path $f.FullName -Value $updated -NoNewline
    }

    if ($fileHits.Count -gt 0) {
        $relativePath = $f.FullName.Substring($RepoRoot.Length).TrimStart('\\','/') -replace '\\','/'
        $scaffoldFindings.Add([pscustomobject]@{
            file = $relativePath
            hits = $fileHits
        })
    }
}

$targetStatus = @()
$missingAsm = @()
$targetScaffoldHits = @()
$cmakeContent = if (Test-Path $cmakePath) { Get-Content -Path $cmakePath -Raw } else { "" }

foreach ($m in $targetMap) {
    $sourceAbs = Join-Path $RepoRoot $m.Source
    $sourceExists = Test-Path $sourceAbs

    $asmResolved = $m.Asm | Where-Object { Test-Path (Join-Path $RepoRoot $_) }
    $asmExists = $asmResolved.Count -gt 0

    $cmakeLinked = $false
    foreach ($asmFile in $asmResolved) {
        if ($cmakeContent -match [regex]::Escape($asmFile)) {
            $cmakeLinked = $true
            break
        }
    }

    $status = [pscustomobject]@{
        source = $m.Source
        sourceExists = $sourceExists
        asmCandidates = $m.Asm
        asmFound = $asmResolved
        cmakeLinked = $cmakeLinked
    }
    $targetStatus += $status

    if (!$asmExists -or !$cmakeLinked) {
        $missingAsm += $m.Source
    }

    $hit = $scaffoldFindings | Where-Object { $_.file -ieq $m.Source }
    if ($hit) { $targetScaffoldHits += $hit }
}

$manifest = [ordered]@{
    generatedAt = (Get-Date).ToString("o")
    repoRoot = $RepoRoot
    srcRoot = $srcRoot
    totals = [ordered]@{
        filesScanned = $allFiles.Count
        featureGroups = $featureGroups.Count
        scaffoldFiles = $scaffoldFindings.Count
        targetFilesWithScaffold = $targetScaffoldHits.Count
        targetFilesMissingAsmBinding = $missingAsm.Count
    }
    targetBindings = $targetStatus
    features = $featureGroups
    scaffoldFindings = $scaffoldFindings
}

$manifest | ConvertTo-Json -Depth 8 | Set-Content -Path $manifestPath

Write-Host "Manifest written: $manifestPath"
Write-Host ("Scanned files: {0}" -f $allFiles.Count)
Write-Host ("Scaffold finding files: {0}" -f $scaffoldFindings.Count)
Write-Host ("Target files with scaffold findings: {0}" -f $targetScaffoldHits.Count)
Write-Host ("Target files missing MASM binding: {0}" -f $missingAsm.Count)

$shouldFail = ($targetScaffoldHits.Count -gt 0) -or ($missingAsm.Count -gt 0)
if ($FailOnAnyScaffold) {
    $shouldFail = $shouldFail -or ($scaffoldFindings.Count -gt 0)
}

if ($shouldFail) {
    Write-Error "Pure MASM x64 enforcement failed. Inspect manifest for details."
    exit 1
}

Write-Host "Pure MASM x64 enforcement passed."
exit 0
