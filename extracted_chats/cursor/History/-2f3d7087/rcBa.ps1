# instruction_compatibility.ps1
# Shows what instructions are supported

$SupportedInstructions = @{
    "Simple Arithmetic" = @("NOP", "RET", "XOR r64,r64", "INC r64", "ADD r64,imm8")
    "Control Flow" = @("JMP rel8", "JNZ rel8", "JNE rel8", "CALL $+5")
    "Data Transfer" = @("MOV r64,imm64", "MOV r64,[r64+offset]", "MOV r64,[gs:offset]")
    "System" = @("SYSCALL")
}

$UnsupportedInstructions = @{
    "Memory Operands" = @("[r64+r64]", "[r64*r64+offset]", "Complex SIB addressing")
    "Segment Registers" = @("FS", "DS", "ES", "CS")
    "Floating Point" = @("All x87/MMX/SSE/AVX instructions")
    "String Operations" = @("MOVS", "CMPS", "SCAS", "LODS", "STOS")
    "Bit Operations" = @("BT", "BTS", "BTR", "BTC")
}

Write-Host "✅ SUPPORTED INSTRUCTIONS:" -ForegroundColor Green
$SupportedInstructions.GetEnumerator() | % {
    Write-Host "  $($_.Key):" -ForegroundColor Cyan
    $_.Value | % { Write-Host "    - $_" -ForegroundColor White }
}

Write-Host "`n❌ UNSUPPORTED INSTRUCTIONS:" -ForegroundColor Red
$UnsupportedInstructions.GetEnumerator() | % {
    Write-Host "  $($_.Key):" -ForegroundColor Yellow
    $_.Value | % { Write-Host "    - $_" -ForegroundColor Gray }
}
