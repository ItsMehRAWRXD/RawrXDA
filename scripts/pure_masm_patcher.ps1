# pure_masm_patcher.ps1 — Apply MASM/reverse-engineering fixes for RawrXD build
# Writes ASM stub files under src/asm/, ensures js_extension_host.hpp has #include <queue>.
# Run from repo root: .\scripts\pure_masm_patcher.ps1 [-Apply]

param(
    [string]$Root = (Get-Location),
    [switch]$Apply
)

$ErrorActionPreference = "Stop"
$Red = "`e[31m"
$Green = "`e[32m"
$Yellow = "`e[33m"
$Cyan = "`e[36m"
$Reset = "`e[0m"

Write-Host "${Cyan}🔧 RawrXD MASM / build fix patcher${Reset}"
Write-Host "Target: $Root`n"

$asmDir = Join-Path $Root "src\asm"
$asmStubs = @(
    @{
        Name = "vulkan_compute_masm.asm"
        Content = @"
; vulkan_compute_masm.asm — Stub for Vulkan compute bridge (MASM64)
.CODE
VulkanCompute_MASM_Stub PROC
    xor eax, eax
    ret
VulkanCompute_MASM_Stub ENDP
END
"@
    },
    @{
        Name = "beacon_integration_masm.asm"
        Content = @"
; beacon_integration_masm.asm — Stub for Circular Beacon integration (MASM64)
.CODE
BeaconIntegration_MASM_Stub PROC
    xor eax, eax
    ret
BeaconIntegration_MASM_Stub ENDP
END
"@
    },
    @{
        Name = "fix_js_extension_queue.asm"
        Content = @"
; fix_js_extension_queue.asm — Stub for JS extension host queue (MASM64)
.CODE
JsExtensionQueue_MASM_Stub PROC
    xor eax, eax
    ret
JsExtensionQueue_MASM_Stub ENDP
END
"@
    }
)

# Write or preview ASM stub files under src/asm/
if (-not (Test-Path $asmDir)) {
    if ($Apply) {
        New-Item -ItemType Directory -Force -Path $asmDir | Out-Null
        Write-Host "${Green}Created${Reset} $asmDir"
    } else {
        Write-Host "${Yellow}Would create${Reset} $asmDir"
    }
}
foreach ($stub in $asmStubs) {
    $path = Join-Path $asmDir $stub.Name
    if ($Apply) {
        Set-Content -Path $path -Value $stub.Content -Encoding UTF8
        Write-Host "  ${Green}Wrote${Reset} src\asm\$($stub.Name)"
    } else {
        Write-Host "  ${Yellow}Would write${Reset} src\asm\$($stub.Name)"
    }
}

# Fix js_extension_host.hpp include
$jsHeader = Join-Path $Root "src\core\js_extension_host.hpp"
if (Test-Path $jsHeader) {
    Write-Host "`nChecking js_extension_host.hpp..." -NoNewline
    $content = Get-Content $jsHeader -Raw -ErrorAction SilentlyContinue
    if ($content -and $content -notmatch "#include\s*<queue>") {
        if ($Apply) {
            $insert = "`n#include <queue>"
            $newContent = $content -replace "(\#include\s+<unordered_map>)", "`$1$insert", 1
            if ($newContent -eq $content) {
                $newContent = $content -replace "(\#include\s+<vector>)", "`$1$insert", 1
            }
            if ($newContent -ne $content) {
                Set-Content -Path $jsHeader -Value $newContent -NoNewline
                Write-Host " ${Green}FIXED (added #include <queue>)${Reset}"
            } else {
                $newContent = $content -replace "^(\s*// =+)", "`$1`n#include <queue>", 1
                if ($newContent -ne $content) {
                    Set-Content -Path $jsHeader -Value $newContent -NoNewline
                    Write-Host " ${Green}FIXED (added #include <queue> at top)${Reset}"
                } else {
                    Write-Host " ${Yellow}Could not find insert point${Reset}"
                }
            }
        } else {
            Write-Host " ${Yellow}NEEDS FIX (missing #include <queue>) — use -Apply${Reset}"
        }
    } else {
        Write-Host " ${Green}OK${Reset}"
    }
} else {
    Write-Host "`n${Yellow}  SKIP${Reset} js_extension_host.hpp not found"
}

# Remind about critical vs optional ASM
$buildScript = Join-Path $Root "scripts\build_arch.ps1"
if (Test-Path $buildScript) {
    Write-Host "`nbuild_arch.ps1: critical ASM (memory_patch.asm, RawrCodex.asm) must exist."
    Write-Host "Optional ASM (kernels.asm, streaming_dma.asm, quant_avx2.asm) are skipped if missing."
}

Write-Host "`n${Cyan}Done.${Reset} Build with: .\scripts\build_arch.ps1 -Config Release"
if (-not $Apply) {
    Write-Host "Preview only. Run with -Apply to modify files." -ForegroundColor Yellow
}
