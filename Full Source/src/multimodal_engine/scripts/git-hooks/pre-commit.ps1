# PowerShell pre-commit hook: run update and fail commit if audit changes
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$Py = "python"
& $Py (Join-Path $ScriptDir '..\update_quant_types.py')
$diff = git diff -- "src/multimodal_engine/MULTIMODAL_ENGINE_FOLDER_AUDIT.md"
if ($diff) {
    Write-Error "MULTIMODAL_ENGINE_FOLDER_AUDIT.md changed. Please add it to the commit."
    git --no-pager diff -- "src/multimodal_engine/MULTIMODAL_ENGINE_FOLDER_AUDIT.md"
    exit 1
}
