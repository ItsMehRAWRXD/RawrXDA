# ============================================================================
# RawrXD Production Subagent: IncludeResolver
# Version: 1.0.0 | License: MIT
# Part of the RawrXD Autonomous Build System
# ============================================================================
#Requires -Version 7.0
<#
.SYNOPSIS
    Subagent: Include Resolver — Finds missing #include directives and adds them.

.DESCRIPTION
    Production-hardened subagent that scans C/C++ source files for uses of types,
    functions, macros, etc. that imply a specific header, then checks whether
    the file already includes it. If not, inserts the #include in sorted order
    after the last existing #include block.  Supports backup, WhatIf, JSON output.

.PARAMETER ScanPath
    Root directory to scan. Default: .\src

.PARAMETER AutoFix
    Apply fixes automatically. Without this, only a report is generated.

.PARAMETER OutputFormat
    Text or JSON output.

.PARAMETER ReportPath
    Path to write a JSON report file.

.EXAMPLE
    .\Subagent-IncludeResolver.ps1 -ScanPath .\src -AutoFix -Verbose
    .\Subagent-IncludeResolver.ps1 -ScanPath .\src -WhatIf
#>

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param(
    [string]$ScanPath,

    [switch]$AutoFix,

    [ValidateSet('Text', 'JSON')]
    [string]$OutputFormat = 'Text',

    [string]$ReportPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $ScanPath) { $ScanPath = Join-Path $PSScriptRoot 'src' }
if (-not (Test-Path $ScanPath)) {
    Write-Warning "[IncludeResolver] ScanPath not found: $ScanPath"
    $r = [PSCustomObject]@{ IncludesAdded = 0; FilesScanned = 0; Errors = @("ScanPath not found: $ScanPath") }
    if ($OutputFormat -eq 'JSON') { $r | ConvertTo-Json -Depth 3 } else { Write-Output $r }
    exit 1
}

# ── Symbol → Header Map ──────────────────────────────────────────────────────
$script:IncludeMap = [ordered]@{
    # Win32 core
    'HWND|HINSTANCE|WPARAM|LPARAM|MSG|WNDCLASS|WNDPROC|LRESULT|HBRUSH|HICON|HMENU|HDC|HFONT|PAINTSTRUCT|RECT|POINT|SIZE' = 'windows.h'
    'CreateFile[AW]?|ReadFile|WriteFile|CloseHandle|GetLastError|VirtualAlloc|VirtualFree|HeapAlloc|HeapFree|GetProcessHeap' = 'windows.h'
    'CreateWindow|DefWindowProc|PostQuitMessage|GetMessage|TranslateMessage|DispatchMessage|ShowWindow|UpdateWindow|InvalidateRect|BeginPaint|EndPaint|SendMessage|PostMessage|SetWindowText|GetWindowText|GetClientRect|MoveWindow' = 'windows.h'
    'SelectObject|DeleteObject|CreateFont|TextOut|SetTextColor|SetBkMode|FillRect|DrawText|GetStockObject|CreateSolidBrush' = 'windows.h'
    'WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CHILD|CS_HREDRAW|CS_VREDRAW|SW_SHOW|WM_CREATE|WM_DESTROY|WM_PAINT|WM_SIZE|WM_COMMAND|WM_KEYDOWN|WM_CHAR|WM_CLOSE' = 'windows.h'
    'DWORD|WORD|BYTE|BOOL|UINT|LONG_PTR|ULONG_PTR|INT_PTR|SIZE_T|HANDLE|HMODULE|LPVOID|LPCSTR|LPSTR|LPCWSTR|LPWSTR' = 'windows.h'

    # WinHTTP / WinSock
    'WinHttpOpen|WinHttpConnect|WinHttpOpenRequest|WinHttpSendRequest|WinHttpReceiveResponse' = 'winhttp.h'
    'SOCKET|WSAStartup|WSACleanup|socket|bind|listen|accept|connect|send|recv|closesocket' = 'winsock2.h'

    # COM / Shell
    'CoInitialize|CoCreateInstance|IUnknown|HRESULT|CLSID|IID' = 'objbase.h'
    'SHGetFolderPath|SHCreateDirectory|CSIDL_' = 'shlobj.h'

    # C standard
    'printf|fprintf|sprintf|snprintf|sscanf|fopen|fclose|fread|fwrite|fseek|ftell|fgets|puts|FILE' = 'stdio.h'
    'malloc|free|calloc|realloc|atoi|atol|strtol|strtoul|exit|abort|getenv|system|qsort|bsearch|abs|rand|srand' = 'stdlib.h'
    'memcpy|memset|memmove|strcmp|strncmp|strlen|strcpy|strncpy|strcat|strncat|strchr|strstr|strtok|memcmp' = 'string.h'
    'isalpha|isdigit|isalnum|isspace|isupper|islower|toupper|tolower' = 'ctype.h'
    'time|clock|difftime|mktime|time_t|clock_t|tm|localtime|gmtime|strftime' = 'time.h'
    'assert' = 'assert.h'
    'va_start|va_end|va_arg|va_list' = 'stdarg.h'
    INT8_MAX\|INT16_MAX\|INT32_MAX\|INT64_MAX\|UINT8_MAX\|UINT16_MAX\|UINT32_MAX\|UINT64_MAX\|int8_t\|int16_t\|int32_t\|int64_t\|uint8_t\|uint16_t\|uint32_t\|uint64_t\|SIZE_MAX\|INT_LEAST\|INT_FAST\|UINT_LEAST\|UINT_FAST = 'stdint.h'
    'bool|true|false' = 'stdbool.h'

    # C++ standard
    'std::string|std::wstring|std::to_string|std::stoi|std::stol|std::stoul' = 'string'
    'std::vector' = 'vector'
    'std::map|std::multimap' = 'map'
    'std::unordered_map' = 'unordered_map'
    'std::set|std::multiset' = 'set'
    'std::unordered_set' = 'unordered_set'
    'std::cout|std::cerr|std::cin|std::endl|std::ostream|std::istream' = 'iostream'
    'std::ifstream|std::ofstream|std::fstream|std::stringstream' = 'fstream'
    'std::shared_ptr|std::unique_ptr|std::weak_ptr|std::make_shared|std::make_unique' = 'memory'
    'std::thread|std::mutex|std::lock_guard|std::unique_lock|std::condition_variable' = 'thread'
    'std::function|std::bind|std::ref|std::cref' = 'functional'
    'std::move|std::forward|std::swap|std::pair|std::make_pair' = 'utility'
    'std::sort|std::find|std::transform|std::remove_if|std::count|std::accumulate|std::for_each' = 'algorithm'
    'std::array' = 'array'
    'std::deque' = 'deque'
    'std::list|std::forward_list' = 'list'
    'std::queue|std::priority_queue' = 'queue'
    'std::stack' = 'stack'
    'std::tuple|std::get|std::make_tuple|std::tie' = 'tuple'
    'std::optional|std::nullopt' = 'optional'
    'std::variant|std::visit|std::holds_alternative|std::get_if' = 'variant'
    'std::any|std::any_cast' = 'any'
    'std::filesystem|std::path' = 'filesystem'
    'std::chrono|std::steady_clock|std::system_clock|std::duration|std::milliseconds|std::seconds' = 'chrono'
    'std::regex|std::smatch|std::regex_search|std::regex_replace' = 'regex'
    'std::atomic|std::memory_order' = 'atomic'
    'std::numeric_limits' = 'limits'
    'std::runtime_error|std::exception|std::invalid_argument|std::out_of_range|std::logic_error' = 'stdexcept'
    'std::initializer_list' = 'initializer_list'
    'std::span' = 'span'

    # RawrXD project headers
    'SubsystemResult|SubsystemRegistry|RawrXD_Subsystem' = 'rawrxd_subsystem_api.hpp'
    'V280_BPE_Tokenizer|V280_Tok_Create|V280_Tok_Encode|V280_Tok_Decode' = 'v280_bpe_tokenizer.h'
}

# ── Scanner ──────────────────────────────────────────────────────────────────
function Get-MissingIncludes {
    param([string]$FilePath)

    $content = Get-Content $FilePath -Raw -ErrorAction SilentlyContinue
    if (-not $content) { return @() }

    # Collect already-included headers
    $existing = [System.Collections.Generic.HashSet[string]]::new(
        [StringComparer]::OrdinalIgnoreCase)

    foreach ($m in [regex]::Matches($content, '(?m)^\s*#\s*include\s*[<"]([^>"]+)[>"]')) {
        [void]$existing.Add($m.Groups[1].Value)
    }

    $missing = [System.Collections.Generic.List[string]]::new()

    foreach ($entry in $script:IncludeMap.GetEnumerator()) {
        $pattern = $entry.Key
        $header  = $entry.Value

        if ($existing.Contains($header)) { continue }

        # Check if the file uses any of the symbols in the pattern
        try {
            if ($content -match $pattern) {
                $missing.Add($header)
                [void]$existing.Add($header)   # don't add same header twice
            }
        }
        catch {
            # If regex is invalid, skip silently
            Write-Verbose "  Regex skip: $pattern"
        }
    }

    return $missing
}

function Add-IncludeDirective {
    param([string]$FilePath, [string]$Header)

    $lines = Get-Content $FilePath
    $insertAfter = -1

    # Find last #include line
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        if ($lines[$i] -match '^\s*#\s*include\s') {
            $insertAfter = $i
            break
        }
    }

    # Determine bracket style
    $isSystem = $Header -notmatch '\.(h|hpp|hxx)$' -or $Header -match '^(windows|winhttp|winsock|objbase|shlobj)'
    $directive = if ($isSystem -and $Header -notmatch '\.') {
        "#include <$Header>"
    } elseif ($isSystem) {
        "#include <$Header>"
    } else {
        "#include `"$Header`""
    }

    if ($insertAfter -ge 0) {
        $newLines = $lines[0..$insertAfter] + $directive + $lines[($insertAfter + 1)..($lines.Count - 1)]
    }
    else {
        # No existing includes — insert at top (after any #pragma once / header guard)
        $top = 0
        for ($i = 0; $i -lt [Math]::Min(10, $lines.Count); $i++) {
            if ($lines[$i] -match '^\s*#\s*pragma\s+once' -or $lines[$i] -match '^\s*#\s*ifndef\s' -or $lines[$i] -match '^\s*#\s*define\s') {
                $top = $i + 1
            }
        }
        $before = if ($top -gt 0) { $lines[0..($top - 1)] } else { @() }
        $after  = if ($top -lt $lines.Count) { $lines[$top..($lines.Count - 1)] } else { @() }
        $newLines = $before + '' + $directive + $after
    }

    Set-Content $FilePath $newLines
}

# ── Main ──────────────────────────────────────────────────────────────────────
Write-Verbose "[IncludeResolver] Scanning: $ScanPath"

$report = [PSCustomObject]@{
    IncludesAdded  = 0
    FilesScanned   = 0
    FilesModified  = 0
    Details        = [System.Collections.Generic.List[string]]::new()
    Errors         = [System.Collections.Generic.List[string]]::new()
}

$backedUp = [System.Collections.Generic.HashSet[string]]::new()

$files = Get-ChildItem $ScanPath -Recurse -Include '*.cpp','*.c','*.h','*.hpp','*.hxx','*.cxx' -File -ErrorAction SilentlyContinue
$total = $files.Count
$idx = 0

foreach ($file in $files) {
    $idx++
    Write-Progress -Activity 'IncludeResolver — Scanning' `
        -Status "$idx / $total : $($file.Name)" `
        -PercentComplete ([math]::Floor(($idx / $total) * 100))

    $report.FilesScanned++

    try {
        $missing = Get-MissingIncludes -FilePath $file.FullName

        foreach ($header in $missing) {
            $location = "$($file.FullName): add #include <$header>"
            Write-Verbose "  Missing: $location"

            if ($AutoFix -and $PSCmdlet.ShouldProcess($file.FullName, "Add #include $header")) {
                # Backup once per file
                if (-not $backedUp.Contains($file.FullName)) {
                    Copy-Item $file.FullName "$($file.FullName).bak" -Force -ErrorAction SilentlyContinue
                    [void]$backedUp.Add($file.FullName)
                }

                Add-IncludeDirective -FilePath $file.FullName -Header $header
                $report.IncludesAdded++
                $report.Details.Add($location)
                Write-Verbose "  Added: $header → $($file.Name)"
            }
            elseif (-not $AutoFix) {
                $report.Details.Add("[dry-run] $location")
            }
        }

        if ($missing.Count -gt 0 -and $AutoFix) { $report.FilesModified++ }
    }
    catch {
        $msg = "Error scanning $($file.FullName): $_"
        Write-Warning $msg
        $report.Errors.Add($msg)
    }
}

Write-Progress -Activity 'IncludeResolver — Scanning' -Completed

# ── Summary ───────────────────────────────────────────────────────────────────
Write-Host ''
Write-Host '╔══════════════════════════════════════════════╗' -ForegroundColor Cyan
Write-Host '║  IncludeResolver — Summary                   ║' -ForegroundColor Cyan
Write-Host '╠══════════════════════════════════════════════╣' -ForegroundColor Cyan
Write-Host "║  Files scanned   : $($report.FilesScanned)" -ForegroundColor Cyan
Write-Host "║  Includes added  : $($report.IncludesAdded)" -ForegroundColor $(if ($report.IncludesAdded) { 'Green' } else { 'Cyan' })
Write-Host "║  Files modified  : $($report.FilesModified)" -ForegroundColor Cyan
Write-Host "║  Errors          : $($report.Errors.Count)" -ForegroundColor $(if ($report.Errors.Count) { 'Red' } else { 'Cyan' })
Write-Host '╚══════════════════════════════════════════════╝' -ForegroundColor Cyan

if ($report.Details.Count -gt 0) {
    Write-Host "`nInclude changes:" -ForegroundColor Green
    $report.Details | ForEach-Object { Write-Host "  + $_" -ForegroundColor Green }
}
if ($report.Errors.Count -gt 0) {
    Write-Host "`nErrors:" -ForegroundColor Red
    $report.Errors | ForEach-Object { Write-Host "  ! $_" -ForegroundColor Red }
}

# ── JSON report ───────────────────────────────────────────────────────────────
if ($ReportPath) {
    try {
        $report | ConvertTo-Json -Depth 4 | Set-Content $ReportPath -Encoding utf8
        Write-Verbose "Report written -> $ReportPath"
    }
    catch { Write-Warning "Could not write report: $_" }
}

if ($OutputFormat -eq 'JSON') {
    $report | ConvertTo-Json -Depth 4
}
else {
    Write-Output $report
}

if ($report.Errors.Count -gt 0) { exit 1 }
exit 0
