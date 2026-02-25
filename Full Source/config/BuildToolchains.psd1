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
            Enabled = $false
            Description = "Microsoft Macro Assembler (Visual Studio)"
            Performance = "~150ns classification (fastest)"
            RequiredTools = @("ml64.exe", "link.exe")
            OutputFormat = "DLL"
            CompilerPath = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\ml64.exe"
            LinkerPath = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\link.exe"
            CompilerOptions = @{
                Architecture = "x64"
                EnableAVX512 = $true
                DebugInfo = $false
                Optimize = $true
            }
        }
        
        NASM = @{
            Enabled = $false
            Description = "Netwide Assembler (Cross-platform)"
            Performance = "~200ns classification"
            RequiredTools = @("nasm.exe", "link.exe")
            CompilerPath = "C:\nasm\nasm.exe"
            LinkerPath = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.38.33130\bin\Hostx64\x64\link.exe"
            CompilerOptions = @{
                Format = "win64"
                DebugFormat = "null"
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
        BinPath = "D:\lazy init ide\bin"
        ModuleName = "RawrXD_PatternBridge"
        IntermediatePath = "D:\lazy init ide\obj"
        LogPath = "D:\lazy init ide\logs\build"
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
            "C:\Program Files\Microsoft Visual Studio"
            "C:\mingw64"
            "C:\msys64\mingw64"
            "C:\nasm"
            "C:\masm32"
        )
        CacheResults = $true
        CachePath = "D:\lazy init ide\bin\.toolchain-cache.json"
    }
}
