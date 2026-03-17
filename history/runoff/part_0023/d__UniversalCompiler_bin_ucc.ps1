[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Arguments
)

# Load universal compiler
if (Test-Path 'D:\COMPLETE-UNIVERSAL-COMPILER-INTEGRATION.ps1') {
    . 'D:\COMPLETE-UNIVERSAL-COMPILER-INTEGRATION.ps1'
} else {
    Write-Error "Universal compiler not found at D:\COMPLETE-UNIVERSAL-COMPILER-INTEGRATION.ps1"
    exit 1
}

# Parse GCC/Clang-style arguments
$inputFiles = @()
$outputFile = $null
$optimize = 'none'
$debug = $false
$compileOnly = $false
$assembleOnly = $false
$targetFormat = 'PE32+'

for ($i = 0; $i -lt $Arguments.Count; $i++) {
    $arg = $Arguments[$i]
    
    switch -Regex ($arg) {
        '^-o$' {
            $outputFile = $Arguments[++$i]
        }
        '^-O0$' { $optimize = 'none' }
        '^-O1$' { $optimize = 'O1' }
        '^-O2$' { $optimize = 'O2' }
        '^-O3$' { $optimize = 'O3' }
        '^-g$' { $debug = $true }
        '^-c$' { $compileOnly = $true }
        '^-S$' { $assembleOnly = $true }
        '^--target=.*linux.*$' { $targetFormat = 'ELF64' }
        '^--target=.*darwin.*$' { $targetFormat = 'Mach-O' }
        default {
            if ($arg -notmatch '^-') {
                $inputFiles += $arg
            }
        }
    }
}

# Determine output file if not specified
if (-not $outputFile -and $inputFiles.Count -gt 0) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($inputFiles[0])
    $outputFile = if ($compileOnly) { "$baseName.obj" } 
                  elseif ($assembleOnly) { "$baseName.s" } 
                  else { "$baseName.exe" }
}

# Auto-detect language from file extension
$language = 'C'  # Default
if ($inputFiles.Count -gt 0) {
    $ext = [System.IO.Path]::GetExtension($inputFiles[0]).ToLower()
    $languageMap = @{
        '.c' = 'C'
        '.cpp' = 'C++'
        '.cxx' = 'C++'
        '.cc' = 'C++'
        '.c++' = 'C++'
        '.cs' = 'C#'
        '.java' = 'Java'
        '.py' = 'Python'
        '.js' = 'JavaScript'
        '.ts' = 'TypeScript'
        '.rs' = 'Rust'
        '.go' = 'Go'
        '.swift' = 'Swift'
        '.rb' = 'Ruby'
        '.php' = 'PHP'
        '.pl' = 'Perl'
        '.lua' = 'Lua'
        '.r' = 'R'
        '.m' = 'Objective-C'
        '.mm' = 'Objective-C++'
        '.kt' = 'Kotlin'
        '.scala' = 'Scala'
        '.clj' = 'Clojure'
        '.fs' = 'F#'
        '.vb' = 'VB.NET'
        '.d' = 'D'
        '.nim' = 'Nim'
        '.zig' = 'Zig'
        '.v' = 'V'
        '.cr' = 'Crystal'
        '.jl' = 'Julia'
        '.dart' = 'Dart'
        '.hs' = 'Haskell'
        '.ml' = 'OCaml'
        '.ex' = 'Elixir'
        '.erl' = 'Erlang'
    }
    if ($languageMap.ContainsKey($ext)) {
        $language = $languageMap[$ext]
    }
}

# Compile
$mode = if ($compileOnly) { 'compile' } elseif ($assembleOnly) { 'assemble' } else { 'full' }

try {
    Invoke-UniversalCompiler -InputFiles $inputFiles `
                             -OutputFile $outputFile `
                             -Language $language `
                             -Mode $mode `
                             -TargetFormat $targetFormat `
                             -Optimization $optimize `
                             -Debug:$debug
    exit 0
} catch {
    Write-Error $_
    exit 1
}
