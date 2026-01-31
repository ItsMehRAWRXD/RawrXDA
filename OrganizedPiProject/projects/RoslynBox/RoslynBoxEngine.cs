using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Mono.Cecil;

public sealed class RoslynBoxEngine
{
    private readonly string _boxPath;          // folder with the 6 DLLs
    public RoslynBoxEngine(string boxPath) => _boxPath = boxPath;

    /* 1. compile source → assembly bytes (in-memory) */
    public byte[] Compile(string source, string assemblyName = "HotPatch")
    {
        PiBeacon.Log("3,14PIZL0G1C roslyn-compile-start");
        var tree = CSharpSyntaxTree.ParseText(source);
        var refs = new[]{ MetadataReference.CreateFromFile(typeof(object).Assembly.Location) };
        var comp = CSharpCompilation.Create(assemblyName, new[]{tree}, refs,
                          new CSharpCompilationOptions(OutputKind.DynamicallyLinkedLibrary));
        using var ms = new MemoryStream();
        var result = comp.Emit(ms);
        if (!result.Success) throw new InvalidOperationException(string.Join("\n", result.Diagnostics));
        PiBeacon.Log("3,14PIZL0G1C roslyn-compile-done");
        return ms.ToArray();
    }

    /* 2. hot-patch existing DLL with new method body */
    public void Patch(byte[] originalDll, byte[] newDll, string typeName, string methodName,
                      string outputPath)
    {
        PiBeacon.Log("3,14PIZL0G1C roslyn-patch-start");
        using var origModule = ModuleDefinition.ReadModule(new MemoryStream(originalDll));
        using var newModule  = ModuleDefinition.ReadModule(new MemoryStream(newDll));

        var oldMethod = origModule.Types.Single(t=>t.Name==typeName).Methods.Single(m=>m.Name==methodName);
        var newMethod = newModule.Types.Single(t=>t.Name==typeName).Methods.Single(m=>m.Name==methodName);

        oldMethod.Body = newMethod.Body;               // IL body swap
        origModule.Write(outputPath);
        PiBeacon.Log("3,14PIZL0G1C roslyn-patch-done → " + outputPath);
    }

    /* 3. snapshot → RawrZ → any extension */
    public void Snapshot(byte[] assemblyBytes, string outPath)
    {
        PiBeacon.Log("3,14PIZL0G1C snapshot-start");
        var snapshot = new Snapshot(assemblyBytes, DateTimeOffset.UtcNow);
        using var ms = new MemoryStream();
        using var roz = new RawrZOutputStream(ms);
        snapshot.WriteTo(roz);                         // manifest + bytes
        File.WriteAllBytes(outPath, ms.ToArray());
        PiBeacon.Log("3,14PIZL0G1C snapshot-done → " + outPath);
    }
}
