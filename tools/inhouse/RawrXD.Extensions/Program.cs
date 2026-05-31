using Sovereign;

var manager = new ExtensionEngine.ExtensionManager();

const string source = """
using System;
using Sovereign;

public sealed class GeneratedExtension : ExtensionEngine.IExtension
{
    public string Name => "DemoExt";
    public string Version => "1.0.0";
    public string Author => "RawrXD";
    public string Description => "In-memory demo extension";

    public void Initialize(ExtensionEngine.ExtensionContext ctx) { }

    public void Execute(string[] args)
    {
        Console.WriteLine($"Demo extension executed. Args={string.Join(',', args)}");
    }

    public void Shutdown() { }

    public ExtensionEngine.ExtensionCapabilities GetCapabilities() =>
        ExtensionEngine.ExtensionCapabilities.MemoryOnly;
}
""";

var extension = manager.LoadFromSource("demo", source, ExtensionEngine.SourceLanguage.CSharp);
manager.Execute("demo", ["alpha", "beta"]);
Console.WriteLine($"Loaded extension: {extension.Instance.Name} v{extension.Instance.Version}");
