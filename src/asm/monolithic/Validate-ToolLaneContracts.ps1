#!/usr/bin/env pwsh
[CmdletBinding()]
param(
    [string]$MonolithicDir = $PSScriptRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$agentToolsPath = Join-Path $MonolithicDir "agent_tools.asm"
$rtpHandlersPath = Join-Path $MonolithicDir "rtp_tool_handlers.asm"

if (-not (Test-Path -LiteralPath $agentToolsPath)) {
    Write-Error "Missing file: $agentToolsPath"
    exit 2
}
if (-not (Test-Path -LiteralPath $rtpHandlersPath)) {
    Write-Error "Missing file: $rtpHandlersPath"
    exit 2
}

$agentTools = Get-Content -LiteralPath $agentToolsPath -Raw
$rtpHandlers = Get-Content -LiteralPath $rtpHandlersPath -Raw

$errors = New-Object System.Collections.Generic.List[string]

function Assert-Pattern {
    param(
        [string]$Text,
        [string]$Pattern,
        [string]$Message
    )
    if (-not [regex]::IsMatch($Text, $Pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
        $errors.Add($Message)
    }
}

# Core explicit impl dispatch assertions (IDs 0..6)
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_ReadFile_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*0\*8\],\s*rax' -Message 'Dispatch ID 0 must route to Tool_ReadFile_Impl.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_EditFile_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*1\*8\],\s*rax' -Message 'Dispatch ID 1 must route to Tool_EditFile_Impl.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_ListDir_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*2\*8\],\s*rax' -Message 'Dispatch ID 2 must route to Tool_ListDir_Impl.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_RunCommand_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*3\*8\],\s*rax' -Message 'Dispatch ID 3 must route to Tool_RunCommand_Impl.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_SearchCode_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*4\*8\],\s*rax' -Message 'Dispatch ID 4 must route to Tool_SearchCode_Impl.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_GetDiagnostics_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*5\*8\],\s*rax' -Message 'Dispatch ID 5 must route to Tool_GetDiagnostics_Impl.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_GetSymbols_Impl\s*\r?\n\s*mov\s+qword ptr\s*\[rbx\s*\+\s*6\*8\],\s*rax' -Message 'Dispatch ID 6 must route to Tool_GetSymbols_Impl.'

# Family lanes 7..15 should route to ShellExec impl.
foreach ($lane in @('TOOL_ID_GIT_OPS','TOOL_ID_BUILD_OPS','TOOL_ID_TEST_OPS','TOOL_ID_PROCESS_OPS','TOOL_ID_SHELL_OPS','TOOL_ID_AGENT_OPS','TOOL_ID_MODEL_OPS','TOOL_ID_WEB_OPS','TOOL_ID_SYSTEM_OPS')) {
    $pat = 'mov\s+qword ptr\s*\[rbx\s*\+\s*' + [regex]::Escape($lane) + '\*8\],\s*rax'
    Assert-Pattern -Text $agentTools -Pattern $pat -Message "Family lane $lane must be assigned in Tool_Init."
}
Assert-Pattern -Text $agentTools -Pattern 'Family lanes use ShellExec impl' -Message 'Tool_Init should document family lane ShellExec routing.'
Assert-Pattern -Text $agentTools -Pattern 'lea\s+rax,\s*Tool_ShellExec_Impl' -Message 'Tool_Init must load Tool_ShellExec_Impl for family lanes.'

# RTP contract assertions.
Assert-Pattern -Text $rtpHandlers -Pattern 'FORWARD_HANDLER\s+RawrXD_Tools_ExecuteCommand,\s*11\b' -Message 'ExecuteCommand must route to lane 11.'
if ([regex]::IsMatch($rtpHandlers, 'FORWARD_HANDLER\s+RawrXD_Tools_ExecuteCommand,\s*3\b', [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)) {
    $errors.Add('ExecuteCommand must not route to lane 3.')
}

if ($errors.Count -gt 0) {
    Write-Host '[contracts] FAILED' -ForegroundColor Red
    foreach ($e in $errors) {
        Write-Host "  - $e" -ForegroundColor Red
    }
    exit 1
}

Write-Host '[contracts] OK - monolithic tool lane contracts verified.' -ForegroundColor Green
exit 0
