<#!
AgenticFramework.psm1 - Minimal agentic loop scaffolding for external models (e.g., Ollama) in pure PowerShell.
Provides tool registration, controlled shell execution, and a simple ReAct loop.
#>

# region State
$Script:AF_Config = [pscustomobject]@{
    Model   = 'ollama-default' # placeholder; set with Set-AgentModel
    Tools   = @{}
    MaxTurns = 10
}
# endregion

function Set-AgentModel {
<#!
.SYNOPSIS Set external model name used by Invoke-ExternalModel.
.PARAMETER Name Name of model (e.g. 'llama3.2:1b' or full HF reference). 
#>
    [CmdletBinding()] param([Parameter(Mandatory)][string]$Name)
    $Script:AF_Config.Model = $Name
    return $Script:AF_Config.Model
}

function Register-AgentTool {
<#!
.SYNOPSIS Register a tool function (ScriptBlock) for agent use.
.PARAMETER Name Tool invocation name.
.PARAMETER ScriptBlock The code to run; receives one argument ($ArgsJson or raw string).
.EXAMPLE
    Register-AgentTool -Name shell -ScriptBlock { param($cmd) & $env:COMSPEC /c $cmd }
#>
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][ScriptBlock]$ScriptBlock
    )
    $Script:AF_Config.Tools[$Name] = $ScriptBlock
    return $Name
}

function Get-AgentTools { $Script:AF_Config.Tools.Keys }

function Invoke-AgentTool {
<#!
.SYNOPSIS Invoke a registered tool by name.
.PARAMETER Name Tool name.
.PARAMETER Arg Argument string (or JSON) passed to ScriptBlock.
#>
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string]$Name,
        [string]$Arg = ''
    )
    if (-not $Script:AF_Config.Tools.ContainsKey($Name)) { throw "Tool '$Name' not registered." }
    try { & $Script:AF_Config.Tools[$Name] $Arg } catch { $_ | Out-String }
}

function Invoke-ExternalModel {
<#!
.SYNOPSIS Simple wrapper calling an Ollama model via CLI and returning text output.
.PARAMETER Model Model name set earlier or passed explicitly.
.PARAMETER Prompt Prompt text.
#>
    [CmdletBinding()] param(
        [string]$Model = $Script:AF_Config.Model,
        [Parameter(Mandatory)][string]$Prompt
    )
    # Use ollama CLI; capture output until it exits.
    try {
        if (-not (Get-Command ollama -ErrorAction SilentlyContinue)) { return 'ERROR: ollama CLI not found in PATH.' }
        $cmdArgs = @('run', $Model, '--', $Prompt)
        # Direct invocation of ollama binary (must be in PATH)
        $outLines = & ollama @cmdArgs 2>&1
        if ($outLines -is [System.Array]) { return ($outLines -join "`n") }
        return [string]$outLines
    } catch { return ($_ | Out-String) }
}

function Start-AgentLoop {
<#!
.SYNOPSIS Run a minimal ReAct agent loop with TOOL:/ANSWER: pattern expectations.
.PARAMETER Prompt Initial user task.
.PARAMETER MaxTurns Override default max turns.
.EXAMPLE
    Start-AgentLoop -Prompt 'Add 2+3, answer JSON.' -MaxTurns 5
#>
    [CmdletBinding()] param(
        [Parameter(Mandatory)][string]$Prompt,
        [int]$MaxTurns = $Script:AF_Config.MaxTurns
    )
    $messages = @()
    $system = @'
You are a PowerShell agent. Reply either:
TOOL:{name}:{argument}
ANSWER:{final answer}
If you need a tool, issue TOOL: first. Tools available: {tools}
'@
    $toolsList = ($Script:AF_Config.Tools.Keys -join ', ')
    $system = $system -replace '{tools}', $toolsList
    $messages += [pscustomobject]@{ role='system'; content=$system }
    $messages += [pscustomobject]@{ role='user'; content=$Prompt }

    for ($turn=1; $turn -le $MaxTurns; $turn++) {
        $promptText = ($messages | ForEach-Object { "[$($_.role)] $($_.content)" }) -join "`n"
        $modelOut = Invoke-ExternalModel -Prompt $promptText
        # crude last line parse
        $lastLine = ($modelOut -split "`n")[-1].Trim()
        Write-Verbose "Model raw last line: $lastLine"
        if ($lastLine -like 'ANSWER:*') {
            $answer = $lastLine.Substring(7).Trim()
            return [pscustomobject]@{ Turns=$turn; Answer=$answer }
        } elseif ($lastLine -like 'TOOL:*') {
            $parts = $lastLine.Split(':',3)
            if ($parts.Count -ge 3) {
                $toolName = $parts[1]; $arg = $parts[2]
                $obs = Invoke-AgentTool -Name $toolName -Arg $arg
                $messages += [pscustomobject]@{ role='assistant'; content=$lastLine }
                $messages += [pscustomobject]@{ role='user'; content="Observation: $obs" }
                continue
            } else {
                $messages += [pscustomobject]@{ role='user'; content='Malformed tool request.' }
                continue
            }
        } else {
            # remind format
            $messages += [pscustomobject]@{ role='user'; content='Please respond with TOOL: or ANSWER:' }
            continue
        }
    }
    return [pscustomobject]@{ Turns=$MaxTurns; Answer='Max turns reached without ANSWER.' }
}

# Provide safe default tools
Register-AgentTool -Name shell -ScriptBlock { param($cmd) if ($cmd.Length -gt 120) { 'Command too long.' } else { & $env:COMSPEC /c $cmd 2>&1 | Out-String } }
Register-AgentTool -Name echo -ScriptBlock { param($text) $text }

Export-ModuleMember -Function Set-AgentModel, Register-AgentTool, Get-AgentTools, Invoke-AgentTool, Invoke-ExternalModel, Start-AgentLoop
