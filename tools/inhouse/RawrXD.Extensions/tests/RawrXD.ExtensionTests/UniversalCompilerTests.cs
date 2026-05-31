using System.Reflection;
using Xunit;

namespace Sovereign.Tests;

public class UniversalCompilerTests
{
    private const string ValidExtensionSource = """
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
    public void Compile_CSharpSource_ReturnsValidAssembly()
    {
        var compiler = new ExtensionEngine.UniversalCompiler();
        var bytes = compiler.Compile(ValidExtensionSource, ExtensionEngine.SourceLanguage.CSharp, ExtensionEngine.TargetPlatform.Windows);

        Assert.NotNull(bytes);
        Assert.True(bytes.Length > 0);

        var assembly = Assembly.Load(bytes);
        var type = assembly.GetTypes().FirstOrDefault(t => typeof(ExtensionEngine.IExtension).IsAssignableFrom(t) && !t.IsInterface);
        Assert.NotNull(type);
    }

    [Fact]
    public void Compile_AndExecute_ProducesExpectedOutput()
    {
        var compiler = new ExtensionEngine.UniversalCompiler();
        var bytes = compiler.Compile(ValidExtensionSource, ExtensionEngine.SourceLanguage.CSharp, ExtensionEngine.TargetPlatform.Windows);
        var assembly = Assembly.Load(bytes);
        var type = assembly.GetTypes().First(t => typeof(ExtensionEngine.IExtension).IsAssignableFrom(t) && !t.IsInterface);
        var instance = (ExtensionEngine.IExtension)Activator.CreateInstance(type)!;

        instance.Initialize(new ExtensionEngine.ExtensionContext());

        using var sw = new System.IO.StringWriter();
        Console.SetOut(sw);
        instance.Execute(new[] { "alpha", "beta" });
        var output = sw.ToString();

        Assert.Contains("Demo extension executed", output);
        Assert.Contains("alpha,beta", output);
    }

    [Fact]
    public void Compile_PythonTranspile_ReturnsAssembly()
    {
        var compiler = new ExtensionEngine.UniversalCompiler();
        var py = "print('hello from python')";
        var bytes = compiler.Compile(py, ExtensionEngine.SourceLanguage.Python, ExtensionEngine.TargetPlatform.Windows);

        Assert.NotNull(bytes);
        Assert.True(bytes.Length > 0);
    }

    [Fact]
    public void Compile_InvalidSource_Throws()
    {
        var compiler = new ExtensionEngine.UniversalCompiler();
        Assert.Throws<InvalidOperationException>(() =>
            compiler.Compile("not valid C# at all !!!", ExtensionEngine.SourceLanguage.CSharp, ExtensionEngine.TargetPlatform.Windows));
    }
}
