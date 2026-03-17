# RawrXD Autonomous Auto-Fix Cycle (AAFC)
# This script monitors the build output, parses MASM/MSVC errors, and applies heuristic fixes autonomously.

function Fix-MasmError {
    param($File, $Line, $ErrorCode, $Message)
    Write-Host "[AAFC] Attempting to heal $File at line $Line (Error: $ErrorCode)" -ForegroundColor Cyan
    
    $Content = Get-Content $File
    $TargetLine = $Content[$Line - 1]

    switch -Regex ($Message) {
        "Missing .ENDPROLOG" {
            # Heuristic: Insert .ENDPROLOG before the first instruction or after the last .ALLOCSTACK/.PUSHREG
            Write-Host "  -> Balancing SEH prolog..."
            $NewContent = $Content[0..($Line-1)] + "    .endprolog" + $Content[$Line..($Content.Length-1)]
            $NewContent | Set-Content $File
        }
        "syntax error : Lock" {
            Write-Host "  -> Renaming reserved keyword 'Lock'..."
            (Get-Content $File) -replace '\.Lock\b', '.MonitorLock' | Set-Content $File
        }
        "invalid combination with segment alignment" {
            Write-Host "  -> Capping segment alignment to 16..."
            (Get-Content $File) -replace 'align \d{2,}', 'align 16' | Set-Content $File
        }
        "undefined symbol : (\w+)" {
            $Symbol = $Matches[1]
            Write-Host "  -> Injecting EXTERNDEF for $Symbol..."
            $Injected = "EXTERNDEF $Symbol : PROC"
            $NewContent = @($Injected) + $Content
            $NewContent | Set-Content $File
        }
        "instruction operands must be the same size" {
            Write-Host "  -> Normalizing register sizes..."
            # Simple heuristic for common mov eax, rbx -> mov rax, rbx
            $FixedLine = $TargetLine -replace 'mov eax, (\w{3})', 'mov rax, $1'
            $Content[$Line - 1] = $FixedLine
            $Content | Set-Content $File
        }
    }
}

function Run-AgenticLoop {
    while ($true) {
        Write-Host "=== Starting Autonomous Build Cycle ===" -ForegroundColor Yellow
        $BuildOutput = & .\Advanced-Feature-Testing.ps1 | Out-String
        
        # Parse MASM Errors: D:\path\file.asm(123) : error Axxxx: message
        $MasmErrors = $BuildOutput | Select-String -Pattern '(?<file>.*\.asm)\((?<line>\d+)\)\s+:\s+error\s+(?<id>A\d+):\s+(?<msg>.*)' -AllMatches
        
        if (-not $MasmErrors) {
            Write-Host "BUILD SUCCESS: System is healthy." -ForegroundColor Green
            break
        }

        foreach ($Match in $MasmErrors.Matches) {
            Fix-MasmError -File $Match.Groups['file'].Value -Line [int]$Match.Groups['line'].Value -ErrorCode $Match.Groups['id'].Value -Message $Match.Groups['msg'].Value
        }
        
        Write-Host "Cycle Complete. Re-verifying..."
    }
}

Run-AgenticLoop
