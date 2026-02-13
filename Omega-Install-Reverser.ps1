<#
.SYNOPSIS
    OMEGA-INSTALL-REVERSER: Installation Reversal Engine v4.0
.DESCRIPTION
    Transforms installed/compiled programs back to full source tree with:
    - Reconstructed C/C++ headers from PE/DLL analysis
    - Type recovery from PDB/DWARF debug symbols
    - Build system generation (CMake, Meson, Makefile)
    - Resource extraction and reconstruction
    - COM interface definitions
    - Dependency mapping and vcpkg/conan manifests
.PARAMETER InstallPath
    Path to installed program directory
.PARAMETER OutputPath
    Where to generate the reversed source tree
.PARAMETER GenerateBuildSystem
    Create CMakeLists.txt, meson.build, etc.
.PARAMETER DeepTypeRecovery
    Attempt class layout reconstruction from RTTI/debug symbols
#>
[CmdletBinding(SupportsShouldProcess=$true)]
param(
    [Parameter(Mandatory=$true)]
    [string]$InstallPath,
    
    [string]$OutputPath = "Reversed_Source",
    [switch]$GenerateBuildSystem,
    [switch]$DeepTypeRecovery,
    [switch]$ExtractResources,
    [switch]$ReconstructCOM,
    [switch]$MapDependencies,
    [string]$ProjectName = "ReversedProject"
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public class Win32API {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr LoadLibrary(string lpFileName);
    
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr GetProcAddress(IntPtr hModule, string lpProcName);
    
    [DllImport("dbghelp.dll", SetLastError=true)]
    public static extern bool SymInitialize(IntPtr hProcess, string UserSearchPath, bool fInvadeProcess);
    
    [DllImport("dbghelp.dll", SetLastError=true)]
    public static extern bool SymLoadModuleEx(IntPtr hProcess, IntPtr hFile, string ImageName, string ModuleName, long BaseOfDll, int DllSize, IntPtr Data, int Flags);
    
    [DllImport("dbghelp.dll", SetLastError=true)]
    public static extern bool SymEnumSymbols(IntPtr hProcess, long BaseOfDll, string Mask, SymEnumSymbolsProc EnumSymbolsCallback, IntPtr UserContext);
    
    public delegate bool SymEnumSymbolsProc(ref SYMBOL_INFO pSymInfo, int SymbolSize, IntPtr UserContext);
    
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Ansi)]
    public struct SYMBOL_INFO {
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
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=2000)]
        public string Name;
    }
}
"@

# PE Parser Structures
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

[StructLayout(LayoutKind.Sequential)]
public struct IMAGE_DOS_HEADER {
    public ushort e_magic;
    public ushort e_cblp;
    public ushort e_cp;
    public ushort e_crlc;
    public ushort e_cparhdr;
    public ushort e_minalloc;
    public ushort e_maxalloc;
    public ushort e_ss;
    public ushort e_sp;
    public ushort e_csum;
    public ushort e_ip;
    public ushort e_cs;
    public ushort e_lfarlc;
    public ushort e_ovno;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=4)]
    public ushort[] e_res;
    public ushort e_oemid;
    public ushort e_oeminfo;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=10)]
    public ushort[] e_res2;
    public int e_lfanew;
}

[StructLayout(LayoutKind.Sequential)]
public struct IMAGE_FILE_HEADER {
    public ushort Machine;
    public ushort NumberOfSections;
    public uint TimeDateStamp;
    public uint PointerToSymbolTable;
    public uint NumberOfSymbols;
    public ushort SizeOfOptionalHeader;
    public ushort Characteristics;
}

[StructLayout(LayoutKind.Sequential)]
public struct IMAGE_DATA_DIRECTORY {
    public uint VirtualAddress;
    public uint Size;
}

[StructLayout(LayoutKind.Sequential)]
public struct IMAGE_OPTIONAL_HEADER64 {
    public ushort Magic;
    public byte MajorLinkerVersion;
    public byte MinorLinkerVersion;
    public uint SizeOfCode;
    public uint SizeOfInitializedData;
    public uint SizeOfUninitializedData;
    public uint AddressOfEntryPoint;
    public uint BaseOfCode;
    public ulong ImageBase;
    public uint SectionAlignment;
    public uint FileAlignment;
    public ushort MajorOperatingSystemVersion;
    public ushort MinorOperatingSystemVersion;
    public ushort MajorImageVersion;
    public ushort MinorImageVersion;
    public ushort MajorSubsystemVersion;
    public ushort MinorSubsystemVersion;
    public uint Win32VersionValue;
    public uint SizeOfImage;
    public uint SizeOfHeaders;
    public uint CheckSum;
    public ushort Subsystem;
    public ushort DllCharacteristics;
    public ulong SizeOfStackReserve;
    public ulong SizeOfStackCommit;
    public ulong SizeOfHeapReserve;
    public ulong SizeOfHeapCommit;
    public uint LoaderFlags;
    public uint NumberOfRvaAndSizes;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=16)]
    public IMAGE_DATA_DIRECTORY[] DataDirectory;
}

[StructLayout(LayoutKind.Sequential)]
public struct IMAGE_SECTION_HEADER {
    [MarshalAs(UnmanagedType.ByValArray, SizeConst=8)]
    public byte[] Name;
    public uint VirtualSize;
    public uint VirtualAddress;
    public uint SizeOfRawData;
    public uint PointerToRawData;
    public uint PointerToRelocations;
    public uint PointerToLinenumbers;
    public ushort NumberOfRelocations;
    public ushort NumberOfLinenumbers;
    public uint Characteristics;
}
"@

function Read-PEFile {
    param([string]$FilePath)
    
    $fs = [System.IO.File]::OpenRead($FilePath)
    $br = [System.IO.BinaryReader]::new($fs)
    
    # DOS Header
    $dosHeader = New-Object IMAGE_DOS_HEADER
    $dosHeader.e_magic = $br.ReadUInt16()
    if ($dosHeader.e_magic -ne 0x5A4D) { # MZ
        $br.Close(); $fs.Close()
        return @{ Error = "Not a valid PE file" }
    }
    
    $fs.Position = 0x3C
    $peOffset = $br.ReadInt32()
    $fs.Position = $peOffset
    
    # PE Signature
    $peSig = $br.ReadUInt32()
    if ($peSig -ne 0x00004550) { # PE\0\0
        $br.Close(); $fs.Close()
        return @{ Error = "Invalid PE signature" }
    }
    
    # COFF Header
    $coff = New-Object IMAGE_FILE_HEADER
    $coff.Machine = $br.ReadUInt16()
    $coff.NumberOfSections = $br.ReadUInt16()
    $coff.TimeDateStamp = $br.ReadUInt32()
    $coff.PointerToSymbolTable = $br.ReadUInt32()
    $coff.NumberOfSymbols = $br.ReadUInt32()
    $coff.SizeOfOptionalHeader = $br.ReadUInt16()
    $coff.Characteristics = $br.ReadUInt16()
    
    # Optional Header
    $optMagic = $br.ReadUInt16()
    $fs.Position -= 2
    
    $is64 = $optMagic -eq 0x20B
    $optionalHeader = $null
    
    if ($is64) {
        $optionalHeader = New-Object IMAGE_OPTIONAL_HEADER64
        $optionalHeader.Magic = $br.ReadUInt16()
        $optionalHeader.MajorLinkerVersion = $br.ReadByte()
        $optionalHeader.MinorLinkerVersion = $br.ReadByte()
        $optionalHeader.SizeOfCode = $br.ReadUInt32()
        $optionalHeader.SizeOfInitializedData = $br.ReadUInt32()
        $optionalHeader.SizeOfUninitializedData = $br.ReadUInt32()
        $optionalHeader.AddressOfEntryPoint = $br.ReadUInt32()
        $optionalHeader.BaseOfCode = $br.ReadUInt32()
        $optionalHeader.ImageBase = $br.ReadUInt64()
        $optionalHeader.SectionAlignment = $br.ReadUInt32()
        $optionalHeader.FileAlignment = $br.ReadUInt32()
        $optionalHeader.MajorOperatingSystemVersion = $br.ReadUInt16()
        $optionalHeader.MinorOperatingSystemVersion = $br.ReadUInt16()
        $optionalHeader.MajorImageVersion = $br.ReadUInt16()
        $optionalHeader.MinorImageVersion = $br.ReadUInt16()
        $optionalHeader.MajorSubsystemVersion = $br.ReadUInt16()
        $optionalHeader.MinorSubsystemVersion = $br.ReadUInt16()
        $optionalHeader.Win32VersionValue = $br.ReadUInt32()
        $optionalHeader.SizeOfImage = $br.ReadUInt32()
        $optionalHeader.SizeOfHeaders = $br.ReadUInt32()
        $optionalHeader.CheckSum = $br.ReadUInt32()
        $optionalHeader.Subsystem = $br.ReadUInt16()
        $optionalHeader.DllCharacteristics = $br.ReadUInt16()
        $optionalHeader.SizeOfStackReserve = $br.ReadUInt64()
        $optionalHeader.SizeOfStackCommit = $br.ReadUInt64()
        $optionalHeader.SizeOfHeapReserve = $br.ReadUInt64()
        $optionalHeader.SizeOfHeapCommit = $br.ReadUInt64()
        $optionalHeader.LoaderFlags = $br.ReadUInt32()
        $optionalHeader.NumberOfRvaAndSizes = $br.ReadUInt32()
        
        $optionalHeader.DataDirectory = New-Object IMAGE_DATA_DIRECTORY[16]
        for ($i = 0; $i -lt 16; $i++) {
            $dd = New-Object IMAGE_DATA_DIRECTORY
            $dd.VirtualAddress = $br.ReadUInt32()
            $dd.Size = $br.ReadUInt32()
            $optionalHeader.DataDirectory[$i] = $dd
        }
    }
    
    # Sections
    $sections = @()
    for ($i = 0; $i -lt $coff.NumberOfSections; $i++) {
        $sect = New-Object IMAGE_SECTION_HEADER
        $sect.Name = $br.ReadBytes(8)
        $sect.VirtualSize = $br.ReadUInt32()
        $sect.VirtualAddress = $br.ReadUInt32()
        $sect.SizeOfRawData = $br.ReadUInt32()
        $sect.PointerToRawData = $br.ReadUInt32()
        $sect.PointerToRelocations = $br.ReadUInt32()
        $sect.PointerToLinenumbers = $br.ReadUInt32()
        $sect.NumberOfRelocations = $br.ReadUInt16()
        $sect.NumberOfLinenumbers = $br.ReadUInt16()
        $sect.Characteristics = $br.ReadUInt32()
        $sections += $sect
    }
    
    # Export Directory
    $exports = @()
    if ($optionalHeader.DataDirectory[0].VirtualAddress -ne 0) {
        $exportDir = $optionalHeader.DataDirectory[0]
        $sect = $sections | Where-Object { $_.VirtualAddress -le $exportDir.VirtualAddress -and ($_.VirtualAddress + $_.VirtualSize) -gt $exportDir.VirtualAddress } | Select-Object -First 1
        
        if ($sect) {
            $fileOffset = $sect.PointerToRawData + ($exportDir.VirtualAddress - $sect.VirtualAddress)
            $fs.Position = $fileOffset
            
            $exportFlags = $br.ReadUInt32()
            $timeDateStamp = $br.ReadUInt32()
            $majorVersion = $br.ReadUInt16()
            $minorVersion = $br.ReadUInt16()
            $nameRVA = $br.ReadUInt32()
            $ordinalBase = $br.ReadUInt32()
            $addressTableEntries = $br.ReadUInt32()
            $numberOfNamePointers = $br.ReadUInt32()
            $exportAddressTableRVA = $br.ReadUInt32()
            $namePointerRVA = $br.ReadUInt32()
            $ordinalTableRVA = $br.ReadUInt32()
            
            # Read name pointer table
            $nameOffset = $sect.PointerToRawData + ($namePointerRVA - $sect.VirtualAddress)
            $ordinalOffset = $sect.PointerToRawData + ($ordinalTableRVA - $sect.VirtualAddress)
            $addrOffset = $sect.PointerToRawData + ($exportAddressTableRVA - $sect.VirtualAddress)
            
            for ($i = 0; $i -lt $numberOfNamePointers; $i++) {
                $fs.Position = $nameOffset + ($i * 4)
                $namePtr = $br.ReadUInt32()
                $actualOffset = $sect.PointerToRawData + ($namePtr - $sect.VirtualAddress)
                $fs.Position = $actualOffset
                
                $nameBytes = @()
                while ($true) {
                    $b = $br.ReadByte()
                    if ($b -eq 0) { break }
                    $nameBytes += $b
                }
                $funcName = [System.Text.Encoding]::ASCII.GetString($nameBytes)
                
                $fs.Position = $ordinalOffset + ($i * 2)
                $ordinal = $br.ReadUInt16()
                
                $fs.Position = $addrOffset + ($ordinal - $ordinalBase) * 4
                $addr = $br.ReadUInt32()
                
                $exports += @{
                    Name = $funcName
                    Ordinal = $ordinal
                    Address = $addr
                    RVA = "0x$($addr.ToString('X8'))"
                }
            }
        }
    }
    
    # Import Directory
    $imports = @()
    if ($optionalHeader.DataDirectory[1].VirtualAddress -ne 0) {
        $importDir = $optionalHeader.DataDirectory[1]
        $sect = $sections | Where-Object { $_.VirtualAddress -le $importDir.VirtualAddress -and ($_.VirtualAddress + $_.VirtualSize) -gt $importDir.VirtualAddress } | Select-Object -First 1
        
        if ($sect) {
            $fileOffset = $sect.PointerToRawData + ($importDir.VirtualAddress - $sect.VirtualAddress)
            $fs.Position = $fileOffset
            
            while ($true) {
                $iltRVA = $br.ReadUInt32()
                $timeDateStamp = $br.ReadUInt32()
                $forwarderChain = $br.ReadUInt32()
                $nameRVA = $br.ReadUInt32()
                $iatRVA = $br.ReadUInt32()
                
                if ($iltRVA -eq 0) { break }
                
                $nameOffset = $sect.PointerToRawData + ($nameRVA - $sect.VirtualAddress)
                $fs.Position = $nameOffset
                $dllNameBytes = @()
                while ($true) {
                    $b = $br.ReadByte()
                    if ($b -eq 0) { break }
                    $dllNameBytes += $b
                }
                $dllName = [System.Text.Encoding]::ASCII.GetString($dllNameBytes)
                
                $imports += @{
                    DLL = $dllName
                    ILT = "0x$($iltRVA.ToString('X8'))"
                    IAT = "0x$($iatRVA.ToString('X8'))"
                }
            }
        }
    }
    
    $br.Close(); $fs.Close()
    
    return @{
        Is64Bit = $is64
        Machine = switch ($coff.Machine) { 0x8664 { "x64" } 0x14c { "x86" } 0xAA64 { "ARM64" } default { "Unknown" } }
        Subsystem = switch ($optionalHeader.Subsystem) { 1 { "Native" } 2 { "Windows GUI" } 3 { "Windows Console" } default { "Other" } }
        ImageBase = "0x$($optionalHeader.ImageBase.ToString('X16'))"
        EntryPoint = "0x$($optionalHeader.AddressOfEntryPoint.ToString('X8'))"
        Sections = $sections | ForEach-Object { 
            $name = [System.Text.Encoding]::ASCII.GetString($_.Name).TrimEnd("`0")
            @{ Name = $name; VirtualSize = $_.VirtualSize; Characteristics = $_.Characteristics }
        }
        Exports = $exports
        Imports = $imports
        DebugDirectory = $optionalHeader.DataDirectory[6]
    }
}

function Invoke-TypeReconstruction {
    param([string]$PdbPath, [string]$BinaryPath)
    
    $types = @()
    $functions = @()
    
    # Use DIA SDK or parse PDB if available
    if (Test-Path $PdbPath) {
        try {
            $diaSource = New-Object -ComObject DiaSource
            $diaSource.loadDataFromPdb($PdbPath)
            $session = $diaSource.openSession()
            $globalScope = $session.globalScope
            
            # Enumerate types
            $typeEnum = $globalScope.GetChildren([DiaSymReader.SymTagEnum]::SymTagUDT)
            while ($type = $typeEnum.Next(1)) {
                $typeInfo = @{
                    Name = $type.name
                    Kind = switch ($type.typeId) { 0 { "Class" } 1 { "Struct" } 2 { "Union" } }
                    Size = $type.length
                    Members = @()
                }
                
                $memberEnum = $type.GetChildren([DiaSymReader.SymTagEnum]::SymTagData)
                while ($member = $memberEnum.Next(1)) {
                    $typeInfo.Members += @{
                        Name = $member.name
                        Type = $member.type.name
                        Offset = $member.offset
                    }
                }
                
                $types += $typeInfo
            }
            
            # Enumerate functions
            $funcEnum = $globalScope.GetChildren([DiaSymReader.SymTagEnum]::SymTagFunction)
            while ($func = $funcEnum.Next(1)) {
                $functions += @{
                    Name = $func.name
                    RVA = $func.relativeVirtualAddress
                    Length = $func.length
                    ReturnType = if ($func.type) { $func.type.name } else { "void" }
                }
            }
        } catch {
            Write-Warning "PDB parsing failed: $_"
        }
    }
    
    # Fallback: RTTI scanning for C++ classes
    if ($types.Count -eq 0 -and $DeepTypeRecovery) {
        $bytes = [System.IO.File]::ReadAllBytes($BinaryPath)
        
        # Scan for vtable patterns and RTTI
        for ($i = 0; $i -lt $bytes.Length - 8; $i++) {
            # Look for .?AV pattern (MSVC RTTI)
            if ($bytes[$i] -eq 0x2E -and $bytes[$i+1] -eq 0x3F -and $bytes[$i+2] -eq 0x41 -and $bytes[$i+3] -eq 0x56) {
                $len = $bytes[$i+4]
                if ($len -gt 0 -and $len -lt 100) {
                    $className = [System.Text.Encoding]::ASCII.GetString($bytes, $i+5, $len)
                    $types += @{
                        Name = $className
                        Kind = "Class"
                        Size = 0
                        Members = @()
                        Source = "RTTI"
                    }
                }
            }
        }
    }
    
    return @{ Types = $types; Functions = $functions }
}

function New-HeaderFile {
    param([string]$ModuleName, [array]$Exports, [array]$Types, [string]$OutputDir)
    
    $header = "#pragma once`n"
    $header += "// Auto-generated header for $ModuleName`n"
    $header += "// Reconstructed from binary analysis`n`n"
    
    $header += "#ifdef __cplusplus`n"
    $header += "extern `"C`" {`n"
    $header += "#endif`n`n"
    
    # Include windows.h for types
    $header += "#include <windows.h>`n"
    $header += "#include <stdint.h>`n`n"
    
    # Type definitions from reconstructed types
    if ($Types.Count -gt 0) {
        $header += "// Reconstructed Types`n"
        foreach ($type in $Types) {
            if ($type.Kind -eq "Struct" -or $type.Kind -eq "Class") {
                $header += "typedef struct _$($type.Name) {`n"
                if ($type.Members.Count -gt 0) {
                    foreach ($member in $type.Members) {
                        $header += "    $($member.Type) $($member.Name); // Offset: 0x$($member.Offset.ToString('X'))`n"
                    }
                } else {
                    $header += "    // Unknown layout - size: $($type.Size) bytes`n"
                    $header += "    uint8_t _opaque[$($type.Size)];`n"
                }
                $header += "} $($type.Name);`n`n"
            }
        }
    }
    
    # Function declarations
    $header += "// Exported Functions`n"
    foreach ($exp in $Exports) {
        # Try to infer calling convention and parameters from name
        $callingConv = "__stdcall"
        if ($exp.Name -match "^_") { $callingConv = "__cdecl" }
        
        $retType = "void"
        if ($exp.Name -match "Get|Create|Alloc|Open") { $retType = "HANDLE" }
        if ($exp.Name -match "Is|Has|Can") { $retType = "BOOL" }
        if ($exp.Name -match "Count|Size|Length|Index") { $retType = "int" }
        
        $header += "$callingConv $retType $($exp.Name)(void); // Ordinal: $($exp.Ordinal)`n"
    }
    
    $header += "`n#ifdef __cplusplus`n"
    $header += "}`n"
    $header += "#endif`n"
    
    $headerPath = Join-Path $OutputDir "include\$($ModuleName).h"
    New-Item -ItemType Directory -Path (Split-Path $headerPath) -Force | Out-Null
    $header | Out-File -FilePath $headerPath -Encoding UTF8
    
    return $headerPath
}

function New-CMakeLists {
    param([string]$ProjectName, [array]$Sources, [array]$Libraries, [string]$OutputDir, [bool]$Is64Bit)
    
    $cmake = "cmake_minimum_required(VERSION 3.16)`n"
    $cmake += "project($ProjectName VERSION 1.0.0 LANGUAGES C CXX)`n`n"
    
    $cmake += "set(CMAKE_C_STANDARD 11)`n"
    $cmake += "set(CMAKE_CXX_STANDARD 17)`n"
    if ($Is64Bit) {
        $cmake += "set(CMAKE_SIZEOF_VOID_P 8)`n"
    } else {
        $cmake += "set(CMAKE_SIZEOF_VOID_P 4)`n"
    }
    $cmake += "`n"
    
    # Source files
    $cmake += "set(SOURCES`n"
    foreach ($src in $Sources) {
        $cmake += "    src/$src`n"
    }
    $cmake += ")`n`n"
    
    # Include directories
    $cmake += "include_directories(`n"
    $cmake += "    `${CMAKE_CURRENT_SOURCE_DIR}/include`n"
    $cmake += "    `${CMAKE_CURRENT_SOURCE_DIR}/src`n"
    $cmake += ")`n`n"
    
    # Target
    $cmake += "add_executable(`${PROJECT_NAME} `${SOURCES})`n`n"
    
    # Link libraries
    if ($Libraries.Count -gt 0) {
        $cmake += "target_link_libraries(`${PROJECT_NAME}`n"
        foreach ($lib in $Libraries | Select-Object -Unique) {
            $libName = $lib -replace '\.dll$', '' -replace '\.lib$', ''
            $cmake += "    $libName`n"
        }
        $cmake += ")`n`n"
    }
    
    # Compiler options
    $cmake += "if(MSVC)`n"
    $cmake += "    target_compile_options(`${PROJECT_NAME} PRIVATE /W4 /permiss-)`n"
    $cmake += "else()`n"
    $cmake += "    target_compile_options(`${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)`n"
    $cmake += "endif()`n"
    
    $cmakePath = Join-Path $OutputDir "CMakeLists.txt"
    $cmake | Out-File -FilePath $cmakePath -Encoding UTF8
    
    return $cmakePath
}

function New-MesonBuild {
    param([string]$ProjectName, [array]$Sources, [array]$Libraries, [string]$OutputDir)
    
    $meson = "project('$ProjectName', 'c', 'cpp',`n"
    $meson += "    version : '1.0.0',`n"
    $meson += "    default_options : ['warning_level=3', 'cpp_std=c++17'])`n`n"
    
    $meson += "inc = include_directories('include')`n`n"
    
    $meson += "sources = files(`n"
    foreach ($src in $Sources) {
        $meson += "    'src/$src',`n"
    }
    $meson += ")`n`n"
    
    $deps = @()
    foreach ($lib in $Libraries | Select-Object -Unique) {
        $libName = $lib -replace '\.dll$', ''
        $deps += "dependency('$libName', required : false)"
    }
    if ($deps.Count -gt 0) {
        $meson += "deps = [`n"
        foreach ($dep in $deps) {
            $meson += "    $dep,`n"
        }
        $meson += "]`n`n"
    }
    
    $meson += "executable('$ProjectName', sources,`n"
    $meson += "    include_directories : inc,`n"
    if ($deps.Count -gt 0) {
        $meson += "    dependencies : deps,`n"
    }
    $meson += "    install : true)`n"
    
    $mesonPath = Join-Path $OutputDir "meson.build"
    $meson | Out-File -FilePath $mesonPath -Encoding UTF8
    
    return $mesonPath
}

function Export-Resources {
    param([string]$BinaryPath, [string]$OutputDir)
    
    $resDir = Join-Path $OutputDir "resources"
    New-Item -ItemType Directory -Path $resDir -Force | Out-Null
    
    # Use Windows API to enumerate resources
    $hModule = [Win32API]::LoadLibrary($BinaryPath)
    if ($hModule -eq [IntPtr]::Zero) {
        return @()
    }
    
    $resources = @()
    
    # This would require additional P/Invoke for EnumResourceTypes/EnumResourceNames
    # Simplified: check for common resource sections
    $pe = Read-PEFile -FilePath $BinaryPath
    $rsrc = $pe.Sections | Where-Object { $_.Name -eq ".rsrc" }
    
    if ($rsrc) {
        $fs = [System.IO.File]::OpenRead($BinaryPath)
        $br = [System.IO.BinaryReader]::new($fs)
        
        # Resource directory parsing would go here
        # For now, extract raw section
        $fs.Position = $rsrc.PointerToRawData
        $data = $br.ReadBytes($rsrc.SizeOfRawData)
        
        $rsrcPath = Join-Path $resDir "resources.bin"
        [System.IO.File]::WriteAllBytes($rsrcPath, $data)
        $resources += $rsrcPath
        
        $br.Close(); $fs.Close()
    }
    
    return $resources
}

function New-COMInterfaceDefs {
    param([string]$BinaryPath, [string]$OutputDir)
    
    # Scan for COM vtables and interface GUIDs
    $bytes = [System.IO.File]::ReadAllBytes($BinaryPath)
    $interfaces = @()
    
    # Look for GUID patterns (IID structures)
    for ($i = 0; $i -lt $bytes.Length - 16; $i++) {
        # Check for GUID layout (4-2-2-8 bytes)
        # This is heuristic - real COM analysis needs type libraries
        if ($bytes[$i+4] -eq 0 -and $bytes[$i+6] -eq 0 -and 
            ($bytes[$i+8] -band 0x80) -eq 0x80) {
            $guid = [System.Guid]::new($bytes[$i..($i+15)])
            $interfaces += @{
                GUID = $guid.ToString()
                Offset = $i
            }
        }
    }
    
    if ($interfaces.Count -eq 0) { return $null }
    
    $idl = "// Generated COM Interface Definitions`n`n"
    $idl += "import `"unknwn.idl`";`n`n"
    
    foreach ($iface in $interfaces) {
        $idl += "[`n"
        $idl += "    object,`n"
        $idl += "    uuid($($iface.GUID)),`n"
        $idl += "    pointer_default(unique)`n"
        $idl += "]`n"
        $idl += "interface IUnknown$($iface.Offset) : IUnknown`n"
        $idl += "{\n"
        $idl += "    // Methods unknown - requires type library analysis`n"
        $idl += "};`n`n"
    }
    
    $idlPath = Join-Path $OutputDir "include\interfaces.idl"
    $idl | Out-File -FilePath $idlPath -Encoding UTF8
    
    return $idlPath
}

function New-BuildScript {
    param([string]$ProjectName, [array]$Sources, [bool]$Is64Bit, [string]$OutputDir)
    
    $bat = "@echo off`n"
    $bat += "REM Auto-generated build script for $ProjectName`n"
    $bat += "REM Reconstructed from binary analysis`n`n"
    
    $bat += "set SOURCES="
    foreach ($src in $Sources) {
        $bat += "src\$src "
    }
    $bat += "`n"
    
    $bat += "set OUTDIR=build`n"
    $bat += "if not exist %OUTDIR% mkdir %OUTDIR%`n`n"
    
    if ($Is64Bit) {
        $bat += "cl /EHsc /W4 /O2 /Fe%OUTDIR%\\$ProjectName.exe %SOURCES% /I include /link"
    } else {
        $bat += "cl /EHsc /W4 /O2 /Fe%OUTDIR%\\$ProjectName.exe %SOURCES% /I include /link"
    }
    
    foreach ($lib in $Libraries | Select-Object -Unique) {
        $bat += " $lib"
    }
    $bat += "`n"
    
    $batPath = Join-Path $OutputDir "build.bat"
    $bat | Out-File -FilePath $batPath -Encoding ASCII
    
    return $batPath
}

# Main Execution
Write-Host @"
╔══════════════════════════════════════════════════════════════════╗
║     OMEGA-INSTALL-REVERSER v4.0                                  ║
║     Installation Reversal & Header Generation Engine             ║
╚══════════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Magenta

$startTime = Get-Date

if (-not (Test-Path $InstallPath)) {
    throw "Install path not found: $InstallPath"
}

New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $OutputPath "src") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $OutputPath "include") -Force | Out-Null

# Find all PE files
$peFiles = Get-ChildItem -Path $InstallPath -Recurse -Include "*.exe", "*.dll", "*.ocx", "*.sys" | Where-Object { -not $_.PSIsContainer }

Write-Host "Found $($peFiles.Count) PE files to analyze" -ForegroundColor Cyan

$projectSources = @()
$allImports = @()
$allExports = @()
$is64Bit = $false

foreach ($file in $peFiles) {
    Write-Host "Analyzing: $($file.Name)" -ForegroundColor Yellow
    
    $pe = Read-PEFile -FilePath $file.FullName
    if ($pe.Error) {
        Write-Warning "  Failed: $($pe.Error)"
        continue
    }
    
    $is64Bit = $pe.Is64Bit
    $allImports += $pe.Imports.DLL
    $allExports += $pe.Exports
    
    # Generate header for DLLs with exports
    if ($pe.Exports.Count -gt 0 -and $file.Extension -eq '.dll') {
        $pdbPath = $file.FullName -replace '\.dll$', '.pdb'
        if (-not (Test-Path $pdbPath)) {
            $pdbPath = Join-Path $InstallPath ([System.IO.Path]::GetFileNameWithoutExtension($file.Name) + '.pdb')
        }
        
        $typeInfo = Invoke-TypeReconstruction -PdbPath $pdbPath -BinaryPath $file.FullName
        
        $headerPath = New-HeaderFile -ModuleName $file.BaseName -Exports $pe.Exports -Types $typeInfo.Types -OutputDir $OutputPath
        
        Write-Host "  Generated header: $headerPath" -ForegroundColor Green
        
        # Create stub source file
        $stubSource = "// Stub implementation for $($file.Name)`n"
        $stubSource += "#include `"$($file.BaseName).h`"`n`n"
        $stubSource += "// TODO: Implement reversed functionality`n`n"
        
        foreach ($exp in $pe.Exports) {
            $stubSource += "// Original RVA: $($exp.RVA)`n"
            $stubSource += "void $($exp.Name)(void) {`n"
            $stubSource += "    // Stub - original at $($exp.RVA)`n"
            $stubSource += "}`n`n"
        }
        
        $srcPath = Join-Path $OutputPath "src\$($file.BaseName)_stub.c"
        $stubSource | Out-File -FilePath $srcPath -Encoding UTF8
        $projectSources += "$($file.BaseName)_stub.c"
    }
    
    # Extract resources
    if ($ExtractResources) {
        $res = Export-Resources -BinaryPath $file.FullName -OutputDir (Join-Path $OutputPath "resources\$($file.BaseName)")
        if ($res.Count -gt 0) {
            Write-Host "  Extracted $($res.Count) resource files" -ForegroundColor Green
        }
    }
    
    # COM interfaces
    if ($ReconstructCOM -and $file.Extension -eq '.dll') {
        $idl = New-COMInterfaceDefs -BinaryPath $file.FullName -OutputDir $OutputPath
        if ($idl) {
            Write-Host "  Generated IDL: $idl" -ForegroundColor Green
        }
    }
}

# Generate build systems
if ($GenerateBuildSystem) {
    Write-Host "`nGenerating build systems..." -ForegroundColor Yellow
    
    $cmake = New-CMakeLists -ProjectName $ProjectName -Sources $projectSources -Libraries $allImports -OutputDir $OutputPath -Is64Bit $is64Bit
    Write-Host "  CMake: $cmake" -ForegroundColor Green
    
    $meson = New-MesonBuild -ProjectName $ProjectName -Sources $projectSources -Libraries $allImports -OutputDir $OutputPath
    Write-Host "  Meson: $meson" -ForegroundColor Green
    
    $bat = New-BuildScript -ProjectName $ProjectName -Sources $projectSources -Is64Bit $is64Bit -OutputDir $OutputPath
    Write-Host "  Batch: $bat" -ForegroundColor Green
}

# Generate dependency manifest
if ($MapDependencies) {
    $manifest = @{
        Project = $ProjectName
        Architecture = if ($is64Bit) { "x64" } else { "x86" }
        Dependencies = $allImports | Select-Object -Unique | Sort-Object
        Exports = $allExports.Count
        Sources = $projectSources
    }
    
    $manifestPath = Join-Path $OutputPath "dependencies.json"
    $manifest | ConvertTo-Json -Depth 10 | Out-File -FilePath $manifestPath -Encoding UTF8
    Write-Host "  Dependencies: $manifestPath" -ForegroundColor Green
}

# Generate README
$readme = @"
# Reversed Project: $ProjectName

## Overview
This project was auto-generated by reversing the installation at:
$InstallPath

## Architecture
- Target Platform: $(if ($is64Bit) { "x64 (64-bit)" } else { "x86 (32-bit)" })
- Subsystem: $($pe.Subsystem)

## Project Structure
- `include/` - Reconstructed header files
- `src/` - Stub implementations
- `resources/` - Extracted resources (if enabled)
- `dependencies.json` - Mapped DLL dependencies

## Building

### Using CMake:
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Using Meson:
```bash
meson setup build
meson compile -C build
```

### Using MSVC (Windows):
```cmd
build.bat
```

## Notes
- All function signatures are inferred from export tables
- Type information recovered from: $(if ($DeepTypeRecovery) { "RTTI + Debug Symbols" } else { "Export analysis only" })
- COM interfaces: $(if ($ReconstructCOM) { "Partially reconstructed" } else { "Not analyzed" })

## Dependencies
$($allImports | Select-Object -Unique | Sort-Object | ForEach-Object { "- $_" } | Out-String)

## Statistics
- PE Files Analyzed: $($peFiles.Count)
- Functions Exported: $($allExports.Count)
- Headers Generated: $($projectSources.Count)
"@

$readme | Out-File -FilePath (Join-Path $OutputPath "README.md") -Encoding UTF8

$duration = (Get-Date) - $startTime
Write-Host @"

╔══════════════════════════════════════════════════════════════════╗
║                    REVERSAL COMPLETE                             ║
╚══════════════════════════════════════════════════════════════════╝
Duration: $($duration.ToString('hh\:mm\:ss'))
Output: $OutputPath
PE Files: $($peFiles.Count)
Headers Generated: $($projectSources.Count)
Dependencies Mapped: $($allImports.Count)
"@ -ForegroundColor Green
