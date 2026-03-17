param(
    [Parameter(Mandatory=$true, Position=0)] [string]$InputPath,
    [Parameter(Mandatory=$false)] [string]$OutDir = (Join-Path (Split-Path -Parent $InputPath) 'masm-out'),
    [Parameter(Mandatory=$false)] [ValidateSet('stubs','asm')] [string]$Mode = 'stubs',
    [Parameter(Mandatory=$false)] [string]$AdditionalIncludes = ''
)

$ErrorActionPreference = 'Stop'

function Ensure-OutDir($dir) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }
}

function New-MasmStubFromHeader([string]$headerPath, [string]$outFile) {
    $content = Get-Content -Raw -LiteralPath $headerPath
    # Strip comments
    $content = [regex]::Replace($content, @"/\*.*?\*/"@, '', 'Singleline')
    $content = [regex]::Replace($content, @"//.*$"@, '', 'Multiline')

    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine(".code")

    # Match simple C-style functions: return name(args);
    $regex = '(?m)^[\t ]*([A-Za-z_][A-Za-z0-9_:\\* &<>,\[\]]+)\s+([A-Za-z_][A-Za-z0-9_:]*)\s*\(([^;{}()]*)\)\s*;'
    $matches = [regex]::Matches($content, $regex)

    foreach ($m in $matches) {
        $ret = $m.Groups[1].Value.Trim()
        $name = $m.Groups[2].Value.Trim()
        $args = $m.Groups[3].Value.Trim()
        if ($name.StartsWith('~')) { continue }
        if ($name.Contains('operator')) { continue }
        # Simplify name for export
        $procName = $name
        [void]$sb.AppendLine("PUBLIC $procName")
        [void]$sb.AppendLine("$procName PROC")
        [void]$sb.AppendLine("    ; TODO: implement converted body")
        if ($ret -match '^void(\s|$)') {
            [void]$sb.AppendLine("    ret")
        } else {
            [void]$sb.AppendLine("    xor rax, rax ; default return 0")
            [void]$sb.AppendLine("    ret")
        }
        [void]$sb.AppendLine("$procName ENDP")
        [void]$sb.AppendLine("")
    }

    [void]$sb.AppendLine("end")
    Set-Content -LiteralPath $outFile -Value $sb.ToString() -Encoding ASCII
}

function Invoke-ClToAsm([string]$cppPath, [string]$outDir, [string]$includes) {
    # Try to locate VsDevCmd.bat for MSVC environment
    $vsWhere = "$Env:ProgramFiles(x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vsWhere)) { throw "vswhere.exe not found at $vsWhere" }
    $vs = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw 'Visual Studio with VC tools not found' }
    $vsDevCmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path $vsDevCmd)) { throw "VsDevCmd.bat not found at $vsDevCmd" }

    $cppAbs = (Resolve-Path -LiteralPath $cppPath).Path
    $outAbs = (Resolve-Path -LiteralPath $outDir).Path
    $asmOut = Join-Path $outAbs ((Split-Path -Leaf $cppAbs) + '.asm')

    $incArgs = ''
    if ($includes) { $incArgs = $includes.Split(';') | ForEach-Object { '/I "' + $_ + '"' } | Out-String }

    $cmd = "\"$vsDevCmd\" -arch=amd64 -host_arch=amd64 && cl.exe /nologo /c /O2 /Z7 /std:c++20 /FAcs /Fa:\"$asmOut\" $incArgs \"$cppAbs\""
    Write-Host "Invoking: $cmd"
    cmd.exe /c $cmd
    if (-not (Test-Path $asmOut)) { throw "Assembly listing not produced: $asmOut" }
    Write-Host "Assembly listing written: $asmOut"
}

Ensure-OutDir $OutDir

if ($Mode -eq 'stubs') {
    if ((Split-Path -Extension $InputPath) -in @('.h','.hpp','.hh')) {
        $out = Join-Path $OutDir ((Split-Path -LeafBase $InputPath) + '.asm')
        New-MasmStubFromHeader -headerPath $InputPath -outFile $out
        Write-Host "MASM stub generated: $out"
    } else {
        throw "For Mode=stubs, please pass a header file (.h/.hpp)"
    }
} elseif ($Mode -eq 'asm') {
    if ((Split-Path -Extension $InputPath) -in @('.cpp','.cc','.cxx','.c')) {
        Invoke-ClToAsm -cppPath $InputPath -outDir $OutDir -includes $AdditionalIncludes
    } else {
        throw "For Mode=asm, please pass a C/C++ source file (.cpp/.cc/.cxx/.c)"
    }
} else {
    throw "Unknown mode: $Mode"
}
