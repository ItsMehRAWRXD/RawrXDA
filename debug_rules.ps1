$RootPath = 'd:\lazy init ide'
$ModuleOverridePrecedence = 'BaseFirst'
$overrideMode = 'merge'
$RulesProfile = 'default'
$RulesConfigPath = Join-Path $RootPath '.wiringdigestrules.json'
$rules = Get-Content $RulesConfigPath -Raw | ConvertFrom-Json
$resolvedRules = $rules
if ($rules.profiles -and $rules.profiles.$RulesProfile) {
    $resolvedRules = $rules.profiles.$RulesProfile
}
"resolvedRulesType=$($resolvedRules.GetType().FullName)"
"resolvedKeys=$($resolvedRules.PSObject.Properties.Name -join ',')"
"rulesProfilesType=$($rules.profiles.GetType().FullName)"
"rulesProfilesKeys=$($rules.profiles.PSObject.Properties.Name -join ',')"
