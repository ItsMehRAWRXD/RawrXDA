using System.Diagnostics;
using System.Text.RegularExpressions;

static class Program
{
    private enum Mode
    {
        DirectLink,
        LibMode,
        LinkMode
    }

    public static int Main(string[] args)
    {
        if (args.Length == 0)
        {
            Console.Error.WriteLine("rawrxd_compiler: missing args. Expected /DIRECTLINK, /LIBMODE, or /LINKMODE.");
            return 2;
        }

        if (!TryGetMode(args, out var mode))
        {
            Console.Error.WriteLine("rawrxd_compiler: unknown mode. Provide one of: /DIRECTLINK | /LIBMODE | /LINKMODE");
            return 2;
        }

        try
        {
            return mode switch
            {
                Mode.DirectLink => RunDirectLink(args),
                Mode.LibMode => RunLibMode(args),
                Mode.LinkMode => RunLinkMode(args),
                _ => 2
            };
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"rawrxd_compiler: fatal: {ex.Message}");
            return 1;
        }
    }

    private static bool TryGetMode(string[] args, out Mode mode)
    {
        if (args.Any(a => string.Equals(a, "/DIRECTLINK", StringComparison.OrdinalIgnoreCase)))
        {
            mode = Mode.DirectLink;
            return true;
        }

        if (args.Any(a => string.Equals(a, "/LIBMODE", StringComparison.OrdinalIgnoreCase)))
        {
            mode = Mode.LibMode;
            return true;
        }

        if (args.Any(a => string.Equals(a, "/LINKMODE", StringComparison.OrdinalIgnoreCase)))
        {
            mode = Mode.LinkMode;
            return true;
        }

        mode = default;
        return false;
    }

    private static string? GetFlagValue(string[] args, string prefix)
    {
        foreach (var token in args)
        {
            if (token.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
            {
                return Unquote(token[prefix.Length..]);
            }
        }

        return null;
    }

    private static string Unquote(string value) => value.Trim().Trim('"');

    private static string ResolveTool(string toolName)
    {
        toolName = toolName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase) ? toolName : toolName + ".exe";

        var fromPath = FindOnPath(toolName);
        if (fromPath != null)
        {
            return fromPath;
        }

        var vswhere = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            "Microsoft Visual Studio", "Installer", "vswhere.exe");

        var fallbackGlobs = new[]
        {
            $@"C:\VS2022Enterprise\VC\Tools\MSVC\*\bin\Hostx64\x64\{toolName}",
            $@"C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\{toolName}",
            $@"C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\{toolName}",
        };

        if (File.Exists(vswhere))
        {
            var installPath = RunAndCapture(vswhere,
                "-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath",
                timeoutMs: 10_000).Trim();

            if (!string.IsNullOrWhiteSpace(installPath))
            {
                var msvcRoot = Path.Combine(installPath, "VC", "Tools", "MSVC");
                if (Directory.Exists(msvcRoot))
                {
                    foreach (var msvc in Directory.EnumerateDirectories(msvcRoot).OrderByDescending(p => p))
                    {
                        var candidate = Path.Combine(msvc, "bin", "Hostx64", "x64", toolName);
                        if (File.Exists(candidate))
                        {
                            return candidate;
                        }
                    }
                }
            }
        }

        foreach (var pattern in fallbackGlobs)
        {
            var hit = GlobFirst(pattern);
            if (hit != null)
            {
                return hit;
            }
        }

        throw new FileNotFoundException($"{toolName} not found (PATH/VS install).");
    }

    private static string? FindOnPath(string exeName)
    {
        var path = Environment.GetEnvironmentVariable("PATH") ?? "";
        foreach (var dir in path.Split(';'))
        {
            if (string.IsNullOrWhiteSpace(dir)) continue;
            try
            {
                var candidate = Path.Combine(dir.Trim(), exeName);
                if (File.Exists(candidate))
                {
                    return candidate;
                }
            }
            catch { }
        }

        return null;
    }

    private static string? GlobFirst(string pattern)
    {
        var normalized = pattern.Replace('/', '\\');
        var root = Path.GetPathRoot(normalized);
        if (string.IsNullOrWhiteSpace(root)) return null;

        var parts = normalized[root.Length..].Split('\\', StringSplitOptions.RemoveEmptyEntries);
        var candidates = new List<string> { root };
        foreach (var part in parts)
        {
            var next = new List<string>();
            foreach (var baseDir in candidates)
            {
                if (part.Contains('*') || part.Contains('?'))
                {
                    try
                    {
                        if (Directory.Exists(baseDir))
                        {
                            next.AddRange(Directory.EnumerateFileSystemEntries(baseDir, part));
                        }
                    }
                    catch { }
                }
                else
                {
                    next.Add(Path.Combine(baseDir, part));
                }
            }
            candidates = next;
            if (candidates.Count == 0) return null;
        }

        foreach (var c in candidates)
        {
            if (File.Exists(c)) return c;
        }
        return null;
    }

    private static IReadOnlyList<string> ResolveWindowsLibPaths(string linkExe)
    {
        var results = new List<string>();

        // VC lib: ...\VC\Tools\MSVC\<ver>\bin\Hostx64\x64\link.exe -> ...\VC\Tools\MSVC\<ver>\lib\x64
        try
        {
            var linkDir = Path.GetDirectoryName(linkExe);
            if (!string.IsNullOrWhiteSpace(linkDir))
            {
                var msvcVerDir = Directory.GetParent(Directory.GetParent(Directory.GetParent(linkDir)!.FullName)!.FullName)!.FullName;
                var vcLib = Path.Combine(msvcVerDir, "lib", "x64");
                if (Directory.Exists(vcLib)) results.Add(vcLib);
            }
        }
        catch { }

        // Windows SDK lib paths (newest)
        var sdkRoot = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            "Windows Kits", "10", "Lib");
        if (Directory.Exists(sdkRoot))
        {
            Version? best = null;
            string? bestDir = null;
            foreach (var dir in Directory.EnumerateDirectories(sdkRoot))
            {
                var name = Path.GetFileName(dir);
                if (Version.TryParse(name, out var v))
                {
                    if (best == null || v > best)
                    {
                        best = v;
                        bestDir = dir;
                    }
                }
            }

            if (bestDir != null)
            {
                var um = Path.Combine(bestDir, "um", "x64");
                var ucrt = Path.Combine(bestDir, "ucrt", "x64");
                if (Directory.Exists(um)) results.Add(um);
                if (Directory.Exists(ucrt)) results.Add(ucrt);
            }
        }

        return results;
    }

    private static int RunDirectLink(string[] args)
    {
        var source = GetFlagValue(args, "/SOURCE:");
        var outObj = GetFlagValue(args, "/OUT:");
        if (string.IsNullOrWhiteSpace(source)) throw new ArgumentException("Missing /SOURCE:<file.asm>");
        if (string.IsNullOrWhiteSpace(outObj)) throw new ArgumentException("Missing /OUT:<file.obj>");
        if (!File.Exists(source)) throw new FileNotFoundException($"SOURCE not found: {source}");

        Directory.CreateDirectory(Path.GetDirectoryName(outObj)!);

        var ml64 = ResolveTool("ml64");
        var passThrough = new List<string>();
        var hasInclude = false;
        foreach (var t in args)
        {
            if (t.StartsWith("/I:", StringComparison.OrdinalIgnoreCase) || t.StartsWith("/D:", StringComparison.OrdinalIgnoreCase))
            {
                passThrough.Add(t);
            }
            if (t.StartsWith("/I:", StringComparison.OrdinalIgnoreCase))
            {
                hasInclude = true;
            }
        }

        var mlArgs = new List<string> { "/nologo", "/c", $"/Fo{outObj}" };
        if (!hasInclude)
        {
            var srcDir = Path.GetDirectoryName(source);
            if (!string.IsNullOrWhiteSpace(srcDir))
            {
                mlArgs.Add($"/I:{srcDir}");
            }
        }
        mlArgs.AddRange(passThrough);
        mlArgs.Add(source);

        Console.WriteLine($"Assembling: {source}");
        return RunTool(ml64, mlArgs);
    }

    private static int RunLibMode(string[] args)
    {
        var outLib = GetFlagValue(args, "/OUT:");
        if (string.IsNullOrWhiteSpace(outLib)) throw new ArgumentException("Missing /OUT:<file.lib>");
        Directory.CreateDirectory(Path.GetDirectoryName(outLib)!);

        var objs = args.Select(Unquote).Where(a => a.EndsWith(".obj", StringComparison.OrdinalIgnoreCase) && File.Exists(a)).ToList();
        if (objs.Count == 0) throw new ArgumentException("No .obj inputs found for /LIBMODE");

        var libExe = ResolveTool("lib");
        var libArgs = new List<string> { "/NOLOGO", "/MACHINE:X64", $"/OUT:{outLib}" };
        libArgs.AddRange(objs);
        return RunTool(libExe, libArgs);
    }

    private static int RunLinkMode(string[] args)
    {
        var outExe = GetFlagValue(args, "/OUT:");
        if (string.IsNullOrWhiteSpace(outExe)) throw new ArgumentException("Missing /OUT:<file.exe>");
        Directory.CreateDirectory(Path.GetDirectoryName(outExe)!);

        var linkExe = ResolveTool("link");
        var linkArgs = new List<string> { "/NOLOGO", "/MACHINE:X64", "/INCREMENTAL:NO", $"/OUT:{outExe}" };
        linkArgs.AddRange(ResolveWindowsLibPaths(linkExe).Select(p => $"/LIBPATH:{p}"));

        foreach (var t in args)
        {
            var u = Unquote(t);
            if (u.StartsWith("/SUBSYSTEM:", StringComparison.OrdinalIgnoreCase) ||
                u.StartsWith("/ENTRY:", StringComparison.OrdinalIgnoreCase) ||
                u.StartsWith("/LIBPATH:", StringComparison.OrdinalIgnoreCase) ||
                u.StartsWith("/NODEFAULTLIB:", StringComparison.OrdinalIgnoreCase) ||
                u.StartsWith("/MERGE:", StringComparison.OrdinalIgnoreCase) ||
                u.StartsWith("/FIXED:", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(u, "/LARGEADDRESSAWARE", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(u, "/DYNAMICBASE", StringComparison.OrdinalIgnoreCase) ||
                string.Equals(u, "/NXCOMPAT", StringComparison.OrdinalIgnoreCase) ||
                u.StartsWith("/FORCE:", StringComparison.OrdinalIgnoreCase))
            {
                linkArgs.Add(u);
                continue;
            }

            if (u.EndsWith(".obj", StringComparison.OrdinalIgnoreCase) ||
                u.EndsWith(".lib", StringComparison.OrdinalIgnoreCase) ||
                u.EndsWith(".res", StringComparison.OrdinalIgnoreCase))
            {
                linkArgs.Add(u);
                continue;
            }

            // system libs like kernel32.lib, user32.lib, winhttp.lib, ...
            if (Regex.IsMatch(u, @"^[A-Za-z0-9_\-]+\.lib$", RegexOptions.IgnoreCase))
            {
                linkArgs.Add(u);
            }
        }

        return RunTool(linkExe, linkArgs);
    }

    private static int RunTool(string exe, IReadOnlyList<string> args)
    {
        var psi = new ProcessStartInfo(exe)
        {
            UseShellExecute = false,
        };
        foreach (var a in args)
        {
            psi.ArgumentList.Add(a);
        }

        using var proc = Process.Start(psi) ?? throw new InvalidOperationException($"Failed to start {exe}");
        proc.WaitForExit();
        return proc.ExitCode;
    }

    private static string RunAndCapture(string exe, string arguments, int timeoutMs)
    {
        var psi = new ProcessStartInfo(exe, arguments)
        {
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };

        using var proc = Process.Start(psi) ?? throw new InvalidOperationException($"Failed to start {exe}");
        if (!proc.WaitForExit(timeoutMs))
        {
            try { proc.Kill(entireProcessTree: true); } catch { }
            throw new TimeoutException($"{exe} timed out");
        }
        return proc.StandardOutput.ReadToEnd();
    }
}
