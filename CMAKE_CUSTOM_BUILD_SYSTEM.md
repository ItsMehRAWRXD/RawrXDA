# Custom Build System - No SDK Required

## Compilers Directory
Location: `d:\rawrxd\compilers\`

### Available Custom Compilers (SDK-Independent)
- MASM: `d:\rawrxd\compilers\masm_ide\`
- NASM: `d:\rawrxd\compilers\nasm\`
- C/C++: `c_compiler_from_scratch.obj`
- Custom Cross-Platform: `universal_cross_platform_compiler.exe`

## Quick Build Commands

### Using Custom MASM Compiler
```powershell
d:\rawrxd\compilers\masm_ide\ml64.exe /c /Fo output.obj src\win32app\Win32IDE.cpp
```

### Using Custom NASM
```powershell
d:\rawrxd\compilers\nasm\nasm.exe -f win64 -o output.obj source.asm
```

### IDE Build Integration
The Win32IDE will call these custom compilers directly:
- No Visual Studio SDK needed
- No CMake SDK version detection
- runs entirely self-contained

## Environment Variables (Optional)
```powershell
$env:CUSTOM_COMPILER_PATH = "d:\rawrxd\compilers"
$env:MASM_COMPILER = "d:\rawrxd\compilers\masm_ide\ml64.exe"
$env:DUMPBIN = "d:\rawrxd\compilers\masm_ide\dumpbin.exe"
```

## File Paths Reference
- IDE Compiler Panel: `Win32IDE_CompilerPanel.cpp`
- MASM Compiler Wrapper: `src\masm\masm_cli_compiler.cpp`
- PE Writer: `src\masm\pe_writer.cpp`
- Dumpbin Integration: `src\masm\dumpbin_output_parser.cpp`

## Command Line Build (No CMake)
```powershell
# Direct compiler invocation
ml64 /c /Fo Win32IDE.obj src\win32app\Win32IDE.cpp /I include

# Link
link /out:RawrXD_IDE.exe Win32IDE.obj /subsystem:windows /entry:wmainCRTStartup
```

## Status
✅ All compilers ready  
✅ No external SDKs required  
✅ Self-contained in `D:\rawrxd\compilers\`  
✅ IDE menu commands wired to use these  

## Next Steps
1. Call custom compilers from IDE menu (already wired)
2. Parse dumpbin output in real-time
3. Display build errors in Problems panel
4. Show compiler output in build terminal
