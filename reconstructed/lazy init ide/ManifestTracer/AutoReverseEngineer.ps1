# ManifestTracer_AutoReverseEngineer.ps1
# Fully automatic reverse engineering and method generation from manifest tracer report

param(
    [string]$ReportPath = "D:/lazy init ide/orchestrator_smoke_output/manifest_tracer/ManifestTracer_Report_20260123.json",
    [string]$OutputDir = "D:/lazy init ide/auto_generated_methods"
)

if (-not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

function Invoke-SelfReverseEngineer {
    $report = Get-Content $ReportPath -Raw | ConvertFrom-Json
    foreach ($detail in $report.details) {
        $file = $detail.file
        $format = $detail.format
        $result = $detail.result
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($file)
        $methodFile = Join-Path $OutputDir ("${baseName}_AutoMethod.ps1")

        $method = @()
        $method += "# Auto-generated method for $baseName ($format)"
        $method += "function Invoke-${baseName}Auto {"
        $method += "    [CmdletBinding()]"
        $method += "    param()"
        $method += "    Write-Host 'Auto-executing $baseName ($format) -- result: $result'"
        $method += "    # TODO: Insert reverse engineered logic here"
        $method += "    # This is a stub generated from the manifest tracer report."
        $method += "    return '$result'"
        $method += "}"
        $method += ""
        $method -join "`n" | Set-Content $methodFile
    }
    Write-Host "[AutoReverseEngineer] Methods generated for $($report.details.Count) manifests in $OutputDir"
}

# Initial run
Invoke-SelfReverseEngineer

# Watch for new/updated manifest reports and re-run automatically
while ($true) {
    Start-Sleep -Seconds 5
    $currentHash = Get-FileHash $ReportPath -ErrorAction SilentlyContinue
    if ($script:lastHash -ne $currentHash.Hash) {
        $script:lastHash = $currentHash.Hash
        Invoke-SelfReverseEngineer
    }
}