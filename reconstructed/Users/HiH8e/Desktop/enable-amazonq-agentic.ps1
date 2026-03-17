$ErrorActionPreference = 'Stop'
$settingsPath = Join-Path $env:APPDATA 'Code\User\settings.json'
if (-not (Test-Path $settingsPath)) {
    New-Item -ItemType File -Path $settingsPath -Force | Out-Null
    Set-Content -Path $settingsPath -Value '{}' -Encoding UTF8
}
$ts = Get-Date -Format 'yyyyMMdd-HHmmss'
$bak = Join-Path $env:USERPROFILE "Desktop\settings.json.backup-$ts"
Copy-Item -Path $settingsPath -Destination $bak -Force
Write-Output "BACKUP_CREATED:$bak"

# Read existing settings
$content = Get-Content -Raw -Path $settingsPath -ErrorAction SilentlyContinue
# Convert existing JSON to a PowerShell hashtable (dictionary) so we can set keys with dots
$dict = @{}
if ($content -and $content.Trim().Length -gt 0) {
    try {
        $parsed = $content | ConvertFrom-Json -ErrorAction Stop
        foreach ($p in $parsed.PSObject.Properties) {
            $dict[$p.Name] = $p.Value
        }
    } catch {
        # If parsing fails, start with empty hashtable
        $dict = @{}
    }
}

# Set permissive agentic object (reversible). Use the full setting key (dots are allowed in JSON keys).
$dict['amazonQ.allowFeatureDevelopmentToRunCodeAndTests'] = @{ 
    autoApplyEdits = $true; 
    allowRunTests = $true; 
    allowRunCommands = $true 
}

# Ensure existing amazonQ.suppressPrompts remains if present (no-op otherwise)
# (This preserves other settings)

# Write back
# Convert back to JSON and write. ConvertTo-Json on a hashtable yields desired object structure.
$dict | ConvertTo-Json -Depth 10 | Set-Content -Path $settingsPath -Encoding UTF8
Write-Output 'UPDATED_OK'
Write-Output '---NEW_SETTINGS_START---'
Get-Content -Raw -Path $settingsPath
Write-Output '---NEW_SETTINGS_END---'
