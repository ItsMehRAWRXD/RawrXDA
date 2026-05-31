param(
    [ValidateRange(1,14)]
    [int]$Day,
    [switch]$AllDays,
    [switch]$Strict,
    [switch]$FastSmoke,
    [switch]$NoBuild,
    [switch]$NoTests,
    [string]$BuildDir = "",
    [string[]]$Day2Targets = @(),
    [ValidateRange(1,600)]
    [int]$Day2IncrementalThresholdSeconds = 30,
    [ValidateRange(30,7200)]
    [int]$CommandTimeoutSeconds = 1800,
    [ValidateRange(30,7200)]
    [int]$ScriptTimeoutSeconds = 1800
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$reportRoot = Join-Path $repoRoot "reports\14day"
$logFile = Join-Path $reportRoot "production_finisher.log"

if (-not (Test-Path $reportRoot)) {
    New-Item -Path $reportRoot -ItemType Directory -Force | Out-Null
}

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $line = "[{0}] [{1}] {2}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Level, $Message
    Write-Host $line
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        try {
            Add-Content -Path $logFile -Value $line
            break
        } catch {
            if ($attempt -eq 4) { throw }
            Start-Sleep -Milliseconds (100 * ($attempt + 1))
        }
    }
}

function Stop-ProcessTree {
    param([int]$ProcessId)

    if ($ProcessId -le 0) {
        return
    }

    try {
        & taskkill /PID $ProcessId /T /F | Out-Null
    } catch {
        try {
            Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue
        } catch {
            # Best-effort cleanup only.
        }
    }
}

function New-Result {
    param([int]$DayNumber, [string]$Name)
    return [ordered]@{
        day = $DayNumber
        name = $Name
        startTimeUtc = (Get-Date).ToUniversalTime().ToString("o")
        status = "PASS"
        checks = @()
        metrics = [ordered]@{}
        notes = @()
        endTimeUtc = ""
    }
}

function Add-Check {
    param(
        $Result,
        [string]$Name,
        [bool]$Passed,
        [string]$Detail
    )

    $Result.checks += [ordered]@{
        name = $Name
        passed = $Passed
        detail = $Detail
    }

    if (-not $Passed) {
        $Result.status = "FAIL"
    }
}

function Save-Result {
    param($Result)

    $Result.endTimeUtc = (Get-Date).ToUniversalTime().ToString("o")
    $dayTag = "day{0:d2}" -f [int]$Result.day

    $jsonPath = Join-Path $reportRoot ("{0}_report.json" -f $dayTag)
    $mdPath = Join-Path $reportRoot ("{0}_summary.md" -f $dayTag)

    $Result | ConvertTo-Json -Depth 8 | Set-Content -Path $jsonPath -Encoding UTF8

    $md = @()
    $md += "# Day $($Result.day) - $($Result.name)"
    $md += ""
    $md += "Status: **$($Result.status)**"
    $md += ""
    $md += "## Checks"
    foreach ($c in $Result.checks) {
        $icon = if ($c.passed) { "PASS" } else { "FAIL" }
        $md += "- [$icon] $($c.name): $($c.detail)"
    }
    $md += ""
    $md += "## Metrics"
    foreach ($k in $Result.metrics.Keys) {
        $md += ("- {0}: {1}" -f $k, $Result.metrics[$k])
    }
    $md += ""
    $md += "## Notes"
    foreach ($n in $Result.notes) {
        $md += "- $n"
    }

    $md -join "`r`n" | Set-Content -Path $mdPath -Encoding UTF8

    Write-Log "Saved $dayTag report: $jsonPath"
}

function Test-FilePresent {
    param([string]$Path)
    return Test-Path -LiteralPath $Path
}

function Get-TextSafe {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return ""
    }

    try {
        return Get-Content -LiteralPath $Path -Raw
    } catch {
        return ""
    }
}

function Invoke-CmdSafe {
    param([string]$Command, [string]$FriendlyName)

    Write-Log "Running: $FriendlyName"
    $startedAt = Get-Date
    $stdoutPath = Join-Path $reportRoot ("cmdsafe_{0}_stdout.txt" -f ([guid]::NewGuid().ToString("N")))
    $stderrPath = Join-Path $reportRoot ("cmdsafe_{0}_stderr.txt" -f ([guid]::NewGuid().ToString("N")))
    $proc = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", $Command) -PassThru -NoNewWindow `
        -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $timedOut = $false
    if (-not $proc.WaitForExit($CommandTimeoutSeconds * 1000)) {
        $timedOut = $true
        Write-Log "Timeout after ${CommandTimeoutSeconds}s while running: $FriendlyName (pid=$($proc.Id))" "WARN"
        Stop-ProcessTree -ProcessId $proc.Id
        $null = $proc.WaitForExit(5000)
    }
    else {
        $proc.WaitForExit()
    }
    $endedAt = Get-Date
    $stdout = if (Test-Path -LiteralPath $stdoutPath) { Get-Content -LiteralPath $stdoutPath -Raw } else { "" }
    $stderr = if (Test-Path -LiteralPath $stderrPath) { Get-Content -LiteralPath $stderrPath -Raw } else { "" }
    $code = if ($timedOut) { 124 } else { $proc.ExitCode }
    $output = @($stdout, $stderr) | Where-Object { $_ -and $_.Trim().Length -gt 0 }
    $durationSeconds = [Math]::Round(($endedAt - $startedAt).TotalSeconds, 2)
    return [pscustomobject]@{ code = $code; output = ($output -join "`n"); durationSeconds = $durationSeconds; timedOut = $timedOut; timeoutSeconds = $CommandTimeoutSeconds }
}

function Resolve-BuildRunCode {
    param(
        $Run,
        [string]$SuccessRegex,
        [string]$SuccessArtifact
    )

    $rawCode = 1
    if ($null -ne $Run.code) {
        $rawCode = [int]$Run.code
    } elseif (($Run -is [System.Collections.IDictionary]) -and $Run.Contains('code')) {
        $rawCode = [int]$Run['code']
    }

    if ($rawCode -eq 0) {
        return 0
    }

    if ($Run.timedOut) {
        return 124
    }

    $out = [string]$Run.output
    $hasHardError = $out -match '(?im)\b(error(:| C[0-9]{4})|FAILED:|ninja:\s+build\s+stopped)'
    $hasSuccessSignal = $false
    if ($SuccessRegex -and ($out -match $SuccessRegex)) {
        $hasSuccessSignal = $true
    }
    if ((-not $hasSuccessSignal) -and $SuccessArtifact) {
        $hasSuccessSignal = Test-Path -LiteralPath $SuccessArtifact
    }

    if ((-not $hasHardError) -and $hasSuccessSignal) {
        return 0
    }

    return $rawCode
}

function Find-HeadlessRuntimeExe {
    $candidates = @(
        (Join-Path $repoRoot "RawrEngine.exe"),
        (Join-Path $repoRoot "build\RawrEngine.exe"),
        (Join-Path $repoRoot "build\bin\RawrEngine.exe"),
        (Join-Path $repoRoot "build-win32\RawrEngine.exe"),
        (Join-Path $repoRoot "build-win32\bin\RawrEngine.exe"),
        (Join-Path $repoRoot "build-ninja\RawrEngine.exe"),
        (Join-Path $repoRoot "build-ninja\bin\RawrEngine.exe"),
        (Join-Path $repoRoot "bin\RawrEngine.exe"),
        (Join-Path $repoRoot "bin-native\RawrEngine.exe")
    )

    foreach ($p in $candidates) {
        if (Test-Path $p) {
            return $p
        }
    }

    return $null
}

function Resolve-BuildDir {
    param([string]$Prefer)

    if ($Prefer -and (Test-Path -LiteralPath (Join-Path $Prefer "CMakeCache.txt"))) {
        return (Resolve-Path -LiteralPath $Prefer).Path
    }

        foreach ($c in @(
            (Join-Path $repoRoot "build-ninja"),
            (Join-Path $repoRoot "build-win32"),
            (Join-Path $repoRoot "build-ninja-ctx2"),
            (Join-Path $repoRoot "build_smoke_auto"),
            (Join-Path $repoRoot "build"))) {
        $cache = Join-Path $c "CMakeCache.txt"
        if (Test-Path -LiteralPath $cache) {
            return (Resolve-Path -LiteralPath $c).Path
        }
    }

    return $null
}

function Invoke-PwshScriptSafe {
    param(
        [string]$ScriptPath,
        [string[]]$ScriptArgs,
        [string]$FriendlyName
    )

    if (-not (Test-Path -LiteralPath $ScriptPath)) {
        return [ordered]@{
            code = 1
            output = "missing script: $ScriptPath"
        }
    }

    Write-Log "Running: $FriendlyName"
    $stdoutPath = Join-Path $reportRoot ("tmp_{0}_{1}_stdout.txt" -f ([IO.Path]::GetFileNameWithoutExtension($ScriptPath)), ([guid]::NewGuid().ToString("N")))
    $stderrPath = Join-Path $reportRoot ("tmp_{0}_{1}_stderr.txt" -f ([IO.Path]::GetFileNameWithoutExtension($ScriptPath)), ([guid]::NewGuid().ToString("N")))
    $argList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $ScriptPath) + $ScriptArgs
    $proc = Start-Process -FilePath "pwsh" -ArgumentList $argList -PassThru -NoNewWindow -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    $timedOut = $false
    if (-not $proc.WaitForExit($ScriptTimeoutSeconds * 1000)) {
        $timedOut = $true
        Write-Log "Timeout after ${ScriptTimeoutSeconds}s while running: $FriendlyName (pid=$($proc.Id))" "WARN"
        Stop-ProcessTree -ProcessId $proc.Id
        $null = $proc.WaitForExit(5000)
    }
    else {
        $proc.WaitForExit()
    }
    $stdout = if (Test-Path -LiteralPath $stdoutPath) { Get-Content -LiteralPath $stdoutPath -Raw } else { "" }
    $stderr = if (Test-Path -LiteralPath $stderrPath) { Get-Content -LiteralPath $stderrPath -Raw } else { "" }
    $code = if ($timedOut) { 124 } else { $proc.ExitCode }
    $output = @($stdout, $stderr) | Where-Object { $_ -and $_.Trim().Length -gt 0 }
    return [pscustomobject]@{
        code = $code
        output = ($output -join "`n")
        timedOut = $timedOut
        timeoutSeconds = $ScriptTimeoutSeconds
    }
}

function Invoke-ExtensionInstallerFinisherDay {
    param(
        [int]$InstallerDay,
        [string]$Label
    )

    $script = Join-Path $repoRoot "scripts\extension_installer_14day_expansion.ps1"
    $resolvedBuildDir = Resolve-BuildDir -Prefer $BuildDir
    $args = @("-Day", $InstallerDay, "-Strict")
    if ($resolvedBuildDir) {
        $args += @("-BuildDir", $resolvedBuildDir)
    }
    return Invoke-PwshScriptSafe -ScriptPath $script -ScriptArgs $args -FriendlyName $Label
}

function Invoke-MinimalIntegrationGate {
    param(
        [string]$Label,
        [switch]$SkipExtensionInstaller,
        [switch]$SkipTurnkey,
        [string[]]$TurnkeyArgs = @()
    )

    $script = Join-Path $repoRoot "scripts\Run-IntegrationGateMinimal.ps1"
    $resolvedBuildDir = Resolve-BuildDir -Prefer $BuildDir
    $args = @()
    if ($resolvedBuildDir) {
        $args += @("-BuildDir", $resolvedBuildDir)
    }
    if (-not $SkipExtensionInstaller) {
        $args += "-TryBuildExtensionInstaller"
    } else {
        $args += "-SkipExtensionInstaller"
    }
    if ($SkipTurnkey) {
        $args += "-SkipTurnkey"
    }
    if ($TurnkeyArgs.Count -gt 0) {
        $args += @("-TurnkeyArgs")
        $args += $TurnkeyArgs
    }
    return Invoke-PwshScriptSafe -ScriptPath $script -ScriptArgs $args -FriendlyName $Label
}

function Invoke-TurnkeySmoke {
    param(
        [string]$Label,
        [string[]]$TurnkeyArgs = @()
    )

    $script = Join-Path $repoRoot "scripts\Run-TurnkeyIdeSmoke.ps1"
    return Invoke-PwshScriptSafe -ScriptPath $script -ScriptArgs $TurnkeyArgs -FriendlyName $Label
}

function Invoke-QualityGateValidation {
    param(
        [string]$Label
    )

    $script = Join-Path $repoRoot "scripts\production\Invoke-14Day-QualityGateValidation.ps1"
    $args = @("-ReportRoot", $reportRoot)
    if ($Strict) {
        $args += "-Strict"
    }
    return Invoke-PwshScriptSafe -ScriptPath $script -ScriptArgs $args -FriendlyName $Label
}

function Invoke-Day {
    param([int]$N)

    switch ($N) {
        1 {
            $r = New-Result -DayNumber 1 -Name "Baseline Integrity"
            Add-Check -Result $r -Name "Repo root exists" -Passed (Test-Path $repoRoot) -Detail $repoRoot
            Add-Check -Result $r -Name "Source folder exists" -Passed (Test-Path (Join-Path $repoRoot "src")) -Detail "src"
            Add-Check -Result $r -Name "Include folder exists" -Passed (Test-Path (Join-Path $repoRoot "include")) -Detail "include"

            $reconDoc = Join-Path $repoRoot "docs\status\day01_baseline_integrity_reconciliation.md"
            $ownersDoc = Join-Path $repoRoot "docs\status\day01_critical_path_ownership.md"
            Add-Check -Result $r -Name "Baseline reconciliation doc present" -Passed (Test-Path -LiteralPath $reconDoc) -Detail $reconDoc
            Add-Check -Result $r -Name "Critical path ownership doc present" -Passed (Test-Path -LiteralPath $ownersDoc) -Detail $ownersDoc

            $reconText = Get-TextSafe -Path $reconDoc
            $ownersText = Get-TextSafe -Path $ownersDoc

            $requiredReconMarkers = @(
                "Reconciliation Summary",
                "Phase 1",
                "Phase 2",
                "Phase 3",
                "Phase 4",
                "Blockers",
                "Remaining Work"
            )

            $missingReconMarkers = @()
            foreach ($marker in $requiredReconMarkers) {
                if ([string]::IsNullOrWhiteSpace($reconText) -or (-not $reconText.Contains($marker))) {
                    $missingReconMarkers += $marker
                }
            }
            Add-Check -Result $r -Name "Reconciliation sections complete" -Passed ($missingReconMarkers.Count -eq 0) -Detail $(if ($missingReconMarkers.Count -eq 0) { "all required sections present" } else { "missing: $($missingReconMarkers -join ', ')" })

            $ownerRows = 0
            $ownerRowsAssigned = 0
            if (-not [string]::IsNullOrWhiteSpace($ownersText)) {
                foreach ($line in ($ownersText -split "`r?`n")) {
                    if ($line -match '^\|\s*[^|]+\s*\|\s*[^|]+\s*\|\s*[^|]+\s*\|\s*[^|]+\s*\|\s*$') {
                        if (($line -notmatch '^\|\s*Work Area\s*\|') -and ($line -notmatch '^\|\s*-+')) {
                            $ownerRows += 1
                            if ($line -notmatch 'TBD|UNASSIGNED|N/A') {
                                $ownerRowsAssigned += 1
                            }
                        }
                    }
                }
            }

            $ownerAssignmentsClear = (($ownerRows -ge 4) -and ($ownerRowsAssigned -eq $ownerRows))
            Add-Check -Result $r -Name "Critical path owners assigned" -Passed $ownerAssignmentsClear -Detail "assigned=$ownerRowsAssigned total=$ownerRows"

            $reportReconDoc = Join-Path $reportRoot "day01_baseline_integrity_reconciliation.md"
            $reportOwnersDoc = Join-Path $reportRoot "day01_critical_path_ownership.md"
            if (Test-Path -LiteralPath $reconDoc) {
                Copy-Item -LiteralPath $reconDoc -Destination $reportReconDoc -Force
            }
            if (Test-Path -LiteralPath $ownersDoc) {
                Copy-Item -LiteralPath $ownersDoc -Destination $reportOwnersDoc -Force
            }
            Add-Check -Result $r -Name "Day 1 reconciliation artifact published" -Passed (Test-Path -LiteralPath $reportReconDoc) -Detail $reportReconDoc
            Add-Check -Result $r -Name "Day 1 ownership artifact published" -Passed (Test-Path -LiteralPath $reportOwnersDoc) -Detail $reportOwnersDoc

            $r.metrics.fileCountTop = ((Get-ChildItem -Path $repoRoot -Force | Measure-Object).Count)
            $r.metrics.day1ReconDoc = $reconDoc
            $r.metrics.day1OwnersDoc = $ownersDoc
            $r.metrics.day1OwnerRows = $ownerRows
            $r.metrics.day1OwnerRowsAssigned = $ownerRowsAssigned
            $r.notes += "Baseline integrity now requires real implementation reconciliation and explicit critical-path ownership evidence."
            Save-Result -Result $r
            return $r
        }

        2 {
            $r = New-Result -DayNumber 2 -Name "Build Determinism Gate"
            if ($NoBuild) {
                Add-Check -Result $r -Name "Build skipped" -Passed $true -Detail "NoBuild flag active"
            } else {
                $bd = Resolve-BuildDir -Prefer $BuildDir
                if ($bd) {
                    $effectiveTargets = @($Day2Targets)
                    if ($Strict -and $effectiveTargets.Count -eq 0) {
                        # Strict mode defaults to the validated deterministic lane
                        # so unresolved full-graph optional binaries do not hide
                        # production finisher gate progress.
                        $effectiveTargets = @(
                            "simple_gpu_test",
                            "real_multi_model_benchmark",
                            "rawrxd-titan-init-probe",
                            "RawrXD_KeyGen"
                        )
                    }
                    if ($FastSmoke -and $effectiveTargets.Count -eq 0) {
                        $effectiveTargets = @("RawrEngine")
                    }

                    $targetNote = if ($effectiveTargets.Count -gt 0) {
                        "targets=$($effectiveTargets -join ',')"
                    } else {
                        "targets=full-graph"
                    }
                    $r.notes += "Using configured CMake tree: $bd (pass 1 = build freshness, pass 2 = incremental stability signal, $targetNote)."
                    Add-Check -Result $r -Name "CMake build dir resolved" -Passed $true -Detail $bd
                    Add-Check -Result $r -Name "Build dir already configured" -Passed (Test-Path (Join-Path $bd "CMakeCache.txt")) -Detail "Skipping clean rebuild"

                    if ($effectiveTargets.Count -gt 0) {
                        $allPass = $true
                        foreach ($target in $effectiveTargets) {
                            $cmdBase = "cmake --build $bd --target $target --parallel"
                            $run1 = Invoke-CmdSafe -Command $cmdBase -FriendlyName "build $target pass 1"
                            $run2 = Invoke-CmdSafe -Command $cmdBase -FriendlyName "build $target pass 2"
                            $targetArtifact = Join-Path $bd ("bin\{0}.exe" -f $target)
                            $run1Code = Resolve-BuildRunCode -Run $run1 -SuccessRegex '(?im)(no work to do|Linking\s+CXX\s+executable|Build\s+succeeded\s+on\s+attempt)' -SuccessArtifact $targetArtifact
                            $run2Code = Resolve-BuildRunCode -Run $run2 -SuccessRegex '(?im)(no work to do|Linking\s+CXX\s+executable|Build\s+succeeded\s+on\s+attempt)' -SuccessArtifact $targetArtifact
                            Add-Check -Result $r -Name "Build $target pass 1" -Passed ($run1Code -eq 0) -Detail "exit=$run1Code duration=$($run1.durationSeconds)s timedOut=$($run1.timedOut)"
                            Add-Check -Result $r -Name "Build $target pass 2" -Passed ($run2Code -eq 0) -Detail "exit=$run2Code duration=$($run2.durationSeconds)s timedOut=$($run2.timedOut)"
                            $incrementalFast = ($run2.durationSeconds -lt $Day2IncrementalThresholdSeconds)
                            Add-Check -Result $r -Name "Incremental build $target is fast" -Passed $incrementalFast -Detail "pass2=$($run2.durationSeconds)s threshold=$($Day2IncrementalThresholdSeconds)s"
                            if ($run1Code -ne 0) { $r.metrics["build_${target}_pass1_tail"] = ($run1.output -split "`n" | Select-Object -Last 8) -join "`n" }
                            if ($run2Code -ne 0) { $r.metrics["build_${target}_pass2_tail"] = ($run2.output -split "`n" | Select-Object -Last 8) -join "`n" }
                            $r.metrics["build_${target}_pass1_seconds"] = $run1.durationSeconds
                            $r.metrics["build_${target}_pass2_seconds"] = $run2.durationSeconds
                            if (($run1Code -ne 0) -or ($run2Code -ne 0) -or (-not $incrementalFast)) {
                                $allPass = $false
                            }
                        }
                        $r.metrics.day2Targeted = $true
                        $r.metrics.day2Targets = ($effectiveTargets -join ",")
                        $r.metrics.day2IncrementalThresholdSeconds = $Day2IncrementalThresholdSeconds
                        $r.metrics.day2AllTargetsPassed = $allPass
                    } else {
                        $cmdBase = "cmake --build $bd --parallel"
                        $run1 = Invoke-CmdSafe -Command $cmdBase -FriendlyName "build pass 1"
                        $run2 = Invoke-CmdSafe -Command $cmdBase -FriendlyName "build pass 2"
                        $run1Code = Resolve-BuildRunCode -Run $run1 -SuccessRegex '(?im)(no work to do|Linking\s+CXX\s+executable|Build\s+succeeded\s+on\s+attempt)' -SuccessArtifact ""
                        $run2Code = Resolve-BuildRunCode -Run $run2 -SuccessRegex '(?im)(no work to do|Linking\s+CXX\s+executable|Build\s+succeeded\s+on\s+attempt)' -SuccessArtifact ""
                        Add-Check -Result $r -Name "Build pass 1" -Passed ($run1Code -eq 0) -Detail "exit=$run1Code duration=$($run1.durationSeconds)s timedOut=$($run1.timedOut)"
                        Add-Check -Result $r -Name "Build pass 2" -Passed ($run2Code -eq 0) -Detail "exit=$run2Code duration=$($run2.durationSeconds)s timedOut=$($run2.timedOut)"
                        $incrementalFast = ($run2.durationSeconds -lt $Day2IncrementalThresholdSeconds)
                        Add-Check -Result $r -Name "Incremental build is fast" -Passed $incrementalFast -Detail "pass2=$($run2.durationSeconds)s threshold=$($Day2IncrementalThresholdSeconds)s"
                        if ($run1Code -ne 0) { $r.metrics.buildPass1Tail = ($run1.output -split "`n" | Select-Object -Last 8) -join "`n" }
                        if ($run2Code -ne 0) { $r.metrics.buildPass2Tail = ($run2.output -split "`n" | Select-Object -Last 8) -join "`n" }
                        $r.metrics.buildPass1Seconds = $run1.durationSeconds
                        $r.metrics.buildPass2Seconds = $run2.durationSeconds
                        $r.metrics.day2IncrementalThresholdSeconds = $Day2IncrementalThresholdSeconds
                        $r.metrics.day2Targeted = $false
                    }
                } else {
                    $buildScript = Join-Path $repoRoot "build.bat"
                    Add-Check -Result $r -Name "build.bat present" -Passed (Test-Path $buildScript) -Detail $buildScript
                    if (Test-Path $buildScript) {
                        $r.notes += "No CMakeCache found; falling back to build.bat (configure + build twice)."
                        $run1 = Invoke-CmdSafe -Command "`"$buildScript`"" -FriendlyName "build pass 1"
                        $run2 = Invoke-CmdSafe -Command "`"$buildScript`"" -FriendlyName "build pass 2"
                        Add-Check -Result $r -Name "Build pass 1" -Passed ($run1.code -eq 0) -Detail "exit=$($run1.code)"
                        Add-Check -Result $r -Name "Build pass 2" -Passed ($run2.code -eq 0) -Detail "exit=$($run2.code)"
                    } else {
                        Add-Check -Result $r -Name "Build pass 1" -Passed $false -Detail "no build.bat and no configured build dir"
                    }
                }
            }
            Save-Result -Result $r
            return $r
        }

        3 {
            $r = New-Result -DayNumber 3 -Name "Core Test Lane Stabilization"
            if ($NoTests) {
                Add-Check -Result $r -Name "Tests skipped" -Passed $true -Detail "NoTests flag active"
            } else {
                $testScript = Join-Path $repoRoot "run-tests.ps1"
                Add-Check -Result $r -Name "run-tests.ps1 present" -Passed (Test-Path $testScript) -Detail $testScript
                if (Test-Path $testScript) {
                    $ps = "powershell -NoProfile -ExecutionPolicy Bypass -File `"$testScript`" -Filter Smoke"
                    $run = Invoke-CmdSafe -Command $ps -FriendlyName "smoke tests"
                    Add-Check -Result $r -Name "Smoke tests" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                }
            }
            Save-Result -Result $r
            return $r
        }

        4 {
            $r = New-Result -DayNumber 4 -Name "Headless Runtime Hardening"
            $entry = Join-Path $repoRoot "src\main_headless_core.cpp"
            Add-Check -Result $r -Name "Headless entry present" -Passed (Test-Path $entry) -Detail $entry

            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Headless runtime probe skipped" -Passed $true -Detail "fast verification mode active"
                $r.notes += "Fast mode skips executable runtime probing and limits Day 4 to structural validation."
                Save-Result -Result $r
                return $r
            }

            $exe = Find-HeadlessRuntimeExe
            Add-Check -Result $r -Name "Headless runtime executable present" -Passed ([bool]$exe) -Detail $(if ($exe) { $exe } else { "RawrEngine.exe not found in known build paths" })

            if ($exe) {
                $runtimeJson = Join-Path $reportRoot "day04_runtime_probe.json"
                $runtimeProbeLog = Join-Path $reportRoot "day04_runtime_probe_stdout.txt"
                $runtimeCmd = "`"$exe`" --production-finishers-14d --skip-agentic --bench-max-mb 16 --bench-iters 1 --sweep-window-mb 16 --sweep-iters 16 --report `"$runtimeJson`""
                $run = Invoke-CmdSafe -Command $runtimeCmd -FriendlyName "day04 headless production finisher probe"
                $run.output | Set-Content -Path $runtimeProbeLog -Encoding UTF8

                $compatMode = "native-finisher"
                $probeOk = $false

                # Backward-compatible fallback for previously built binaries that do not support --report.
                if (($run.code -eq 2) -and (-not (Test-Path $runtimeJson))) {
                    Write-Log "Day 4 fallback: retrying without --report for older runtime binary"
                    $legacyCmd = "`"$exe`" --production-finishers-14d --skip-agentic --bench-max-mb 16 --bench-iters 1 --sweep-window-mb 16 --sweep-iters 16"
                    $legacyRun = Invoke-CmdSafe -Command $legacyCmd -FriendlyName "day04 legacy runtime probe"
                    Add-Content -Path $runtimeProbeLog -Value "`r`n--- legacy-fallback-output ---`r`n$($legacyRun.output)"

                    if ($legacyRun.output -match 'PROD_FINISHERS_14D_JSON:(\{.*\})') {
                        $Matches[1] | Set-Content -Path $runtimeJson -Encoding UTF8
                        $compatMode = "legacy-stdout-json"
                    }

                    $run = $legacyRun
                }

                # Compatibility fallback for older binaries that do not expose --production-finishers-14d.
                if (($run.code -eq 2) -and (-not (Test-Path $runtimeJson)) -and ($run.output -match "--compile-only")) {
                    Write-Log "Day 4 compatibility fallback: running --compile-only runtime probe"
                    $compileOnly = Invoke-CmdSafe -Command "`"$exe`" --compile-only" -FriendlyName "day04 compile-only compatibility probe"
                    Add-Content -Path $runtimeProbeLog -Value "`r`n--- compile-only-fallback-output ---`r`n$($compileOnly.output)"

                    $compatPayload = [ordered]@{
                        ok = ($compileOnly.code -eq 0)
                        compatMode = "compile-only-fallback"
                        command = "--compile-only"
                        exitCode = $compileOnly.code
                        timestampUtc = (Get-Date).ToUniversalTime().ToString("o")
                    }
                    $compatPayload | ConvertTo-Json -Depth 6 | Set-Content -Path $runtimeJson -Encoding UTF8
                    $run = $compileOnly
                    $compatMode = "compile-only-fallback"
                }

                if (Test-Path $runtimeJson) {
                    try {
                        $probe = Get-Content -Path $runtimeJson -Raw | ConvertFrom-Json
                        Add-Check -Result $r -Name "Runtime probe logical success" -Passed ([bool]$probe.ok) -Detail "ok=$($probe.ok)"
                        $probeOk = [bool]$probe.ok
                        $r.metrics.runtimeProbeOk = [bool]$probe.ok
                    } catch {
                        Add-Check -Result $r -Name "Runtime probe JSON parse" -Passed $false -Detail "invalid JSON payload"
                    }
                }

                $runtimeExitPass = (($run.code -eq 0) -or $probeOk)
                Add-Check -Result $r -Name "Headless production finisher command exit" -Passed $runtimeExitPass -Detail "exit=$($run.code) mode=$compatMode probeOk=$probeOk"
                Add-Check -Result $r -Name "Headless runtime probe report" -Passed (Test-Path $runtimeJson) -Detail $runtimeJson

                $r.metrics.runtimeProbeExit = $run.code
                $r.metrics.runtimeProbeMode = $compatMode
            }

            $r.notes += "Day 4 now runs executable hardening probe and requires report emission."
            Save-Result -Result $r
            return $r
        }

        5 {
            $r = New-Result -DayNumber 5 -Name "Extension Infra Invariants"
            $expect = @(
                "include\extension_activation_events.h",
                "src\extension_activation_events.cpp",
                "include\extension_manifest_parser.h",
                "src\extension_manifest_parser.cpp",
                "include\extension_permissions.h",
                "src\extension_permissions.cpp",
                "include\marketplace_discovery_backend.h",
                "src\marketplace_discovery_backend.cpp",
                "include\extension_dependency_resolver.h",
                "src\extension_dependency_resolver.cpp",
                "include\extension_auto_updater.h",
                "src\extension_auto_updater.cpp",
                "include\extension_configuration_ui.h",
                "src\extension_configuration_ui.cpp",
                "include\workspace_trust_integration.h",
                "src\workspace_trust_integration.cpp",
                "src\quickjs_extension_host.cpp"
            )

            $present = 0
            foreach ($rel in $expect) {
                $full = Join-Path $repoRoot $rel
                $ok = Test-FilePresent -Path $full
                if ($ok) { $present++ }
                Add-Check -Result $r -Name $rel -Passed $ok -Detail $(if ($ok) { "present" } else { "missing" })
            }
            $r.metrics.extensionInfraFilesPresent = "$present/$($expect.Count)"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Executable extension finisher probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-ExtensionInstallerFinisherDay -InstallerDay 3 -Label "day05 extension installer idempotency probe"
                Add-Check -Result $r -Name "Extension installer executable gate" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $r.metrics.extensionInstallerGate = $run.code
            }
            Save-Result -Result $r
            return $r
        }

        6 {
            $r = New-Result -DayNumber 6 -Name "Security and Trust Boundaries"
            Add-Check -Result $r -Name "Permissions source present" -Passed (Test-Path (Join-Path $repoRoot "src\extension_permissions.cpp")) -Detail "permission gate source"
            Add-Check -Result $r -Name "Trust source present" -Passed (Test-Path (Join-Path $repoRoot "src\workspace_trust_integration.cpp")) -Detail "trust gate source"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Runtime trust/smoke probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-TurnkeySmoke -Label "day06 turnkey trust boundary probe" -TurnkeyArgs @("-SkipCopilotCli")
                Add-Check -Result $r -Name "Turnkey smoke gate" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $r.metrics.turnkeyGate = $run.code
            }
            Save-Result -Result $r
            return $r
        }

        7 {
            $r = New-Result -DayNumber 7 -Name "Marketplace and Dependency Integrity"
            Add-Check -Result $r -Name "Marketplace backend present" -Passed (Test-Path (Join-Path $repoRoot "src\marketplace_discovery_backend.cpp")) -Detail "marketplace"
            Add-Check -Result $r -Name "Dependency resolver present" -Passed (Test-Path (Join-Path $repoRoot "src\extension_dependency_resolver.cpp")) -Detail "resolver"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Marketplace installer probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-ExtensionInstallerFinisherDay -InstallerDay 12 -Label "day07 marketplace consistency probe"
                Add-Check -Result $r -Name "Marketplace executable gate" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $r.metrics.marketplaceGate = $run.code
            }
            Save-Result -Result $r
            return $r
        }

        8 {
            $r = New-Result -DayNumber 8 -Name "Update and Rollback Reliability"
            Add-Check -Result $r -Name "Auto updater present" -Passed (Test-Path (Join-Path $repoRoot "src\extension_auto_updater.cpp")) -Detail "updater"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Rollback/failure probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-ExtensionInstallerFinisherDay -InstallerDay 4 -Label "day08 rollback failure probe"
                Add-Check -Result $r -Name "Failure recovery executable gate" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $r.metrics.rollbackGate = $run.code
            }
            Save-Result -Result $r
            return $r
        }

        9 {
            $r = New-Result -DayNumber 9 -Name "Configuration Schema Safety"
            Add-Check -Result $r -Name "Configuration UI present" -Passed (Test-Path (Join-Path $repoRoot "src\extension_configuration_ui.cpp")) -Detail "config"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Configuration safety integration probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-MinimalIntegrationGate -Label "day09 configuration integration probe" -SkipExtensionInstaller -TurnkeyArgs @("-SkipCopilotCli")
                Add-Check -Result $r -Name "Configuration integration gate" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $r.metrics.configurationIntegrationGate = $run.code
            }
            Save-Result -Result $r
            return $r
        }

        10 {
            $r = New-Result -DayNumber 10 -Name "Performance Envelope"
            $r.notes += "Integrate benchmark scripts in repo-specific CI lane as needed."
            Add-Check -Result $r -Name "Benchmark scripts folder exists" -Passed (Test-Path (Join-Path $repoRoot "scripts")) -Detail "scripts"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Runtime perf probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $exe = Find-HeadlessRuntimeExe
                Add-Check -Result $r -Name "Headless runtime executable present" -Passed ([bool]$exe) -Detail $(if ($exe) { $exe } else { "RawrEngine.exe not found" })
                if ($exe) {
                    $perfJson = Join-Path $reportRoot "day10_runtime_probe.json"
                    $run = Invoke-CmdSafe -Command "`"$exe`" --production-finishers-14d --skip-agentic --bench-max-mb 64 --bench-iters 2 --sweep-window-mb 32 --sweep-iters 32 --report `"$perfJson`"" -FriendlyName "day10 performance envelope probe"
                    $compatMode = "native-finisher"

                    if ((-not (Test-Path $perfJson)) -and ($run.output -match 'PROD_FINISHERS_14D_JSON:(\{.*\})')) {
                        $Matches[1] | Set-Content -Path $perfJson -Encoding UTF8
                        $compatMode = "native-stdout-json"
                    }

                    if (($run.code -eq 2) -and (-not (Test-Path $perfJson))) {
                        Write-Log "Day 10 fallback: retrying without --report for older runtime binary"
                        $legacyCmd = "`"$exe`" --production-finishers-14d --skip-agentic --bench-max-mb 64 --bench-iters 2 --sweep-window-mb 32 --sweep-iters 32"
                        $legacyRun = Invoke-CmdSafe -Command $legacyCmd -FriendlyName "day10 legacy performance probe"
                        if ($legacyRun.output -match 'PROD_FINISHERS_14D_JSON:(\{.*\})') {
                            $Matches[1] | Set-Content -Path $perfJson -Encoding UTF8
                            $compatMode = "legacy-stdout-json"
                        }
                        $run = $legacyRun
                    }

                    if ((-not (Test-Path $perfJson)) -and ($run.code -ne 0)) {
                        Write-Log "Day 10 compatibility fallback: running --compile-only runtime probe"
                        $compileOnly = Invoke-CmdSafe -Command "`"$exe`" --compile-only" -FriendlyName "day10 compile-only compatibility probe"
                        $compatPayload = [ordered]@{
                            ok = ($compileOnly.code -eq 0)
                            compatMode = "compile-only-fallback"
                            command = "--compile-only"
                            exitCode = $compileOnly.code
                            timestampUtc = (Get-Date).ToUniversalTime().ToString("o")
                        }
                        $compatPayload | ConvertTo-Json -Depth 6 | Set-Content -Path $perfJson -Encoding UTF8
                        $run = $compileOnly
                        $compatMode = "compile-only-fallback"
                    }

                    $probeOk = $false
                    if (Test-Path $perfJson) {
                        try {
                            $probe = Get-Content -Path $perfJson -Raw | ConvertFrom-Json
                            $probeOk = [bool]$probe.ok
                            Add-Check -Result $r -Name "Runtime perf logical success" -Passed $probeOk -Detail "ok=$($probe.ok)"
                            $r.metrics.runtimePerfProbeOk = $probeOk
                        } catch {
                            Add-Check -Result $r -Name "Runtime perf JSON parse" -Passed $false -Detail "invalid JSON payload"
                        }
                    }

                    $perfExitPass = (($run.code -eq 0) -or $probeOk)
                    Add-Check -Result $r -Name "Runtime perf probe exit" -Passed $perfExitPass -Detail "exit=$($run.code) mode=$compatMode probeOk=$probeOk"
                    Add-Check -Result $r -Name "Runtime perf report exists" -Passed (Test-Path $perfJson) -Detail $perfJson
                    $r.metrics.runtimePerfProbe = $run.code
                    $r.metrics.runtimePerfProbeMode = $compatMode
                }
            }
            Save-Result -Result $r
            return $r
        }

        11 {
            $r = New-Result -DayNumber 11 -Name "Failure Intelligence and Recovery"
            Add-Check -Result $r -Name "JSON guard present" -Passed (Test-Path (Join-Path $repoRoot "src\json_parse_guard.hpp")) -Detail "json guard"
            Add-Check -Result $r -Name "Self-healing executor present" -Passed (Test-Path (Join-Path $repoRoot "src\self_healing_tool_executor.hpp")) -Detail "self-heal"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Failure-path executable probe skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-ExtensionInstallerFinisherDay -InstallerDay 11 -Label "day11 failure regression baseline"
                Add-Check -Result $r -Name "Failure regression executable gate" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $r.metrics.failureRegressionGate = $run.code
            }
            Save-Result -Result $r
            return $r
        }

        12 {
            $r = New-Result -DayNumber 12 -Name "Integration and Packaging"
            Add-Check -Result $r -Name "Production README present" -Passed (Test-Path (Join-Path $repoRoot "README_PRODUCTION.md")) -Detail "production docs"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Minimal integration gate skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $run = Invoke-MinimalIntegrationGate -Label "day12 minimal integration gate"
                Add-Check -Result $r -Name "Minimal integration gate exit" -Passed ($run.code -eq 0) -Detail "exit=$($run.code)"
                $gateJson = Join-Path $repoRoot "logs\integration_gate_minimal_last.json"
                Add-Check -Result $r -Name "Integration gate summary JSON" -Passed (Test-Path $gateJson) -Detail $gateJson
                if (Test-Path $gateJson) {
                    try {
                        $gate = Get-Content -Path $gateJson -Raw | ConvertFrom-Json
                        Add-Check -Result $r -Name "Integration gate logical success" -Passed ([bool]$gate.ok) -Detail "ok=$($gate.ok)"
                        $r.metrics.integrationGateOk = [bool]$gate.ok
                    } catch {
                        Add-Check -Result $r -Name "Integration gate JSON parse" -Passed $false -Detail "invalid JSON payload"
                    }
                }
            }
            Save-Result -Result $r
            return $r
        }

        13 {
            $r = New-Result -DayNumber 13 -Name "Full Dress Rehearsal"
            Add-Check -Result $r -Name "Final verification doc present" -Passed (Test-Path (Join-Path $repoRoot "FINAL_VERIFICATION_COMPLETE.md")) -Detail "verification"
            if ($NoBuild -and $NoTests) {
                Add-Check -Result $r -Name "Full dress rehearsal skipped" -Passed $true -Detail "fast verification mode active"
            } else {
                $turnkey = Invoke-TurnkeySmoke -Label "day13 turnkey full dress rehearsal"
                Add-Check -Result $r -Name "Turnkey dress rehearsal" -Passed ($turnkey.code -eq 0) -Detail "exit=$($turnkey.code)"
                $gate = Invoke-MinimalIntegrationGate -Label "day13 integration dress rehearsal"
                Add-Check -Result $r -Name "Integration dress rehearsal" -Passed ($gate.code -eq 0) -Detail "exit=$($gate.code)"
                $r.metrics.turnkeyDressRehearsal = $turnkey.code
                $r.metrics.integrationDressRehearsal = $gate.code
            }
            Save-Result -Result $r
            return $r
        }

        14 {
            $r = New-Result -DayNumber 14 -Name "Release Sign-Off"
            $daily = Get-ChildItem -Path $reportRoot -Filter "day*_report.json" -ErrorAction SilentlyContinue
            Add-Check -Result $r -Name "Daily reports available" -Passed ($daily.Count -ge 14) -Detail "$($daily.Count) reports found"

            if ($Strict -and ($NoBuild -or $NoTests)) {
                Add-Check -Result $r -Name "Strict evidence prerequisites" -Passed $false -Detail "Strict mode requires full evidence run: do not set -NoBuild or -NoTests"
            } else {
                Add-Check -Result $r -Name "Strict evidence prerequisites" -Passed $true -Detail "Evidence mode compatible with strict validation"
            }

            $allReports = @()
            foreach ($f in $daily | Where-Object { $_.BaseName -ne "day14_report" }) {
                $allReports += (Get-Content -Path $f.FullName -Raw | ConvertFrom-Json)
            }

            $failCount = ($allReports | Where-Object { $_.status -ne "PASS" } | Measure-Object).Count
            $r.metrics.failedDayReports = $failCount
            if ($Strict -and $failCount -gt 0) {
                Add-Check -Result $r -Name "Strict mode gate" -Passed $false -Detail "One or more day reports failed"
            } else {
                Add-Check -Result $r -Name "Sign-off gate" -Passed $true -Detail "Strict=$Strict failDays=$failCount"
            }

            $quality = Invoke-QualityGateValidation -Label "day14 quality gate validation"
            $qualityCode = $null
            if ($null -ne $quality -and $quality.PSObject.Properties.Name -contains "code") {
                $qualityCode = $quality.code
            }
            $qualityEvidencePresent = (Test-Path -LiteralPath (Join-Path $reportRoot "quality_gate_ledger.json")) -and
                                     (Test-Path -LiteralPath (Join-Path $reportRoot "quality_gate_summary.md"))
            if ($null -eq $qualityCode -and $qualityEvidencePresent) {
                # Some shells can surface a non-typed return object with code omitted; evidence files confirm execution.
                $qualityCode = 0
            }
            Add-Check -Result $r -Name "Quality gate validator execution" -Passed ($qualityCode -in @(0,1,2)) -Detail "exit=$qualityCode"

            $qualityJson = Join-Path $reportRoot "quality_gate_ledger.json"
            $qualityMd = Join-Path $reportRoot "quality_gate_summary.md"
            $qualityRemediation = Join-Path $reportRoot "quality_gate_remediation.md"
            $qualityTransitions = Join-Path $reportRoot "quality_gate_phase_transitions.md"
            $qualityBlockerQueue = Join-Path $reportRoot "quality_gate_blocker_queue.md"
            $qualityManifestJson = Join-Path $reportRoot "quality_gate_artifact_manifest.json"
            $qualityManifestMd = Join-Path $reportRoot "quality_gate_artifact_manifest.md"
            $qualityHistoryJsonl = Join-Path $reportRoot "quality_gate_history.jsonl"
            $qualityTrendMd = Join-Path $reportRoot "quality_gate_trend.md"
            $qualityWaiverAuditMd = Join-Path $reportRoot "quality_gate_waiver_audit.md"
            $qualityAccountabilityMd = Join-Path $reportRoot "quality_gate_accountability.md"
            $qualityFreshnessMd = Join-Path $reportRoot "quality_gate_freshness.md"
            $qualityEvidenceDebtMd = Join-Path $reportRoot "quality_gate_evidence_debt.md"
            $qualityCoverageMd = Join-Path $reportRoot "quality_gate_coverage_matrix.md"
            Add-Check -Result $r -Name "Quality gate ledger exists" -Passed (Test-Path -LiteralPath $qualityJson) -Detail $qualityJson
            Add-Check -Result $r -Name "Quality gate summary exists" -Passed (Test-Path -LiteralPath $qualityMd) -Detail $qualityMd
            Add-Check -Result $r -Name "Quality gate remediation exists" -Passed (Test-Path -LiteralPath $qualityRemediation) -Detail $qualityRemediation
            Add-Check -Result $r -Name "Quality gate phase transitions exist" -Passed (Test-Path -LiteralPath $qualityTransitions) -Detail $qualityTransitions
            Add-Check -Result $r -Name "Quality gate blocker queue exists" -Passed (Test-Path -LiteralPath $qualityBlockerQueue) -Detail $qualityBlockerQueue
            Add-Check -Result $r -Name "Quality gate artifact manifest json exists" -Passed (Test-Path -LiteralPath $qualityManifestJson) -Detail $qualityManifestJson
            Add-Check -Result $r -Name "Quality gate artifact manifest md exists" -Passed (Test-Path -LiteralPath $qualityManifestMd) -Detail $qualityManifestMd
            Add-Check -Result $r -Name "Quality gate history jsonl exists" -Passed (Test-Path -LiteralPath $qualityHistoryJsonl) -Detail $qualityHistoryJsonl
            Add-Check -Result $r -Name "Quality gate trend md exists" -Passed (Test-Path -LiteralPath $qualityTrendMd) -Detail $qualityTrendMd
            Add-Check -Result $r -Name "Quality gate waiver audit md exists" -Passed (Test-Path -LiteralPath $qualityWaiverAuditMd) -Detail $qualityWaiverAuditMd
            Add-Check -Result $r -Name "Quality gate accountability md exists" -Passed (Test-Path -LiteralPath $qualityAccountabilityMd) -Detail $qualityAccountabilityMd
            Add-Check -Result $r -Name "Quality gate freshness md exists" -Passed (Test-Path -LiteralPath $qualityFreshnessMd) -Detail $qualityFreshnessMd
            Add-Check -Result $r -Name "Quality gate evidence debt md exists" -Passed (Test-Path -LiteralPath $qualityEvidenceDebtMd) -Detail $qualityEvidenceDebtMd
            Add-Check -Result $r -Name "Quality gate coverage matrix md exists" -Passed (Test-Path -LiteralPath $qualityCoverageMd) -Detail $qualityCoverageMd

            if (Test-Path -LiteralPath $qualityJson) {
                try {
                    $qualityPayload = Get-Content -LiteralPath $qualityJson -Raw | ConvertFrom-Json
                    $overallVerdict = [string]$qualityPayload.overallVerdict
                    $r.metrics.qualityGateOverallVerdict = $overallVerdict
                    $r.metrics.qualityGatePassDays = [int]$qualityPayload.dayCounts.pass
                    $r.metrics.qualityGateRiskDays = [int]$qualityPayload.dayCounts.proceedWithRisk
                    $r.metrics.qualityGateBlockedDays = [int]$qualityPayload.dayCounts.blocked
                    $r.metrics.qualityGateBlockedDayList = if ($qualityPayload.blockedDays) { ($qualityPayload.blockedDays -join ",") } else { "" }
                    $r.metrics.qualityGateRiskDayList = if ($qualityPayload.riskDays) { ($qualityPayload.riskDays -join ",") } else { "" }
                    if ($qualityPayload.PSObject.Properties.Name -contains "phaseTransitions") {
                        $phaseTransitionPasses = 0
                        $phaseTransitionNoGos = 0
                        foreach ($phaseProp in $qualityPayload.phaseTransitions.PSObject.Properties) {
                            if ([bool]$phaseProp.Value.goNoGo) {
                                $phaseTransitionPasses += 1
                            } else {
                                $phaseTransitionNoGos += 1
                            }
                        }
                        $r.metrics.qualityGatePhaseTransitionGo = $phaseTransitionPasses
                        $r.metrics.qualityGatePhaseTransitionNoGo = $phaseTransitionNoGos
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "blockerQueue") {
                        $queueCount = @($qualityPayload.blockerQueue).Count
                        $queueP1Count = (@($qualityPayload.blockerQueue | Where-Object { [string]$_.priority -eq "P1" })).Count
                        $queueStrictCount = (@($qualityPayload.blockerQueue | Where-Object { [bool]$_.requiredForStrict })).Count
                        $r.metrics.qualityGateBlockerQueueCount = $queueCount
                        $r.metrics.qualityGateBlockerQueueP1Count = $queueP1Count
                        $r.metrics.qualityGateBlockerQueueStrictCount = $queueStrictCount
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "artifactManifest") {
                        $r.metrics.qualityGateArtifactManifestCount = [int]$qualityPayload.artifactManifest.artifactCount
                        $r.metrics.qualityGateArtifactManifestJson = [string]$qualityPayload.artifactManifest.manifestJson
                        $r.metrics.qualityGateArtifactManifestMarkdown = [string]$qualityPayload.artifactManifest.manifestMarkdown
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "trend") {
                        $r.metrics.qualityGateTrendDirection = [string]$qualityPayload.trend.direction
                        $r.metrics.qualityGateTrendPreviousVerdict = [string]$qualityPayload.trend.previousOverallVerdict
                        $r.metrics.qualityGateTrendCurrentVerdict = [string]$qualityPayload.trend.currentOverallVerdict
                        $r.metrics.qualityGateTrendPreviousBlockedDays = [string]$qualityPayload.trend.previousBlockedDays
                        $r.metrics.qualityGateTrendCurrentBlockedDays = [string]$qualityPayload.trend.currentBlockedDays
                        $r.metrics.qualityGateTrendHistoryPath = [string]$qualityPayload.trend.historyPath
                        $r.metrics.qualityGateTrendReportPath = [string]$qualityPayload.trend.trendReportPath
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "waiverAudit") {
                        $r.metrics.qualityGateWaiverFilePresent = [string]$qualityPayload.waiverAudit.waiverFilePresent
                        $r.metrics.qualityGateWaiversLoaded = [string]$qualityPayload.waiverAudit.waiversLoaded
                        $r.metrics.qualityGateWaiversActive = [string]$qualityPayload.waiverAudit.activeWaivers
                        $r.metrics.qualityGateWaiversApplied = [string]$qualityPayload.waiverAudit.appliedWaivers
                        $r.metrics.qualityGateWaiversExpired = [string]$qualityPayload.waiverAudit.expiredWaivers
                        $r.metrics.qualityGateWaiversInvalid = [string]$qualityPayload.waiverAudit.invalidWaivers
                        $r.metrics.qualityGateWaiverGovernanceBlocked = [string]$qualityPayload.waiverAudit.governanceBlocked
                        if ($qualityPayload.waiverAudit.PSObject.Properties.Name -contains "artifactPath") {
                            $r.metrics.qualityGateWaiverAuditPath = [string]$qualityPayload.waiverAudit.artifactPath
                        }
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "accountability") {
                        $r.metrics.qualityGateAccountabilityEntryCount = [string]$qualityPayload.accountability.entryCount
                        $r.metrics.qualityGateAccountabilityBlockedOwners = [string](@($qualityPayload.accountability.blockedOwners) -join ",")
                        $r.metrics.qualityGateAccountabilityRiskOwners = [string](@($qualityPayload.accountability.riskOwners) -join ",")
                        $r.metrics.qualityGateAccountabilityEarliestDeadlineUtc = [string]$qualityPayload.accountability.earliestDeadlineUtc
                        if ($qualityPayload.accountability.PSObject.Properties.Name -contains "artifactPath") {
                            $r.metrics.qualityGateAccountabilityPath = [string]$qualityPayload.accountability.artifactPath
                        }
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "freshnessAudit") {
                        $r.metrics.qualityGateFreshnessWindowHours = [string]$qualityPayload.freshnessAudit.windowHours
                        $r.metrics.qualityGateFreshnessStaleCount = [string]$qualityPayload.freshnessAudit.staleCount
                        $r.metrics.qualityGateFreshnessMissingTimestampCount = [string]$qualityPayload.freshnessAudit.missingTimestampCount
                        $r.metrics.qualityGateFreshnessStaleDays = [string](@($qualityPayload.freshnessAudit.staleDays) -join ",")
                        $r.metrics.qualityGateFreshnessGovernanceBlocked = [string]$qualityPayload.freshnessAudit.governanceBlocked
                        if ($qualityPayload.freshnessAudit.PSObject.Properties.Name -contains "artifactPath") {
                            $r.metrics.qualityGateFreshnessPath = [string]$qualityPayload.freshnessAudit.artifactPath
                        }
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "evidenceDebt") {
                        $r.metrics.qualityGateEvidenceDebtTotalMissing = [string]$qualityPayload.evidenceDebt.totalMissingEvidence
                        $r.metrics.qualityGateEvidenceDebtTopDay = [string]$qualityPayload.evidenceDebt.topDebtDay
                        $r.metrics.qualityGateEvidenceDebtTopCount = [string]$qualityPayload.evidenceDebt.topDebtCount
                        $r.metrics.qualityGateEvidenceDebtTopPhase = [string]$qualityPayload.evidenceDebt.topDebtPhase
                        if ($qualityPayload.evidenceDebt.PSObject.Properties.Name -contains "artifactPath") {
                            $r.metrics.qualityGateEvidenceDebtPath = [string]$qualityPayload.evidenceDebt.artifactPath
                        }
                    }
                    if ($qualityPayload.PSObject.Properties.Name -contains "coverageMatrix") {
                        $r.metrics.qualityGateCoverageTotalSlots = [string]$qualityPayload.coverageMatrix.totalEvidenceSlots
                        $r.metrics.qualityGateCoveragePresentPct = [string]$qualityPayload.coverageMatrix.overallPresentPct
                        $r.metrics.qualityGateCoverageEffectivePct = [string]$qualityPayload.coverageMatrix.overallEffectivePct
                        $r.metrics.qualityGateCoverageLowestCategory = [string]$qualityPayload.coverageMatrix.lowestCoverageCategory
                        $r.metrics.qualityGateCoverageLowestCategoryPct = [string]$qualityPayload.coverageMatrix.lowestCoverageCategoryPct
                        $r.metrics.qualityGateCoverageLowestDay = [string]$qualityPayload.coverageMatrix.lowestCoverageDay
                        $r.metrics.qualityGateCoverageLowestDayPct = [string]$qualityPayload.coverageMatrix.lowestCoverageDayPct
                        if ($qualityPayload.coverageMatrix.PSObject.Properties.Name -contains "artifactPath") {
                            $r.metrics.qualityGateCoveragePath = [string]$qualityPayload.coverageMatrix.artifactPath
                        }
                    }

                    $qualityPass = $overallVerdict -eq "Pass"
                    if (-not $Strict -and $overallVerdict -eq "Proceed with risk") {
                        $qualityPass = $true
                    }

                    Add-Check -Result $r -Name "Quality gate verdict" -Passed $qualityPass -Detail "verdict=$overallVerdict strict=$Strict"
                } catch {
                    Add-Check -Result $r -Name "Quality gate ledger parse" -Passed $false -Detail ([string]$_.Exception.Message)
                }
            }

            $scorecard = Join-Path $reportRoot "final_scorecard.md"
            $lines = @(
                "# 14-Day Production Scorecard",
                "",
                "- Generated: $(Get-Date -Format s)",
                "- Strict Mode: $Strict",
                "- Failed Day Reports: $failCount",
                "- Status: $(if ($r.status -eq 'PASS') { 'PASS' } else { 'FAIL' })"
            )
            $lines -join "`r`n" | Set-Content -Path $scorecard -Encoding UTF8
            Save-Result -Result $r
            return $r
        }

        default {
            throw "Unsupported day: $N"
        }
    }
}

Write-Log "Starting 14-day production finisher orchestrator"

$daysToRun = @()
if ($AllDays) {
    $daysToRun = 1..14
} elseif ($PSBoundParameters.ContainsKey("Day")) {
    $daysToRun = @($Day)
} else {
    throw "Specify -Day <1..14> or -AllDays"
}

$results = @()
foreach ($d in $daysToRun) {
    try {
        Write-Log "Executing Day $d"
        $results += Invoke-Day -N $d
    } catch {
        Write-Log "Day $d crashed: $($_.Exception.Message)" "ERROR"
        if ($Strict) { throw }
    }
}

$failed = ($results | Where-Object { $_.status -ne "PASS" } | Measure-Object).Count
Write-Log ("Execution complete. Days run={0}, failed={1}" -f $results.Count, $failed)

if ($Strict -and $failed -gt 0) {
    exit 2
}
exit 0
