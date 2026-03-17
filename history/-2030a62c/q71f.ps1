
# Production-ready feature: SelfHealingModule
function Invoke-SelfHealingModule {
    [CmdletBinding()]
    param(
        [string]$ModuleDir = "D:/lazy init ide/auto_generated_methods"
    )
    try {
        $errorCount = 0
        $moduleFiles = Get-ChildItem -Path $ModuleDir -Filter "*_AutoFeature.ps1"
        foreach ($file in $moduleFiles) {
            try {
                . $file.FullName
                $funcName = "Invoke-" + ($file.BaseName -replace '_AutoFeature$','')
                if (Get-Command $funcName -ErrorAction SilentlyContinue) {
                    & $funcName | Out-Null
                }
            } catch {
                $errorCount++
                Write-Host "[SelfHealingModule] Error in $($file.Name). Attempting reload..."
                Remove-Module ($file.BaseName -replace '_AutoFeature$','') -ErrorAction SilentlyContinue
                try {
                    . $file.FullName
                    Write-Host "[SelfHealingModule] Reloaded $($file.Name)"
                } catch {
                    Write-Error "[SelfHealingModule][ERROR] Failed to reload $($file.Name): $_"
                }
            }
        }
        Write-Host "[SelfHealingModule] Error count: $errorCount"
        return $errorCount
    } catch {
        Write-Error "[SelfHealingModule][ERROR] $_"
        return $null
    }
}

