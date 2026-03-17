<#
.SYNOPSIS
  Find and report incomplete/empty/stub implementations across the RawrXD workspace.

.DESCRIPTION
  Scans source-like files (MASM .asm/.inc, C/C++ .c/.cpp/.h, scripts) and detects:
  - MASM PROC blocks that are effectively stubs (e.g., only `xor eax, eax` + `ret`).
  - C/C++ functions with empty bodies or trivial placeholder returns.
  - TODO/FIXME/NOT IMPLEMENTED markers near function blocks.

  Outputs a JSON and CSV report with file paths and 1-based line ranges.

.PARAMETER Root
  Root directory to scan.

.PARAMETER OutputDir
  Output directory for reports.

.PARAMETER ExcludeDirs
  Directory names to exclude anywhere in the path.

.PARAMETER Extensions
  File extensions to scan.

.PARAMETER MaxFileBytes
  Skip files larger than this.

.EXAMPLE
  pwsh -File .\tools\Find-IncompleteStubs.ps1 -Root . -OutputDir .\stub_reports
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory = $false)]
  [string]$Root = (Get-Location).Path,

  [Parameter(Mandatory = $false)]
  [string]$OutputDir = (Join-Path (Get-Location).Path "stub_reports"),

  [Parameter(Mandatory = $false)]
  [string[]]$ExcludeDirs = @(
    ".git",
    ".vs",
    ".vscode",
    "build",
    "obj",
    "bin",
    "out",
    "dist",
    "node_modules",
    "3rdparty",
    "third_party",
    "Release",
    "x64",
    "Win32",
    "CMakeFiles"
  ),

  [Parameter(Mandatory = $false)]
  [string[]]$Extensions = @(
    ".asm", ".inc",
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hpp", ".hxx",
    ".ps1"
  ),

  [Parameter(Mandatory = $false)]
  [long]$MaxFileBytes = 2MB
)

# Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function New-Finding {
  param(
    [string]$Type,
    [string]$File,
    [int]$StartLine,
    [int]$EndLine,
    [string]$Name,
    [string]$Reason,
    [string]$Preview
  )

  [pscustomobject]@{
    type      = $Type
    file      = $File
    startLine = $StartLine
    endLine   = $EndLine
    name      = $Name
    reason    = $Reason
    preview   = $Preview
  }
}

function Test-IsExcludedPath {
  param(
    [string]$FullName,
    [string[]]$ExcludeDirs
  )

  $normalized = $FullName -replace '/', '\\'
  foreach ($dir in $ExcludeDirs) {
    if ($normalized -match [regex]::Escape("\\$dir\\") -or $normalized.EndsWith("\\$dir", [System.StringComparison]::OrdinalIgnoreCase)) {
      return $true
    }
  }
  return $false
}

function Get-TextLines {
  param([string]$Path)

  # Use -Raw then split to preserve line numbers consistently.
  $raw = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop
  return $raw -split "\r?\n", -1
}

function Find-MasmProcStubs {
  param(
    [string]$Path,
    [string[]]$Lines
  )

  $findings = @()

  # PROC header: label PROC (optionally with qualifiers)
  # We keep it permissive because codebase varies.
  $procHeader = [regex]'^(?<name>[A-Za-z_.$@?][\w.$@?]*)\s+PROC\b'
  $endpLine   = [regex]'^(?<name>[A-Za-z_.$@?][\w.$@?]*)\s+ENDP\b'

  $current = $null

  for ($i = 0; $i -lt $Lines.Length; $i++) {
    $line = $Lines[$i]

    if (-not $current) {
      $m = $procHeader.Match($line)
      if ($m.Success) {
        $current = [pscustomobject]@{
          name = $m.Groups['name'].Value
          start = $i
          body = New-Object System.Collections.Generic.List[string]
        }
      }
      continue
    }

    # inside PROC
    $mEnd = $endpLine.Match($line)
    if ($mEnd.Success) {
      $endIdx = $i

      # Analyze body lines: ignore blank + comments
      $effective = @(
        $current.body |
          Where-Object {
            $t = $_.Trim()
            if ($t.Length -eq 0) { return $false }
            if ($t.StartsWith(';')) { return $false }
            # allow label-only lines like "label:" or "ALIGN 16"
            if ($t -match '^[A-Za-z_.$@?][\w.$@?]*:\s*$') { return $false }
            return $true
          } |
          ForEach-Object { $_.Trim() }
      )

      # Common MASM stub shapes.
      $stub = $false
      $reason = $null

      $joined = ($effective -join "\n")

      # Extremely empty PROC
      if ($effective.Count -eq 0) {
        $stub = $true
        $reason = 'Empty PROC body'
      }

      # xor eax,eax + ret (or mov eax,0 + ret)
      if (-not $stub) {
        $normalized = $effective | ForEach-Object { ($_ -replace '\s+', ' ').ToLowerInvariant() }
        $normJoined = ($normalized -join ';')

        $patterns = @(
          'xor eax, eax;ret',
          'xor eax,eax;ret',
          'mov eax, 0;ret',
          'mov eax,0;ret',
          'xor rax, rax;ret',
          'xor rax,rax;ret',
          'mov rax, 0;ret',
          'mov rax,0;ret'
        )

        foreach ($p in $patterns) {
          if ($normJoined -eq $p) {
            $stub = $true
            $reason = "Trivial return stub ($p)"
            break
          }
        }
      }

      # TODO/FIXME markers inside a PROC
      if (-not $stub) {
        if ($current.body -match '(?i)\bTODO\b|\bFIXME\b|\bNOT\s+IMPLEMENTED\b|\bSTUB\b') {
          # Not always a stub, but worth reporting.
          $stub = $true
          $reason = 'Marker inside PROC (TODO/FIXME/STUB/NOT IMPLEMENTED)'
        }
      }

      if ($stub) {
        $previewLines = $Lines[$current.start..$endIdx] | Select-Object -First 12
        $preview = ($previewLines -join "`n")

        $findings += (New-Finding -Type 'masm-proc' -File $Path -StartLine ($current.start + 1) -EndLine ($endIdx + 1) -Name $current.name -Reason $reason -Preview $preview)
      }

      $current = $null
      continue
    }

    # keep collecting body
    $current.body.Add($line) | Out-Null
  }

  return $findings
}

function Find-TextMarkers {
  param(
    [string]$Path,
    [string[]]$Lines
  )

  $findings = @()
  $rx = [regex]'(?i)\bTODO\b|\bFIXME\b|\bNOT\s+IMPLEMENTED\b|\bPLACEHOLDER\b|\bSTUB\b'

  for ($i = 0; $i -lt $Lines.Length; $i++) {
    if ($rx.IsMatch($Lines[$i])) {
      $start = [Math]::Max(0, $i - 2)
      $end = [Math]::Min($Lines.Length - 1, $i + 2)
      $preview = ($Lines[$start..$end] -join "`n")

      $findings += (New-Finding -Type 'marker' -File $Path -StartLine ($i + 1) -EndLine ($i + 1) -Name '' -Reason 'TODO/FIXME/STUB-like marker' -Preview $preview)
    }
  }

  return $findings
}

function Find-CppEmptyBodies {
  param(
    [string]$Path,
    [string[]]$Lines
  )

  # Heuristic only: catches common empty / trivial return implementations.
  # We avoid full parsing to keep it fast and dependency-free.

  $findings = @()

  $text = ($Lines -join "`n")

  # Function with empty body: ") { }" possibly with whitespace/newlines.
  $rxEmpty = [regex]'(?ms)(?<sig>^[^\n;{}#]+\)\s*\{\s*\})'

  # Function that just returns 0/false/nullptr.
  $rxReturn = [regex]'(?ms)(?<sig>^[^\n;{}#]+\)\s*\{\s*(return\s+(0|false|nullptr|NULL)\s*;\s*)\})'

  foreach ($rx in @($rxEmpty, $rxReturn)) {
    $matches = $rx.Matches($text)
    foreach ($m in $matches) {
      $block = $m.Groups['sig'].Value
      $firstLine = ($block -split "\r?\n")[0].Trim()

      # Determine line number by counting newlines before match.
      $prefix = $text.Substring(0, $m.Index)
      $startLine = ($prefix -split "\r?\n").Length
      $endLine = $startLine + (($block -split "\r?\n").Length - 1)

      $reason = if ($rx -eq $rxEmpty) { 'Empty C/C++ function body' } else { 'Trivial return C/C++ function body' }

      $preview = ($block -split "\r?\n" | Select-Object -First 10) -join "`n"

      $findings += (New-Finding -Type 'cpp-fn' -File $Path -StartLine $startLine -EndLine $endLine -Name $firstLine -Reason $reason -Preview $preview)
    }
  }

  return $findings
}

Write-Host "[StubSniffer] Root: $Root" -ForegroundColor Cyan
Write-Host "[StubSniffer] Output: $OutputDir" -ForegroundColor Cyan
Write-Host "[StubSniffer] Extensions: $($Extensions -join ', ')" -ForegroundColor Cyan

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$allFindings = @()

$files = Get-ChildItem -Path $Root -Recurse -File -ErrorAction Stop |
  Where-Object {
    $ext = $_.Extension.ToLowerInvariant()
    $Extensions -contains $ext -and -not (Test-IsExcludedPath -FullName $_.FullName -ExcludeDirs $ExcludeDirs) -and $_.Length -le $MaxFileBytes
  }

Write-Host "[StubSniffer] Files found: $($files.Count)" -ForegroundColor Yellow

$scanned = 0
foreach ($f in $files) {
  $scanned++
  if (($scanned % 250) -eq 0) {
    Write-Host "[StubSniffer] Scanned $scanned / $($files.Count) files..." -ForegroundColor DarkGray
  }

  try {
    $lines = Get-TextLines -Path $f.FullName

    # Markers (all extensions)
    foreach ($x in (Find-TextMarkers -Path $f.FullName -Lines $lines)) { $allFindings += $x }

    # MASM PROC blocks
    if ($f.Extension -in @('.asm', '.inc')) {
      foreach ($x in (Find-MasmProcStubs -Path $f.FullName -Lines $lines)) { $allFindings += $x }
    }

    # C/C++ empty bodies
    if ($f.Extension -in @('.c', '.cc', '.cpp', '.cxx', '.h', '.hpp', '.hxx')) {
      foreach ($x in (Find-CppEmptyBodies -Path $f.FullName -Lines $lines)) { $allFindings += $x }
    }
  }
  catch {
    $allFindings += (New-Finding -Type 'error' -File $f.FullName -StartLine 0 -EndLine 0 -Name '' -Reason $_.Exception.Message -Preview '')
  }
}

# Sort for stable output
$sorted = $allFindings | Sort-Object -Property file, startLine, type

$jsonPath = Join-Path $OutputDir 'stub_report.json'
$csvPath  = Join-Path $OutputDir 'stub_report.csv'

$sorted | ConvertTo-Json -Depth 6 | Out-File -FilePath $jsonPath -Encoding utf8
$sorted | Export-Csv -Path $csvPath -NoTypeInformation -Encoding utf8

$byType = $sorted | Group-Object -Property type | Sort-Object -Property Count -Descending

Write-Host "" 
Write-Host "[StubSniffer] Done." -ForegroundColor Green
Write-Host "[StubSniffer] Files scanned: $scanned" -ForegroundColor Green
Write-Host "[StubSniffer] Findings: $($sorted.Count)" -ForegroundColor Green
Write-Host "[StubSniffer] Report: $jsonPath" -ForegroundColor Yellow
Write-Host "[StubSniffer] Report: $csvPath" -ForegroundColor Yellow
Write-Host "" 
Write-Host "[StubSniffer] Findings by type:" -ForegroundColor Cyan
$byType | ForEach-Object { Write-Host ("  {0,-10} {1,6}" -f $_.Name, $_.Count) }
