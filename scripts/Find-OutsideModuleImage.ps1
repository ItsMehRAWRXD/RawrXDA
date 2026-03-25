param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$ModulePath,

    [string]$MapPath,

    [string]$SymbolPath,

    [switch]$SkipPdb,

    [switch]$IncludeNtSymbolPath,

    [Parameter(Mandatory = $true, Position = 1)]
    [string[]]$Address
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:DbgHelpTypeLoadError = $null
$dbgHelpTypeName = 'RawrXD.Debugging.DbgHelpSession'
if (-not ($dbgHelpTypeName -as [type])) {
    try {
        Add-Type -TypeDefinition @"
using System;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace RawrXD.Debugging
{
    public sealed class ResolvedAddress
    {
        public bool SymbolResolved { get; set; }
        public string SymbolName { get; set; }
        public ulong SymbolAddress { get; set; }
        public ulong SymbolDisplacement { get; set; }
        public bool LineResolved { get; set; }
        public string FileName { get; set; }
        public uint LineNumber { get; set; }
        public uint LineDisplacement { get; set; }
        public string Error { get; set; }
    }

    public sealed class DbgHelpSession : IDisposable
    {
        private const uint SYMOPT_UNDNAME = 0x00000002;
        private const uint SYMOPT_DEFERRED_LOADS = 0x00000004;
        private const uint SYMOPT_LOAD_LINES = 0x00000010;
        private const uint SYMOPT_FAIL_CRITICAL_ERRORS = 0x00000200;

        [StructLayout(LayoutKind.Sequential)]
        private struct SYMBOL_INFO
        {
            public uint SizeOfStruct;
            public uint TypeIndex;
            public ulong Reserved1;
            public ulong Reserved2;
            public uint Index;
            public uint Size;
            public ulong ModBase;
            public uint Flags;
            public ulong Value;
            public ulong Address;
            public uint Register;
            public uint Scope;
            public uint Tag;
            public uint NameLen;
            public uint MaxNameLen;
            public byte Name;
        }

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        private struct IMAGEHLP_LINE64
        {
            public uint SizeOfStruct;
            public IntPtr Key;
            public uint LineNumber;
            [MarshalAs(UnmanagedType.LPStr)]
            public string FileName;
            public ulong Address;
        }

        [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
        private static extern bool SymInitialize(IntPtr hProcess, string UserSearchPath, bool fInvadeProcess);

        [DllImport("dbghelp.dll", SetLastError = true)]
        private static extern bool SymCleanup(IntPtr hProcess);

        [DllImport("dbghelp.dll", SetLastError = true)]
        private static extern uint SymSetOptions(uint SymOptions);

        [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
        private static extern ulong SymLoadModuleEx(IntPtr hProcess, IntPtr hFile, string ImageName, string ModuleName, ulong BaseOfDll, uint DllSize, IntPtr Data, uint Flags);

        [DllImport("dbghelp.dll", SetLastError = true)]
        private static extern bool SymFromAddr(IntPtr hProcess, ulong Address, out ulong Displacement, IntPtr Symbol);

        [DllImport("dbghelp.dll", SetLastError = true)]
        private static extern bool SymGetLineFromAddr64(IntPtr hProcess, ulong qwAddr, out uint pdwDisplacement, ref IMAGEHLP_LINE64 Line64);

        private readonly IntPtr _processHandle;
        private bool _initialized;

        public DbgHelpSession(string searchPath)
        {
            _processHandle = Process.GetCurrentProcess().Handle;
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS);
            _initialized = SymInitialize(_processHandle, searchPath, false);
            if (!_initialized)
            {
                InitializationError = "SymInitialize failed with Win32 error " + Marshal.GetLastWin32Error().ToString();
            }
        }

        public bool IsInitialized
        {
            get { return _initialized; }
        }

        public string InitializationError { get; private set; }

        public string ModuleLoadError { get; private set; }

        public bool LoadModule(string modulePath, ulong imageBase, uint sizeOfImage)
        {
            if (!_initialized)
            {
                ModuleLoadError = "DbgHelp session is not initialized.";
                return false;
            }

            ulong loadedBase = SymLoadModuleEx(_processHandle, IntPtr.Zero, modulePath, null, imageBase, sizeOfImage, IntPtr.Zero, 0);
            if (loadedBase == 0)
            {
                ModuleLoadError = "SymLoadModuleEx failed with Win32 error " + Marshal.GetLastWin32Error().ToString();
                return false;
            }

            return true;
        }

        public ResolvedAddress Resolve(ulong address)
        {
            var result = new ResolvedAddress();
            if (!_initialized)
            {
                result.Error = InitializationError ?? "DbgHelp session is not initialized.";
                return result;
            }

            const int maxNameLength = 1024;
            int symbolInfoSize = Marshal.SizeOf(typeof(SYMBOL_INFO)) + maxNameLength;
            IntPtr symbolBuffer = Marshal.AllocHGlobal(symbolInfoSize);

            try
            {
                var symbolInfo = new SYMBOL_INFO();
                symbolInfo.SizeOfStruct = (uint)Marshal.SizeOf(typeof(SYMBOL_INFO));
                symbolInfo.MaxNameLen = maxNameLength;
                Marshal.StructureToPtr(symbolInfo, symbolBuffer, false);

                ulong displacement;
                if (SymFromAddr(_processHandle, address, out displacement, symbolBuffer))
                {
                    symbolInfo = (SYMBOL_INFO)Marshal.PtrToStructure(symbolBuffer, typeof(SYMBOL_INFO));
                    IntPtr namePtr = IntPtr.Add(symbolBuffer, Marshal.OffsetOf(typeof(SYMBOL_INFO), "Name").ToInt32());
                    result.SymbolResolved = true;
                    result.SymbolAddress = symbolInfo.Address;
                    result.SymbolDisplacement = displacement;
                    result.SymbolName = Marshal.PtrToStringAnsi(namePtr, (int)symbolInfo.NameLen);
                }

                IMAGEHLP_LINE64 line = new IMAGEHLP_LINE64();
                line.SizeOfStruct = (uint)Marshal.SizeOf(typeof(IMAGEHLP_LINE64));
                uint lineDisplacement;
                if (SymGetLineFromAddr64(_processHandle, address, out lineDisplacement, ref line))
                {
                    result.LineResolved = true;
                    result.FileName = line.FileName;
                    result.LineNumber = line.LineNumber;
                    result.LineDisplacement = lineDisplacement;
                }

                if (!result.SymbolResolved && !result.LineResolved)
                {
                    result.Error = "No symbol or source line was resolved. Win32 error " + Marshal.GetLastWin32Error().ToString();
                }

                return result;
            }
            finally
            {
                Marshal.FreeHGlobal(symbolBuffer);
            }
        }

        public void Dispose()
        {
            if (_initialized)
            {
                SymCleanup(_processHandle);
                _initialized = false;
            }
        }
    }
}
"@
    }
    catch {
        $script:DbgHelpTypeLoadError = $_.Exception.Message
    }
}

function ConvertTo-UInt64 {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Value
    )

    $trimmed = $Value.Trim()
    if ($trimmed.StartsWith('0x', [System.StringComparison]::OrdinalIgnoreCase)) {
        $trimmed = $trimmed.Substring(2)
    }

    return [Convert]::ToUInt64($trimmed, 16)
}

function Get-DbgHelpLibraryPath {
    $candidates = @(
        (Join-Path $PSScriptRoot 'dbghelp.dll'),
        (Join-Path $env:WINDIR 'System32\dbghelp.dll'),
        (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64\dbghelp.dll'),
        (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64\srcsrv\dbghelp.dll'),
        (Join-Path $env:ProgramFiles 'Windows Kits\10\Debuggers\x64\dbghelp.dll'),
        (Join-Path $env:ProgramFiles 'Windows Kits\10\Debuggers\x64\srcsrv\dbghelp.dll')
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Get-PortableExecutableImageRange {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $file = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::Read)
    try {
        $reader = [System.IO.BinaryReader]::new($file)

        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "Not a valid PE file: $Path"
        }

        $file.Position = 0x3C
        $peHeaderOffset = $reader.ReadUInt32()
        $file.Position = $peHeaderOffset

        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Missing PE signature: $Path"
        }

        $null = $reader.ReadUInt16()
        $numberOfSections = $reader.ReadUInt16()
        $null = $reader.ReadUInt32()
        $null = $reader.ReadUInt32()
        $null = $reader.ReadUInt32()
        $sizeOfOptionalHeader = $reader.ReadUInt16()
        $null = $reader.ReadUInt16()

        $optionalHeaderStart = $file.Position
        $magic = $reader.ReadUInt16()

        switch ($magic) {
            0x20B {
                $file.Position = $optionalHeaderStart + 0x18
                $imageBase = $reader.ReadUInt64()
            }
            0x10B {
                $file.Position = $optionalHeaderStart + 0x1C
                $imageBase = [uint64]$reader.ReadUInt32()
            }
            default {
                throw ("Unsupported PE optional header magic 0x{0:X}" -f $magic)
            }
        }

        $file.Position = $optionalHeaderStart + 0x38
        $sizeOfImage = [uint64]$reader.ReadUInt32()

        if ($sizeOfOptionalHeader -le 0 -or $numberOfSections -le 0) {
            throw "Corrupt PE headers in $Path"
        }

        [pscustomobject]@{
            ImageBase = $imageBase
            SizeOfImage = $sizeOfImage
            ImageEnd = $imageBase + $sizeOfImage - 1
        }
    }
    finally {
        $file.Dispose()
    }
}

function Get-DefaultMapPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedModulePath
    )

    $moduleDirectory = Split-Path -Path $ResolvedModulePath -Parent
    $moduleBaseName = [System.IO.Path]::GetFileNameWithoutExtension($ResolvedModulePath)
    $candidate = Join-Path -Path $moduleDirectory -ChildPath ($moduleBaseName + '.map')

    if (Test-Path -LiteralPath $candidate) {
        return (Resolve-Path -LiteralPath $candidate).Path
    }

    return $null
}

function Get-DefaultSymbolPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedModulePath,

        [Parameter(Mandatory = $true)]
        [bool]$IncludeNtPaths
    )

    $paths = [System.Collections.Generic.List[string]]::new()
    $moduleDirectory = Split-Path -Path $ResolvedModulePath -Parent
    $workingDirectory = (Get-Location).Path

    $baseCandidates = @($moduleDirectory, $workingDirectory, $SymbolPath)
    if ($IncludeNtPaths) {
        $baseCandidates += @($env:_NT_SYMBOL_PATH, $env:_NT_ALT_SYMBOL_PATH)
    }

    foreach ($candidate in $baseCandidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and -not $paths.Contains($candidate)) {
            $paths.Add($candidate)
        }
    }

    return ($paths -join ';')
}

function Get-MapEntries {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedMapPath
    )

    $entries = [System.Collections.Generic.List[object]]::new()
    $pattern = '^[\s]*([0-9A-Fa-f]+):([0-9A-Fa-f]+)\s+(\S+)\s+[0-9A-Fa-f]+\s*(.*)$'

    foreach ($line in [System.IO.File]::ReadLines($ResolvedMapPath)) {
        $match = [System.Text.RegularExpressions.Regex]::Match($line, $pattern)
        if (-not $match.Success) {
            continue
        }

        $symbolName = $match.Groups[3].Value
        if ($symbolName -in @('Address', 'entry', 'Static', 'Program')) {
            continue
        }

        $objectFile = $match.Groups[4].Value.Trim()
        $segment = [Convert]::ToUInt32($match.Groups[1].Value, 16)
        $offset = [Convert]::ToUInt64($match.Groups[2].Value, 16)

        $entries.Add([pscustomobject]@{
            Segment = $segment
            Offset = $offset
            Rva = $offset
            Symbol = $symbolName
            Object = $objectFile
            NextRva = [UInt64]::MaxValue
        })
    }

    $sortedEntries = @($entries | Sort-Object -Property Rva)
    for ($index = 0; $index -lt $sortedEntries.Count - 1; $index++) {
        $sortedEntries[$index].NextRva = $sortedEntries[$index + 1].Rva
    }

    return $sortedEntries
}

function Resolve-MapSymbol {
    param(
        [Parameter(Mandatory = $true)]
        [object[]]$Entries,

        [Parameter(Mandatory = $true)]
        [UInt64]$Rva
    )

    $low = 0
    $high = $Entries.Count - 1
    $bestIndex = -1

    while ($low -le $high) {
        $mid = [int](($low + $high) / 2)
        $midRva = [UInt64]$Entries[$mid].Rva

        if ($midRva -le $Rva) {
            $bestIndex = $mid
            $low = $mid + 1
        }
        else {
            $high = $mid - 1
        }
    }

    if ($bestIndex -lt 0) {
        return $null
    }

    $best = $Entries[$bestIndex]
    while ($best.Symbol -like '$unwind$*' -or $best.Symbol -like '$pdata$*') {
        $bestIndex--
        if ($bestIndex -lt 0) {
            break
        }

        $best = $Entries[$bestIndex]
    }

    if ($bestIndex -lt 0) {
        return $null
    }

    $containsAddress = $Rva -ge $best.Rva -and $Rva -lt $best.NextRva

    [pscustomobject]@{
        Symbol = $best.Symbol
        SymbolRva = $best.Rva
        Displacement = $Rva - $best.Rva
        Segment = $best.Segment
        Object = $best.Object
        ContainsAddress = $containsAddress
        NextRva = $best.NextRva
    }
}

function Emit-Line {
    param(
        [AllowNull()]
        [string]$Text = ''
    )

    Write-Output $Text
}

try {
    $resolvedModulePath = (Resolve-Path -LiteralPath $ModulePath).Path
    $image = Get-PortableExecutableImageRange -Path $resolvedModulePath
    $resolvedMapPath = $null
    $dbgHelpLibraryPath = Get-DbgHelpLibraryPath

    if (-not [string]::IsNullOrWhiteSpace($MapPath)) {
        $resolvedMapPath = (Resolve-Path -LiteralPath $MapPath).Path
    }
    else {
        $resolvedMapPath = Get-DefaultMapPath -ResolvedModulePath $resolvedModulePath
    }

    $resolvedSymbolPath = Get-DefaultSymbolPath -ResolvedModulePath $resolvedModulePath -IncludeNtPaths:$IncludeNtSymbolPath
    $mapEntries = $null
    if ($resolvedMapPath) {
        $mapEntries = Get-MapEntries -ResolvedMapPath $resolvedMapPath
    }

    $dbgHelpSession = $null
    $dbgHelpReady = $false
    $dbgHelpModuleLoaded = $false

    if ($dbgHelpLibraryPath -and -not $SkipPdb -and -not $script:DbgHelpTypeLoadError) {
        $dbgHelpSession = [RawrXD.Debugging.DbgHelpSession]::new($resolvedSymbolPath)
        $dbgHelpReady = $dbgHelpSession.IsInitialized
        if ($dbgHelpReady) {
            $dbgHelpModuleLoaded = $dbgHelpSession.LoadModule($resolvedModulePath, $image.ImageBase, [uint32]$image.SizeOfImage)
        }
    }

    Emit-Line ("Module      : {0}" -f $resolvedModulePath)
    Emit-Line ("Image Base  : 0x{0:X16}" -f $image.ImageBase)
    Emit-Line ("Image End   : 0x{0:X16}" -f $image.ImageEnd)
    Emit-Line ("Image Size  : 0x{0:X}" -f $image.SizeOfImage)
    Emit-Line ("Symbol Path : {0}" -f $resolvedSymbolPath)
    Emit-Line ("DbgHelp     : {0}" -f $(if ($dbgHelpLibraryPath) { $dbgHelpLibraryPath } else { '<not found>' }))
    Emit-Line ("PDB Mode    : {0}" -f $(if ($SkipPdb) { 'disabled (-SkipPdb)' } elseif ($IncludeNtSymbolPath) { 'enabled (+NT symbol paths)' } else { 'enabled (local symbol paths only)' }))

    if ($resolvedMapPath) {
        Emit-Line ("Map File    : {0}" -f $resolvedMapPath)
    }
    else {
        Emit-Line 'Map File    : <not found>'
    }

    if ($SkipPdb) {
        Emit-Line 'PDB Status  : skipped by request'
    }
    elseif ($script:DbgHelpTypeLoadError) {
        Emit-Line ("PDB Status  : unavailable (dbghelp wrapper type load failed: {0})" -f $script:DbgHelpTypeLoadError)
    }
    elseif (-not $dbgHelpLibraryPath) {
        Emit-Line 'PDB Status  : unavailable (dbghelp.dll not found in script/system/SDK paths)'
    }
    elseif ($dbgHelpReady -and $dbgHelpModuleLoaded) {
        Emit-Line 'PDB Status  : symbol engine initialized'
    }
    elseif ($dbgHelpReady) {
        Emit-Line ("PDB Status  : symbol engine initialized, module load failed ({0})" -f $dbgHelpSession.ModuleLoadError)
    }
    else {
        Emit-Line ("PDB Status  : unavailable ({0})" -f $dbgHelpSession.InitializationError)
    }

    $dbgHelpErrorReported = $false
    try {
        foreach ($rawAddress in $Address) {
            $addressValue = ConvertTo-UInt64 -Value $rawAddress
            $isInside = $addressValue -ge $image.ImageBase -and $addressValue -le $image.ImageEnd

            Emit-Line ''
            Emit-Line ("Address     : 0x{0:X16}" -f $addressValue)

            if (-not $isInside) {
                Emit-Line 'Result      : That address is also outside the module image'
                continue
            }

            $rva = $addressValue - $image.ImageBase
            Emit-Line ("Result      : inside the module image (RVA 0x{0:X})" -f $rva)

            $resolved = $null
            if ($dbgHelpReady -and $dbgHelpModuleLoaded) {
                $resolved = $dbgHelpSession.Resolve($addressValue)
                if ($resolved.SymbolResolved) {
                    Emit-Line ("PDB Symbol  : {0} +0x{1:X}" -f $resolved.SymbolName, $resolved.SymbolDisplacement)
                }

                if ($resolved.LineResolved) {
                    Emit-Line ("Source      : {0}:{1}" -f $resolved.FileName, $resolved.LineNumber)
                    if ($resolved.LineDisplacement -gt 0) {
                        Emit-Line ("Line Disp   : 0x{0:X}" -f $resolved.LineDisplacement)
                    }
                }

                if (-not $resolved.SymbolResolved -and -not $resolved.LineResolved -and -not $dbgHelpErrorReported) {
                    Emit-Line ("PDB Note    : {0}" -f $resolved.Error)
                    $dbgHelpErrorReported = $true
                }
            }

            if ($mapEntries) {
                $mapResolved = Resolve-MapSymbol -Entries $mapEntries -Rva $rva
                if ($mapResolved) {
                    Emit-Line ("Map Symbol  : {0} +0x{1:X}" -f $mapResolved.Symbol, $mapResolved.Displacement)
                    if (-not [string]::IsNullOrWhiteSpace($mapResolved.Object)) {
                        Emit-Line ("Map Object  : {0}" -f $mapResolved.Object)
                    }
                    Emit-Line ("Map Match   : {0}" -f $(if ($mapResolved.ContainsAddress) { 'contained' } else { 'nearest-lower' }))
                }
            }

            if (($null -eq $resolved -or (-not $resolved.LineResolved)) -and -not $mapEntries) {
                Emit-Line 'Source      : unavailable (no PDB line info and no map file fallback)'
            }
        }
    }
    finally {
        if ($dbgHelpSession) {
            $dbgHelpSession.Dispose()
        }
    }
}
catch {
    Emit-Line ("Fatal       : {0}" -f $_.Exception.Message)
    Emit-Line ("Type        : {0}" -f $_.Exception.GetType().FullName)
    if ($_.ScriptStackTrace) {
        Emit-Line ("Stack       : {0}" -f $_.ScriptStackTrace)
    }
    exit 1
}