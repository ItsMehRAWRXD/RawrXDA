param(
    [switch]$DryRun
)

$py = "python"
$script = Join-Path -Path $PSScriptRoot -ChildPath 'update_quant_types.py'
if ($DryRun) {
    & $py $script --dry-run
} else {
    & $py $script
}
