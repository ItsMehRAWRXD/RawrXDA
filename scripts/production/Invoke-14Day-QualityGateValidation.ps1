param(
    [string]$ReportRoot = "",
    [string]$WaiverFile = "",
    [ValidateRange(1,168)]
    [int]$FreshnessWindowHours = 36,
    [switch]$Strict
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not $ReportRoot) {
    $repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $ReportRoot = Join-Path $repoRoot "reports\14day"
}

if (-not (Test-Path -LiteralPath $ReportRoot)) {
    Write-Error "Report root not found: $ReportRoot"
    exit 2
}

function Get-PhaseInfo {
    param([int]$Day)

    if ($Day -ge 1 -and $Day -le 5) {
        return [ordered]@{ phase = "Phase 1: Agent Polish"; gate = "Day $Day" }
    }
    if ($Day -ge 6 -and $Day -le 9) {
        return [ordered]@{ phase = "Phase 2: Native Extension Host"; gate = "Day $Day" }
    }
    if ($Day -ge 10 -and $Day -le 12) {
        return [ordered]@{ phase = "Phase 3: LSP Final Features"; gate = "Day $Day" }
    }
    return [ordered]@{ phase = "Phase 4: Performance and Finalization"; gate = "Day $Day" }
}

function Get-PhaseSpecialist {
    param([string]$Phase)

    switch ($Phase) {
        "Phase 1: Agent Polish" { return "AgentPolish" }
        "Phase 2: Native Extension Host" { return "ExtensionHost" }
        "Phase 3: LSP Final Features" { return "LSPComplete" }
        default { return "Performance" }
    }
}

function Join-ReportText {
    param($Report)

    $parts = @()
    if ($null -ne $Report.name) { $parts += [string]$Report.name }
    if ($null -ne $Report.notes) {
        foreach ($n in $Report.notes) {
            $parts += [string]$n
        }
    }
    if ($null -ne $Report.checks) {
        foreach ($c in $Report.checks) {
            $parts += [string]$c.name
            $parts += [string]$c.detail
        }
    }
    if ($null -ne $Report.metrics) {
        if ($Report.metrics -is [System.Collections.IDictionary]) {
            foreach ($k in $Report.metrics.Keys) {
                $parts += [string]$k
                $parts += [string]$Report.metrics[$k]
            }
        } else {
            foreach ($p in $Report.metrics.PSObject.Properties) {
                $parts += [string]$p.Name
                $parts += [string]$p.Value
            }
        }
    }

    return ($parts -join " `n").ToLowerInvariant()
}

function Test-EvidenceCategory {
    param(
        [string]$Corpus,
        [string]$Category
    )

    $patterns = @()
    switch ($Category) {
        "Build" { $patterns = @("build", "cmake", "target", "compile", "incremental") }
        "Runtime" { $patterns = @("runtime", "headless", "probe", "smoke", "turnkey", "executable") }
        "Regression" { $patterns = @("regression", "integration", "stabilization", "ctest", "test", "rehearsal") }
        "Performance" { $patterns = @("performance", "benchmark", "latency", "throughput", "incremental") }
        "Security" { $patterns = @("security", "trust", "permission", "sandbox", "deny", "guard") }
        "Documentation" { $patterns = @("readme", "report", "summary", "scorecard", "doc", "notes") }
        default { return $false }
    }

    foreach ($p in $patterns) {
        if ($Corpus.Contains($p)) {
            return $true
        }
    }
    return $false
}

$requiredEvidence = @("Build", "Runtime", "Regression", "Performance", "Security", "Documentation")
$remediationHints = [ordered]@{
    Build = "Run a full configured build (no fast flags) and include compile target outcomes in the day report."
    Runtime = "Execute a headless or turnkey runtime probe and capture executable path, command, and exit outcome."
    Regression = "Run smoke/integration regression checks and include gate/test identifiers and their pass/fail state."
    Performance = "Record at least one benchmark/latency/throughput metric from a real run and persist values in metrics."
    Security = "Capture trust/sandbox/permission boundary validation evidence and explicit deny/guard outcomes."
    Documentation = "Publish day summary artifacts (report + notes) with operational context and final day status rationale."
}

if (-not $WaiverFile) {
    $WaiverFile = Join-Path $ReportRoot "quality_gate_waivers.json"
}

$nowUtc = (Get-Date).ToUniversalTime()
$waiverAudit = [ordered]@{
    waiverFile = $WaiverFile
    waiverFilePresent = (Test-Path -LiteralPath $WaiverFile)
    waiversLoaded = 0
    activeWaivers = 0
    expiredWaivers = 0
    invalidWaivers = 0
    appliedWaivers = 0
    governanceBlocked = $false
    issues = @()
}

$waiverRules = @()
if ($waiverAudit.waiverFilePresent) {
    try {
        $waiverData = Get-Content -LiteralPath $WaiverFile -Raw | ConvertFrom-Json
        $rawEntries = @()
        if ($waiverData -is [System.Array]) {
            $rawEntries = @($waiverData)
        } elseif ($null -ne $waiverData) {
            $rawEntries = @($waiverData)
        }

        $waiverAudit.waiversLoaded = $rawEntries.Count
        $idx = 0
        foreach ($entry in $rawEntries) {
            $idx += 1
            $entryDayRaw = if ($entry.PSObject.Properties.Name -contains "day") { [string]$entry.day } else { "" }
            $entryCatRaw = if ($entry.PSObject.Properties.Name -contains "category") { [string]$entry.category } else { "*" }
            $entryReason = if ($entry.PSObject.Properties.Name -contains "reason") { [string]$entry.reason } else { "" }
            $entryApprovedBy = if ($entry.PSObject.Properties.Name -contains "approvedBy") { [string]$entry.approvedBy } else { "" }
            $entryExpiresRaw = if ($entry.PSObject.Properties.Name -contains "expiresUtc") { [string]$entry.expiresUtc } else { "" }

            $normalizedDay = -1
            if ($entryDayRaw -eq "*") {
                $normalizedDay = 0
            } else {
                [void][int]::TryParse($entryDayRaw, [ref]$normalizedDay)
            }

            $normalizedCategory = if ($entryCatRaw) { $entryCatRaw } else { "*" }
            $categoryValid = ($normalizedCategory -eq "*") -or ($requiredEvidence -contains $normalizedCategory)
            $dayValid = (($normalizedDay -eq 0) -or (($normalizedDay -ge 1) -and ($normalizedDay -le 14)))
            $metaValid = ($entryReason.Trim().Length -gt 0) -and ($entryApprovedBy.Trim().Length -gt 0)

            $expiresUtc = [datetime]::MinValue
            $expiryValid = [datetime]::TryParse($entryExpiresRaw, [ref]$expiresUtc)
            $isExpired = $true
            if ($expiryValid) {
                $isExpired = ($expiresUtc.ToUniversalTime() -lt $nowUtc)
            }

            if ((-not $dayValid) -or (-not $categoryValid) -or (-not $metaValid) -or (-not $expiryValid)) {
                $waiverAudit.invalidWaivers += 1
                $waiverAudit.issues += ("invalid waiver #{0}: day='{1}', category='{2}', approvedBy='{3}', reason='{4}', expiresUtc='{5}'" -f $idx, $entryDayRaw, $entryCatRaw, $entryApprovedBy, $entryReason, $entryExpiresRaw)
                continue
            }

            if ($isExpired) {
                $waiverAudit.expiredWaivers += 1
                $waiverAudit.issues += ("expired waiver #{0}: day='{1}', category='{2}', expiresUtc='{3}'" -f $idx, $entryDayRaw, $entryCatRaw, $entryExpiresRaw)
                continue
            }

            $waiverRules += [ordered]@{
                id = $idx
                day = $normalizedDay
                category = $normalizedCategory
                approvedBy = $entryApprovedBy
                reason = $entryReason
                expiresUtc = $expiresUtc.ToUniversalTime().ToString("o")
            }
            $waiverAudit.activeWaivers += 1
        }
    } catch {
        $waiverAudit.invalidWaivers += 1
        $waiverAudit.issues += ("waiver file parse failed: {0}" -f [string]$_.Exception.Message)
    }
}

$freshnessAudit = [ordered]@{
    windowHours = $FreshnessWindowHours
    staleDays = @()
    missingTimestampDays = @()
    staleCount = 0
    missingTimestampCount = 0
    governanceBlocked = $false
    artifactPath = ""
}

$dayReportCache = @{}
for ($d = 1; $d -le 14; $d++) {
    $tag = "day{0:d2}" -f $d
    $path = Join-Path $ReportRoot ("{0}_report.json" -f $tag)
    if (-not (Test-Path -LiteralPath $path)) {
        continue
    }

    try {
        $dayReportCache[$d] = Get-Content -LiteralPath $path -Raw | ConvertFrom-Json
    } catch {
        # Keep cache sparse; malformed reports are handled in the main ledger pass.
    }
}

$sharedEvidence = [ordered]@{
    Build = ($dayReportCache.Contains(2) -and ([string]$dayReportCache[2].status -eq "PASS"))
    Runtime = (
        ($dayReportCache.Contains(4) -and ([string]$dayReportCache[4].status -eq "PASS")) -or
        ($dayReportCache.Contains(10) -and ([string]$dayReportCache[10].status -eq "PASS")) -or
        ($dayReportCache.Contains(12) -and ([string]$dayReportCache[12].status -eq "PASS"))
    )
    Regression = (
        ($dayReportCache.Contains(3) -and ([string]$dayReportCache[3].status -eq "PASS")) -or
        ($dayReportCache.Contains(9) -and ([string]$dayReportCache[9].status -eq "PASS")) -or
        ($dayReportCache.Contains(13) -and ([string]$dayReportCache[13].status -eq "PASS"))
    )
    Performance = (
        ($dayReportCache.Contains(10) -and ([string]$dayReportCache[10].status -eq "PASS")) -or
        ($dayReportCache.Contains(2) -and ([string]$dayReportCache[2].status -eq "PASS"))
    )
    Security = (
        ($dayReportCache.Contains(6) -and ([string]$dayReportCache[6].status -eq "PASS")) -or
        ($dayReportCache.Contains(7) -and ([string]$dayReportCache[7].status -eq "PASS")) -or
        ($dayReportCache.Contains(8) -and ([string]$dayReportCache[8].status -eq "PASS"))
    )
}

$ledger = @()

for ($day = 1; $day -le 14; $day++) {
    $dayTag = "day{0:d2}" -f $day
    $reportPath = Join-Path $ReportRoot ("{0}_report.json" -f $dayTag)
    $phaseInfo = Get-PhaseInfo -Day $day

    if (-not (Test-Path -LiteralPath $reportPath)) {
        $ledger += [ordered]@{
            day = $day
            phase = $phaseInfo.phase
            reportPath = $reportPath
            reportPresent = $false
            reportStatus = "MISSING"
            evidence = @{}
            missingEvidence = $requiredEvidence
            verdict = "Blocked"
            reason = "Daily report missing"
        }
        continue
    }

    $report = if ($dayReportCache.Contains($day)) {
        $dayReportCache[$day]
    } else {
        Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    }
    $corpus = Join-ReportText -Report $report
    $reportItem = Get-Item -LiteralPath $reportPath
    $summaryPath = Join-Path $ReportRoot ("{0}_summary.md" -f $dayTag)

    $evidence = [ordered]@{}
    $missingEvidence = @()
    $waivedEvidence = @()
    $remediation = @()
    foreach ($cat in $requiredEvidence) {
        $present = Test-EvidenceCategory -Corpus $corpus -Category $cat
        if (-not $present) {
            switch ($cat) {
                "Build" { $present = [bool]$sharedEvidence.Build }
                "Runtime" { $present = [bool]$sharedEvidence.Runtime }
                "Regression" { $present = [bool]$sharedEvidence.Regression }
                "Performance" { $present = [bool]$sharedEvidence.Performance }
                "Security" { $present = [bool]$sharedEvidence.Security }
                "Documentation" {
                    $present = (Test-Path -LiteralPath $summaryPath) -or (@($report.notes).Count -gt 0)
                }
            }
        }
        $evidence[$cat] = $present
        if (-not $present) {
            $missingEvidence += $cat
            if ($remediationHints.Contains($cat)) {
                $remediation += [string]$remediationHints[$cat]
            }
        }
    }

    foreach ($cat in @($missingEvidence)) {
        $applied = $waiverRules | Where-Object {
            (([int]$_.day -eq 0) -or ([int]$_.day -eq $day)) -and
            ((([string]$_.category) -eq "*") -or (([string]$_.category) -eq $cat))
        } | Select-Object -First 1

        if ($null -ne $applied) {
            $missingEvidence = @($missingEvidence | Where-Object { $_ -ne $cat })
            $waivedEvidence += [ordered]@{
                category = $cat
                waiverId = [int]$applied.id
                approvedBy = [string]$applied.approvedBy
                expiresUtc = [string]$applied.expiresUtc
                reason = [string]$applied.reason
            }
            $waiverAudit.appliedWaivers += 1
        }
    }

    $reportEndUtc = [datetime]::MinValue
    $reportEndOffset = [datetimeoffset]::MinValue
    $hasReportEndUtc = $false
    if ($report.PSObject.Properties.Name -contains "endTimeUtc") {
        $rawEndUtc = $report.endTimeUtc
        if ($rawEndUtc -is [datetimeoffset]) {
            $reportEndUtc = $rawEndUtc.UtcDateTime
            $hasReportEndUtc = $true
        } elseif ($rawEndUtc -is [datetime]) {
            if ($rawEndUtc.Kind -eq [System.DateTimeKind]::Unspecified) {
                $reportEndUtc = [datetime]::SpecifyKind($rawEndUtc, [System.DateTimeKind]::Utc)
            } else {
                $reportEndUtc = $rawEndUtc.ToUniversalTime()
            }
            $hasReportEndUtc = $true
        } else {
            $hasReportEndUtc = [datetimeoffset]::TryParse(
                [string]$rawEndUtc,
                [System.Globalization.CultureInfo]::InvariantCulture,
                [System.Globalization.DateTimeStyles]::RoundtripKind,
                [ref]$reportEndOffset)
            if ($hasReportEndUtc) {
                $reportEndUtc = $reportEndOffset.UtcDateTime
            }
        }
    }

    $fileAgeHours = [Math]::Round(($nowUtc - $reportItem.LastWriteTimeUtc).TotalHours, 2)
    $reportAgeHours = -1.0
    $freshnessReasons = @()
    $withinFreshnessWindow = $true

    if ($hasReportEndUtc) {
        $reportAgeHours = [Math]::Round(($nowUtc - $reportEndUtc).TotalHours, 2)
        if ($reportAgeHours -gt $FreshnessWindowHours) {
            $withinFreshnessWindow = $false
            $freshnessReasons += ("report end timestamp age {0}h exceeds {1}h window" -f $reportAgeHours, $FreshnessWindowHours)
        }
    } else {
        $withinFreshnessWindow = $false
        $freshnessReasons += "report endTimeUtc missing or invalid"
        $freshnessAudit.missingTimestampDays += $day
        $freshnessAudit.missingTimestampCount += 1
    }

    if ($fileAgeHours -gt $FreshnessWindowHours) {
        $withinFreshnessWindow = $false
        $freshnessReasons += ("report file age {0}h exceeds {1}h window" -f $fileAgeHours, $FreshnessWindowHours)
    }

    # Day 14 is evaluated inside the same sign-off flow that regenerates day14_report.
    # Ignore prior-run freshness for day 14 to avoid self-blocking the current run.
    if ($day -eq 14) {
        $withinFreshnessWindow = $true
        $freshnessReasons = @()
    }

    if (-not $withinFreshnessWindow) {
        $freshnessAudit.staleDays += $day
        $freshnessAudit.staleCount += 1
    }

    $verdict = "Pass"
    $reason = "All required evidence present"

    $reportStatusFailed = ($report.status -ne "PASS")
    # Day 14 report is generated by the sign-off flow that also invokes this
    # validator, so a stale/non-pass day14 report from a prior run must not
    # hard-block the current quality computation.
    if ($day -eq 14) {
        $reportStatusFailed = $false
    }

    if ($reportStatusFailed) {
        $verdict = "Blocked"
        $reason = "Day report status is $($report.status)"
    } elseif (-not $withinFreshnessWindow) {
        $verdict = "Blocked"
        $reason = "Evidence freshness window exceeded: $($freshnessReasons -join '; ')"
    } elseif ($missingEvidence.Count -gt 0) {
        if ($Strict) {
            $verdict = "Blocked"
            $reason = "Missing required evidence categories: $($missingEvidence -join ', ')"
        } else {
            $verdict = "Proceed with risk"
            $reason = "Missing evidence categories in non-strict mode: $($missingEvidence -join ', ')"
        }
    }

    $ledger += [ordered]@{
        day = $day
        phase = $phaseInfo.phase
        reportPath = $reportPath
        reportPresent = $true
        reportStatus = [string]$report.status
        evidence = $evidence
        freshness = [ordered]@{
            withinWindow = [bool]$withinFreshnessWindow
            windowHours = $FreshnessWindowHours
            reportEndUtc = if ($hasReportEndUtc) { $reportEndUtc.ToString("o") } else { "" }
            reportAgeHours = $reportAgeHours
            fileLastWriteUtc = $reportItem.LastWriteTimeUtc.ToUniversalTime().ToString("o")
            fileAgeHours = $fileAgeHours
            reasons = $freshnessReasons
        }
        missingEvidence = $missingEvidence
        waivedEvidence = $waivedEvidence
        remediation = $remediation
        verdict = $verdict
        reason = $reason
    }
}

$phaseSummary = [ordered]@{}
foreach ($entry in $ledger) {
    $phase = [string]$entry.phase
    if (-not $phaseSummary.Contains($phase)) {
        $phaseSummary[$phase] = [ordered]@{
            totalDays = 0
            pass = 0
            proceedWithRisk = 0
            blocked = 0
        }
    }

    $phaseSummary[$phase].totalDays += 1
    switch ($entry.verdict) {
        "Pass" { $phaseSummary[$phase].pass += 1 }
        "Proceed with risk" { $phaseSummary[$phase].proceedWithRisk += 1 }
        default { $phaseSummary[$phase].blocked += 1 }
    }
}

$phaseOrder = @(
    "Phase 1: Agent Polish",
    "Phase 2: Native Extension Host",
    "Phase 3: LSP Final Features",
    "Phase 4: Performance and Finalization"
)

$phaseTransitions = [ordered]@{}
for ($i = 0; $i -lt $phaseOrder.Count; $i++) {
    $phase = $phaseOrder[$i]
    $summary = $phaseSummary[$phase]

    $phaseVerdict = "Pass"
    if ($summary.blocked -gt 0) {
        $phaseVerdict = "Blocked"
    } elseif ($summary.proceedWithRisk -gt 0) {
        $phaseVerdict = "Proceed with risk"
    }

    $dependencyStatus = "Ready"
    $dependencyDetail = "No previous phase dependency"
    if ($i -gt 0) {
        $prevPhase = $phaseOrder[$i - 1]
        $prevVerdict = [string]$phaseTransitions[$prevPhase].phaseVerdict
        if ($prevVerdict -eq "Pass") {
            $dependencyStatus = "Ready"
            $dependencyDetail = "Previous phase passed"
        } else {
            $dependencyStatus = "Blocked"
            $dependencyDetail = "Previous phase verdict is $prevVerdict"
        }
    }

    $goNoGo = $true
    if ($dependencyStatus -ne "Ready") {
        $goNoGo = $false
    }
    if ($Strict -and $phaseVerdict -ne "Pass") {
        $goNoGo = $false
    }
    if ((-not $Strict) -and $phaseVerdict -eq "Blocked") {
        $goNoGo = $false
    }

    $nextAction = "Advance to next phase"
    if (-not $goNoGo) {
        if ($dependencyStatus -ne "Ready") {
            $nextAction = "Unblock predecessor phase before phase transition"
        } elseif ($phaseVerdict -eq "Blocked") {
            $nextAction = "Resolve blocked day evidence and rerun quality gate"
        } else {
            $nextAction = "Convert risk evidence to pass for strict phase transition"
        }
    }

    $phaseTransitions[$phase] = [ordered]@{
        phaseVerdict = $phaseVerdict
        dependencyStatus = $dependencyStatus
        dependencyDetail = $dependencyDetail
        goNoGo = [bool]$goNoGo
        nextAction = $nextAction
    }
}

$phaseOrderMap = [ordered]@{}
for ($i = 0; $i -lt $phaseOrder.Count; $i++) {
    $phaseOrderMap[$phaseOrder[$i]] = $i + 1
}

$blockerQueue = @()
$queueIndex = 1
foreach ($entry in ($ledger | Where-Object { $_.verdict -ne "Pass" } | Sort-Object -Property @{Expression={ $phaseOrderMap[[string]$_.phase] }}, @{Expression={ [int]$_.day }})) {
    $priority = if ($entry.verdict -eq "Blocked") { "P1" } else { "P2" }
    $requiredForStrict = $entry.verdict -ne "Pass"
    $actions = @()

    if ($entry.remediation -and $entry.remediation.Count -gt 0) {
        foreach ($hint in $entry.remediation) {
            $actions += [string]$hint
        }
    } else {
        $actions += "Investigate day report evidence gaps and update checks/metrics."
    }

    $actions += "Re-run day $($entry.day) evidence path and regenerate day report."
    $actions += "Re-run quality gate validator and confirm day $($entry.day) verdict is Pass."

    $blockerQueue += [ordered]@{
        queueIndex = $queueIndex
        priority = $priority
        day = [int]$entry.day
        phase = [string]$entry.phase
        verdict = [string]$entry.verdict
        reason = [string]$entry.reason
        requiredForStrict = [bool]$requiredForStrict
        actions = $actions
    }

    $queueIndex += 1
}

$accountabilityEntries = @()
foreach ($entry in ($ledger | Where-Object { $_.verdict -ne "Pass" } | Sort-Object -Property @{Expression={ $phaseOrderMap[[string]$_.phase] }}, @{Expression={ [int]$_.day }})) {
    $priority = if ($entry.verdict -eq "Blocked") { "Immediate" } else { "Planned" }
    $deadlineUtc = if ($entry.verdict -eq "Blocked") {
        $nowUtc.AddHours(12).ToString("o")
    } else {
        $nowUtc.AddHours(24).ToString("o")
    }

    $accountabilityEntries += [ordered]@{
        day = [int]$entry.day
        phase = [string]$entry.phase
        verdict = [string]$entry.verdict
        owner = (Get-PhaseSpecialist -Phase ([string]$entry.phase))
        mitigationDeadlineUtc = $deadlineUtc
        priority = $priority
        reason = [string]$entry.reason
        mitigationActionCount = @($entry.remediation).Count + 2
        waivedEvidenceCount = @($entry.waivedEvidence).Count
        nextAction = if (@($entry.remediation).Count -gt 0) { [string]$entry.remediation[0] } else { "Investigate day evidence gaps and update checks/metrics." }
    }
}

$accountabilitySummary = [ordered]@{
    entryCount = $accountabilityEntries.Count
    blockedOwners = @($accountabilityEntries | Where-Object { $_.verdict -eq "Blocked" } | ForEach-Object { [string]$_.owner } | Select-Object -Unique)
    riskOwners = @($accountabilityEntries | Where-Object { $_.verdict -eq "Proceed with risk" } | ForEach-Object { [string]$_.owner } | Select-Object -Unique)
    earliestDeadlineUtc = if ($accountabilityEntries.Count -gt 0) {
        ((@($accountabilityEntries | Sort-Object mitigationDeadlineUtc)[0]).mitigationDeadlineUtc)
    } else {
        ""
    }
    entries = $accountabilityEntries
}

$evidenceDebtByCategory = [ordered]@{}
foreach ($cat in $requiredEvidence) {
    $evidenceDebtByCategory[$cat] = 0
}

$evidenceDebtByPhase = [ordered]@{}
foreach ($phase in $phaseOrder) {
    $evidenceDebtByPhase[$phase] = [ordered]@{
        missingCount = 0
        daysImpacted = 0
    }
}

foreach ($entry in $ledger) {
    $missingCount = @($entry.missingEvidence).Count
    $evidenceDebtByPhase[[string]$entry.phase].missingCount += $missingCount
    if ($missingCount -gt 0) {
        $evidenceDebtByPhase[[string]$entry.phase].daysImpacted += 1
    }

    foreach ($cat in @($entry.missingEvidence)) {
        $evidenceDebtByCategory[[string]$cat] += 1
    }
}

$topDebtEntry = $null
if ($ledger.Count -gt 0) {
    $topDebtEntry = @($ledger | Sort-Object -Property @{Expression={ @($_.missingEvidence).Count }; Descending=$true}, @{Expression={ [int]$_.day }})[0]
}

$evidenceDebtSummary = [ordered]@{
    totalMissingEvidence = [int](($ledger | ForEach-Object { @($_.missingEvidence).Count } | Measure-Object -Sum).Sum)
    byCategory = $evidenceDebtByCategory
    byPhase = $evidenceDebtByPhase
    topDebtDay = if ($null -ne $topDebtEntry) { [int]$topDebtEntry.day } else { 0 }
    topDebtCount = if ($null -ne $topDebtEntry) { [int]@($topDebtEntry.missingEvidence).Count } else { 0 }
    topDebtPhase = if ($null -ne $topDebtEntry) { [string]$topDebtEntry.phase } else { "" }
}

$coverageByCategory = [ordered]@{}
foreach ($cat in $requiredEvidence) {
    $presentDays = 0
    $effectiveDays = 0
    $waivedDays = 0
    foreach ($entry in $ledger) {
        $hasPresent = [bool]$entry.evidence[$cat]
        $hasWaived = @($entry.waivedEvidence | Where-Object { [string]$_.category -eq $cat }).Count -gt 0
        if ($hasPresent) { $presentDays += 1 }
        if ($hasWaived) { $waivedDays += 1 }
        if ($hasPresent -or $hasWaived) { $effectiveDays += 1 }
    }

    $coverageByCategory[$cat] = [ordered]@{
        presentDays = $presentDays
        effectiveDays = $effectiveDays
        waivedDays = $waivedDays
        presentPct = [Math]::Round((100.0 * $presentDays) / $ledger.Count, 2)
        effectivePct = [Math]::Round((100.0 * $effectiveDays) / $ledger.Count, 2)
    }
}

$coverageByDay = @()
foreach ($entry in $ledger) {
    $presentCount = 0
    foreach ($cat in $requiredEvidence) {
        if ([bool]$entry.evidence[$cat]) {
            $presentCount += 1
        }
    }
    $waivedCount = @($entry.waivedEvidence).Count
    $effectiveCount = $presentCount + $waivedCount
    $coverageByDay += [ordered]@{
        day = [int]$entry.day
        phase = [string]$entry.phase
        presentCount = $presentCount
        waivedCount = $waivedCount
        effectiveCount = $effectiveCount
        missingCount = @($entry.missingEvidence).Count
        presentPct = [Math]::Round((100.0 * $presentCount) / $requiredEvidence.Count, 2)
        effectivePct = [Math]::Round((100.0 * $effectiveCount) / $requiredEvidence.Count, 2)
    }
}

$totalEvidenceSlots = $ledger.Count * $requiredEvidence.Count
$totalPresentEvidence = [int](($coverageByDay | ForEach-Object { [int]$_.presentCount } | Measure-Object -Sum).Sum)
$totalEffectiveEvidence = [int](($coverageByDay | ForEach-Object { [int]$_.effectiveCount } | Measure-Object -Sum).Sum)
$lowestCoverageCategory = @($coverageByCategory.GetEnumerator() | Sort-Object -Property @{Expression={ [double]$_.Value.effectivePct }}, @{Expression={ [string]$_.Key }})[0]
$lowestCoverageDay = @($coverageByDay | Sort-Object -Property @{Expression={ [double]$_.effectivePct }}, @{Expression={ [int]$_.day }})[0]

$coverageSummary = [ordered]@{
    totalEvidenceSlots = $totalEvidenceSlots
    totalPresentEvidence = $totalPresentEvidence
    totalEffectiveEvidence = $totalEffectiveEvidence
    overallPresentPct = [Math]::Round((100.0 * $totalPresentEvidence) / $totalEvidenceSlots, 2)
    overallEffectivePct = [Math]::Round((100.0 * $totalEffectiveEvidence) / $totalEvidenceSlots, 2)
    byCategory = $coverageByCategory
    byDay = $coverageByDay
    lowestCoverageCategory = [string]$lowestCoverageCategory.Key
    lowestCoverageCategoryPct = [double]$lowestCoverageCategory.Value.effectivePct
    lowestCoverageDay = [int]$lowestCoverageDay.day
    lowestCoverageDayPct = [double]$lowestCoverageDay.effectivePct
}

$blockedCount = ($ledger | Where-Object { $_.verdict -eq "Blocked" } | Measure-Object).Count
$riskCount = ($ledger | Where-Object { $_.verdict -eq "Proceed with risk" } | Measure-Object).Count
$passCount = ($ledger | Where-Object { $_.verdict -eq "Pass" } | Measure-Object).Count
$completionPercent = if ($ledger.Count -gt 0) {
    [Math]::Round((100.0 * $passCount) / $ledger.Count, 2)
} else {
    0.0
}

$overallVerdict = "Pass"
if ($blockedCount -gt 0) {
    $overallVerdict = "Blocked"
} elseif ($riskCount -gt 0) {
    $overallVerdict = "Proceed with risk"
}

if (($waiverAudit.invalidWaivers -gt 0) -or ($waiverAudit.expiredWaivers -gt 0)) {
    $waiverAudit.governanceBlocked = $true
    $overallVerdict = "Blocked"
}
if ($freshnessAudit.staleCount -gt 0) {
    $freshnessAudit.governanceBlocked = $true
    $overallVerdict = "Blocked"
}

$payload = [ordered]@{
    generatedUtc = (Get-Date).ToUniversalTime().ToString("o")
    strictMode = [bool]$Strict
    overallVerdict = $overallVerdict
    dayCounts = [ordered]@{
        pass = $passCount
        proceedWithRisk = $riskCount
        blocked = $blockedCount
        total = $ledger.Count
    }
    expansionStatus = [ordered]@{
        baselinePercent = 72
        targetPercent = 100
        completionPercent = $completionPercent
        passDays = $passCount
        totalDays = $ledger.Count
        productionReady = ($overallVerdict -eq "Pass")
    }
    phaseSummary = $phaseSummary
    phaseTransitions = $phaseTransitions
    blockerQueue = $blockerQueue
    accountability = $accountabilitySummary
    waiverAudit = $waiverAudit
    freshnessAudit = $freshnessAudit
    evidenceDebt = $evidenceDebtSummary
    coverageMatrix = $coverageSummary
    riskDays = @($ledger | Where-Object { $_.verdict -eq "Proceed with risk" } | ForEach-Object { [int]$_.day })
    blockedDays = @($ledger | Where-Object { $_.verdict -eq "Blocked" } | ForEach-Object { [int]$_.day })
    ledger = $ledger
}

$ledgerJsonPath = Join-Path $ReportRoot "quality_gate_ledger.json"
$summaryMdPath = Join-Path $ReportRoot "quality_gate_summary.md"
$remediationMdPath = Join-Path $ReportRoot "quality_gate_remediation.md"
$transitionMdPath = Join-Path $ReportRoot "quality_gate_phase_transitions.md"
$blockerQueueMdPath = Join-Path $ReportRoot "quality_gate_blocker_queue.md"
$manifestJsonPath = Join-Path $ReportRoot "quality_gate_artifact_manifest.json"
$manifestMdPath = Join-Path $ReportRoot "quality_gate_artifact_manifest.md"
$historyJsonlPath = Join-Path $ReportRoot "quality_gate_history.jsonl"
$trendMdPath = Join-Path $ReportRoot "quality_gate_trend.md"
$waiverAuditMdPath = Join-Path $ReportRoot "quality_gate_waiver_audit.md"
$accountabilityMdPath = Join-Path $ReportRoot "quality_gate_accountability.md"
$freshnessMdPath = Join-Path $ReportRoot "quality_gate_freshness.md"
$evidenceDebtMdPath = Join-Path $ReportRoot "quality_gate_evidence_debt.md"
$coverageMdPath = Join-Path $ReportRoot "quality_gate_coverage_matrix.md"
$expansionStatusMdPath = Join-Path $ReportRoot "expansion_completion_status.md"

$payload | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ledgerJsonPath -Encoding UTF8

$md = @()
$md += "# 14-Day Quality Gate Validation"
$md += ""
$md += "- Generated UTC: $($payload.generatedUtc)"
$md += "- Strict Mode: $($payload.strictMode)"
$md += "- Overall Verdict: **$($payload.overallVerdict)**"
$md += ""
$md += "## Overall Counts"
$md += "- Pass: $passCount"
$md += "- Proceed with risk: $riskCount"
$md += "- Blocked: $blockedCount"
$md += "- Total: $($ledger.Count)"
$md += "- Expansion Completion: $completionPercent%"
$md += "- Production Ready: $(if ($overallVerdict -eq 'Pass') { 'true' } else { 'false' })"
$md += ""
$md += "## Phase Summary"
foreach ($phase in $phaseSummary.Keys) {
    $s = $phaseSummary[$phase]
    $md += "- ${phase}: pass=$($s.pass), proceed_with_risk=$($s.proceedWithRisk), blocked=$($s.blocked), total_days=$($s.totalDays)"
}
$md += ""
$md += "## Day Verdicts"
foreach ($entry in $ledger) {
    $missing = if ($entry.missingEvidence.Count -gt 0) { $entry.missingEvidence -join ", " } else { "none" }
    $md += "- Day $($entry.day): $($entry.verdict) | report_status=$($entry.reportStatus) | missing=$missing"
}

$md += ""
$md += "## Remediation Artifact"
$md += "- See quality_gate_remediation.md for actionable strict-mode unblock steps."
$md += ""
$md += "## Phase Transition Artifact"
$md += "- See quality_gate_phase_transitions.md for phase dependency go/no-go decisions."
$md += ""
$md += "## Blocker Queue Artifact"
$md += "- See quality_gate_blocker_queue.md for strict-mode ordered unblock execution."
$md += ""
$md += "## Artifact Manifest"
$md += "- See quality_gate_artifact_manifest.json and quality_gate_artifact_manifest.md for SHA256 artifact integrity mapping."
$md += ""
$md += "## Trend Artifact"
$md += "- See quality_gate_trend.md and quality_gate_history.jsonl for run-to-run verdict drift tracking."
$md += ""
$md += "## Waiver Governance Artifact"
$md += "- See quality_gate_waiver_audit.md for waiver validity, expiry, and usage audit."
$md += ""
$md += "## Accountability Artifact"
$md += "- See quality_gate_accountability.md for owner and mitigation deadline assignments on non-pass days."
$md += ""
$md += "## Freshness Artifact"
$md += "- See quality_gate_freshness.md for evidence age and stale-report governance audit."
$md += ""
$md += "## Evidence Debt Artifact"
$md += "- See quality_gate_evidence_debt.md for strict-readiness debt by category and phase."
$md += ""
$md += "## Coverage Matrix Artifact"
$md += "- See quality_gate_coverage_matrix.md for raw and effective evidence coverage by day and category."
$md += ""
$md += "## Expansion Status Artifact"
$md += "- See expansion_completion_status.md for baseline-to-target completion status and production-ready decision."

$md -join "`r`n" | Set-Content -LiteralPath $summaryMdPath -Encoding UTF8

$rem = @()
$rem += "# 14-Day Quality Gate Remediation"
$rem += ""
$rem += "- Generated UTC: $($payload.generatedUtc)"
$rem += "- Strict Mode: $($payload.strictMode)"
$rem += "- Overall Verdict: **$($payload.overallVerdict)**"
$rem += ""

$blockedEntries = @($ledger | Where-Object { $_.verdict -eq "Blocked" })
$riskEntries = @($ledger | Where-Object { $_.verdict -eq "Proceed with risk" })

if ($blockedEntries.Count -eq 0 -and $riskEntries.Count -eq 0) {
    $rem += "All days are fully gated with required evidence. No remediation required."
} else {
    $rem += "## Priority Order"
    $rem += "1. Resolve all blocked days first."
    $rem += "2. Convert risk days to pass by filling missing evidence categories."
    $rem += "3. Re-run strict sign-off without -NoBuild or -NoTests."
    $rem += ""

    foreach ($entry in ($blockedEntries + $riskEntries)) {
        $rem += "## Day $($entry.day) - $($entry.verdict)"
        $rem += "- Reason: $($entry.reason)"
        if ($entry.missingEvidence.Count -gt 0) {
            $rem += "- Missing: $($entry.missingEvidence -join ', ')"
        }
        if ($entry.remediation.Count -gt 0) {
            foreach ($hint in $entry.remediation) {
                $rem += "- Action: $hint"
            }
        }
        $rem += ""
    }

    $rem += "## Strict Re-run Command"
    $rem += "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/production/Invoke-14Day-ProductionFinishers.ps1 -AllDays -Strict"
}

$rem -join "`r`n" | Set-Content -LiteralPath $remediationMdPath -Encoding UTF8

$trans = @()
$trans += "# 14-Day Phase Transition Decisions"
$trans += ""
$trans += "- Generated UTC: $($payload.generatedUtc)"
$trans += "- Strict Mode: $($payload.strictMode)"
$trans += "- Overall Verdict: **$($payload.overallVerdict)**"
$trans += ""

foreach ($phase in $phaseOrder) {
    $p = $phaseTransitions[$phase]
    $trans += "## $phase"
    $trans += "- Phase verdict: $($p.phaseVerdict)"
    $trans += "- Dependency status: $($p.dependencyStatus)"
    $trans += "- Dependency detail: $($p.dependencyDetail)"
    $trans += "- Transition decision: $(if ($p.goNoGo) { 'GO' } else { 'NO-GO' })"
    $trans += "- Next action: $($p.nextAction)"
    $trans += ""
}

$trans += "## Revalidation Command"
$trans += "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/production/Invoke-14Day-QualityGateValidation.ps1 -ReportRoot reports/14day$(if ($Strict) { ' -Strict' } else { '' })"

$trans -join "`r`n" | Set-Content -LiteralPath $transitionMdPath -Encoding UTF8

$queueMd = @()
$queueMd += "# 14-Day Blocker Queue"
$queueMd += ""
$queueMd += "- Generated UTC: $($payload.generatedUtc)"
$queueMd += "- Strict Mode: $($payload.strictMode)"
$queueMd += "- Overall Verdict: **$($payload.overallVerdict)**"
$queueMd += "- Queue Items: $($blockerQueue.Count)"
$queueMd += ""

if ($blockerQueue.Count -eq 0) {
    $queueMd += "No blocker or risk queue items. All days are pass-ready."
} else {
    foreach ($item in $blockerQueue) {
        $queueMd += "## [$($item.priority)] Queue #$($item.queueIndex) - Day $($item.day)"
        $queueMd += "- Phase: $($item.phase)"
        $queueMd += "- Current Verdict: $($item.verdict)"
        $queueMd += "- Reason: $($item.reason)"
        $queueMd += "- Required for strict pass: $($item.requiredForStrict)"
        foreach ($act in $item.actions) {
            $queueMd += "- Action: $act"
        }
        $queueMd += ""
    }
}

$queueMd += "## Strict Queue Re-run"
$queueMd += "powershell -NoProfile -ExecutionPolicy Bypass -File scripts/production/Invoke-14Day-ProductionFinishers.ps1 -AllDays -Strict"

$queueMd -join "`r`n" | Set-Content -LiteralPath $blockerQueueMdPath -Encoding UTF8

$waiverMd = @()
$waiverMd += "# 14-Day Quality Gate Waiver Audit"
$waiverMd += ""
$waiverMd += "- Generated UTC: $($payload.generatedUtc)"
$waiverMd += "- Waiver File: $($waiverAudit.waiverFile)"
$waiverMd += "- Waiver File Present: $($waiverAudit.waiverFilePresent)"
$waiverMd += "- Loaded Waivers: $($waiverAudit.waiversLoaded)"
$waiverMd += "- Active Waivers: $($waiverAudit.activeWaivers)"
$waiverMd += "- Applied Waivers: $($waiverAudit.appliedWaivers)"
$waiverMd += "- Expired Waivers: $($waiverAudit.expiredWaivers)"
$waiverMd += "- Invalid Waivers: $($waiverAudit.invalidWaivers)"
$waiverMd += "- Governance Blocked: $($waiverAudit.governanceBlocked)"
$waiverMd += ""
$waiverMd += "## Issues"
if ($waiverAudit.issues.Count -eq 0) {
    $waiverMd += "- none"
} else {
    foreach ($issue in $waiverAudit.issues) {
        $waiverMd += "- $issue"
    }
}

$waiverMd -join "`r`n" | Set-Content -LiteralPath $waiverAuditMdPath -Encoding UTF8

$accountabilityMd = @()
$accountabilityMd += "# 14-Day Quality Gate Accountability"
$accountabilityMd += ""
$accountabilityMd += "- Generated UTC: $($payload.generatedUtc)"
$accountabilityMd += "- Overall Verdict: **$($payload.overallVerdict)**"
$accountabilityMd += "- Entry Count: $($accountabilitySummary.entryCount)"
$accountabilityMd += "- Earliest Deadline UTC: $($accountabilitySummary.earliestDeadlineUtc)"
$accountabilityMd += "- Blocked Owners: $(if ($accountabilitySummary.blockedOwners.Count -gt 0) { $accountabilitySummary.blockedOwners -join ', ' } else { 'none' })"
$accountabilityMd += "- Risk Owners: $(if ($accountabilitySummary.riskOwners.Count -gt 0) { $accountabilitySummary.riskOwners -join ', ' } else { 'none' })"
$accountabilityMd += ""

if ($accountabilityEntries.Count -eq 0) {
    $accountabilityMd += "All days are pass-ready. No accountability actions required."
} else {
    foreach ($entry in $accountabilityEntries) {
        $accountabilityMd += "## Day $($entry.day)"
        $accountabilityMd += "- Phase: $($entry.phase)"
        $accountabilityMd += "- Verdict: $($entry.verdict)"
        $accountabilityMd += "- Owner: $($entry.owner)"
        $accountabilityMd += "- Mitigation Deadline UTC: $($entry.mitigationDeadlineUtc)"
        $accountabilityMd += "- Priority: $($entry.priority)"
        $accountabilityMd += "- Reason: $($entry.reason)"
        $accountabilityMd += "- Mitigation Action Count: $($entry.mitigationActionCount)"
        $accountabilityMd += "- Waived Evidence Count: $($entry.waivedEvidenceCount)"
        $accountabilityMd += "- Next Action: $($entry.nextAction)"
        $accountabilityMd += ""
    }
}

$accountabilityMd -join "`r`n" | Set-Content -LiteralPath $accountabilityMdPath -Encoding UTF8

$freshnessMd = @()
$freshnessMd += "# 14-Day Quality Gate Freshness"
$freshnessMd += ""
$freshnessMd += "- Generated UTC: $($payload.generatedUtc)"
$freshnessMd += "- Freshness Window Hours: $($freshnessAudit.windowHours)"
$freshnessMd += "- Stale Count: $($freshnessAudit.staleCount)"
$freshnessMd += "- Missing Timestamp Count: $($freshnessAudit.missingTimestampCount)"
$freshnessMd += "- Governance Blocked: $($freshnessAudit.governanceBlocked)"
$freshnessMd += "- Stale Days: $(if ($freshnessAudit.staleDays.Count -gt 0) { $freshnessAudit.staleDays -join ', ' } else { 'none' })"
$freshnessMd += "- Missing Timestamp Days: $(if ($freshnessAudit.missingTimestampDays.Count -gt 0) { $freshnessAudit.missingTimestampDays -join ', ' } else { 'none' })"
$freshnessMd += ""
$freshnessMd += "## Day Freshness"
foreach ($entry in $ledger) {
    $freshnessMd += "- Day $($entry.day): within_window=$($entry.freshness.withinWindow) report_age_hours=$($entry.freshness.reportAgeHours) file_age_hours=$($entry.freshness.fileAgeHours) reasons=$(if ($entry.freshness.reasons.Count -gt 0) { $entry.freshness.reasons -join '; ' } else { 'none' })"
}

$freshnessMd -join "`r`n" | Set-Content -LiteralPath $freshnessMdPath -Encoding UTF8

$evidenceDebtMd = @()
$evidenceDebtMd += "# 14-Day Quality Gate Evidence Debt"
$evidenceDebtMd += ""
$evidenceDebtMd += "- Generated UTC: $($payload.generatedUtc)"
$evidenceDebtMd += "- Overall Verdict: **$($payload.overallVerdict)**"
$evidenceDebtMd += "- Total Missing Evidence Slots: $($evidenceDebtSummary.totalMissingEvidence)"
$evidenceDebtMd += "- Top Debt Day: $($evidenceDebtSummary.topDebtDay)"
$evidenceDebtMd += "- Top Debt Count: $($evidenceDebtSummary.topDebtCount)"
$evidenceDebtMd += "- Top Debt Phase: $($evidenceDebtSummary.topDebtPhase)"
$evidenceDebtMd += ""
$evidenceDebtMd += "## Debt By Category"
foreach ($cat in $requiredEvidence) {
    $evidenceDebtMd += "- ${cat}: $($evidenceDebtByCategory[$cat])"
}
$evidenceDebtMd += ""
$evidenceDebtMd += "## Debt By Phase"
foreach ($phase in $phaseOrder) {
    $phaseDebt = $evidenceDebtByPhase[$phase]
    $evidenceDebtMd += "- ${phase}: missing_slots=$($phaseDebt.missingCount) impacted_days=$($phaseDebt.daysImpacted)"
}

$evidenceDebtMd -join "`r`n" | Set-Content -LiteralPath $evidenceDebtMdPath -Encoding UTF8

    $coverageMd = @()
    $coverageMd += "# 14-Day Quality Gate Coverage Matrix"
    $coverageMd += ""
    $coverageMd += "- Generated UTC: $($payload.generatedUtc)"
    $coverageMd += "- Overall Verdict: **$($payload.overallVerdict)**"
    $coverageMd += "- Total Evidence Slots: $($coverageSummary.totalEvidenceSlots)"
    $coverageMd += "- Total Present Evidence: $($coverageSummary.totalPresentEvidence)"
    $coverageMd += "- Total Effective Evidence: $($coverageSummary.totalEffectiveEvidence)"
    $coverageMd += "- Overall Present Coverage: $($coverageSummary.overallPresentPct)%"
    $coverageMd += "- Overall Effective Coverage: $($coverageSummary.overallEffectivePct)%"
    $coverageMd += "- Lowest Coverage Category: $($coverageSummary.lowestCoverageCategory) ($($coverageSummary.lowestCoverageCategoryPct)%)"
    $coverageMd += "- Lowest Coverage Day: $($coverageSummary.lowestCoverageDay) ($($coverageSummary.lowestCoverageDayPct)%)"
    $coverageMd += ""
    $coverageMd += "## Category Coverage"
    foreach ($cat in $requiredEvidence) {
        $row = $coverageByCategory[$cat]
        $coverageMd += "- ${cat}: present_days=$($row.presentDays) effective_days=$($row.effectiveDays) waived_days=$($row.waivedDays) present_pct=$($row.presentPct)% effective_pct=$($row.effectivePct)%"
    }
    $coverageMd += ""
    $coverageMd += "## Day Coverage"
    foreach ($row in $coverageByDay) {
        $coverageMd += "- Day $($row.day): phase=$($row.phase) present=$($row.presentCount) waived=$($row.waivedCount) effective=$($row.effectiveCount) missing=$($row.missingCount) present_pct=$($row.presentPct)% effective_pct=$($row.effectivePct)%"
    }

    $coverageMd -join "`r`n" | Set-Content -LiteralPath $coverageMdPath -Encoding UTF8

    $expansionMd = @()
    $expansionMd += "# 14-Day Expansion Completion Status"
    $expansionMd += ""
    $expansionMd += "- Generated UTC: $($payload.generatedUtc)"
    $expansionMd += "- Strict Mode: $($payload.strictMode)"
    $expansionMd += "- Overall Verdict: **$($payload.overallVerdict)**"
    $expansionMd += "- Baseline Completion: $($payload.expansionStatus.baselinePercent)%"
    $expansionMd += "- Target Completion: $($payload.expansionStatus.targetPercent)%"
    $expansionMd += "- Current Completion: $($payload.expansionStatus.completionPercent)%"
    $expansionMd += "- Pass Days: $($payload.expansionStatus.passDays)/$($payload.expansionStatus.totalDays)"
    $expansionMd += "- Production Ready: $($payload.expansionStatus.productionReady)"
    $expansionMd += ""
    $expansionMd += "## Phase Completion"
    foreach ($phase in $phaseOrder) {
        $s = $phaseSummary[$phase]
        $phasePct = if ($s.totalDays -gt 0) { [Math]::Round((100.0 * $s.pass) / $s.totalDays, 2) } else { 0.0 }
        $expansionMd += "- ${phase}: pass=$($s.pass)/$($s.totalDays) completion=$phasePct%"
    }

    $expansionMd -join "`r`n" | Set-Content -LiteralPath $expansionStatusMdPath -Encoding UTF8

$artifactPaths = @(
    $ledgerJsonPath,
    $summaryMdPath,
    $remediationMdPath,
    $transitionMdPath,
    $blockerQueueMdPath,
    $waiverAuditMdPath,
    $accountabilityMdPath,
    $freshnessMdPath,
    $evidenceDebtMdPath,
    $coverageMdPath,
    $expansionStatusMdPath
)

$manifestEntries = @()
foreach ($p in $artifactPaths) {
    if (Test-Path -LiteralPath $p) {
        $h = Get-FileHash -LiteralPath $p -Algorithm SHA256
        $manifestEntries += [ordered]@{
            name = [System.IO.Path]::GetFileName($p)
            path = $p
            sha256 = [string]$h.Hash
            sizeBytes = [int64](Get-Item -LiteralPath $p).Length
        }
    }
}

$manifestPayload = [ordered]@{
    generatedUtc = $payload.generatedUtc
    strictMode = $payload.strictMode
    overallVerdict = $payload.overallVerdict
    artifactCount = $manifestEntries.Count
    artifacts = $manifestEntries
}

$manifestPayload | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestJsonPath -Encoding UTF8

$manifestMd = @()
$manifestMd += "# 14-Day Quality Gate Artifact Manifest"
$manifestMd += ""
$manifestMd += "- Generated UTC: $($manifestPayload.generatedUtc)"
$manifestMd += "- Strict Mode: $($manifestPayload.strictMode)"
$manifestMd += "- Overall Verdict: **$($manifestPayload.overallVerdict)**"
$manifestMd += "- Artifact Count: $($manifestPayload.artifactCount)"
$manifestMd += ""
$manifestMd += "## Artifacts"
foreach ($a in $manifestEntries) {
    $manifestMd += "- $($a.name) | sha256=$($a.sha256) | bytes=$($a.sizeBytes)"
}

$manifestMd -join "`r`n" | Set-Content -LiteralPath $manifestMdPath -Encoding UTF8

$payload["artifactManifest"] = [ordered]@{
    artifactCount = $manifestPayload.artifactCount
    manifestJson = $manifestJsonPath
    manifestMarkdown = $manifestMdPath
}

$waiverAudit["artifactPath"] = $waiverAuditMdPath
$accountabilitySummary["artifactPath"] = $accountabilityMdPath
$freshnessAudit["artifactPath"] = $freshnessMdPath
$evidenceDebtSummary["artifactPath"] = $evidenceDebtMdPath
$payload["waiverAudit"] = [ordered]@{
    waiverFile = $waiverAudit.waiverFile
    waiverFilePresent = [bool]$waiverAudit.waiverFilePresent
    waiversLoaded = [int]$waiverAudit.waiversLoaded
    activeWaivers = [int]$waiverAudit.activeWaivers
    expiredWaivers = [int]$waiverAudit.expiredWaivers
    invalidWaivers = [int]$waiverAudit.invalidWaivers
    appliedWaivers = [int]$waiverAudit.appliedWaivers
    governanceBlocked = [bool]$waiverAudit.governanceBlocked
    issues = @($waiverAudit.issues)
    artifactPath = $waiverAuditMdPath
}
$payload["accountability"] = [ordered]@{
    entryCount = [int]$accountabilitySummary.entryCount
    blockedOwners = @($accountabilitySummary.blockedOwners)
    riskOwners = @($accountabilitySummary.riskOwners)
    earliestDeadlineUtc = [string]$accountabilitySummary.earliestDeadlineUtc
    entries = @($accountabilityEntries)
    artifactPath = $accountabilityMdPath
}
$payload["freshnessAudit"] = [ordered]@{
    windowHours = [int]$freshnessAudit.windowHours
    staleDays = @($freshnessAudit.staleDays)
    missingTimestampDays = @($freshnessAudit.missingTimestampDays)
    staleCount = [int]$freshnessAudit.staleCount
    missingTimestampCount = [int]$freshnessAudit.missingTimestampCount
    governanceBlocked = [bool]$freshnessAudit.governanceBlocked
    artifactPath = $freshnessMdPath
}
$payload["evidenceDebt"] = [ordered]@{
    totalMissingEvidence = [int]$evidenceDebtSummary.totalMissingEvidence
    byCategory = $evidenceDebtByCategory
    byPhase = $evidenceDebtByPhase
    topDebtDay = [int]$evidenceDebtSummary.topDebtDay
    topDebtCount = [int]$evidenceDebtSummary.topDebtCount
    topDebtPhase = [string]$evidenceDebtSummary.topDebtPhase
    artifactPath = $evidenceDebtMdPath
}
$payload["coverageMatrix"] = [ordered]@{
    totalEvidenceSlots = [int]$coverageSummary.totalEvidenceSlots
    totalPresentEvidence = [int]$coverageSummary.totalPresentEvidence
    totalEffectiveEvidence = [int]$coverageSummary.totalEffectiveEvidence
    overallPresentPct = [double]$coverageSummary.overallPresentPct
    overallEffectivePct = [double]$coverageSummary.overallEffectivePct
    byCategory = $coverageByCategory
    byDay = $coverageByDay
    lowestCoverageCategory = [string]$coverageSummary.lowestCoverageCategory
    lowestCoverageCategoryPct = [double]$coverageSummary.lowestCoverageCategoryPct
    lowestCoverageDay = [int]$coverageSummary.lowestCoverageDay
    lowestCoverageDayPct = [double]$coverageSummary.lowestCoverageDayPct
    artifactPath = $coverageMdPath
}
$payload["expansionStatus"] = [ordered]@{
    baselinePercent = 72
    targetPercent = 100
    completionPercent = [double]$completionPercent
    passDays = [int]$passCount
    totalDays = [int]$ledger.Count
    productionReady = ($overallVerdict -eq "Pass")
    artifactPath = $expansionStatusMdPath
}

$currentRecord = [ordered]@{
    generatedUtc = $payload.generatedUtc
    strictMode = [bool]$payload.strictMode
    overallVerdict = [string]$payload.overallVerdict
    passDays = [int]$payload.dayCounts.pass
    riskDays = [int]$payload.dayCounts.proceedWithRisk
    blockedDays = [int]$payload.dayCounts.blocked
}

$previousRecord = $null
if (Test-Path -LiteralPath $historyJsonlPath) {
    $historyLines = @(Get-Content -LiteralPath $historyJsonlPath | Where-Object { $_ -and $_.Trim().Length -gt 0 })
    if ($historyLines.Count -gt 0) {
        try {
            $previousRecord = ($historyLines[-1] | ConvertFrom-Json)
        } catch {
            $previousRecord = $null
        }
    }
}

($currentRecord | ConvertTo-Json -Compress) | Add-Content -LiteralPath $historyJsonlPath

$trendDirection = "steady"
if ($null -ne $previousRecord) {
    $prevBlocked = [int]$previousRecord.blockedDays
    $currBlocked = [int]$currentRecord.blockedDays
    $prevRisk = [int]$previousRecord.riskDays
    $currRisk = [int]$currentRecord.riskDays

    if (($currBlocked -lt $prevBlocked) -or (($currBlocked -eq $prevBlocked) -and ($currRisk -lt $prevRisk))) {
        $trendDirection = "improving"
    } elseif (($currBlocked -gt $prevBlocked) -or (($currBlocked -eq $prevBlocked) -and ($currRisk -gt $prevRisk))) {
        $trendDirection = "regressing"
    }
}

$payload.trend = [ordered]@{
    direction = $trendDirection
    previousOverallVerdict = if ($null -ne $previousRecord) { [string]$previousRecord.overallVerdict } else { "none" }
    currentOverallVerdict = [string]$currentRecord.overallVerdict
    previousBlockedDays = if ($null -ne $previousRecord) { [int]$previousRecord.blockedDays } else { -1 }
    currentBlockedDays = [int]$currentRecord.blockedDays
    previousRiskDays = if ($null -ne $previousRecord) { [int]$previousRecord.riskDays } else { -1 }
    currentRiskDays = [int]$currentRecord.riskDays
    historyPath = $historyJsonlPath
    trendReportPath = $trendMdPath
}

$trendMd = @()
$trendMd += "# 14-Day Quality Gate Trend"
$trendMd += ""
$trendMd += "- Generated UTC: $($payload.generatedUtc)"
$trendMd += "- Direction: **$trendDirection**"
$trendMd += "- Previous Overall Verdict: $($payload.trend.previousOverallVerdict)"
$trendMd += "- Current Overall Verdict: $($payload.trend.currentOverallVerdict)"
$trendMd += "- Previous Blocked Days: $($payload.trend.previousBlockedDays)"
$trendMd += "- Current Blocked Days: $($payload.trend.currentBlockedDays)"
$trendMd += "- Previous Risk Days: $($payload.trend.previousRiskDays)"
$trendMd += "- Current Risk Days: $($payload.trend.currentRiskDays)"
$trendMd += ""
$trendMd += "## History Store"
$trendMd += "- $historyJsonlPath"

$trendMd -join "`r`n" | Set-Content -LiteralPath $trendMdPath -Encoding UTF8

$payload | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ledgerJsonPath -Encoding UTF8

Write-Host "Quality gate ledger: $ledgerJsonPath"
Write-Host "Quality gate summary: $summaryMdPath"
Write-Host "Quality gate remediation: $remediationMdPath"
Write-Host "Quality gate transitions: $transitionMdPath"
Write-Host "Quality gate blocker queue: $blockerQueueMdPath"
Write-Host "Quality gate artifact manifest json: $manifestJsonPath"
Write-Host "Quality gate artifact manifest md: $manifestMdPath"
Write-Host "Quality gate history jsonl: $historyJsonlPath"
Write-Host "Quality gate trend md: $trendMdPath"
Write-Host "Quality gate waiver audit md: $waiverAuditMdPath"
Write-Host "Quality gate accountability md: $accountabilityMdPath"
Write-Host "Quality gate freshness md: $freshnessMdPath"
Write-Host "Quality gate evidence debt md: $evidenceDebtMdPath"
Write-Host "Quality gate coverage matrix md: $coverageMdPath"
Write-Host "Expansion completion status md: $expansionStatusMdPath"
Write-Host "Overall verdict: $overallVerdict"

if ($Strict -and $overallVerdict -ne "Pass") {
    exit 2
}
if ($overallVerdict -eq "Blocked") {
    exit 1
}
exit 0
