using System.Security.Cryptography;
using Xunit;

namespace Sovereign.Tests;

public class PolymorphicStubTests
{
    [Fact]
    public void ApplyPolymorphism_ModifiesPayload()
    {
        var payload = new byte[] { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
        var original = (byte[])payload.Clone();

        PolymorphicStub.ApplyPolymorphism(payload, 0xDEADBEEF);

        Assert.NotEqual(original, payload);
    }

    [Fact]
    public void ApplyPolymorphism_Deterministic_WithSameSeed()
    {
        var payload1 = new byte[] { 0xAB, 0xCD, 0xEF, 0x12, 0x34 };
        var payload2 = (byte[])payload1.Clone();

        PolymorphicStub.ApplyPolymorphism(payload1, 0x12345678);
        PolymorphicStub.ApplyPolymorphism(payload2, 0x12345678);

        Assert.Equal(payload1, payload2);
    }

    [Fact]
    public void GenerateStub_EncryptsAndEmbedsPayload()
    {
        var payload = new byte[] { 0x90, 0x90, 0xCC }; // nop; nop; int3
        var passphrase = "stub-secret";
        var seed = 0xCAFEBABE;

        var stub = PolymorphicStub.GenerateWindowsStub(payload, passphrase, seed);

        Assert.NotNull(stub);
        Assert.True(stub.Length > payload.Length + 512); // headers + payload
    }

    [Fact]
    public void GenerateStub_ContainsMagicMarker()
    {
        var payload = new byte[] { 0x00 };
        var stub = PolymorphicStub.GenerateWindowsStub(payload, "pass", 0);

        var marker = System.Text.Encoding.ASCII.GetBytes("CMLLA");
        bool found = false;
        for (int i = 0; i <= stub.Length - marker.Length; i++)
        {
            if (stub.AsSpan(i, marker.Length).SequenceEqual(marker))
            {
                found = true;
                break;
            }
        }
        Assert.True(found, "Magic marker 'CMLLA' not found in stub");
    }

    [Fact]
    public void GenerateStub_RandomSeed_ProducesDifferentOutput()
    {
        var payload = new byte[] { 0x00, 0x01, 0x02, 0x03 };
        var stub1 = PolymorphicStub.GenerateWindowsStub(payload, "pass", 0);
        var stub2 = PolymorphicStub.GenerateWindowsStub(payload, "pass", 0);

        // When seed==0, random seed is generated internally => outputs should differ
        Assert.NotEqual(stub1, stub2);
    }
}
