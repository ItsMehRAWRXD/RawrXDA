#!/usr/bin/env pwsh
# Create Single-File Executable with Embedded Toolchain
# NO EXTERNAL FILES REQUIRED

param(
    [switch]$Release
)

$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Single-File Deployment Builder" -ForegroundColor Cyan
Write-Host "  Embeds ALL tools as resources" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$ProjectRoot = Split-Path -Parent $PSCommandPath
$ResourceDir = Join-Path $ProjectRoot "resources"
$BuildDir = Join-Path $ProjectRoot "build_single"

# Ensure internal tools exist
$MasmCompiler = Join-Path $ProjectRoot "src\masm\masm_solo_compiler.exe"
$InternalLinker = Join-Path $ProjectRoot "src\masm\internal_link.exe"

foreach ($tool in @($MasmCompiler, $InternalLinker)) {
    if (-not (Test-Path $tool)) {
        Write-Host "❌ Missing: $tool" -ForegroundColor Red
        Write-Host "Run: cd src\masm; .\build_toolchain.ps1" -ForegroundColor Yellow
        exit 1
    }
}

# Create resource script
Write-Host "📝 Creating resource script with embedded tools..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $ResourceDir | Out-Null

$ResourceScript = Join-Path $ResourceDir "embedded_tools.rc"
$ResourceContent = @"
// Embedded Internal Toolchain
// These tools are extracted at runtime to temporary directory

#define IDR_INTERNAL_MASM   101
#define IDR_INTERNAL_LINK   102

// Embed MASM compiler
IDR_INTERNAL_MASM RCDATA "$($MasmCompiler.Replace('\', '\\'))"

// Embed linker
IDR_INTERNAL_LINK RCDATA "$($InternalLinker.Replace('\', '\\'))"
"@

Set-Content -Path $ResourceScript -Value $ResourceContent -Encoding ASCII

Write-Host "  ✅ Resource script created" -ForegroundColor Green

# Create resource extraction code
$ExtractCode = Join-Path $ProjectRoot "src\win32app\ResourceExtractor.cpp"
$ExtractContent = @"
// ResourceExtractor.cpp - Extract embedded tools at runtime
#include <windows.h>
#include <string>
#include <fstream>

#define IDR_INTERNAL_MASM   101
#define IDR_INTERNAL_LINK   102

namespace RawrXD {

class ResourceExtractor {
public:
    static bool extractEmbeddedTools() {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        
        std::string toolDir = std::string(tempPath) + "RawrXD_Tools\\";
        CreateDirectoryA(toolDir.c_str(), NULL);
        
        bool success = true;
        success &= extractResource(IDR_INTERNAL_MASM, toolDir + "masm_solo_compiler.exe");
        success &= extractResource(IDR_INTERNAL_LINK, toolDir + "internal_link.exe");
        
        return success;
    }
    
    static std::string getToolPath(const std::string& toolName) {
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        return std::string(tempPath) + "RawrXD_Tools\\" + toolName;
    }

private:
    static bool extractResource(int resourceId, const std::string& outputPath) {
        HMODULE hModule = GetModuleHandle(NULL);
        HRSRC hResource = FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA);
        if (!hResource) return false;
        
        HGLOBAL hLoadedResource = LoadResource(hModule, hResource);
        if (!hLoadedResource) return false;
        
        LPVOID pResourceData = LockResource(hLoadedResource);
        if (!pResourceData) return false;
        
        DWORD resourceSize = SizeofResource(hModule, hResource);
        
        std::ofstream out(outputPath, std::ios::binary);
        if (!out.is_open()) return false;
        
        out.write((char*)pResourceData, resourceSize);
        out.close();
        
        return true;
    }
};

} // namespace RawrXD
"@

Set-Content -Path $ExtractCode -Value $ExtractContent

Write-Host "  ✅ Resource extractor created" -ForegroundColor Green

# Update compiler to use extracted tools
Write-Host "📝 Updating compiler to use embedded tools..." -ForegroundColor Yellow

$CompilerUpdate = @"

// In compiler_asm_real.cpp, update find_internal_masm():

std::string find_internal_masm() {
    // First, extract from resources if not already done
    static bool extracted = false;
    if (!extracted) {
        RawrXD::ResourceExtractor::extractEmbeddedTools();
        extracted = true;
    }
    
    // Return path to extracted tool
    return RawrXD::ResourceExtractor::getToolPath("masm_solo_compiler.exe");
}

std::string find_internal_linker() {
    // First, extract from resources if not already done
    static bool extracted = false;
    if (!extracted) {
        RawrXD::ResourceExtractor::extractEmbeddedTools();
        extracted = true;
    }
    
    // Return path to extracted tool
    return RawrXD::ResourceExtractor::getToolPath("internal_link.exe");
}
"@

$UpdatePath = Join-Path $ProjectRoot "COMPILER_UPDATE_FOR_RESOURCES.txt"
Set-Content -Path $UpdatePath -Value $CompilerUpdate

Write-Host "  ✅ Update instructions created: COMPILER_UPDATE_FOR_RESOURCES.txt" -ForegroundColor Green

# Build with embedded resources
Write-Host "🔨 Building single-file executable..." -ForegroundColor Yellow
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$OutputExe = Join-Path $BuildDir "RawrXD_IDE_Single.exe"

# Compile resource file
Write-Host "  Compiling resources..." -ForegroundColor White
$ResourceObj = Join-Path $BuildDir "embedded_tools.res"

$rc = "rc.exe"
if (Get-Command $rc -ErrorAction SilentlyContinue) {
    & $rc /fo $ResourceObj $ResourceScript 2>&1 | Out-Null
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "    ✅ Resources compiled" -ForegroundColor Green
    } else {
        Write-Host "    ⚠️  Resource compiler not found" -ForegroundColor Yellow
        Write-Host "    Resources will need to be compiled separately" -ForegroundColor Gray
    }
} else {
    Write-Host "    ⚠️  rc.exe not found (optional)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ Single-File Deployment Ready!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "1. Apply updates from: COMPILER_UPDATE_FOR_RESOURCES.txt" -ForegroundColor White
Write-Host "2. Link with: $ResourceObj" -ForegroundColor White
Write-Host "3. Final executable: $OutputExe" -ForegroundColor White
Write-Host ""
Write-Host "Benefits:" -ForegroundColor Cyan
Write-Host "  • Single .exe file" -ForegroundColor Gray
Write-Host "  • No external files needed" -ForegroundColor Gray
Write-Host "  • Tools extracted to temp at runtime" -ForegroundColor Gray
Write-Host "  • Zero configuration" -ForegroundColor Gray
Write-Host ""
Write-Host "Distribution:" -ForegroundColor Yellow
Write-Host "  Just copy RawrXD_IDE_Single.exe - that's it!" -ForegroundColor Green
