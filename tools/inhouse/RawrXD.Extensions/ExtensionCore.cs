using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;

namespace Sovereign;

public sealed class ExtensionEngine
{
    public interface IExtension
    {
        string Name { get; }
        string Version { get; }
        string Author { get; }
        string Description { get; }
        void Initialize(ExtensionContext ctx);
        void Execute(string[] args);
        void Shutdown();
        ExtensionCapabilities GetCapabilities();
    }

    [Flags]
    public enum ExtensionCapabilities
    {
        None = 0,
        FileIO = 1,
        Network = 2,
        Crypto = 4,
        Compilation = 8,
        MemoryOnly = 16,
        UI = 32,
        AI = 64,
        All = FileIO | Network | Crypto | Compilation | MemoryOnly | UI | AI
    }

    public enum SourceLanguage
    {
        CSharp,
        C,
        Cpp,
        Rust,
        Go,
        Python,
        JavaScript,
        Lua
    }

    public enum ScriptLanguage
    {
        Python,
        Lua,
        JavaScript
    }

    public enum TargetPlatform
    {
        Windows,
        Linux,
        MacOS,
        WebAssembly
    }

    public sealed class ExtensionContext
    {
        public CarmillaEngine Crypto { get; init; } = new();
        public UniversalCompiler Compiler { get; init; } = new();
        public AIClient AI { get; init; } = new();
        public UIManager UI { get; init; } = new();
        public FileSystem FS { get; init; } = new();
        public NetworkManager Network { get; init; } = new();
        public ExtensionCapabilities Permissions { get; internal set; } = ExtensionCapabilities.All;
        public bool IsSandboxed { get; internal set; }

        public bool RequestCapability(ExtensionCapabilities capability)
        {
            if (Permissions.HasFlag(capability))
            {
                return true;
            }

            Console.WriteLine($"Extension requests capability: {capability}");
            Console.Write("Allow? [y/N]: ");
            var answer = Console.ReadLine();
            return string.Equals(answer, "y", StringComparison.OrdinalIgnoreCase);
        }
    }

    public sealed class Extension
    {
        public required IExtension Instance { get; init; }
        public required string Path { get; init; }
        public required string Language { get; init; }
        public required bool IsEncrypted { get; init; }
        public Assembly? Assembly { get; init; }
    }

    public sealed class ExtensionPackage
    {
        public required string Name { get; init; }
        public required string Version { get; init; }
        public required string Author { get; init; }
        public required string Description { get; init; }
        public ExtensionCapabilities Capabilities { get; init; }
        public required string EncryptedData { get; init; }
    }

    public sealed class ExtensionLoader
    {
        public Extension LoadFromAssembly(string path, ExtensionContext ctx)
        {
            var bytes = File.ReadAllBytes(path);
            return LoadFromAssemblyBytes(bytes, ctx, path, false);
        }

        public Extension LoadFromEncrypted(string path, string passphrase, ExtensionContext ctx)
        {
            var encrypted = File.ReadAllBytes(path);
            var decrypted = CarmillaEngine.Decrypt(encrypted, passphrase);
            return LoadFromAssemblyBytes(decrypted, ctx, path, true);
        }

        public Extension LoadFromSource(string sourceCode, SourceLanguage language, ExtensionContext ctx)
        {
            var compiler = new UniversalCompiler();
            var assemblyBytes = compiler.Compile(sourceCode, language, TargetPlatform.Windows);
            return LoadFromAssemblyBytes(assemblyBytes, ctx, "<memory>", false);
        }

        public Extension LoadFromScript(string scriptCode, ScriptLanguage language, ExtensionContext ctx)
        {
            var transpiled = language switch
            {
                ScriptLanguage.Python => new PythonToCSharpTranspiler().Transpile(scriptCode),
                ScriptLanguage.Lua => new LuaToCSharpTranspiler().Transpile(scriptCode),
                ScriptLanguage.JavaScript => new JsToCSharpTranspiler().Transpile(scriptCode),
                _ => throw new NotSupportedException($"Unsupported script language: {language}")
            };

            return LoadFromSource(transpiled, SourceLanguage.CSharp, ctx);
        }

        private static Extension LoadFromAssemblyBytes(byte[] bytes, ExtensionContext ctx, string path, bool encrypted)
        {
            var assembly = Assembly.Load(bytes);
            var extensionType = assembly.GetTypes().FirstOrDefault(t =>
                typeof(IExtension).IsAssignableFrom(t) && !t.IsInterface && !t.IsAbstract);

            if (extensionType is null)
            {
                throw new InvalidOperationException("No IExtension implementation found in assembly.");
            }

            if (Activator.CreateInstance(extensionType) is not IExtension instance)
            {
                throw new InvalidOperationException($"Failed to instantiate extension type {extensionType.FullName}.");
            }

            instance.Initialize(ctx);

            return new Extension
            {
                Instance = instance,
                Path = path,
                Language = "CIL",
                IsEncrypted = encrypted,
                Assembly = assembly
            };
        }
    }

    public sealed class UniversalCompiler
    {
        public byte[] Compile(string source, SourceLanguage language, TargetPlatform target)
        {
            var csharp = language switch
            {
                SourceLanguage.CSharp => source,
                SourceLanguage.Python => new PythonToCSharpTranspiler().Transpile(source),
                SourceLanguage.Lua => new LuaToCSharpTranspiler().Transpile(source),
                SourceLanguage.JavaScript => new JsToCSharpTranspiler().Transpile(source),
                SourceLanguage.C => GenerateStubFromForeign("C", source),
                SourceLanguage.Cpp => GenerateStubFromForeign("C++", source),
                SourceLanguage.Rust => GenerateStubFromForeign("Rust", source),
                SourceLanguage.Go => GenerateStubFromForeign("Go", source),
                _ => throw new NotSupportedException($"Unsupported source language: {language}")
            };

            return CompileCSharp(csharp, target);
        }

        private static byte[] CompileCSharp(string code, TargetPlatform target)
        {
            var syntaxTree = CSharpSyntaxTree.ParseText(code);
            var refs = new List<MetadataReference>();

            var tpaValue = AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") as string;
            if (!string.IsNullOrWhiteSpace(tpaValue))
            {
                foreach (var path in tpaValue.Split(Path.PathSeparator))
                {
                    if (!string.IsNullOrWhiteSpace(path) && File.Exists(path))
                    {
                        refs.Add(MetadataReference.CreateFromFile(path));
                    }
                }
            }

            refs.Add(MetadataReference.CreateFromFile(typeof(ExtensionEngine).Assembly.Location));

            var compilation = CSharpCompilation.Create(
                assemblyName: Path.GetRandomFileName(),
                syntaxTrees: new[] { syntaxTree },
                references: refs,
                options: new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary,
                    platform: target switch
                    {
                        _ => Platform.AnyCpu
                    }));

            using var ms = new MemoryStream();
            var result = compilation.Emit(ms);
            if (!result.Success)
            {
                var errors = string.Join(Environment.NewLine,
                    result.Diagnostics.Where(d => d.Severity == DiagnosticSeverity.Error));
                throw new InvalidOperationException("Compilation failed:" + Environment.NewLine + errors);
            }

            return ms.ToArray();
        }

        private static string GenerateStubFromForeign(string language, string source)
        {
            var escaped = source.Replace("\"", "\"\"");
            return $$"""
using System;
using Sovereign;

public sealed class GeneratedExtension : ExtensionEngine.IExtension
{
    public string Name => "{{language}} Extension";
    public string Version => "1.0.0";
    public string Author => "UniversalCompiler";
    public string Description => "Generated from {{language}} source";

    public void Initialize(ExtensionEngine.ExtensionContext ctx) { }
    public void Execute(string[] args)
    {
        Console.WriteLine("Original {{language}} source captured for later native pipeline compilation.");
        Console.WriteLine(@"{{escaped}}");
    }
    public void Shutdown() { }
    public ExtensionEngine.ExtensionCapabilities GetCapabilities() => ExtensionEngine.ExtensionCapabilities.All;
}
""";
        }
    }

    public sealed class ExtensionManager
    {
        private readonly Dictionary<string, Extension> _extensions = new(StringComparer.OrdinalIgnoreCase);
        private readonly ExtensionLoader _loader = new();
        private readonly ExtensionContext _context = new();

        public Extension Load(string path, string? passphrase = null)
        {
            Extension extension;
            if (path.EndsWith(".cml", StringComparison.OrdinalIgnoreCase))
            {
                if (string.IsNullOrWhiteSpace(passphrase))
                {
                    throw new ArgumentException("Encrypted extension requires passphrase", nameof(passphrase));
                }

                extension = _loader.LoadFromEncrypted(path, passphrase, _context);
            }
            else if (path.EndsWith(".dll", StringComparison.OrdinalIgnoreCase))
            {
                extension = _loader.LoadFromAssembly(path, _context);
            }
            else
            {
                var lang = DetectLanguage(path);
                var source = File.ReadAllText(path);
                extension = _loader.LoadFromSource(source, lang, _context);
            }

            var key = Path.GetFileNameWithoutExtension(path);
            _extensions[key] = extension;
            return extension;
        }

        public Extension LoadFromSource(string name, string sourceCode, SourceLanguage language)
        {
            var extension = _loader.LoadFromSource(sourceCode, language, _context);
            _extensions[name] = extension;
            return extension;
        }

        public void Execute(string name, string[] args)
        {
            if (!_extensions.TryGetValue(name, out var extension))
            {
                throw new KeyNotFoundException($"Extension '{name}' not found");
            }

            extension.Instance.Execute(args);
        }

        public IReadOnlyCollection<string> List() => _extensions.Keys.ToArray();

        public void Package(string name, string passphrase, string outputPath)
        {
            if (!_extensions.TryGetValue(name, out var extension))
            {
                throw new KeyNotFoundException($"Extension '{name}' not found");
            }

            var assemblyPath = extension.Assembly?.Location;
            if (string.IsNullOrWhiteSpace(assemblyPath) || !File.Exists(assemblyPath))
            {
                assemblyPath = extension.Path;
            }

            if (string.IsNullOrWhiteSpace(assemblyPath) || !File.Exists(assemblyPath))
            {
                throw new InvalidOperationException("Cannot package in-memory extension without persisted assembly location.");
            }

            var bytes = File.ReadAllBytes(assemblyPath);
            var encrypted = CarmillaEngine.Encrypt(bytes, passphrase);

            var package = new ExtensionPackage
            {
                Name = name,
                Version = extension.Instance.Version,
                Author = extension.Instance.Author,
                Description = extension.Instance.Description,
                Capabilities = extension.Instance.GetCapabilities(),
                EncryptedData = Convert.ToBase64String(encrypted)
            };

            var json = System.Text.Json.JsonSerializer.Serialize(package, new System.Text.Json.JsonSerializerOptions
            {
                WriteIndented = true
            });

            File.WriteAllText(outputPath, json);
        }

        private static SourceLanguage DetectLanguage(string path)
        {
            return Path.GetExtension(path).ToLowerInvariant() switch
            {
                ".cs" => SourceLanguage.CSharp,
                ".c" => SourceLanguage.C,
                ".cpp" => SourceLanguage.Cpp,
                ".rs" => SourceLanguage.Rust,
                ".go" => SourceLanguage.Go,
                ".py" => SourceLanguage.Python,
                ".js" => SourceLanguage.JavaScript,
                ".lua" => SourceLanguage.Lua,
                _ => throw new NotSupportedException($"Unsupported extension type: {Path.GetExtension(path)}")
            };
        }
    }
}

public sealed class CarmillaEngine
{
    public static byte[] Encrypt(byte[] data, string passphrase)
    {
        var key = SHA256.HashData(Encoding.UTF8.GetBytes(passphrase));
        var nonce = RandomNumberGenerator.GetBytes(12);
        var tag = new byte[16];
        var cipher = new byte[data.Length];

        using var aes = new AesGcm(key, 16);
        aes.Encrypt(nonce, data, cipher, tag);

        using var ms = new MemoryStream();
        ms.Write(nonce);
        ms.Write(tag);
        ms.Write(cipher);
        return ms.ToArray();
    }

    public static byte[] Decrypt(byte[] encrypted, string passphrase)
    {
        if (encrypted.Length < 28)
        {
            throw new InvalidOperationException("Invalid encrypted payload");
        }

        var key = SHA256.HashData(Encoding.UTF8.GetBytes(passphrase));
        var nonce = encrypted.AsSpan(0, 12).ToArray();
        var tag = encrypted.AsSpan(12, 16).ToArray();
        var cipher = encrypted.AsSpan(28).ToArray();
        var plain = new byte[cipher.Length];

        using var aes = new AesGcm(key, 16);
        aes.Decrypt(nonce, cipher, tag, plain);
        return plain;
    }
}

public sealed class AIClient;
public sealed class UIManager;
public sealed class FileSystem;
public sealed class NetworkManager;

public sealed class PythonToCSharpTranspiler
{
    public string Transpile(string python)
    {
        var escaped = python.Replace("\"", "\"\"");
        return $$"""
using System;
using Sovereign;

public sealed class GeneratedExtension : ExtensionEngine.IExtension
{
    public string Name => "Python Extension";
    public string Version => "1.0.0";
    public string Author => "Transpiler";
    public string Description => "Transpiled from Python";

    public void Initialize(ExtensionEngine.ExtensionContext ctx) { }
    public void Execute(string[] args)
    {
        Console.WriteLine("Python extension placeholder execution.");
        Console.WriteLine(@"{{escaped}}");
    }
    public void Shutdown() { }
    public ExtensionEngine.ExtensionCapabilities GetCapabilities() => ExtensionEngine.ExtensionCapabilities.All;
}
""";
    }
}

public sealed class LuaToCSharpTranspiler
{
    public string Transpile(string lua)
    {
        var escaped = lua.Replace("\"", "\"\"");
        return $$"""
using System;
using Sovereign;

public sealed class GeneratedExtension : ExtensionEngine.IExtension
{
    public string Name => "Lua Extension";
    public string Version => "1.0.0";
    public string Author => "Transpiler";
    public string Description => "Transpiled from Lua";

    public void Initialize(ExtensionEngine.ExtensionContext ctx) { }
    public void Execute(string[] args)
    {
        Console.WriteLine("Lua extension placeholder execution.");
        Console.WriteLine(@"{{escaped}}");
    }
    public void Shutdown() { }
    public ExtensionEngine.ExtensionCapabilities GetCapabilities() => ExtensionEngine.ExtensionCapabilities.All;
}
""";
    }
}

public sealed class JsToCSharpTranspiler
{
    public string Transpile(string js)
    {
        var escaped = js.Replace("\"", "\"\"");
        return $$"""
using System;
using Sovereign;

public sealed class GeneratedExtension : ExtensionEngine.IExtension
{
    public string Name => "JavaScript Extension";
    public string Version => "1.0.0";
    public string Author => "Transpiler";
    public string Description => "Transpiled from JavaScript";

    public void Initialize(ExtensionEngine.ExtensionContext ctx) { }
    public void Execute(string[] args)
    {
        Console.WriteLine("JavaScript extension placeholder execution.");
        Console.WriteLine(@"{{escaped}}");
    }
    public void Shutdown() { }
    public ExtensionEngine.ExtensionCapabilities GetCapabilities() => ExtensionEngine.ExtensionCapabilities.All;
}
""";
    }
}

public sealed class PolymorphicStub
{
    private static uint _seed;

    private static byte PolyRand()
    {
        _seed = _seed * 1103515245 + 12345;
        return (byte)((_seed >> 16) & 0xFF);
    }

    public static void ApplyPolymorphism(byte[] data, uint seed)
    {
        _seed = seed;
        var key = new byte[32];
        for (int i = 0; i < 32; i++) key[i] = PolyRand();

        for (int i = 0; i < data.Length; i++)
        {
            data[i] ^= key[i % 32];
            data[i] = (byte)((data[i] << 3) | (data[i] >> 5));
        }
    }

    public static byte[] GenerateWindowsStub(byte[] payload, string passphrase, uint seed)
    {
        var encrypted = CarmillaEngine.Encrypt(payload, passphrase);

        if (seed == 0)
        {
            seed = (uint)RandomNumberGenerator.GetInt32(int.MaxValue);
        }

        ApplyPolymorphism(encrypted, seed);

        const int headerSize = 0x200; // 512 bytes
        const int configSize = 512;
        var codeSize = Win64StubCode.Length;
        var totalSize = headerSize + codeSize + configSize + encrypted.Length + 1024;

        var stub = new byte[totalSize];
        var span = stub.AsSpan();

        // DOS Header
        span[0] = 0x4D; span[1] = 0x5A; // MZ
        BitConverter.GetBytes((uint)headerSize).CopyTo(span.Slice(0x3C, 4));

        // DOS stub
        var dosStub = new byte[]
        {
            0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD, 0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21,
            0x54, 0x68, 0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72, 0x61, 0x6D, 0x20, 0x63,
            0x61, 0x6E, 0x6E, 0x6F, 0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E, 0x20, 0x69,
            0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20, 0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A, 0x24
        };
        dosStub.CopyTo(span.Slice(2, dosStub.Length));

        // PE Signature + File Header
        var peOffset = headerSize;
        BitConverter.GetBytes(0x00004550u).CopyTo(span.Slice(peOffset, 4)); // PE\0\0
        BitConverter.GetBytes((ushort)0x8664).CopyTo(span.Slice(peOffset + 4, 2)); // AMD64
        BitConverter.GetBytes((ushort)1).CopyTo(span.Slice(peOffset + 6, 2)); // NumberOfSections
        BitConverter.GetBytes((ushort)0x20B).CopyTo(span.Slice(peOffset + 24, 2)); // Magic PE64
        BitConverter.GetBytes(0x140000000ul).CopyTo(span.Slice(peOffset + 40, 8)); // ImageBase
        BitConverter.GetBytes(0x1000u).CopyTo(span.Slice(peOffset + 48, 4)); // SectionAlignment
        BitConverter.GetBytes(0x200u).CopyTo(span.Slice(peOffset + 52, 4)); // FileAlignment
        BitConverter.GetBytes((ushort)3).CopyTo(span.Slice(peOffset + 68, 2)); // Subsystem Console
        BitConverter.GetBytes((ushort)0x0020).CopyTo(span.Slice(peOffset + 70, 2)); // HIGH_ENTROPY_VA
        BitConverter.GetBytes(0x100000ul).CopyTo(span.Slice(peOffset + 72, 8)); // SizeOfStackReserve
        BitConverter.GetBytes(0x100000ul).CopyTo(span.Slice(peOffset + 80, 8)); // SizeOfHeapReserve
        BitConverter.GetBytes((uint)16).CopyTo(span.Slice(peOffset + 88, 4)); // NumberOfRvaAndSizes

        // Section Header
        var secOffset = peOffset + 24 + 224; // after optional header
        var secName = System.Text.Encoding.ASCII.GetBytes(".text\0\0\0");
        secName.CopyTo(span.Slice(secOffset, 8));
        BitConverter.GetBytes((uint)(totalSize - headerSize)).CopyTo(span.Slice(secOffset + 8, 4)); // VirtualSize
        BitConverter.GetBytes(0x1000u).CopyTo(span.Slice(secOffset + 12, 4)); // VirtualAddress
        BitConverter.GetBytes((uint)((totalSize - headerSize + 0x1FF) & ~0x1FF)).CopyTo(span.Slice(secOffset + 16, 4)); // SizeOfRawData
        BitConverter.GetBytes((uint)headerSize).CopyTo(span.Slice(secOffset + 20, 4)); // PointerToRawData
        BitConverter.GetBytes(0x60000020u).CopyTo(span.Slice(secOffset + 36, 4)); // Characteristics

        // Entry point
        BitConverter.GetBytes(0x1000u).CopyTo(span.Slice(peOffset + 16, 4)); // AddressOfEntryPoint
        BitConverter.GetBytes((uint)totalSize).CopyTo(span.Slice(peOffset + 56, 4)); // SizeOfImage
        BitConverter.GetBytes((uint)headerSize).CopyTo(span.Slice(peOffset + 60, 4)); // SizeOfHeaders

        // Stub code
        Win64StubCode.CopyTo(span.Slice(headerSize, codeSize));

        // Patch payload pointer and size into stub code
        var payloadPtrOffset = headerSize + codeSize - 16;
        BitConverter.GetBytes((ulong)(headerSize + codeSize + configSize)).CopyTo(span.Slice(payloadPtrOffset, 8));
        BitConverter.GetBytes((ulong)encrypted.Length).CopyTo(span.Slice(payloadPtrOffset + 8, 8));

        // Config block
        var configOffset = headerSize + codeSize;
        BitConverter.GetBytes(0u).CopyTo(span.Slice(configOffset, 4)); // flags
        var passBytes = System.Text.Encoding.UTF8.GetBytes(passphrase);
        passBytes.AsSpan(0, Math.Min(passBytes.Length, 256)).CopyTo(span.Slice(configOffset + 4, Math.Min(passBytes.Length, 256)));

        // Encrypted payload
        var payloadOffset = headerSize + codeSize + configSize;
        encrypted.CopyTo(span.Slice(payloadOffset, encrypted.Length));

        // Magic marker
        var markerOffset = payloadOffset + encrypted.Length;
        System.Text.Encoding.ASCII.GetBytes("CMLLA").CopyTo(span.Slice(markerOffset, 5));
        BitConverter.GetBytes((uint)encrypted.Length).CopyTo(span.Slice(markerOffset + 5, 4));

        return stub;
    }

    private static ReadOnlySpan<byte> Win64StubCode => new byte[]
    {
        0x48, 0x31, 0xC0,                         // xor rax, rax
        0x48, 0x8B, 0x0D, 0x10, 0x00, 0x00, 0x00, // mov rcx, [rip+payload_ptr]
        0x48, 0x8B, 0x15, 0x0A, 0x00, 0x00, 0x00, // mov rdx, [rip+payload_size]
        0xE8, 0x00, 0x00, 0x00, 0x00,             // call decrypt_and_run
        0xC3,                                     // ret
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, // padding
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // payload_ptr
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // payload_size
    };
}
