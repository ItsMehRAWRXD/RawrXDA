# PowerShell 5.1 and in-memory compilation (Tier G note)

## What it is (and is not)

- **`Add-Type -MemberDefinition`** takes a **C#** fragment (attributes such as `[DllImport(...)]` are C#), compiles it with the in-process **C#** compiler, and loads the result. It does **not** compile ISO C or C++.
- The **.NET Framework** on Windows includes **Microsoft.CSharp** (and **Microsoft.VisualBasic**) code providers; that is **C# / VB.NET**, not C.
- Invoking **`csc.exe`** explicitly is a **different** path (subprocess). `Add-Type` typically uses **CodeDOM / Roslyn-style compilation** in-process depending on PowerShell version and API.

## Minimal one-liner (P/Invoke, in-memory)

```powershell
$c=@'
[DllImport("kernel32.dll")]public static extern void ExitProcess(uint u);
[DllImport("user32.dll")]public static extern int MessageBoxA(IntPtr h,string m,string c,uint t);
'@; Add-Type -MemberDefinition $c -Name "Sovereign" -Namespace "Win32"
[Win32.Sovereign]::MessageBoxA([IntPtr]::Zero,"Done","RawrXD",0); [Win32.Sovereign]::ExitProcess(0)
```

Use `[IntPtr]::Zero` instead of `0` for the HWND parameter so the call matches the declared `IntPtr` type.

## Emitting an assembly to disk (optional)

### Native `Add-Type` (Windows PowerShell 5.1+)

`Add-Type` can write the compiled assembly to a path and choose the **managed** output kind without using CodeDom manually:

```powershell
$src = @'
public class Program {
  public static void Main() { }
}
'@
Add-Type -TypeDefinition $src -OutputAssembly output.exe -OutputType ConsoleApplication
```

- **`-OutputAssembly`** — destination file (`.exe` or `.dll`).
- **`-OutputType`** — e.g. **`ConsoleApplication`**, **`Library`**, **`WindowsApplication`** (subsystem / target matches **`OutputAssemblyType`** used by the cmdlet).

With **`-OutputAssembly`**, the type is compiled **to disk**; you typically **do not** load it into the current session unless you also use **`-PassThru`** or **`Import-Module` / `[Reflection.Assembly]::LoadFile`**.

### CodeDom (`CompilerParameters`)

Alternatively, use **`System.CodeDom.Compiler.CompilerParameters`** with **`GenerateExecutable`**, **`OutputAssembly`**, **`GenerateInMemory = $false`**. The repository shows in-memory compilation from a C# source string in **`scripts/Invoke-CSharpCode.ps1`** (extend that pattern for on-disk output).

This is still **C# → managed assembly**, not a replacement for **`cl.exe` / `link.exe`** or the Sovereign **PE** lab emitters in **`include/rawrxd/sovereign_emit_formats.hpp`**.

## Caveats (global-use realism)

- **Execution policy** and **constrained language mode** can still block or limit script features; this is not a universal “policy bypass.”
- **Antivirus** and **AMSI** may inspect compiled or scripted content.
- **`Add-Type`** may cache generated assemblies under the user’s temp profile; “no disk footprint” is not guaranteed across configurations.

See also **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** (§6 / §7) for product vs lab scope.
