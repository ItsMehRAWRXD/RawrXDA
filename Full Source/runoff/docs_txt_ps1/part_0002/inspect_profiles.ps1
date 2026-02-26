Set-StrictMode -Version Latest
$RootPath = 'd:\lazy init ide'
$RulesConfigPath = Join-Path $RootPath '.wiringdigestrules.json'
$rules = Get-Content $RulesConfigPath -Raw | ConvertFrom-Json
$profiles = $rules.profiles
"profiles type = $($profiles.GetType().FullName)"
"profiles properties = $($profiles.PSObject.Properties.Name -join ',')"
$profiles
$profiles.default
