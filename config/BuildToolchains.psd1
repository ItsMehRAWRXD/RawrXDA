@{
    DefaultToolchain = "PowerShell"
    
    Toolchains = @{
        PowerShell = @{
            Enabled = $true
            Description = "Pure PowerShell/C# - No external dependencies"
            Performance = "~500ns classification"
            RequiredTools = @()
            OutputFormat = "PSM1 Module"
            CompilerOptions = @{
                OptimizationLevel = "Aggressive"
                EnableInlining = $true
                TargetFramework = "net48"
            }
        }
        
        MASM = @{
            Enabled = $true
            Description = "Microsoft Macro Assembler (Visual Studio) — Sovereign Kernel + Titan DLL"
            Performance = "~150ns classification (fastest)"
            RequiredTools = @("ml64.exe", "link.exe", "cl.exe")
            OutputFormat = "DLL"
            CompilerPath = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
            CppCompilerPath = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe"
            LinkerPath = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
            CompilerOptions = @{
                Architecture = "x64"
                EnableAVX512 = $true
                DebugInfo = $false
                Optimize = $true
                TitanDLL = $true
                GhostFontItalic = $true
            }
            BuildSteps = @(
                @{ Tool = "ml64"; Args = "/c /nologo src\asm\monolithic\ui.asm /Fo build\obj\ui.obj" }
                @{ Tool = "cl"; Args = "/O2 /EHsc /c src\bridge_titan_4a.cpp /Fo build\obj\bridge_titan_4a.obj" }
                @{ Tool = "cl"; Args = "/O2 /EHsc /c src\winmain_titan.cpp /Fo build\obj\winmain_titan.obj" }
                @{ Tool = "link"; Args = "/SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup /MACHINE:X64 build\obj\ui.obj build\obj\bridge_titan_4a.obj build\obj\winmain_titan.obj kernel32.lib user32.lib gdi32.lib shell32.lib comdlg32.lib msvcrt.lib ucrt.lib vcruntime.lib /OUT:build\bin\RawrXD_Titan_4D.exe" }
            )
        }
        
        NASM = @{
            Enabled = $false
            Description = "Netwide Assembler (Cross-platform)"
            Performance = "~200ns classification"
            RequiredTools = @("nasm.exe", "link.exe")
            CompilerPath = "C:\nasm\nasm.exe"
            LinkerPath = "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
            CompilerOptions = @{
                Format = "win64"
                DebugFormat = "null"
            }
        }
        
        POASM = @{
            Enabled = $true
            Description = "Pelles Macro Assembler (x64) — Sovereign PE Writer"
            Performance = "~150ns classification (zero-CRT)"
            RequiredTools = @("poasm.exe", "polink.exe")
            CompilerPath = "C:\masm32\bin\poasm.exe"
            LinkerPath = "C:\masm32\bin\polink.exe"
            OutputFormat = "EXE"
            CompilerOptions = @{
                Architecture = "AMD64"
                Flags = @("/AAMD64", "/Mx", "/Zi")
                DebugInfo = $true
            }
            LinkerOptions = @{
                Subsystem = "CONSOLE"
                Entry = "WinMainCRTStartup"
                LargeAddressAware = $false
                LibPath = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
                DefaultLibs = @("kernel32.lib", "user32.lib", "gdi32.lib")
            }
        }

        GCC = @{
            Enabled = $false
            Description = "GNU Compiler Collection (MinGW)"
            Performance = "~300ns classification"
            RequiredTools = @("gcc.exe", "g++.exe")
            CompilerPath = "C:\mingw64\bin\gcc.exe"
            LinkerPath = "C:\mingw64\bin\g++.exe"
            CompilerOptions = @{
                OptimizationLevel = "O3"
                Architecture = "x86-64"
                EnableAVX512 = $true
                StaticLink = $true
            }
        }
        
        Custom = @{
            Enabled = $false
            Description = "User-defined custom toolchain"
            Performance = "Variable"
            RequiredTools = @()
            CompilerPath = ""
            LinkerPath = ""
            CompilerOptions = @{
                CommandTemplate = "{compiler} {inputFile} {options} -o {outputFile}"
                LinkerTemplate = "{linker} {objectFiles} {options} -o {outputFile}"
                CustomFlags = @()
            }
        }
    }
    
    Output = @{
        BinPath = "D:\rawrxd\build\bin"
        ModuleName = "RawrXD_PatternBridge"
        IntermediatePath = "D:\rawrxd\build\obj"
        LogPath = "D:\rawrxd\build\logs"
    }
    
    Features = @{
        EnableLearning = $true
        EnableStats = $true
        EnableCaching = $true
        EnableParallelization = $true
        EnableDebugLogs = $false
    }
    
    FallbackChain = @("PowerShell", "GCC", "NASM", "MASM", "Custom")
    
    AutoDetect = @{
        Enabled = $true
        SearchPaths = @(
            "C:\VS2022Enterprise\VC\Tools\MSVC"
            "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
            "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
            "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
            "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
            "C:\mingw64"
            "C:\msys64\mingw64"
            "C:\nasm"
        )
        CacheResults = $true
        CachePath = "D:\rawrxd\build\.toolchain-cache.json"
    }
}
