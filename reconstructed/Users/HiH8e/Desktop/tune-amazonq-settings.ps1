$ErrorActionPreference = 'Stop'
$settingsPath = Join-Path $env:APPDATA 'Code\User\settings.json'
if (-not (Test-Path $settingsPath)) { Write-Error "settings.json not found at $settingsPath" }
$ts = Get-Date -Format 'yyyyMMdd-HHmmss'
$bak = Join-Path $env:USERPROFILE "Desktop\settings.json.tune-backup-$ts"
Copy-Item -Path $settingsPath -Destination $bak -Force
Write-Output "BACKUP_CREATED:$bak"

# Load current settings into a PS hashtable
$content = Get-Content -Raw -Path $settingsPath -ErrorAction SilentlyContinue
$dict = @{}
if ($content -and $content.Trim().Length -gt 0) {
    try {
        $parsed = $content | ConvertFrom-Json -ErrorAction Stop
        foreach ($p in $parsed.PSObject.Properties) {
            $dict[$p.Name] = $p.Value
        }
    } catch {
        Write-Warning "Could not parse settings.json as JSON. Aborting to avoid data loss."; exit 1
    }
}

# Tune the agentic flags to safer defaults
$dict['amazonQ.allowFeatureDevelopmentToRunCodeAndTests'] = @{ 
    autoApplyEdits = $false; 
    allowRunTests = $false; 
    allowRunCommands = $false 
}

# Write back the settings file
$dict | ConvertTo-Json -Depth 10 | Set-Content -Path $settingsPath -Encoding UTF8
Write-Output 'TUNED_OK'
Write-Output '---NEW settings.json---'
Get-Content -Raw -Path $settingsPath
Write-Output '---END---'
