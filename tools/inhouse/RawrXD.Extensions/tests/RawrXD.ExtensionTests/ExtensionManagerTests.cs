using System.Text;
using Xunit;

namespace Sovereign.Tests;

public class ExtensionManagerTests
{
    private const string DemoSource = """
using System;
using Sovereign;

public sealed class DemoExt : ExtensionEngine.IExtension
{
    public string Name => "DemoExt";
    public string Version => "1.0.0";
    public string Author => "Test";
    public string Description => "Test extension";

    public void Initialize(ExtensionEngine.ExtensionContext ctx) { }
    public void Execute(string[] args)
    {
        Console.WriteLine("Demo extension executed. Args=" + string.Join(",", args));
    }
    public void Shutdown() { }
    public ExtensionEngine.ExtensionCapabilities GetCapabilities() => ExtensionEngine.ExtensionCapabilities.All;
}
""";

    [Fact]
    public void LoadFromSource_ThenExecute_Works()
    {
        var mgr = new ExtensionEngine.ExtensionManager();
        mgr.LoadFromSource("DemoExt", DemoSource, ExtensionEngine.SourceLanguage.CSharp);

        using var sw = new System.IO.StringWriter();
        Console.SetOut(sw);
        mgr.Execute("DemoExt", new[] { "a", "b" });

        Assert.Contains("Demo extension executed", sw.ToString());
    }

    [Fact]
    public void List_ReturnsLoadedExtensions()
    {
        var mgr = new ExtensionEngine.ExtensionManager();
        mgr.LoadFromSource("DemoExt", DemoSource, ExtensionEngine.SourceLanguage.CSharp);

        var list = mgr.List();
        Assert.Contains("DemoExt", list);
    }

    [Fact]
    public void Execute_UnknownExtension_Throws()
    {
        var mgr = new ExtensionEngine.ExtensionManager();
        Assert.Throws<KeyNotFoundException>(() => mgr.Execute("NonExistent", Array.Empty<string>()));
    }

    [Fact]
    public void Package_AndLoad_Roundtrip()
    {
        var tmpDir = System.IO.Path.Combine(System.IO.Path.GetTempPath(), $"rawrxd_test_{Guid.NewGuid()}");
        System.IO.Directory.CreateDirectory(tmpDir);
        var dllPath = System.IO.Path.Combine(tmpDir, "DemoExt.dll");
        var pkgPath = System.IO.Path.Combine(tmpDir, "DemoExt.cml");

        // Compile to file first so Package can read the assembly bytes
        var compiler = new ExtensionEngine.UniversalCompiler();
        var asmBytes = compiler.Compile(DemoSource, ExtensionEngine.SourceLanguage.CSharp, ExtensionEngine.TargetPlatform.Windows);
        System.IO.File.WriteAllBytes(dllPath, asmBytes);

        var mgr = new ExtensionEngine.ExtensionManager();
        mgr.Load(dllPath);
        mgr.Package("DemoExt", "test-passphrase-123", pkgPath);

        Assert.True(System.IO.File.Exists(pkgPath));

        var json = System.IO.File.ReadAllText(pkgPath);
        Assert.Contains("DemoExt", json);
        Assert.Contains("EncryptedData", json);

        // Cleanup
        System.IO.Directory.Delete(tmpDir, true);
    }
}
