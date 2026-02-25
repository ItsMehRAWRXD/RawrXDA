# RawrXD_AutomatedIntegrationPipeline.ps1
# Simulates an automated, production-ready integration pipeline

# Dynamically discover and source all feature scripts
$featureDir = "D:/lazy init ide/auto_generated_methods"
$featureScripts = Get-ChildItem -Path $featureDir -Filter "*_AutoFeature.ps1"

foreach ($script in $featureScripts) {
    try {
        . $script.FullName
        Write-Host "[Integration] Sourced: $($script.Name)" -ForegroundColor Green
    } catch {
        Write-Error "[Integration] Failed to source: $($script.Name). Error: $_"
    }
}

# Dynamically invoke all discovered features
$features = @("AutoDependencyGraph", "ManifestChangeNotifier", "DynamicTestHarness", "SourceCodeSummarizer", "AutoRefactorSuggestor", "SecurityVulnerabilityScanner", "LiveMetricsDashboard", "PluginAutoLoader", "SelfHealingModule", "ContinuousIntegrationTrigger")

foreach ($feature in $features) {
    $invokeCommand = "Invoke-$feature"
    if (Get-Command -Name $invokeCommand -ErrorAction SilentlyContinue) {
        try {
            & $invokeCommand
            Write-Host "[Integration] Successfully invoked: $feature" -ForegroundColor Green
        } catch {
            Write-Error "[Integration] Failed to invoke: $feature. Error: $_"
        }
    } else {
        Write-Warning "[Integration] Command not found: $invokeCommand"
    }
}

Write-Host "[Integration] Automated pipeline completed." -ForegroundColor Cyan
